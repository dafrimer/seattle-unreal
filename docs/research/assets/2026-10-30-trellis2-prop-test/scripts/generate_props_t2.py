"""TRELLIS.2-4B image-to-3D on the same SAM 2.1 masks as the v1 test -> raw and game-budget GLBs, turntables, timing, VRAM.

Runs in env-t2 (torch 2.6.0/cu124). Masks come from `generate_props.py --stage masks` (env-sam).
  --check            load every non-gated component and exercise the GPU extensions (works before DINOv3 approval)
  --res 512|1024_cascade   pipeline resolution for the full run (default 512, the cheapest)
"""
import argparse, json, os, subprocess, time
from pathlib import Path
os.environ.setdefault('OPENCV_IO_ENABLE_OPENEXR', '1')
os.environ.setdefault('PYTORCH_CUDA_ALLOC_CONF', 'expandable_segments:True')
import numpy as np
import torch
from PIL import Image

LAB = Path.home() / 'asset-gen-lab'
T2 = LAB / 'vendor' / 'TRELLIS.2'
WORK = Path('/mnt/h/Projects/asset-gen-lab/work')
REPO = 'microsoft/TRELLIS.2-4B'
NAMES = ['lamp', 'signal', 'trash', 'aframe', 'planter']
# Same game budgets as the v1 test so the two runs compare directly.
GAME_BUDGET = {'lamp': 3000, 'signal': 8000, 'trash': 2000, 'aframe': 1000, 'planter': 6000}


def gib(n):
    return round(n / 2**30, 2)


def check():
    """Everything except the DINOv3 image encoder: flow models + decoders on GPU, RMBG on a real input, mesh extensions."""
    from trellis2.pipelines import Trellis2ImageTo3DPipeline, rembg
    from trellis2.pipelines.base import Pipeline
    import cumesh, flex_gemm, nvdiffrast.torch, o_voxel  # noqa: F401  compiled extensions must import against torch 2.6
    result = {'torch': torch.__version__, 'gpu': torch.cuda.get_device_name(0)}
    t0 = time.time()
    pipe = Pipeline.from_pretrained.__func__(Trellis2ImageTo3DPipeline, REPO)
    result['models_loaded'] = sorted(pipe.models)
    result['load_seconds'] = round(time.time() - t0, 1)
    torch.cuda.reset_peak_memory_stats()
    for m in pipe.models.values():
        m.cuda()
    result['all_flow_models_on_gpu_gb'] = gib(torch.cuda.memory_allocated())
    for m in pipe.models.values():
        m.cpu()
    torch.cuda.empty_cache()
    seg = rembg.BiRefNet(model_name='briaai/RMBG-2.0')
    seg.cuda()
    cut = seg(Image.open(WORK / 'inputs' / 'lamp.png').convert('RGB'))
    alpha = np.asarray(cut)[:, :, 3]
    result['rmbg_foreground_fraction'] = round(float((alpha > 127).mean()), 3)
    result['peak_vram_gb'] = gib(torch.cuda.max_memory_allocated())
    out = WORK / 'trellis2-check'
    out.mkdir(exist_ok=True)
    cut.save(out / 'lamp-rmbg.png')
    (out / 'check.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))
    print('T2_CHECK_COMPLETE')


def generate(res):
    import cv2, imageio
    from trellis2.pipelines import Trellis2ImageTo3DPipeline
    from trellis2.renderers import EnvMap
    from trellis2.utils import render_utils
    import o_voxel
    out = WORK / f'trellis2-{res}'
    out.mkdir(exist_ok=True)
    report = json.loads((WORK / 'masked' / 'masks.json').read_text())
    envmap = EnvMap(torch.tensor(cv2.cvtColor(cv2.imread(str(T2 / 'assets/hdri/forest.exr'), cv2.IMREAD_UNCHANGED), cv2.COLOR_BGR2RGB),
                                 dtype=torch.float32, device='cuda'))
    pipe = Trellis2ImageTo3DPipeline.from_pretrained(REPO)
    pipe.cuda()
    for name in NAMES:
        # Masked PNGs carry alpha, so preprocess_image uses the SAM mask instead of re-running RMBG.
        img = Image.open(WORK / 'masked' / f'{name}.png')
        torch.cuda.reset_peak_memory_stats()
        t0 = time.time()
        mesh = pipe.run(img, seed=1, pipeline_type=res)[0]
        t_gen = time.time() - t0
        peak_gen = gib(torch.cuda.max_memory_allocated())
        mesh.simplify(16777216)  # nvdiffrast limit, as in example.py
        raw_faces = int(mesh.faces.shape[0])
        frames = render_utils.render_video(mesh, envmap=envmap, num_frames=8, resolution=512)
        views = range(0, 8, 2)
        imageio.imwrite(out / f'{name}-turntable.png', np.concatenate([
            np.concatenate([frames['shaded'][i] for i in views], axis=1),
            np.concatenate([frames['base_color'][i] for i in views], axis=1)], axis=0))
        exports = {}
        for label, target, tex in (('', 200000, 2048), ('-game', GAME_BUDGET[name], 1024)):
            t1 = time.time()
            glb = o_voxel.postprocess.to_glb(
                vertices=mesh.vertices, faces=mesh.faces, attr_volume=mesh.attrs, coords=mesh.coords,
                attr_layout=mesh.layout, voxel_size=mesh.voxel_size, aabb=[[-0.5, -0.5, -0.5], [0.5, 0.5, 0.5]],
                decimation_target=target, texture_size=tex, remesh=True, remesh_band=1, remesh_project=0)
            glb.export(out / f'{name}{label}.glb', extension_webp=True)
            exports[label] = (glb, round(time.time() - t1, 1))
        glb, t_glb = exports['']
        game, _ = exports['-game']
        report[name].update({
            'generate_seconds': round(t_gen, 1), 'glb_seconds': t_glb,
            'peak_vram_gb': max(peak_gen, gib(torch.cuda.max_memory_allocated())), 'peak_vram_generate_gb': peak_gen,
            'raw_mesh_triangles': raw_faces, 'glb_triangles': int(len(glb.faces)),
            'game_triangles': int(len(game.faces)), 'game_budget': GAME_BUDGET[name],
            'extent_units': [round(float(x), 3) for x in glb.extents],
        })
        print(name, report[name], flush=True)
    commit = subprocess.run(['git', '-C', str(T2), 'rev-parse', 'HEAD'], capture_output=True, text=True).stdout.strip()
    report['_meta'] = {'trellis2_commit': commit, 'pipeline_type': res, 'gpu': torch.cuda.get_device_name(0), 'torch': torch.__version__,
                       'model': f'{REPO} (MIT); image encoder facebook/dinov3-vitl16-pretrain-lvd1689m (DINOv3 License)',
                       'masks': 'SAM 2.1 hiera-large (Apache-2.0), reused from the v1 test',
                       'inputs': 'Panoramax CC-BY-SA-4.0 crops; see work/inputs and scripts/inputs.sh'}
    (out / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    print('TRELLIS2_TEST_COMPLETE')


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('--check', action='store_true')
    ap.add_argument('--res', default='512', choices=['512', '1024', '1024_cascade', '1536_cascade'])
    args = ap.parse_args()
    check() if args.check else generate(args.res)
