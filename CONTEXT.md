---
id: context
class: living
status: current
owner: "@dafrimer"
updated: 2026-10-03
verify: "grep -niE '\\b(todo|roadmap|next step|we will|phase [0-9]|\\[ \\])' CONTEXT.md returns nothing, and every term in the table returns at least one hit from git grep -il '<term>'"
---

# Context

**This file is a glossary and nothing more.** How things are built lives in `docs/architecture/`, why in `docs/decisions/`, work in `docs/stories/`, history in `docs/changelog/`.

## Terms

| Term | Means | Not |
| --- | --- | --- |
| **Hero landmark** | One of the 9 hand-tuned buildings imported from the upstream generator (e.g. Rainier Tower, Fairmont Olympic Hotel), stored as unique meshes in `Content/Seattle/Meshes/`. | Not a **generated building**. Generators must skip hero footprints. |
| **Generated building** | A building assembled at edit time by the PCG graph from OSM geometry plus facade attributes, built from facade-kit modules. | Not a hero landmark; it has no bespoke mesh of its own. |
| **Facade kit** | The fixed set of modular, Nanite-enabled meshes (window bays, piers, cornices, storefronts, entrances, parapets) from which generated buildings are assembled. | Not textures. A kit module carries real depth; appearance variation comes from material parameters. |
| **Facade attributes** | Per-building appearance record (material class, colour, window pattern, storefront type, cornice style) produced by tagging street imagery, consumed by PCG as parameters. | Not geometry. Height, footprint and setbacks come from OSM, not from attributes. |
| **Tile** | A square ~128 m cell of the expansion area; the unit of data preparation, tagging and PCG generation, aligned with World Partition cells. | Not a block. A city block can span several tiles. |
| **Expansion area** | The ~500 m square centred on 5th & University that generated buildings fill. | Not the original 224 × 236 m hero domain, which it contains. |
| **Mapped / inferred** | Mapped = taken from OSM (footprint, height, levels, building:part). Inferred = authored or estimated (facade detail, roofs, props). Every artifact states which. | Not "accurate" vs "inaccurate"; neither claims survey accuracy. |
| **StreetVerify** | Command-line mode (`-StreetVerify`) where the native runtime walks the route out and back, runs wall probes, resets, and captures viewpoints, writing `runtime-verification.json`. | Not StreetRecord; StreetVerify asserts, it does not film. |
| **StreetRecord** | Command-line mode (`-StreetRecord`) that captures the captioned walkthrough frames at a fixed step and writes `capture.json`. | Not a performance benchmark; frames are fixed-step, not real time. |
| **Wall probe** | A layout-defined attempt to walk the real CharacterMovement capsule into a building facade; passes when collision stops it at the swept contact distance. | Not a line trace; it is the gameplay movement path. |
| **Evidence** | Machine-written JSON/PNG proving a run's outcome, stored under `Evidence/`. A claim of "verified" must point at one. | Not logs; logs are local and gitignored. |
| **Source snapshot** | The frozen import inputs in `SourceData/`, captured from the upstream generator's exports by `Scripts/prepare_source.py`. | Not the upstream generator itself, which lives outside this repo. |

## Naming rules

- Generated asset names carry the OSM element id: `osm-<way id>` for buildings — enforced by `Scripts/prepare_source.py` (`safe()`), mirrored in `Content/Python/migrate_scene.py`.
- Actor labels use `<kind>:<id>` (`building:osm-35824976`, `surface:sidewalk-6`) — wall probes match on them in `Source/SeattleUnreal/StreetGameMode.cpp` (`SetupWallProbe`).
