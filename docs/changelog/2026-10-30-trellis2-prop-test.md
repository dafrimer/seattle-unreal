---
id: changelog-2026-10-30-trellis2-prop-test
class: ledger
status: accepted
owner: "@dafrimer"
updated: 2026-10-30
date: 2026-10-30
---

## 2026-10-30 — Reran prop generation with TRELLIS.2-4B (still not adopted)

**Author:** @dafrimer

### What changed

- Added `docs/research/2026-10-30-trellis2-prop-test.md`. Its evidence (turntables, per-resolution reports, the pre-approval smoke check, exact setup and generation scripts) is in `docs/research/assets/2026-10-30-trellis2-prop-test/`.
- No pipeline, runtime or asset change.

### Why

The 2026-10-25 test could only run TRELLIS v1, because TRELLIS.2 needs Meta's gated DINOv3 encoder. Access was granted, so the same five inputs were rerun to see whether the newer model changes the verdict.

### Impact

- Prop generation is still not adopted. The score is 1 pass, 2 partial and 2 fails (v1 scored 1 pass, 1 partial and 3 fails).
- TRELLIS.2 runs on the RTX 3090 Ti with at most 3.1 GB of peak torch-allocated memory, so GPU memory does not block it.
- The next test should use clean single-object photos through TRELLIS.2, not another model.

### Verification

- `report-512.json` and `report-1024_cascade.json` record generation time, peak memory and triangle counts per object.
- All five turntables at both resolutions were inspected visually; the verdicts in the research note come from that inspection.
