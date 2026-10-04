---
id: facade-attribute-tagging
class: flow
status: backlog
owner: "@dafrimer"
updated: 2026-10-03
---

# Tag facade attributes for one tile from open street imagery

## Why

Most buildings have no appearance data in OSM (ADR 0002). Tagging street imagery is how generated buildings stop looking alike without per-building authoring.

## Acceptance criteria

- [ ] A vision model is chosen and recorded in an ADR (license, VRAM on a 24 GB card, measured accuracy on a hand-labelled sample of 20 facades).
- [ ] For one tile, `SourceData/expansion/attributes/{x}_{y}.json` lists every generated building with material class, colour, window pattern, storefront type, cornice style, confidence, and source image ids with license and author.
- [ ] Buildings without usable images are marked `inferred` with the fallback reason.
- [ ] The tile regenerated with attributes produces visibly distinct facades; a before/after capture is committed.
- [ ] Attribution for every image used is added to `SourceData/source-attribution.json`.

## Out of scope

- AI-generated textures (ADR 0002 alternative).
- Tagging all tiles.

## Notes

- Decision: `docs/decisions/0002-derive-facade-appearance-from-street-imagery.md`
- Depends on: `docs/stories/pcg-building-graph.md`

## Closing

