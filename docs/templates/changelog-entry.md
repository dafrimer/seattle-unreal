---
id: changelog-<YYYY-MM-DD>-<slug>
class: ledger
status: accepted
owner: "@<handle>"
updated: <YYYY-MM-DD>
date: <YYYY-MM-DD>
---

<!--
Provenance: this shape is copied from `HomeLab/homeassistant-changelog.md`, the single
positive exemplar found in the 22-folder audit (docs/research/2026-09-11-workspace-doc-audit.md,
"The one exemplar"). It was the best artifact in the workspace for exactly three reasons —
it was dated, it was attributed, and it carried explicit `### Why` sections. Those three
properties are what this template preserves; everything else here is scaffolding around them.

Usage:
- One file per entry: copy to docs/changelog/<YYYY-MM-DD>-<slug>.md and keep one entry below.
- One appended file: paste the entry block into the top of docs/changelog/CHANGELOG.md,
  newest first, and leave that file's frontmatter alone.
Pick one mode per repo and never mix. See README.md in this folder.

Replace every <angle-bracket> placeholder with a real value and delete these comments.
Never commit a literal date placeholder: the audit caught exactly that bug, with
placeholder dates left inside real backup paths in a shipped changelog.
-->

## <YYYY-MM-DD> — <short title: what changed, in one line>

**Author:** @<handle>

### What changed

<!--
Concrete and specific. Name files, services, hosts, versions.
Point at the change rather than restating it — `bootstrap/apps.yaml:14`, a commit SHA,
a PR number. Docs point at config; they never copy it.
-->

- <change>
- <change>

### Why

<!--
Mandatory. The only section that cannot be reconstructed later from git, and the
reason the exemplar was worth templatizing at all.

What forced this: the symptom, the incident, the request, the constraint that moved.
If it was a fix, state what was broken and how it presented. If it undoes an earlier
entry, say which one and what the earlier entry got wrong.
"Cleanup" and "improvements" are not reasons.

If this change also settles a hard-to-reverse trade-off, the reasoning belongs in an
ADR instead — write docs/decisions/NNNN-<slug>.md and link it from here.
-->

<why this happened now>

### Impact

<!--
Who and what is affected, including the operational consequences a reader has to plan for:
downtime taken, restarts required, config other people must update, behaviour that changed
silently, anything a rollback would now have to undo.
If the honest answer is "none visible", say that explicitly — an empty Impact reads as
an unfinished entry.
-->

- <who or what is affected, and how>
- <operational consequence: downtime, restart, manual step, changed default>

### Verification

<!--
How it was confirmed true — the command run or the observation made, plus the result.
Not "tested and working". A reader must be able to re-run this and compare.
-->

- `<command>` → <observed result>
- <observation: what was checked, where, and what it showed>
