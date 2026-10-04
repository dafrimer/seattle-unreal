---
id: adr-0002-derive-facade-appearance-from-street-imagery
class: ledger
status: accepted
owner: "@dafrimer"
updated: 2026-10-03
date: 2026-10-03
---

# 0002. Derive facade appearance from openly licensed street imagery via automated tagging

## Context

OSM supplies shape but almost no appearance. In the expansion area (Overpass, 2026-10-03, 107 buildings):

| Tag | Buildings with it |
| --- | --- |
| `building:levels` | 83 |
| `height` | 28 |
| `building:material` | 14 |
| `building:colour` | 2 |
| `roof:shape` | 3 |

ADR 0001's PCG graph needs per-building parameters to choose kit modules and materials. Without them every generated building looks alike. Hand-describing each building does not scale with the area, and that is the cost this expansion exists to avoid.

Openly licensed street-level imagery covers downtown Seattle. The repo already uses Panoramax CC-BY-SA-4.0 photographs as reference (`SourceData/source-attribution.json`).

## Decision

We will produce a facade-attributes record for each generated building. A vision model tags openly licensed street-level images (Panoramax / Mapillary, CC-BY-SA) that face the building's street-side facades, and returns:

- material class
- dominant colour
- window pattern
- storefront type
- cornice style
- a confidence value

The records are versioned JSON per tile, with the source image ids and licenses kept for attribution. PCG consumes only these records. Where no image or only low-confidence tags exist, the building falls back to OSM tags plus a deterministic seeded style, and the record marks it `inferred`.

The specific vision model is chosen in the tagging story, subject to an open license and 24 GB VRAM feasibility.

## Alternatives considered

### OSM tags plus random seeded styles only

Zero model cost and fully deterministic. It lost because tags exist for under 15% of buildings, so most facades would be random. The result reads as generic and does not track the real streetscape.

### Generative facade textures (diffusion) per building

More variety per building. It lost on three counts:

- model licensing is still unresolved
- it adds GPU time per building
- it produces flat images that work against the kit's real depth (ADR 0001)

It remains a candidate for material variants inside the kit, not as the appearance source.

### Do nothing

Generated buildings would all use one default style. That fails the visual-quality requirement and makes the expansion look procedural rather than like Seattle.

## Consequences

### Accepting

- Appearance scales with image count and GPU time, not with authoring effort. Tagging is batchable per tile, in parallel.
- Every appearance choice traces to an image id or is explicitly marked inferred.

### Costs

- It is a new dependency on a vision model, with license, VRAM and accuracy still to be validated.
- CC-BY-SA imagery requires attribution, and the derived attribute data must be checked for share-alike obligations before redistribution.
- Coverage gaps (alleys, rear facades) fall back to inferred styles.
- Tag errors (e.g. glass vs stone) produce visibly wrong buildings, so review images are required per tile.
