---
id: docs-index
class: living
status: current
owner: "@dafrimer"
updated: 2026-10-03
verify: "node ../agent-docs-kit/bin/agent-docs-kit.mjs lint docs exits 0, and for d in docs/*/; do grep -q \"$d\" docs/README.md || echo MISSING $d; done"
---

# Docs

Canonical documentation for this repo. Every other harness path (`CLAUDE.md`, `.github/copilot/`) holds a one-line pointer here, never a copy.

Each folder below is defined by its **class** — its update rule — not by its topic. Topic tells you what a doc is about; class tells you what you are allowed to do to it, and that is the property that decays when it is left unstated.

## Contents

| Folder | Class | Update rule |
| --- | --- | --- |
| `docs/architecture/` | `living` | Edit in place the moment reality changes. Never append dated entries. Every file carries a `verify:` command. |
| `docs/decisions/` | `ledger` | Append-only. One file per decision, `NNNN-slug.md`. Correct a decision by adding a new one that supersedes it — never by editing the original. |
| `docs/stories/` | `flow` | One file per unit of work. Mutable while active; move to `docs/stories/done/` when it reaches `done` or `dropped`. |
| `docs/stories/done/` | `flow` | Archive. Read-only in practice. Nothing here is in flight. |
| `docs/changelog/` | `ledger` | Append-only. Every entry needs a date, an author, and a `### Why`. |
| `docs/templates/` | — | Blank templates copied from agent-docs-kit. Not docs; skipped by lint. |
| `docs/research/` | `ledger` | Append-only. Dated findings, filename `YYYY-MM-DD-slug.md`. Superseded research is marked, not deleted. |

Top-level `CONTEXT.md` (`living`) is the glossary. Top-level `AGENTS.md` (`living`) is the router.

## Frontmatter contract

Every doc in this tree carries:

```yaml
---
id: <stable-slug>
class: living|ledger|flow
status: <per table>
owner: "@handle"
updated: 2026-10-03
---
```

| class | valid `status` | extra required |
| --- | --- | --- |
| living | `current` | `verify:` |
| ledger | `accepted` / `superseded` / `reverted` | `date:` |
| flow | `backlog` / `active` / `blocked` / `done` / `dropped` | — |

`id` is stable: it never changes, even when the title does. `owner` is never blank — the audit behind this kit found zero documents across 22 project folders carrying a date, version, or owner, so nothing was anyone's job and nothing got fixed.

`verify:` is the anti-stale lever and must be a command or a concrete check, not a sentiment. The audit found a README claiming manual sync against an ApplicationSet configured for automated prune and selfHeal; a `verify` of `grep syncPolicy bootstrap/*.yaml` would have caught it in one command.

## Three rules that override everything

1. **Nothing durable is gitignored.** Two audited repos hid their only real planning doc behind `.gitignore`; both rotted unreviewed.
2. **Docs point at config; they never copy it.** Reference `file:line`. The audit found one manifest stored in three places and a duplicate metrics table that already disagreed with its source at different precision.
3. **One canonical path, thin per-harness pointers.** The best architecture doc in the audit lived under `.github/copilot/skills/`, a path Claude Code and OMP never load.

## Adding a doc

1. Pick the class first. If you cannot name the update rule, you do not yet know where the doc goes.
2. Copy the matching template from `docs/templates/` (`decision.md`, `story.md`, `changelog-entry.md`), fill the frontmatter completely, and replace every `<angle-bracket>` placeholder — including `updated:`. A literal `YYYY-MM-DD` left in prose is a defect the audit caught in a real changelog's backup paths.
3. Link it from the nearest index: architecture from `docs/architecture/overview.md`, everything else from this file's table.
