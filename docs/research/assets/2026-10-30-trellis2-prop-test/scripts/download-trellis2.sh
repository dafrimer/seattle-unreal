#!/usr/bin/env bash
# Pre-fetch every TRELLIS.2 checkpoint that is not gated behind Meta's DINOv3 approval.
set -euo pipefail
LAB="$HOME/asset-gen-lab"
export HF_HOME="$LAB/cache/hf" HF_HUB_DISABLE_PROGRESS_BARS=1
HF="$LAB/env/bin/hf"
"$HF" download microsoft/TRELLIS.2-4B > /dev/null && echo T2-OK
"$HF" download microsoft/TRELLIS-image-large --include 'ckpts/ss_dec_conv3d_16l8_fp16*' > /dev/null && echo SSDEC-OK
"$HF" download briaai/RMBG-2.0 > /dev/null && echo RMBG-OK
du -sh "$HF_HOME"/hub/models--microsoft--TRELLIS.2-4B "$HF_HOME"/hub/models--briaai--RMBG-2.0
