#!/usr/bin/env bash
# TRELLIS.2 env, kept separate from the TRELLIS v1 (torch 2.4) and SAM (torch 2.5) envs.
# Follows vendor/TRELLIS.2/setup.sh (torch 2.6.0/cu124) with three deviations:
#   - prefix env under $LAB, CUDA 12.4 toolkit from conda (no system CUDA in WSL)
#   - a pip constraint file pins torch/torchvision/triton so no later step can drift them
#   - skips `sudo apt install libjpeg-dev` + pillow-simd (speed-only; stock Pillow works)
set -euo pipefail
trap 'echo EXIT=$?' EXIT
LAB="$HOME/asset-gen-lab"
ENV="$LAB/env-t2"
T2="$LAB/vendor/TRELLIS.2"
EXT="$LAB/vendor/t2-ext"
CONDA=/mnt/h/Projects/seattle-world/.runtime/miniforge/bin/conda
export CONDA_PKGS_DIRS="$LAB/cache/conda-pkgs" HF_HOME="$LAB/cache/hf" TORCH_HOME="$LAB/cache/torch" PIP_CACHE_DIR="$LAB/cache/pip"

echo "=== env"
if [ ! -x "$ENV/bin/python" ]; then "$CONDA" create -y -p "$ENV" python=3.10; fi
if [ ! -x "$ENV/bin/nvcc" ]; then "$CONDA" install -y -p "$ENV" -c nvidia/label/cuda-12.4.1 cuda-toolkit; fi
source "$(dirname "$CONDA")/activate" "$ENV"
export CUDA_HOME="$ENV" TORCH_CUDA_ARCH_LIST="8.6" MAX_JOBS=12
# nvdiffrec links -lcuda; the toolkit ships a stub, and WSL provides the real driver lib at runtime.
export LIBRARY_PATH="$ENV/lib/stubs:/usr/lib/wsl/lib${LIBRARY_PATH:+:$LIBRARY_PATH}"

cat > "$LAB/t2-constraints.txt" <<'EOF'
torch==2.6.0
torchvision==0.21.0
triton==3.2.0
EOF
export PIP_CONSTRAINT="$LAB/t2-constraints.txt"

echo "=== torch"
pip install torch==2.6.0 torchvision==0.21.0 --index-url https://download.pytorch.org/whl/cu124

echo "=== basic"
# OpenCV 5.0 wheels ship without OpenEXR; the renderer loads its HDRI env maps as .exr.
# transformers 5.x moved DINOv3ViTModel.layer under .model; TRELLIS.2 iterates .layer directly (4.56+ has DINOv3).
pip install imageio imageio-ffmpeg tqdm easydict "opencv-python-headless<5" ninja trimesh "transformers>=4.56,<5" \
  gradio==6.0.1 tensorboard pandas lpips zstandard plyfile kornia timm
pip install git+https://github.com/EasternJournalist/utils3d.git@9a4eb15e4021b67b12c460c7057d642626897ec8

echo "=== flash-attn (prebuilt wheel matching torch 2.6 / cu12 / cxx11abi=FALSE / cp310)"
pip install "https://github.com/Dao-AILab/flash-attention/releases/download/v2.7.3/flash_attn-2.7.3+cu12torch2.6cxx11abiFALSE-cp310-cp310-linux_x86_64.whl"

echo "=== compiled extensions"
mkdir -p "$EXT"
clone() { [ -d "$EXT/$2" ] || git clone -q --recursive $3 "$1" "$EXT/$2"; echo "$2 $(git -C "$EXT/$2" rev-parse HEAD)" >> "$EXT/commits.txt"; }
: > "$EXT/commits.txt"
clone https://github.com/NVlabs/nvdiffrast.git nvdiffrast "-b v0.4.0"
clone https://github.com/JeffreyXiang/nvdiffrec.git nvdiffrec "-b renderutils"
clone https://github.com/JeffreyXiang/CuMesh.git CuMesh ""
clone https://github.com/JeffreyXiang/FlexGEMM.git FlexGEMM ""
for d in nvdiffrast nvdiffrec CuMesh FlexGEMM; do pip install "$EXT/$d" --no-build-isolation; done
# o-voxel lists cumesh/flex_gemm as git URLs; --no-deps keeps the builds above instead of refetching.
rm -rf "$EXT/o-voxel" && cp -r "$T2/o-voxel" "$EXT/o-voxel"
pip install "$EXT/o-voxel" --no-build-isolation --no-deps

echo "=== verify"
python - <<'PY'
import importlib, torch
print("t2-env torch", torch.__version__, "cuda", torch.version.cuda, "gpu", torch.cuda.get_device_name(0))
assert torch.__version__.startswith("2.6.0"), "torch drifted"
for m in ["flash_attn", "nvdiffrast.torch", "nvdiffrec_render", "cumesh", "flex_gemm", "o_voxel", "utils3d", "transformers"]:
    try:
        importlib.import_module(m); print("ok", m)
    except Exception as e:
        print("FAIL", m, type(e).__name__, e)
PY
pip check | grep -iE 'torch|triton|nvidia|flash' || true
echo LAB_T2_SETUP_COMPLETE
