---
id: decisions-readme
class: living
status: current
owner: "@dafrimer"
updated: 2026-10-03
verify: "Every file matching docs/decisions/[0-9][0-9][0-9][0-9]-*.md has a `## Alternatives considered` section and a non-empty `### Costs` — run node ../agent-docs-kit/bin/agent-docs-kit.mjs lint docs"
---

# Decisions

Architecture Decision Records. One file per decision, append-only, never rewritten.

An ADR exists to answer one question a future reader will actually ask: **why is it like this, and what did we already rule out?** Nothing else in a repo answers that. Code shows the outcome. Config shows the outcome. Only an ADR shows the rejected branch.

## Why this folder exists

The audit behind this kit read 22 top-level project folders. **0 of 22 had any decision record** — the single hardest number it produced (`docs/research/2026-09-11-workspace-doc-audit.md`, Coverage table).

The consequence is concrete. ArgoCD, Longhorn, MetalLB, Traefik and external-secrets were all adopted into a running cluster, and the rationale for every one of them is permanently lost. Nobody can now say whether Longhorn was chosen over the alternatives on performance, on operational simplicity, or because it was the first thing that worked on a Saturday. Replacing any of them means re-deriving constraints that were already understood once — and there is no way to tell which of those constraints still hold.

The sharper failure is `seattle-world`. It pins **two mutually incompatible torch stacks** (`2.4.0+cu124` and `1.13.1+cu117`) and carries the rule:

> Do not loosen a constraint to resolve an installation failure.

That rule is almost certainly correct, and it is unusable. No reason was ever recorded. A future agent hitting an install failure has two options: obey a rule it cannot evaluate, or break something whose breakage mode is undocumented. **A rule whose reason is missing cannot be honoured — only obeyed superstitiously or ignored.** One ADR naming the incident that produced the rule would have made it enforceable.

## Does this deserve an ADR?

Three-part test. It must pass **all three**. Failing any one means skip it — write it in the changelog, or don't write it at all.

1. **Hard to reverse.** Undoing it costs migration work, downtime, or data movement — not an edit. Choosing a storage layer qualifies; choosing a log format usually does not.
2. **Surprising without context.** A competent stranger reading the code would ask "why this?" or, worse, would confidently "fix" it. If the choice is self-evident from the code, the code is already the record.
3. **A real trade-off with genuine alternatives.** At least two options were live, and the winner has costs you are knowingly paying. If there was only one viable option, you did not make a decision — you noted a constraint.

An ADR folder that logs everything is as useless as one that logs nothing: nobody reads 200 records to find the four that mattered. Keep the bar high.

Borderline cases that usually **fail** the test: dependency version bumps, formatter and lint settings, directory renames, anything already enforced and explained by a config file.

Borderline cases that usually **pass**: choosing between two tools with overlapping scope, accepting a known operational burden, deliberately not adopting something popular, and any rule stated as a prohibition — prohibitions are exactly the artifacts that rot into superstition without a recorded why.

## Numbering

- Filename: `NNNN-kebab-slug.md`. Four digits, zero-padded, monotonic.
- Take the highest number in the folder and add one. Never reuse a number; never renumber on supersede. The number is the decision's permanent address, cited from code comments, stories and changelog entries.
- The template lives at `docs/templates/decision.md` and is not a decision. Real records start at `0001`.
- The slug is stable once committed. Rename the title in the body if it improves; leave the filename alone so inbound links survive.

## Index

Keep the table below current — it is the only reason this folder is scannable. Newest last, matching file order. One row per record; the title column is the decision, not the topic.

| # | Decision | Status | Date |
| --- | --- | --- | --- |
| [0001](0001-generate-buildings-in-unreal-pcg-from-osm.md) | Generate expansion buildings in Unreal PCG from OSM, assembled from a modular Nanite facade kit | accepted | 2026-10-03 |
| [0002](0002-derive-facade-appearance-from-street-imagery.md) | Derive facade appearance from openly licensed street imagery via automated tagging | accepted | 2026-10-03 |
| [0003](0003-use-amb3r-for-validation-not-geometry.md) | Use AMB3R video reconstruction to measure and validate, not to supply runtime geometry | accepted | 2026-10-03 |
| [0004](0004-keep-existing-buildings-as-hero-landmarks.md) | Keep the 9 existing buildings as hand-tuned hero landmarks; generators skip their footprints | accepted | 2026-10-03 |

`Status` is copied from the record's frontmatter: `accepted`, `superseded`, or `reverted`. A superseded row stays in the table — deleting it recreates exactly the hole this folder exists to prevent.

## Superseding

Shipped ADRs are not edited. To change a decision:

1. Write a new record with the next number; set `supersedes: NNNN-old-slug` in its frontmatter.
2. In the old record, set `status: superseded` and add `superseded_by: MMMM-new-slug`. Change nothing else — not the Context, not the Decision, not a typo.
3. If the decision was rolled back rather than replaced, the old record's status is `reverted` and the new record explains what broke.

Frontmatter status is the only permitted edit to a shipped ADR. The old, wrong decision is the reason the current one exists; erasing it erases the argument.
