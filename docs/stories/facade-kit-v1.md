---
id: facade-kit-v1
class: flow
status: backlog
owner: "@dafrimer"
updated: 2026-10-03
---

# Model and import the v1 modular facade kit

## Why

Generated buildings can only look as good as the parts they are assembled from (ADR 0001). The kit must exist, render well under Lumen/VSM and share materials with the hero landmarks before any graph output is worth reviewing.

## Acceptance criteria

- [ ] `Content/Seattle/Kit/` contains Nanite-enabled modules for window bay, pier, cornice, storefront, entrance, parapet and corner, each on a documented metric grid (bay width and floor height recorded in the kit README).
- [ ] Kit materials expose parameters for material class and colour, matching the facade-attributes fields (ADR 0002).
- [ ] An editor review map renders all modules in at least three material/colour variants; review PNGs are committed under `Evidence/kit/`.
- [ ] Every module has simple collision that blocks the walker (checked by one wall probe against a kit-built test wall).

## Out of scope

- Placing modules on real footprints (story `pcg-building-graph`).
- Hero landmark materials changes.

## Notes

- Decision: `docs/decisions/0001-generate-buildings-in-unreal-pcg-from-osm.md`
- Hero materials: `Content/Seattle/Materials/`

## Closing

