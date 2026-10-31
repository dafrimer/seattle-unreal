---
id: research-2026-10-30-trellis2-prop-test
class: ledger
status: accepted
owner: "@dafrimer"
updated: 2026-10-30
date: 2026-10-30
---

# Image-to-3D props: TRELLIS.2-4B rerun on the same Seattle inputs

**Question:** Does TRELLIS.2-4B do better than TRELLIS v1 ([2026-10-25 test](2026-10-25-trellis-prop-test.md)) on the same five photo crops and SAM 2.1 masks, and does it fit the RTX 3090 Ti?

**Verdict: still not adopted. 1 pass, 2 partial and 2 fails, against the same bar of at least 3 of 5.** TRELLIS.2 moved the trash cans from fail to partial and gave the signal clearer arms and heads. The other results did not change. It fits the GPU easily. The inputs are still the limiting factor.

## Setup

| Item | Value |
| --- | --- |
| Image-to-3D | `microsoft/TRELLIS.2-4B` (MIT), repo commit `75fbf01`. |
| Image encoder | `facebook/dinov3-vitl16-pretrain-lvd1689m`, DINOv3 License. Access is gated and was granted manually by Meta. |
| Background removal | `briaai/RMBG-2.0` (non-commercial) is loaded by the pipeline but was **not used**: the inputs are the RGBA SAM masks, and `preprocess_image` skips RMBG when alpha is present. |
| Inputs and masks | Identical to the v1 test: `assets/2026-10-25-trellis-prop-test/inputs/` and `masks/`. |
| Resolutions | `512` and the default `1024_cascade`, both with seed 1. |
| Hardware | RTX 3090 Ti 24 GB, WSL2; torch 2.6.0 + cu124, flash-attn 2.7.3, flex_gemm sparse conv. |
| Game version | `o_voxel.postprocess.to_glb` with remeshing, decimated to the same per-prop budgets as v1, with a 1024 px texture. |

Scripts, exactly as run: `assets/2026-10-30-trellis2-prop-test/scripts/`. Raw numbers: `report-512.json`, `report-1024_cascade.json`, and `check-nongated.json` from the smoke check run before DINOv3 was approved.

## Results (1024 cascade)

| Object | Generate | Peak torch memory | Raw → game triangles | v1 verdict | TRELLIS.2 verdict |
| --- | --- | --- | --- | --- | --- |
| Globe pedestrian lamp | 47.4 s | 2.7 GB | 542,070 → 2,934 | Pass | **Pass.** Clean silhouette, base and globe from every angle. |
| Traffic-signal pole | 24.4 s | 2.9 GB | 802,920 → 7,898 | Partial | **Partial.** Pole, twin luminaire arms and signal heads are clearer than v1, but detached heads and blade fragments still float beside it. The texture is baked almost black. |
| Twin trash cans | 52.9 s | 3.1 GB | 1,665,882 → 1,836 | Fail | **Partial.** Three separate cans with lids are recognisable. The texture is a smeared photo bake and would need repainting. |
| A-frame sign | 20.0 s | 3.0 GB | 225,670 → 956 | Fail | **Fail.** A single flat panel (depth about 5% of its width) plus a floating fragment, not an A-frame. |
| Planter with tree | 35.0 s | 2.9 GB | 398,956 → 5,786 | Fail | **Fail.** A flat canopy on a stick, surrounded by detached foliage fragments. |

![lamp](assets/2026-10-30-trellis2-prop-test/turntables-1024/lamp.png)
![signal](assets/2026-10-30-trellis2-prop-test/turntables-1024/signal.png)
![trash](assets/2026-10-30-trellis2-prop-test/turntables-1024/trash.png)
![aframe](assets/2026-10-30-trellis2-prop-test/turntables-1024/aframe.png)
![planter](assets/2026-10-30-trellis2-prop-test/turntables-1024/planter.png)

Each turntable shows four shaded views above the matching base-colour views. The 512 run (`contact-512.png`, shaded views only) gives the same verdicts per object.

## What this means

1. **The GPU is not a constraint.** TRELLIS.2's model card asks for at least 24 GB. Its default low-VRAM mode moves one stage onto the GPU at a time, and peak torch-allocated memory stayed at or below 3.1 GB at both resolutions. The 1536 cascade was not tried.
2. **Timings are rough.** At 1024, generation took 20–53 s per prop, 1.1–4.4× slower than v1. The likely cause is low-VRAM mode swapping stages between CPU and GPU; this was not profiled. The 512 run was slower than the 1024 run (27–123 s) because it ran first, from a cold cache and with the DINOv3 download included.
3. **A better model helped on solid objects.** The trash cans went from fail to partial on the same shaded input. Thin structures (blade signs), occluded objects (the A-frame) and foliage still fail, as they did in v1.
4. **The earlier conclusion stands.** The next useful test is clean, single-object, evenly lit photos rather than a third model. TRELLIS.2 is now the model to run them through.

## Licensing notes

- TRELLIS.2 code and weights are MIT. DINOv3 is under Meta's DINOv3 License (custom, gated). Read it before any use of outputs beyond research.
- RMBG-2.0 is non-commercial. It was not run on these inputs. Any future input without an alpha mask would invoke it, unless the pipeline's `rembg_model` is replaced.
- The inputs, masks and the turntables derived from them are adaptations of Panoramax photos by **slinky309**, shared under CC-BY-SA-4.0. The capture IDs are listed in the [2026-10-25 note](2026-10-25-trellis-prop-test.md#licensing-notes).

## Environment pitfalls hit (for whoever reruns this)

- **Keep TRELLIS.2 in its own env.** It needs torch 2.6.0/cu124, while v1 needs 2.4.0 and SAM 2.1 needs 2.5.1. A pip constraint file pins torch, torchvision and triton for every install step.
- **flash-attn:** install the prebuilt `cu12torch2.6cxx11abiFALSE-cp310` wheel from the v2.7.3 GitHub release. Building it from source is slow and memory-heavy.
- **nvdiffrec links `-lcuda`:** in WSL, set `LIBRARY_PATH` to the conda toolkit's `lib/stubs` and `/usr/lib/wsl/lib`.
- **OpenCV 5.0 wheels lack OpenEXR**, so the HDRI env map loads as empty. Pin `opencv-python-headless<5`.
- **transformers 5.x moved `DINOv3ViTModel.layer`**, and TRELLIS.2 iterates `.layer` directly. Pin `transformers>=4.56,<5`.
- **o-voxel declares CuMesh and FlexGEMM as git URLs.** Install it with `--no-deps` after building those two, or pip refetches them.
