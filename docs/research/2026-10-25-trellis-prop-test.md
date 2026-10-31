---
id: research-2026-10-25-trellis-prop-test
class: ledger
status: accepted
owner: "@dafrimer"
updated: 2026-10-25
date: 2026-10-25
---

# Image-to-3D props from existing Seattle photos: SAM 2.1 + TRELLIS test

**Question:** Can open image-to-3D models turn objects cut out of our street photos into game-ready props for the stylized district (ADR 0005)? This decides whether a prop-generation step is worth adding to the pipeline.

**Verdict: not yet. 1 pass, 1 partial and 3 fails, against a bar of at least 3 of 5 clean, recognisable props.** The model is not the limiting factor. When the input crop was well lit, unobstructed and showed one whole object, TRELLIS produced a recognisable game-ready lamp in about 30 s on our GPU. Every failure traces to the input photo.

## Setup

| Item | Value |
| --- | --- |
| Masks | SAM 2.1 hiera-large (Apache-2.0, ungated), box prompts. SAM 3 is manually gated, so it was not used. |
| Image-to-3D | Microsoft TRELLIS v1, `microsoft/TRELLIS-image-large` (MIT, ungated), repo commit `442aa1e`. |
| Not tested | TRELLIS.2-4B (MIT). It loads DINOv3, which needs manual approval from Meta, and its default background remover, RMBG-2.0, is non-commercial. |
| Hardware | RTX 3090 Ti 24 GB in WSL2 Ubuntu 22.04; torch 2.4.0 + cu121, xformers attention. |
| Inputs | Undistorted perspective crops from the 12 Panoramax panoramas already used in this project (16 px per degree). |
| Game version | TRELLIS `to_glb`, simplified to a per-prop triangle budget, with a 512 px baked texture. |

Scripts, exactly as run: `assets/2026-10-25-trellis-prop-test/scripts/`. Raw numbers: `assets/2026-10-25-trellis-prop-test/report.json`.

## Results

| Object | Mask score | Generate | Peak GPU memory | Raw → game triangles | Verdict |
| --- | --- | --- | --- | --- | --- |
| Globe pedestrian lamp | 0.94 | 11.5 s | 9.4 GB | 40,472 → 1,186 | **Pass.** Correct silhouette from every angle. |
| Traffic-signal pole | 0.73 | 10.9 s | 9.6 GB | 177,744 → 7,790 | **Partial.** Correct pole, mast arms and heads. SAM dropped the street-name blades, which left floating fragments and a smeared texture. |
| Twin trash cans | 0.96 | 11.9 s | 9.8 GB | 269,824 → 932 | **Fail.** The input is in deep shade, so little geometry was recovered. |
| A-frame sign | 0.97 | 17.5 s | 9.7 GB | 208,710 → 684 | **Fail.** A pedestrian occludes it, and the output is a flat, door-like panel. |
| Planter with tree | 0.65 | 10.3 s | 9.7 GB | 229,096 → 6,000 | **Fail.** Foliage within a scene is not a single object; the output is a flat billboard. |

Turntables (four views each), in the same order:

![lamp](assets/2026-10-25-trellis-prop-test/turntables/lamp.png)
![signal](assets/2026-10-25-trellis-prop-test/turntables/signal.png)
![trash](assets/2026-10-25-trellis-prop-test/turntables/trash.png)
![aframe](assets/2026-10-25-trellis-prop-test/turntables/aframe.png)
![planter](assets/2026-10-25-trellis-prop-test/turntables/planter.png)

Inputs and masks are in `assets/2026-10-25-trellis-prop-test/inputs/` and `masks/`. The signal and A-frame masks show the loss of the street-name blades and the occlusion gap.

## In Unreal

The lamp and signal game versions were imported as GLB into the native map, scaled to the hand-authored props' heights (588 cm and 587 cm), and placed beside `SM_prop_lamp` and `SM_prop_signal`. The editor renders showed:

- **Lamp:** the shape holds up, but the texture baked in the photo's shade, so the lamp renders nearly black with a purple globe.
- **Signal:** an oversized dark blob that does not read as a signal.

Nothing was saved to the project. Headless screenshot framing proved unreliable, so no in-engine image is committed; the turntables are the evidence of record.

## What this means

1. **Input quality is the bottleneck.** Panoramas taken from a car roof give small, shaded, occluded objects. Clean single-object photos of benches, lamps, bins and signs, taken in even light, are the input this needs.
2. **Textures need the stylization pass regardless.** Baked photo lighting conflicts with the stylized target (ADR 0005). Generated geometry plus palette or trim-sheet materials is the realistic use.
3. **It fits the hardware.** Peak GPU memory was under 10 GB, and each prop took about 30 s end to end, including both GLB exports.
4. **Thin structures and foliage are out of scope.** Use authored kit pieces or existing tree assets instead.

## Licensing notes

- **Code and weights:** TRELLIS v1 code and weights are MIT, and SAM 2.1 is Apache-2.0.
- **Inputs:** the crops come from Panoramax photos by **slinky309**, CC-BY-SA-4.0, captured 2025-08-01: `f63ca3da-3c7a-4e7e-adb9-c96f8437e391` (lamp), `f5f61cb3-6a46-44e5-8aa1-17e5c4f191f5` (signal), `d40a2fee-7aff-4218-bed2-fe8ca463878b` (trash), `73f1007f-b3d0-4689-8dac-9c193f658500` (A-frame), `5281e919-8e15-4e00-9b99-681d051a0453` (planter).
- **Derived images:** the inputs, masks and turntables committed here are adaptations, shared under CC-BY-SA-4.0.

## Environment pitfalls hit (for whoever reruns this)

- **SAM 2.1 vs TRELLIS torch versions:** SAM 2.1 requires torch ≥ 2.5.1, while TRELLIS's prebuilt kaolin and xformers wheels pin torch 2.4.0. Installing both in one env silently upgraded torch to 2.14 and broke every compiled extension. Use two environments; the masks pass between them as PNG files.
- **Build isolation:** TRELLIS `setup.sh` builds extensions without `--no-build-isolation`. Under current pip they cannot see torch, so install them directly with `--no-build-isolation`.
- **Checkpoint loading:** TRELLIS `from_pretrained` swallows errors with a bare `except`, and the fallback then reports a misleading 401 on a nested Hugging Face repo id. Load from a local `snapshot_download`.
- **Triton:** pin `triton==3.0.0` for torch 2.4.0.
