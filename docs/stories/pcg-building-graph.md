---
id: pcg-building-graph
class: flow
status: backlog
owner: "@dafrimer"
updated: 2026-10-03
---

# Generate buildings for one tile with a PCG graph

## Why

This is the core of the expansion: a graph that turns a tile JSON (and attributes when present) into buildings from the facade kit. Without it, each block remains hand work.

## Acceptance criteria

- [ ] PCG, plus the needed interop plugins, are enabled in `SeattleUnreal.uproject`, and the reason for each beta plugin is noted in the PR.
- [ ] One PCG graph reads `SourceData/expansion/tiles/{x}_{y}.json` and generates every non-excluded building in that tile: per-floor extrusion, facade bays via grammar, kit modules placed with collision.
- [ ] Missing attributes fall back to a deterministic seeded style; regenerating the same tile yields identical instance transforms (compared by hash of the generated instance list).
- [ ] Generated buildings never intersect hero footprints (an automated check over generated bounds vs hero bounds passes).
- [ ] An editor capture of the tile from street level and aerial is committed under `Evidence/tiles/`.

## Out of scope

- Generating all tiles (story `expand-all-tiles`).
- Street surfaces, props and trees outside hero area.

## Notes

- Decision: `docs/decisions/0001-generate-buildings-in-unreal-pcg-from-osm.md`
- Depends on: `docs/stories/osm-tile-pipeline.md`, `docs/stories/facade-kit-v1.md`

## Closing

