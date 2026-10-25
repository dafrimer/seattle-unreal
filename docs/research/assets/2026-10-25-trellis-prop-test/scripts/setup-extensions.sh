#!/usr/bin/env bash
# Same sources as TRELLIS setup.sh, built with --no-build-isolation so the env's torch is visible.
set -euo pipefail
LAB="$HOME/asset-gen-lab"
CONDA=/mnt/h/Projects/seattle-world/.runtime/miniforge/bin/conda
export CONDA_PKGS_DIRS="$LAB/cache/conda-pkgs" HF_HOME="$LAB/cache/hf" TORCH_HOME="$LAB/cache/torch" PIP_CACHE_DIR="$LAB/cache/pip"
source "$(dirname "$CONDA")/activate" "$LAB/env"
export CUDA_HOME="$LAB/env" TORCH_CUDA_ARCH_LIST="8.6" MAX_JOBS=12
pip install setuptools wheel ninja
X="$LAB/vendor/ext"; mkdir -p "$X"; cd "$X"
[ -d diffoctreerast ] || git clone --recurse-submodules https://github.com/JeffreyXiang/diffoctreerast.git
[ -d mip-splatting ] || git clone https://github.com/autonomousvision/mip-splatting.git
[ -d nvdiffrast ] || git clone https://github.com/NVlabs/nvdiffrast.git
for d in diffoctreerast mip-splatting nvdiffrast; do git -C "$d" rev-parse HEAD > "$LAB/$d.commit"; done
pip install --no-build-isolation ./diffoctreerast
pip install --no-build-isolation ./mip-splatting/submodules/diff-gaussian-rasterization/
pip install --no-build-isolation ./nvdiffrast
pip install spconv-cu120
pip install kaolin -f https://nvidia-kaolin.s3.us-east-2.amazonaws.com/torch-2.4.0_cu121.html
pip install --no-build-isolation -e "$LAB/vendor/sam2"
python - <<'PY'
import torch, importlib
print("torch", torch.__version__, "cuda", torch.version.cuda, "gpu", torch.cuda.get_device_name(0))
for m in ["xformers","spconv","kaolin","nvdiffrast.torch","diffoctreerast","diff_gaussian_rasterization","sam2","trimesh","xatlas","utils3d"]:
    importlib.import_module(m); print("ok", m)
PY
echo LAB_SETUP_COMPLETE
