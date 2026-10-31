#!/usr/bin/env bash
# Two envs: TRELLIS (torch 2.4.0/cu121, pinned by kaolin/xformers) and SAM 2.1 (needs torch>=2.5.1).
set -euo pipefail
LAB="$HOME/asset-gen-lab"
CONDA=/mnt/h/Projects/seattle-world/.runtime/miniforge/bin/conda
export CONDA_PKGS_DIRS="$LAB/cache/conda-pkgs" HF_HOME="$LAB/cache/hf" TORCH_HOME="$LAB/cache/torch" PIP_CACHE_DIR="$LAB/cache/pip"

# 1. Repair the TRELLIS env.
source "$(dirname "$CONDA")/activate" "$LAB/env"
pip uninstall -y SAM-2
pip install --force-reinstall --no-deps torch==2.4.0 torchvision==0.19.0 --index-url https://download.pytorch.org/whl/cu121
python - <<'PY'
import torch, importlib
print("trellis-env torch", torch.__version__, torch.version.cuda)
for m in ["xformers","spconv","kaolin","nvdiffrast.torch","diffoctreerast","diff_gaussian_rasterization","trimesh","xatlas","utils3d"]:
    importlib.import_module(m); print("ok", m)
PY
conda deactivate

# 2. Separate SAM 2.1 env (Python-only build; masks are written to disk).
if [ ! -x "$LAB/env-sam/bin/python" ]; then "$CONDA" create -y -p "$LAB/env-sam" python=3.10; fi
source "$(dirname "$CONDA")/activate" "$LAB/env-sam"
pip install torch==2.5.1 torchvision==0.20.1 --index-url https://download.pytorch.org/whl/cu121
SAM2_BUILD_CUDA=0 pip install -e "$LAB/vendor/sam2"
python -c "import torch, sam2; print('sam-env torch', torch.__version__, 'ok sam2')"
echo LAB_SETUP_COMPLETE
