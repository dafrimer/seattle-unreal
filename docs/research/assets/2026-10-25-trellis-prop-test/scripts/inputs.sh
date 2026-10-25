set -e
PY=/mnt/h/Projects/seattle-world/.runtime/fit/bin/python
S=/mnt/h/Tools/Seattle3D/Checks/seattle-recovery-mmh2glhd/sources
O=/mnt/h/Projects/asset-gen-lab/work/inputs
C=/mnt/h/Projects/asset-gen-lab/scripts/pano_crop.py
$PY $C $S/f63ca3da-3c7a-4e7e-adb9-c96f8437e391.jpg $O/lamp.png    --yaw 248.5 --pitch 5  --fov 30 --size 768
$PY $C $S/f5f61cb3-6a46-44e5-8aa1-17e5c4f191f5.jpg $O/signal.png  --yaw 229.5 --pitch 14 --fov 52 --size 1024
$PY $C $S/d40a2fee-7aff-4218-bed2-fe8ca463878b.jpg $O/trash.png   --yaw 60    --pitch -7.5 --fov 11 --size 512
$PY $C $S/73f1007f-b3d0-4689-8dac-9c193f658500.jpg $O/aframe.png  --yaw 117   --pitch -3.5 --fov 10 --size 512
$PY $C $S/5281e919-8e15-4e00-9b99-681d051a0453.jpg $O/planter.png --yaw 295   --pitch -4 --fov 24 --size 768
