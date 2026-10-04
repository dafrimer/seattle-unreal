---
id: adr-0001-generate-buildings-in-unreal-pcg-from-osm
class: ledger
status: accepted
owner: "@dafrimer"
updated: 2026-10-03
date: 2026-10-03
---

# 0001. Generate expansion buildings in Unreal PCG from OSM, assembled from a modular Nanite facade kit

## Context

The map covers a 224 × 236 m domain with 9 buildings. Each was produced by a bespoke style in an upstream Unity generator, exported as one unique mesh (5–44 MB OBJ per landmark), converted in Blender and imported one by one (`docs/architecture/overview.md`). Expanding to a ~500 m square means about 107 OSM buildings plus 110 `building:part` ways. That count comes from an Overpass query on 2026-10-03, bbox ±250 m around 47.609942, −122.336477. Continuing per-building authoring would make cost scale with building count, both in agent effort and in asset size.

Two requirements pull against each other:

- **Visual quality in Unreal matters.** Facades need real depth so that Lumen, virtual shadow maps and Nanite pay off.
- **Generation must scale and parallelise.**

The local UE 5.8 install ships the PCG framework (`Engine/Plugins/PCG/PCG.uplugin`, version 1.0), which includes:

- Grammar nodes (`PCGGrammar`, `PCGSubdivideSegment`, `PCGDuplicateCrossSections`)
- Polygon2D nodes

Its data bridges (`PCGExternalDataInterop`, `PCGPythonInterop`, `PCGGeometryScriptInterop`) are marked beta, and `PCGNaniteAssembliesInterop` is experimental.

## Decision

We will generate every non-hero building in the expansion area inside Unreal, with a PCG graph.

- **Inputs:** per-tile OSM data (footprints, `building:part` volumes, height/levels) plus per-building facade attributes (ADR 0002).
- **Construction:** the graph extrudes floors from footprints, splits each facade into bays with a shape grammar, and places modules from one shared, Nanite-enabled facade kit. Modules are window bay, pier, cornice, storefront, entrance, parapet and corner.
- **Tiling:** the expansion area is cut into ~128 m tiles aligned with World Partition cells. Data preparation, tagging and generation run per tile and in parallel.
- **Separation:** generated content never edits hero landmarks (ADR 0004).

## Alternatives considered

### Bake unique meshes per building off-engine (extend the current Python/Unity path)

Each building becomes its own mesh, as today. It is easy to test without the editor. It lost because:

- asset size grows with building count (the hero OBJs are already 5–44 MB each)
- nothing is shared or instanced, so draw and memory cost scale with the map
- window depth must be baked per building or faked in textures, which works against the "Unreal graphics are important" requirement

### Third-party procedural tools (Houdini Engine, CityEngine)

These are mature building grammars. They lost because they add a licensed external tool and a second runtime to operate, outside the engine and toolchain this repo already verifies. PCG ships with the engine version we pin (`SeattleUnreal.uproject:3`).

### Do nothing

Stay at 9 buildings, or keep hand-authoring per building. That leaves the map at one intersection. Every added block would cost bespoke generator code and review effort proportional to its building count.

## Consequences

### Accepting

- A new block costs data and compute, not code, once the kit and graph exist.
- Generated buildings share kit meshes. Instancing plus Nanite keeps draw cost roughly flat as the area grows, and World Partition streams tiles.
- Tiles are independent units, so data prep, tagging and generation can run concurrently, and PRs can land tile by tile.

### Costs

- The facade kit must be modelled up front, and its look caps the quality of every generated building.
- It depends on beta PCG interop plugins; an engine upgrade may break the data bridge.
- Verification needs real editor or game renders. Off-engine unit tests cannot prove the graph output.
- Generated facades are generic by construction. They read as "Seattle downtown", not as specific buildings, unless ADR 0002 supplies good attributes.
