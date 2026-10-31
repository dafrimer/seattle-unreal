---
id: adr-0004-keep-existing-buildings-as-hero-landmarks
class: ledger
status: accepted
owner: "@dafrimer"
updated: 2026-10-03
date: 2026-10-03
---

# 0004. Keep the 9 existing buildings as hand-tuned hero landmarks; generators skip their footprints

## Context

The current 9 buildings have landmark-specific geometry that a generic facade grammar cannot reproduce. Examples are Rainier Tower's flared pedestal, Rainier Square's tapered tower and the Fairmont's stone courtyard. Their styles live in the upstream generator's bespoke builders (`BuildRainierTower`, `BuildRainierSquare`, `BuildMasonry`), outside this repo, and arrive here only as finished meshes listed in `SourceData/import-manifest.json`. They are already verified in the native runtime: three wall probes target them, and `Evidence/runtime-verification.json` shows all passing.

ADR 0001 introduces a second producer of buildings over an area that contains them.

## Decision

We will keep the 9 imported buildings as **hero landmarks**, unchanged, on the existing import path. The PCG generator takes an exclusion list of their OSM ids, plus every `building:part` belonging to them, and generates nothing inside their footprints.

Any future building can be promoted to hero only through a new ADR or a story that names it.

## Alternatives considered

### Regenerate all buildings, including the 9, through PCG

One pipeline and one look. It lost because the landmarks would lose their distinctive silhouettes, which are what make the intersection recognisable. It would also discard geometry that already passes runtime verification.

### Re-author the landmarks inside the facade kit

Shared materials and instancing. It lost because it spends effort on buildings that are already done and verified, against the goal of putting effort into scale.

### Do nothing (no rule)

Without an explicit exclusion, PCG would generate duplicate buildings overlapping the heroes. Collision and z-fighting would break the existing wall probes.

## Consequences

### Accepting

- The verified street keeps passing `-StreetVerify` while the expansion grows around it.
- Heroes act as the visual benchmark that generated buildings are compared against.

### Costs

- There are two building pipelines to maintain, and their materials and lighting must stay visually consistent.
- Hero edits still depend on the upstream generator outside this repo (see Known gaps in `docs/architecture/overview.md`).
