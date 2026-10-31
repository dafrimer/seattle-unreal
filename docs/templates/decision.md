---
id: adr-0000-template
class: ledger
status: accepted
owner: "@<handle>"
updated: <YYYY-MM-DD>
date: <YYYY-MM-DD>
---

<!--
Copy this file to docs/decisions/NNNN-kebab-slug.md.
NNNN is zero-padded and monotonic: take the highest existing number and add one.
Never reuse or renumber. The number is the permanent address of this decision.

Delete every comment block before committing. Leave no <angle-bracket> placeholder behind.
-->

# NNNN. <Title stated as the decision, not the question>

<!--
Title is a claim, not a topic. "Use ArgoCD for cluster reconciliation", not "Cluster reconciliation".
A reader scanning the index should learn the outcome from the title alone.
-->

## Context

<!--
The forces, written so a stranger with no memory of this week understands the pressure.
State what was true when the decision was made, not what you wish were true:
constraints, deadlines, existing commitments, things that had already broken.

Point at config and code rather than restating them: `bootstrap/apps.yaml:14`, not a pasted block.
If the pressure came from an incident or an earlier decision, link it.
No decision here. Context that already implies the answer is not context, it is advocacy.
-->

<paragraph: what was happening, what constrained us, why a choice had to be made now>

## Decision

<!-- Active voice, present commitment. One paragraph. -->

We will <the decision, stated so it can be complied with or violated unambiguously>.

## Alternatives considered

<!--
Mandatory, and the whole point of the file. A decision with no recorded alternatives
is indistinguishable from a default, and a future reader cannot tell whether the
option they are about to "discover" was already rejected for a reason.

At least two. Each gets why it lost, in terms of the forces above.
"Not a good fit" is not a reason. "Requires a second control plane we have no one to operate" is.
An alternative that lost on a cost we later stop paying is exactly the trigger for a superseding ADR.
-->

### <Alternative A>

<what it was, and the specific force that killed it>

### <Alternative B>

<what it was, and the specific force that killed it>

### Do nothing

<what staying on the current path would have cost — include this unless it is genuinely absurd>

## Consequences

### Accepting

<!-- What becomes true, and what is now allowed or unblocked. -->

- <consequence>
- <consequence>

### Costs

<!--
Non-empty. Always. A decision with no cost was not a trade-off — it was an obvious
move that did not need an ADR, or the costs were not looked for hard enough.

Include the ongoing ones: operational burden, a dependency now load-bearing,
a door closed, knowledge only one person has.
-->

- <cost we are choosing to pay>
- <cost we are choosing to pay>

## Superseding

<!--
Delete this section in real ADRs; it is guidance, not content.

A shipped ADR is never edited. Its value is that it records what was believed
at a point in time — rewriting it destroys exactly the thing it exists to hold.

To change a decision:
1. Write a new ADR with the next number. Give it `supersedes: NNNN-old-slug`.
2. In the old file, change `status:` to `superseded` and add `superseded_by: MMMM-new-slug`.
   Touch nothing else in the old file. Not the Context, not the Decision, not a typo.
3. If a decision was rolled back rather than replaced, the old file's status becomes
   `reverted` and the new ADR explains what broke.

Status/frontmatter edits are the only permitted change to a shipped ADR.
-->
