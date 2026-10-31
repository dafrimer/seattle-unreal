---
id: expand-all-tiles
class: flow
status: backlog
owner: "@dafrimer"
updated: 2026-10-03
---

# Generate the full ~500 m expansion area

## Why

Once one tile is proven, the remaining tiles should land as data and compute rather than new code, giving a walkable multi-block district.

## Acceptance criteria

- [ ] Every tile in the expansion area is generated and committed, with per-tile captures under `Evidence/tiles/`.
- [ ] Tile generation runs in parallel and the wall-clock time is recorded.
- [ ] The walk route and views in `Content/StreetData/layout.json` are extended into generated blocks, and `-StreetVerify` passes in the packaged player.
- [ ] Packaged-player frame time at fixed cameras stays within a budget stated in the PR.
- [ ] The README status table and architecture overview are updated.

## Out of scope

- New hero landmarks (needs its own ADR, per ADR 0004).
- Areas beyond ~500 m.

## Notes

- Depends on: `docs/stories/first-tile-proof.md`, `docs/stories/facade-attribute-tagging.md`

## Closing

