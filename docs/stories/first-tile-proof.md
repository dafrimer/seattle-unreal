---
id: first-tile-proof
class: flow
status: backlog
owner: "@dafrimer"
updated: 2026-10-03
---

# Prove one generated tile beside the hero street in the native runtime

## Why

Before scaling, one generated tile must coexist with the verified street. The walk, collision and visuals have to hold up in the packaged player, not only in the editor.

## Acceptance criteria

- [ ] The tile adjacent to the hero street is generated and saved into `FifthUniversity` (World Partition enabled, or a documented reason why not yet).
- [ ] `-StreetVerify` still reports `success=1` in the editor and the packaged player, with all existing wall probes passing.
- [ ] One new wall probe against a generated building passes.
- [ ] Side-by-side review renders (hero vs generated, same lighting) are committed, with a written visual verdict.
- [ ] Packaged-player frame time is recorded at a fixed camera before and after adding the tile.

## Out of scope

- Remaining tiles.
- Imagery-derived attributes (fallback styles are acceptable here).

## Notes

- Depends on: `docs/stories/pcg-building-graph.md`
- Evidence contract: `Source/SeattleUnreal/StreetGameMode.cpp` (`BaseEvidence`)

## Closing

