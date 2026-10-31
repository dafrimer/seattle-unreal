---
id: agents
class: living
status: current
owner: "@dafrimer"
updated: 2026-10-03
verify: "for d in */; do grep -q \"$d\" AGENTS.md || echo MISSING $d; done"
---

# seattle-unreal

Walkable Unreal Engine 5.8 reconstruction of 5th Ave & University St, Seattle (native C++ runtime, verified walk/collision), now expanding to ~500 m via OSM-driven PCG generation.

## Map

| Path | What lives there |
| --- | --- |
| `Source/` | Native runtime module: `StreetGameMode`, `StreetCharacter`, `StreetHUD`, build targets. |
| `Content/` | Unreal assets. `Content/Seattle/` hero meshes/materials/map (LFS); `Content/Python/` editor import scripts; `Content/StreetData/layout.json` runtime layout. |
| `Config/` | Engine/game ini (Lumen, VSM, exposure, default map). |
| `Scripts/` | Off-engine tooling: source snapshot, Blender FBX export, build-tools installer, MP4 encoder, checkpoint packager. |
| `SourceData/` | Frozen import inputs: manifest, FBX, collider OBJs, CC0 textures, attribution. |
| `Evidence/` | Durable verification output (JSON + review PNGs). Frames, logs and MP4 are gitignored. |
| `docs/` | Canonical documentation. Start at `docs/README.md`. |
| `Binaries/`, `Build/`, `Intermediate/`, `Saved/`, `DerivedDataCache/`, `Prerequisites/` | Local only, gitignored: build output, caches, Microsoft bootstrapper. |

## Docs

- `CONTEXT.md` — glossary. Read when a term here is ambiguous. Definitions only.
- `docs/architecture/overview.md` — entry points, data flow, dependencies.
- `docs/decisions/` — why a choice was made. Read before changing or arguing with one.
- `docs/stories/` — current work state. One story per PR-sized slice.
- `docs/changelog/` — what changed, when, by whom, and why.
- `docs/research/` — dated research notes (ledger).

## Commands

```sh
Install-Unreal-Build-Tools.cmd     # MSVC + Windows SDK + .NET Framework SDK (one-time, elevated)
Build-Unreal-Street.cmd            # compile, re-import, gate, package to Build/
Build/Windows/SeattleUnreal.exe -StreetVerify -StreetEvidence=H:/abs/evidence/dir   # automated walk/collision check
node ../agent-docs-kit/bin/agent-docs-kit.mjs lint docs                            # docs contract (CI pins the kit: .github/workflows/docs.yml:20)
```

Copied from `Build-Unreal-Street.cmd:5-11`, `Source/SeattleUnreal/StreetGameMode.cpp:122-126`, `.github/workflows/docs.yml`. If a command drifts, fix it there first, then here.

Verification passes only on `STREET_VERIFY_COMPLETE success=1` / `"success": true` in the evidence JSON; the process exit code is not trustworthy (see overview, Known gaps).

## Doc classes

Every doc declares a `class` in frontmatter, which states its update rule: `living` is edited in place and carries a `verify:` check, `ledger` is append-only and carries a `date:`, `flow` moves through states and archives into a `done/` folder. Never append a dated entry to a living doc and never rewrite a ledger entry.

## Per-harness pointers

Canonical docs live in `docs/`. Any harness-specific file (`CLAUDE.md`, `.github/copilot/README.md`) is a one-line pointer to `AGENTS.md`, never a copy.
