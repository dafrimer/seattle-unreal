---
id: changelog-readme
class: living
status: current
owner: "@dafrimer"
updated: 2026-10-03
verify: "Every entry heading under docs/changelog/ is a real ISO date (no literal placeholder), and every entry has an Author line and a `### Why` — run node ../agent-docs-kit/bin/agent-docs-kit.mjs lint docs"
---

# Changelog

Dated records of what changed and why. Append-only: entries are added, never rewritten.

This is the ledger for changes that are **not** hard-to-reverse trade-offs. If a change settles a real trade-off with genuine alternatives, it needs a decision record instead — see `../decisions/README.md`. A changelog entry says what happened; an ADR says what was rejected.

## Pick one layout, per repo

Two layouts work. Mixing them does not.

- **One file per entry** — `docs/changelog/{date}-{slug}.md`, one entry per file. Right when entries are long, land on separate branches, or are written concurrently: no merge conflicts, and each entry is independently linkable.
- **One appended file** — `docs/changelog/CHANGELOG.md`, entries stacked newest-first under one frontmatter block. Right when entries are short and one person writes most of them: the whole history reads top to bottom in one open.

Choose once, on day one, and record the choice here:

> **This repo uses:** one file per entry (`docs/changelog/{date}-{slug}.md`), because entries land on separate PR branches.

Half-and-half is the failure mode. Once some entries live in their own files and some are buried in a shared one, nobody can answer "what shipped last month" without reading both, so they read neither.

Copy `docs/templates/changelog-entry.md` for every new entry either way. Its heading shape is fixed so that tooling and agents can find `### Why` and `### Verification` without guessing.

## Every entry carries date, author, and Why

Three fields, all non-negotiable:

- **Date** — a real ISO date in the `##` heading, and in `date:` when the entry is its own file. It is the ordering key and the only thing that makes an entry answerable.
- **Author** — `**Author:** @handle` directly under the heading. Not decoration: it is who to ask when the entry's Why turns out to be incomplete. The audit found zero documents across 22 folders carrying a date, version or owner, so nothing was anyone's job.
- **Why** — the reason the change happened. The only part that git cannot reconstruct. A diff shows what moved; nothing but this section shows what forced it.

This shape is lifted from `HomeLab/homeassistant-changelog.md`, the audit's single positive exemplar. It won that title purely on those three properties (`docs/research/2026-09-11-workspace-doc-audit.md`, "The one exemplar").

## Entries are never edited, only superseded

An entry is a claim about a moment. Editing it destroys the record of what was believed then, which is the entire value of a ledger.

Got it wrong? Write a new entry, dated today, that says what the earlier entry got wrong and what is true now. Link back to it. If the earlier entry was fully rolled back, set its `status:` to `reverted`; if it was replaced, `superseded` plus `superseded_by:`. Frontmatter status is the only permitted edit to a shipped entry — never the body.

## Two failures this folder exists to prevent

Both are from the audit, both from real files.

**A header that lies about the body.** `learned-fixes.md` still printed `_No entries yet._` at the top while carrying **six real entries** underneath. The placeholder was written on day one and never removed, so every reader — human or agent — was told at the top of the file that there was nothing to read. Six recorded fixes were functionally invisible. There is no scaffolding line in `TEMPLATE.md` that survives a real entry: delete the comments, fill the placeholders, and the file states only what is true.

**Placeholder dates shipped as content.** `HomeLab/homeassistant-changelog.md` — the exemplar itself — is a single entry misnamed as a changelog, and it left literal `YYYY-MM-DD` placeholders inside real backup paths. A path containing a placeholder is not a path; anyone following it finds nothing, and it is impossible to tell afterward which backup the entry actually meant. Placeholders here are written `<angle-bracketed>` so an unfilled one is unmistakable on sight and greppable in CI. A date-shaped string that was never a date is not.

One more from the same finding: a file holding one entry is not a changelog, it is a note. If a repo has only ever recorded one thing, the honest fix is to record the others, not to rename the file.
