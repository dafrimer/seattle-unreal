---
id: disable-android-file-server-plugin
class: flow
status: backlog
owner: "@dafrimer"
updated: 2026-10-25
---

# Stop the editor regenerating an Android File Server token in tracked config

## Why

Every editor launch writes a fresh `SecurityToken` into `Config/DefaultEngine.ini` (`[/Script/AndroidFileServerEditor.AndroidFileServerRuntimeSettings]`). The token was stripped before publishing; it reappeared after each of the editor runs on 2026-10-25 and had to be reverted by hand. Android is not a target platform, so the plugin should not run at all, and the public repo should never be one careless commit away from shipping a token.

## Acceptance criteria

- [ ] `SeattleUnreal.uproject` and `SeattleAssetsPreview.uproject` disable the `AndroidFileServer` plugin.
- [ ] `Config/DefaultEngine.ini` no longer contains an `AndroidFileServerRuntimeSettings` section.
- [ ] After launching and closing the editor, `git status --short` shows no change to `Config/DefaultEngine.ini`.
- [ ] `Build-Unreal-Street.cmd` and `-StreetVerify` in the packaged player still pass.

## Out of scope

- Any other platform plugin changes.

## Notes

- Config: `Config/DefaultEngine.ini` (AFS section)
- Changelog: `docs/changelog/2026-10-03-public-repo-and-expansion-docs.md` (original token removal)

## Closing
