---
id: adr-0003-use-amb3r-for-validation-not-geometry
class: ledger
status: superseded
superseded_by: 0005-target-stylized-game-art-not-photoreal-capture
owner: "@dafrimer"
updated: 2026-10-03
date: 2026-10-03
---

# 0003. Use AMB3R video reconstruction to measure and validate, not to supply runtime geometry

## Context

AMB3R (CVPR 2026) recovers per-frame camera poses, dense pointmaps and a metric-scale estimate from walking video, with no intrinsics required. AMB3R-SLAM (arXiv 2609.19518) adds loop closure, robustness to moving objects, and a bounded 10–14 GB peak on a 24 GB GPU. Full findings and sources are in `docs/research/2026-10-02-amb3r-video-to-world.md`.

Facts that bound its role:

- **Output:** point clouds only. The SLAM paper names duplicated surfaces and ghosting in ambiguous regions, such as repetitive facades.
- **No meshes, and no COLMAP export** (requested upstream, still open).
- **Code license:** none; the repo has `license: null`.
- **Weights:** expected to be non-commercial.
- **SLAM availability:** code is unreleased as of 2026-10-03.
- **Coverage:** only covers what was filmed.

Unreal walking needs watertight, collision-ready meshes over the whole expansion area (ADR 0001).

## Decision

We will use AMB3R (and AMB3R-SLAM once released) as a measurement and validation tool, in a research-only pilot. It will:

- measure building heights, setback positions and facade planes where we have footage
- check OSM-derived geometry and generated buildings against those measurements

Its point clouds and weights do not ship in the map, and the expansion never waits on footage.

## Alternatives considered

### AMB3R as the geometry source for each block

It gives real shapes where filmed. It lost because:

- it needs footage for every block
- output is a ghosting-prone point cloud that still needs meshing and collision proxies
- non-commercial weights would encumber the shipped map

### Gaussian-splat backdrop from video

Photoreal far views. It lost for now because splats carry no collision and mix poorly with Lumen-lit meshes. The earlier diffusion-assisted splat experiment did not reach usable completeness. It remains a possible later backdrop, which would need its own ADR.

### Do nothing (no video at all)

Generated geometry would rest entirely on OSM tags, with nothing measuring where heights or setbacks are wrong. That is acceptable for v1, but it leaves generated geometry with no independent accuracy check.

## Consequences

### Accepting

- The expansion proceeds from OSM plus imagery alone. Footage adds accuracy checks where it exists.
- Validation becomes measurable: per-building height error and facade-plane offset against video-derived metric points.

### Costs

- The pilot needs captured video (1–2 min, closed loop, ≤150 m) and a separate research-only WSL environment.
- It carries license risk: results stay internal until the AMB3R license is clarified.
- Scale from monocular video can drift. Checks need OSM street widths or a taped reference.
