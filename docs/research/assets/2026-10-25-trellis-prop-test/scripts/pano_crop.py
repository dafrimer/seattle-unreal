"""Undistorted perspective crop from an equirectangular panorama: yaw/pitch/fov in degrees."""
import argparse
from pathlib import Path
import numpy as np
from PIL import Image

p = argparse.ArgumentParser()
p.add_argument('pano'); p.add_argument('out')
p.add_argument('--yaw', type=float, required=True); p.add_argument('--pitch', type=float, default=0)
p.add_argument('--fov', type=float, default=30); p.add_argument('--size', type=int, default=1024)
a = p.parse_args()
img = np.asarray(Image.open(a.pano).convert('RGB')); H, W = img.shape[:2]
n = a.size; f = 0.5 * n / np.tan(np.radians(a.fov) / 2)
u, v = np.meshgrid(np.arange(n) - n / 2 + 0.5, np.arange(n) - n / 2 + 0.5)
d = np.stack([u, -v, np.full_like(u, f)], -1); d /= np.linalg.norm(d, axis=-1, keepdims=True)
cp, sp = np.cos(np.radians(a.pitch)), np.sin(np.radians(a.pitch))
x, y, z = d[..., 0], d[..., 1] * cp + d[..., 2] * sp, -d[..., 1] * sp + d[..., 2] * cp
lon = np.degrees(np.arctan2(x, z)) + a.yaw; lat = np.degrees(np.arcsin(np.clip(y, -1, 1)))
px = ((lon % 360) / 360 * W).astype(int) % W; py = np.clip(((90 - lat) / 180 * H).astype(int), 0, H - 1)
Path(a.out).parent.mkdir(parents=True, exist_ok=True)
Image.fromarray(img[py, px]).save(a.out)
print(a.out, W, H, 'px/deg', W / 360)
