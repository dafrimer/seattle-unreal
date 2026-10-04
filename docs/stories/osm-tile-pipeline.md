---
id: osm-tile-pipeline
class: flow
status: backlog
owner: "@dafrimer"
updated: 2026-10-03
---

# Freeze expansion-area OSM data and split it into tiles

## Why

The generator needs a stable, reproducible input. Today the only OSM data is a 224 × 236 m snapshot held outside this repo (`docs/architecture/overview.md`, Known gaps). Until the ~500 m area is frozen in-repo and tiled, nothing downstream can be generated, reviewed or re-run from a clone.

## Acceptance criteria

- [ ] `SourceData/expansion/osm-snapshot.json` holds the Overpass result for the ±250 m square around 47.609942, −122.336477, with its query and timestamp recorded alongside.
- [ ] A tiling script writes one `SourceData/expansion/tiles/{x}_{y}.json` per ~128 m tile. Each contains footprints, `building:part` volumes, height and levels in Unreal centimetres, with every value marked mapped or inferred.
- [ ] Running the script twice from the frozen snapshot produces byte-identical tiles (no network access during tiling).
- [ ] The 9 hero OSM ids and their `building:part`s appear in no tile output, only in an `excluded` list (ADR 0004).
- [ ] Building counts summed over tiles plus excluded ones equal the snapshot count (107 at time of writing); a mismatch exits non-zero.

## Out of scope

- Facade appearance (story `facade-attribute-tagging`).
- Any Unreal-side generation (story `pcg-building-graph`).

## Notes

- Decision: `docs/decisions/0001-generate-buildings-in-unreal-pcg-from-osm.md`
- Decision: `docs/decisions/0004-keep-existing-buildings-as-hero-landmarks.md`

## Closing

