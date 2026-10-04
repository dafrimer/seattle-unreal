# seattle-unreal

A walkable, stylized reconstruction of 5th Avenue at University Street, Seattle, in Unreal Engine 5.8. It runs as a native C++ runtime with CharacterMovement walking, real facade collision, Lumen lighting and Nanite buildings.

Not a survey: footprints and heights come from OpenStreetMap, while facades, roofs and props are authored approximations.

## Status

| Area | State | Evidence |
| --- | --- | --- |
| 9 buildings, 88 props, 115 street surfaces imported | Verified | `Evidence/import-result.json` |
| Native walk / collision / reset / viewpoints (editor and packaged player) | Passing | `Evidence/runtime-verification.json`, `Evidence/packaged/runtime-verification.json` |
| 64 s captioned walkthrough (StreetRecord) | Recorded | `Evidence/capture.json` |
| Map expansion to ~500 m | Planned | `docs/` (agent-docs-kit stories and decisions) |

## Build and run (Windows)

1. Install Unreal Engine 5.8, then run `Install-Unreal-Build-Tools.cmd`. It installs MSVC, the Windows SDK and the .NET Framework SDK. You must supply `Prerequisites/vs_BuildTools.exe` from Microsoft yourself; its SHA-256 is pinned in `Scripts/install-build-tools.ps1`, and the file is not redistributed here.
2. `Build-Unreal-Street.cmd` compiles the runtime, re-imports the scene and packages the player into `Build/`.
3. `Play-Seattle-Unreal.cmd` launches the packaged player. `Open-Unreal-Assets.cmd` opens the project in the editor.

Automated checks run with the packaged or editor game: `-StreetVerify` walks the route and runs the facade-collision checks, and `-StreetRecord` captures the walkthrough frames.

## Attribution

- Map data © OpenStreetMap contributors, ODbL-1.0 (https://www.openstreetmap.org/copyright). Footprints and heights from the snapshot recorded in `SourceData/source-attribution.json`.
- Photographic references: Panoramax contributors, CC-BY-SA-4.0. They were used as reference only and are not shipped as textures; the list is in `SourceData/source-attribution.json`.
- PBR textures: CC0 (see `SourceData/material-sources.json`, `SourceData/CC0-1.0.txt`).

## License

Code and scripts: MIT (`LICENSE`). Third-party data and assets keep their own licenses, listed above.
