---
id: adr-0005-target-stylized-game-art-not-photoreal-capture
class: ledger
status: accepted
owner: "@dafrimer"
updated: 2026-10-03
date: 2026-10-03
supersedes: 0003-use-amb3r-for-validation-not-geometry
---

# 0005. Target stylized, game-ready art (GTA V-like), not photoreal capture; drop video reconstruction from the pipeline

## Context

Five photoreal experiments in the sibling `seattle-world` project failed their quality gates (`seattle-world/runs/*/evaluation/report.json`). They used Gaussian splats from 15 panoramas, with and without diffusion fill. Coverage fell or stayed below 0.85, holdout PSNR peaked at about 15 dB, and visual review found smearing, haze and doubled structures in every run. The proposed remedy was dense video, AMB3R poses and further splat fitting. That keeps investing in photorealism.

The product goal has since been stated: the expanded district only needs to **extend** the map. Its final look is stylized, game-ready art comparable to GTA V, which is modular authored assets on real street layouts, not a reconstruction of real facades. ADR 0003 kept AMB3R in the plan as a measurement and validation tool. Under this goal, video-measured heights and setbacks are not needed. OSM footprints and levels give the layout and scale a stylized district requires.

## Decision

We will target stylized, game-ready art for all expansion content:

- modular kit meshes with tiling textures or trim sheets
- material palettes and authored props
- plausible, not measured, facades

Photoreal reconstruction (splats, diffusion view synthesis, AMB3R/AMB3R-SLAM video capture) is out of the pipeline. ADR 0003 is superseded, and the `amb3r-validation-pilot` story is dropped.

Street imagery (ADR 0002) is used only to choose a fitting style class per building (material family, colour family, storefront type), not to reproduce specific facades. ADRs 0001 (PCG from OSM with a facade kit) and 0004 (hero landmarks) stand unchanged.

## Alternatives considered

### Continue photoreal reconstruction with dense video (the remedy plan)

It might reach usable quality where filmed. It lost because it optimises for realism the target art style throws away. It also needs footage of every block, and it still leaves gaps that need generative fill, which is the failure mode already measured five times.

### Keep AMB3R as a validation-only tool (ADR 0003)

It is an independent height and setback check. It lost because stylized art tolerates the metre-level errors it would catch. Its costs remain: a non-commercial license, unreleased SLAM code, a separate environment, and capture effort.

### Do nothing (leave ADR 0003 and the pilot story in place)

The plan would keep pointing at a research track that cannot change the shipped look. Future work would spend effort on it.

## Consequences

### Accepting

- Expansion is bounded by kit quality and generator coverage, not by capture or reconstruction quality.
- No research-only, non-commercially licensed models are on the path to shipping.
- The art bar is explicit: readable, consistent, game-like. Review compares generated blocks against the kit and the hero landmarks, not against photographs.

### Costs

- Generated buildings will not resemble their real counterparts beyond footprint, height and broad style class.
- The kit's art quality becomes the single biggest lever and needs real art time.
- The existing hero landmarks and renderer settings (Lumen, Nanite, physically based materials) may need a later art-direction pass to match the stylized kit. That needs its own story or ADR.
