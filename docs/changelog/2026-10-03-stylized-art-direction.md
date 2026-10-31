---
id: changelog-2026-10-03-stylized-art-direction
class: ledger
status: accepted
owner: "@dafrimer"
updated: 2026-10-03
date: 2026-10-03
---

## 2026-10-03 — Set stylized game art as the target; dropped video reconstruction

**Author:** @dafrimer

### What changed

- Added `docs/decisions/0005-target-stylized-game-art-not-photoreal-capture.md`.
- Marked `docs/decisions/0003-use-amb3r-for-validation-not-geometry.md` as `superseded`.
- Dropped the `amb3r-validation-pilot` story, now archived in `docs/stories/done/` with its reason.
- Updated the decision index and the dependency table in `docs/architecture/overview.md`.

### Why

The five photoreal splat/diffusion runs in `seattle-world` all failed their quality gates, and the follow-up remedy (dense video plus AMB3R) still aimed at realism. The stated goal is to extend the map in stylized, game-ready art comparable to GTA V. Measured facades and video capture cannot change that result, so they were removed before any effort went into them.

### Impact

- No runtime change. The expansion path is now OSM → PCG → facade kit, with street imagery used only to pick a style class.
- The kit's art quality is now the main lever. A later art-direction pass on hero landmarks and renderer settings may follow.

### Verification

- `node ../agent-docs-kit/bin/agent-docs-kit.mjs lint docs` → 0 errors.
- `git grep amb3r-validation-pilot docs/stories/*.md` → no active story references.
