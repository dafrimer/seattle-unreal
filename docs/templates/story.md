---
id: <story-slug>
class: flow
status: backlog
owner: "@handle"
updated: <YYYY-MM-DD>
---

# <Story title — one imperative line, e.g. "Serve device metrics on the status page">

## Why

<One short paragraph. State the user-visible outcome: who is worse off today, and
what is different for them once this ships. Not the implementation. If you cannot
write this paragraph without naming a class, a table, or a library, the story is a
task and belongs inside another story's acceptance criteria.>

## Acceptance criteria

<!--
A criterion is something a consumer can observe from outside the change: a
command's output, a screen, an HTTP response, a file that now exists, an error
that no longer happens. "Refactored the loader" is not a criterion — nobody can
observe it, so nobody can tell you it is done. If you cannot say how you would
check it, delete the line; it is a note, not a criterion.

Every box here must be checkable by someone who did not write the code.
-->

- [ ] <Observable outcome, stated as a fact that is either true or false.>
- [ ] <Observable outcome. Include the check: `<command>` prints `<expected>`.>
- [ ] <Failure path counts too: given `<bad input>`, `<surface>` shows `<message>`.>

## Out of scope

<Explicit non-goals. Every item here is something a reasonable reader would
otherwise assume was included. This list is what you point at when the work starts
growing — if a request is not in Acceptance criteria and not contradicted here,
it is a new story, not an addition to this one.>

- <Non-goal, and the story or doc that would cover it instead, if any.>
- <Non-goal.>

## Notes

<Links only. Reference the path (and line, when you mean a specific setting);
never paste the content here. A copy is a second source of truth and it starts
disagreeing with the first the day after you write it.>

- Decision: `docs/decisions/<NNNN-slug>.md`
- Architecture: `docs/architecture/<doc>.md`
- Related story: `docs/stories/<slug>.md`
- Config this depends on: `<path/to/file>:<line>`

## Closing

<Leave this section empty until `status` becomes `done` or `dropped`. Delete
nothing above it — the plan and the outcome are read together.>

- **Date:** <YYYY-MM-DD>
- **Shipped:** <What actually landed, and where to see it: commit, path, or command.>
- **Changed vs the plan:** <Which criteria moved, were dropped, or were added, and
  why. "Nothing" is a valid answer and worth stating explicitly.>
- **If dropped:** <Why the work stopped, and what would have to change for it to be
  worth reopening. Required when `status: dropped` — a story with no recorded reason
  is indistinguishable from one that was simply forgotten.>
