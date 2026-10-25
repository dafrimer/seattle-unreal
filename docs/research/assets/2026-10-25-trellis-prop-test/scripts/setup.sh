#!/usr/bin/env bash
# Isolated TRELLIS (v1, MIT) + SAM 2.1 (Apache-2.0) environment for the prop-generation test.
# Nothing is installed globally; everything lives under $LAB.
set -euo pipefail
LAB="$HOME/asset-gen-lab"
CONDA=/mnt/h/Projects/seattle-world/.runtime/miniforge/bin/conda
mkdir -p "$LAB"/{vendor,cache,out}
export CONDA_PKGS_DIRS="$LAB/cache/conda-pkgs" HF_HOME="$LAB/cache/hf" TORCH_HOME="$LAB/cache/torch" PIP_CACHE_DIR="$LAB/cache/pip"
cd "$LAB/vendor"
[ -d TRELLIS ] || git clone --recursive https://github.com/microsoft/TRELLIS.git
[ -d sam2 ] || git clone https://github.com/facebookresearch/sam2.git
git -C TRELLIS rev-parse HEAD > "$LAB/trellis.commit"
git -C sam2 rev-parse HEAD > "$LAB/sam2.commit"

if [ ! -x "$LAB/env/bin/python" ]; then
  "$CONDA" create -y -p "$LAB/env" python=3.10
  "$CONDA" install -y -p "$LAB/env" -c nvidia/label/cuda-12.1.1 cuda-toolkit
fi
source "$(dirname "$CONDA")/activate" "$LAB/env"
export CUDA_HOME="$LAB/env" TORCH_CUDA_ARCH_LIST="8.6" MAX_JOBS=12
pip install torch==2.4.0 torchvision==0.19.0 --index-url https://download.pytorch.org/whl/cu121
cd "$LAB/vendor/TRELLIS"
# setup.sh flags per the TRELLIS README; --new-env omitted because we manage the env above.
. ./setup.sh --basic --xformers --flash-attn --diffoctreerast --spconv --mipgaussian --kaolin --nvdiffrast
pip install -e "$LAB/vendor/sam2"
python - <<'PY'
import torch, importlib
print("torch", torch.__version__, "cuda", torch.version.cuda, "gpu", torch.cuda.get_device_name(0))
for m in ["xformers","flash_attn","spconv","kaolin","nvdiffrast.torch","diffoctreerast","diff_gaussian_rasterization","sam2"]:
    importlib.import_module(m); print("ok", m)
PY
echo LAB_SETUP_COMPLETE
