"""SAM 2.1 box-prompt masks -> TRELLIS (v1) image-to-3D -> raw and game-budget GLBs, with timing and VRAM evidence."""
import json, os, time
from pathlib import Path
os.environ.setdefault('SPCONV_ALGO', 'native')
os.environ.setdefault('ATTN_BACKEND', 'xformers')
import numpy as np
import torch
from PIL import Image

LAB = Path.home() / 'asset-gen-lab'
WORK = Path('/mnt/h/Projects/asset-gen-lab/work')
OUT = WORK / 'trellis-v1'
OUT.mkdir(parents=True, exist_ok=True)
# Boxes (x0, y0, x1, y1) in input-crop pixels, chosen by inspection of work/inputs/*.png.
TARGETS = {
    'lamp':    (325, 25, 460, 725),
    'signal':  (85, 20, 1010, 930),
    'trash':   (65, 55, 445, 455),
    'aframe':  (172, 88, 378, 418),
    'planter': (0, 0, 768, 768),
}
# Triangle budgets for game-ready props (GTA-style street furniture).
GAME_BUDGET = {'lamp': 3000, 'signal': 8000, 'trash': 2000, 'aframe': 1000, 'planter': 6000}
SAM_CKPT = LAB / 'cache' / 'sam2.1_hiera_large.pt'


def masks():
    from sam2.build_sam import build_sam2
    from sam2.sam2_image_predictor import SAM2ImagePredictor
    if not SAM_CKPT.exists():
        torch.hub.download_url_to_file('https://dl.fbaipublicfiles.com/segment_anything_2/092824/sam2.1_hiera_large.pt', str(SAM_CKPT))
    predictor = SAM2ImagePredictor(build_sam2('configs/sam2.1/sam2.1_hiera_l.yaml', str(SAM_CKPT), device='cuda'))
    result = {}
    for name, box in TARGETS.items():
        rgb = np.asarray(Image.open(WORK / 'inputs' / f'{name}.png').convert('RGB'))
        with torch.inference_mode(), torch.autocast('cuda', dtype=torch.bfloat16):
            predictor.set_image(rgb)
            m, scores, _ = predictor.predict(box=np.array(box), multimask_output=False)
        alpha = (m[0] > 0).astype(np.uint8) * 255
        Image.fromarray(np.dstack([rgb, alpha]), 'RGBA').save(WORK / 'masked' / f'{name}.png')
        result[name] = {'box': box, 'mask_score': float(scores[0]), 'mask_fraction': round(float(alpha.mean() / 255), 4)}
        print('mask', name, result[name], flush=True)
    del predictor
    torch.cuda.empty_cache()
    return result


def generate(report):
    import imageio
    from trellis.pipelines import TrellisImageTo3DPipeline
    from trellis.utils import postprocessing_utils, render_utils
    from huggingface_hub import snapshot_download
    # TRELLIS builds nested repo ids ("org/repo/ckpts/x"), which current huggingface_hub rejects; load from a local snapshot.
    pipe = TrellisImageTo3DPipeline.from_pretrained(snapshot_download('microsoft/TRELLIS-image-large'))
    pipe.cuda()
    for name in TARGETS:
        img = Image.open(WORK / 'masked' / f'{name}.png')
        torch.cuda.reset_peak_memory_stats()
        t0 = time.time()
        out = pipe.run(img, seed=1)
        t_gen = time.time() - t0
        gaussian, mesh = out['gaussian'][0], out['mesh'][0]
        frames = render_utils.render_video(gaussian, num_frames=8)['color']
        imageio.imwrite(OUT / f'{name}-turntable.png', np.concatenate(frames[::2], axis=1))
        raw_faces = int(mesh.faces.shape[0])
        t1 = time.time()
        glb = postprocessing_utils.to_glb(gaussian, mesh, simplify=0.95, texture_size=1024, verbose=False)
        glb.export(OUT / f'{name}.glb')
        t_glb = time.time() - t1
        budget = GAME_BUDGET[name]
        game = postprocessing_utils.to_glb(gaussian, mesh, simplify=max(0.95, 1 - budget / raw_faces), texture_size=512, verbose=False)
        game.export(OUT / f'{name}-game.glb')
        report[name].update({
            'generate_seconds': round(t_gen, 1), 'glb_seconds': round(t_glb, 1),
            'peak_vram_gb': round(torch.cuda.max_memory_allocated() / 2**30, 2),
            'raw_mesh_triangles': raw_faces, 'glb_triangles': int(len(glb.faces)),
            'game_triangles': int(len(game.faces)), 'game_budget': budget,
            'extent_units': [round(float(x), 3) for x in glb.extents],
        })
        print(name, report[name], flush=True)

if __name__ == '__main__':
    import sys
    stage = sys.argv[sys.argv.index('--stage') + 1]
    (WORK / 'masked').mkdir(exist_ok=True)
    mask_report = WORK / 'masked' / 'masks.json'
    if stage == 'masks':
        mask_report.write_text(json.dumps(masks(), indent=2) + '\n')
        print('MASKS_COMPLETE')
    else:
        report = json.loads(mask_report.read_text())
        generate(report)
        report['_meta'] = {'trellis_commit': (LAB / 'trellis.commit').read_text().strip(),
                           'sam2_commit': (LAB / 'sam2.commit').read_text().strip(),
                           'gpu': torch.cuda.get_device_name(0),
                           'model': 'microsoft/TRELLIS-image-large (MIT)', 'masks': 'SAM 2.1 hiera-large (Apache-2.0)',
                           'inputs': 'Panoramax CC-BY-SA-4.0 crops; see work/inputs and scripts/inputs.sh'}
        (OUT / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
        print('TRELLIS_TEST_COMPLETE')
