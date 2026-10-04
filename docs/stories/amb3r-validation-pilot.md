---
id: amb3r-validation-pilot
class: flow
status: backlog
owner: "@dafrimer"
updated: 2026-10-03
---

# Pilot AMB3R height and setback validation on one captured walk

## Why

Generated geometry rests on OSM tags that are partly missing or unverified. A video-derived metric check tells us where heights and setbacks are wrong (ADR 0003).

## Acceptance criteria

- [ ] A 1–2 min closed-loop walking video (≤150 m, with a taped reference distance) and its extracted frames are stored outside git, with their location and hash recorded in the story Closing.
- [ ] AMB3R-SfM runs in a research-only WSL environment on the RTX 3090 Ti; peak VRAM and the registered-frame fraction are recorded.
- [ ] Per-building height error and facade-plane offset (video vs OSM/generated) are reported for every building in view, with scale checked against the taped distance.
- [ ] Results stay internal; no AMB3R weights or point clouds are committed.

## Out of scope

- Using point clouds as runtime geometry.
- AMB3R-SLAM until its code is released.

## Notes

- Decision: `docs/decisions/0003-use-amb3r-for-validation-not-geometry.md`
- Research: `docs/research/2026-10-02-amb3r-video-to-world.md`

## Closing

