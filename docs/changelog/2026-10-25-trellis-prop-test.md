---
id: changelog-2026-10-25-trellis-prop-test
class: ledger
status: accepted
owner: "@dafrimer"
updated: 2026-10-25
date: 2026-10-25
---

## 2026-10-25 — Tested image-to-3D prop generation on Seattle photos (not adopted yet)

**Author:** @dafrimer

### What changed

- Added `docs/research/2026-10-25-trellis-prop-test.md`. Its evidence (inputs, masks, turntables, `report.json`, exact scripts) is in `docs/research/assets/2026-10-25-trellis-prop-test/`.
- Opened backlog story `docs/stories/disable-android-file-server-plugin.md`.
- No pipeline, runtime or asset change.

### Why

We needed to know whether cutting objects out of street photos with SAM and generating 3D props with an open image-to-3D model could supply props for the stylized district (ADR 0005), before committing a PR to it. The test also showed that each editor launch re-adds an Android File Server token to tracked config.

### Impact

- Prop generation is not adopted. 1 of 5 objects passed; failures trace to input photo quality, not the model.
- Until the new story ships, every editor run must be followed by reverting `Config/DefaultEngine.ini` before committing.

### Verification

- `docs/research/assets/2026-10-25-trellis-prop-test/report.json` records generation time, peak VRAM and triangle counts per object on an RTX 3090 Ti.
- `git status --short` was clean after reverting the regenerated token; no test assets were written to `Content/`.
