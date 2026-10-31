---
id: changelog-2026-10-03-public-repo-and-expansion-docs
class: ledger
status: accepted
owner: "@dafrimer"
updated: 2026-10-03
date: 2026-10-03
---

## 2026-10-03 — Published the verified street and documented the expansion approach

**Author:** @dafrimer

### What changed

- Created the public repository `dafrimer/seattle-unreal`. Import commit `9190ccc` on `main` holds the verified UE 5.8 project, with binaries in Git LFS (500 objects, 263 MB).
- Excluded from git (`.gitignore`): build output, caches, logs, walkthrough frames and MP4, and `Prerequisites/vs_BuildTools.exe` (Microsoft license forbids redistribution).
- Removed the auto-generated Android File Server `SecurityToken` from `Config/DefaultEngine.ini` before publishing.
- Scaffolded agent-docs-kit: `AGENTS.md`, `CONTEXT.md`, `docs/architecture/overview.md`.
- Recorded ADRs 0001–0004 (PCG generation, imagery-derived facade attributes, AMB3R for validation only, hero landmarks).
- Opened seven backlog stories, one per planned PR.
- Moved the AMB3R research note into `docs/research/`.
- Added a docs-lint CI workflow.

### Why

The map is expanding from one intersection to a ~500 m area in a series of small PRs. The approach (generate in Unreal PCG from OSM, derive appearance from street imagery, use AMB3R only to validate) was agreed before any code. It needed a durable home, with rationale and per-PR acceptance criteria, before work fans out across sessions and agents. Publishing required stripping non-redistributable and machine-local files first.

### Impact

- Cloning the repo needs Git LFS. Building it needs UE 5.8 and the Microsoft bootstrapper obtained separately (`README.md`).
- Every documentation change must now pass docs lint in CI.
- Runtime behaviour: none changed.

### Verification

- `git lfs ls-files` → 500 tracked binaries. The push to `origin/main` completed with all 500 LFS objects uploaded.
- `node ../agent-docs-kit/bin/agent-docs-kit.mjs lint docs` → exit 0 (see this PR's CI run).
