# Recording Actions for OBS Studio

[![Windows build](https://github.com/Diddlik/obs-recording-actions/actions/workflows/build.yml/badge.svg?branch=main&event=push)](https://github.com/Diddlik/obs-recording-actions/actions/workflows/build.yml)
[![Latest release](https://img.shields.io/github/v/release/Diddlik/obs-recording-actions?label=release&cacheSeconds=300)](https://github.com/Diddlik/obs-recording-actions/releases/latest)

A native Windows x64 OBS plugin with three hotkeys:

- Stop recording and move its completed file to either of two configurable folders.
- Stop recording and permanently delete its completed file.

Each destination has a Unicode display alias. Aliases do not affect file paths. There is no Python dependency, recording restart, replay-buffer action, or game-specific behavior.

## Requirements

- Windows 10/11 x64 and OBS Studio **32.2.2** or a compatible newer version.
## Install

### Windows installer (recommended)

1. Download the `*-windows-x64-setup.exe` from [the latest release](https://github.com/Diddlik/obs-recording-actions/releases/latest).
2. Close OBS Studio. Setup never closes OBS or stops a recording for you.
3. Run setup and select the OBS installation folder, usually `C:\Program Files\obs-studio`. It must contain `bin\64bit\obs64.exe`.
4. Start OBS and open **Tools → Recording Actions**.

Setup installs the plugin, English/German language files, the HTTPS backend, and license notices together. It updates an existing copy in the selected OBS folder and preserves your OBS plugin settings and hotkeys. If you previously installed a ProgramData copy, remove only that plugin copy first to avoid loading two copies.

### Manual ZIP installation

Download the `*-windows-x64.zip` from [GitHub Releases](https://github.com/Diddlik/obs-recording-actions/releases/latest). Close OBS and extract it into `%ProgramData%\obs-studio\plugins`. Keep the complete folder structure, including these files:

```text
%ProgramData%\obs-studio\plugins\obs-recording-actions\bin\64bit\obs-recording-actions.dll
%ProgramData%\obs-studio\plugins\obs-recording-actions\data\locale\en-US.ini
%ProgramData%\obs-studio\plugins\obs-recording-actions\data\locale\de-DE.ini
%ProgramData%\obs-studio\plugins\obs-recording-actions\data\qt\tls\qschannelbackend.dll
```

For portable OBS, copy the DLL to `<OBS>\obs-plugins\64bit` and the complete contents of the packaged `data` folder to `<OBS>\data\obs-plugins\obs-recording-actions`. Install only one copy. Copying only the DLL leaves update checking unavailable, even though embedded translations keep the settings readable.

### Verify a download

Each release includes `SHA256SUMS.txt` for the installer, binary ZIP, and corresponding source ZIP. Compare the downloaded file's SHA-256 with its entry:

```powershell
Get-FileHash .\obs-recording-actions-1.1.0-windows-x64-setup.exe -Algorithm SHA256
```

### Uninstall

Close OBS. For installer installations, use **Windows Settings → Apps → Installed apps → OBS Recording Actions → Uninstall**. For manual installations, remove only the plugin's DLL and its associated data folder. Settings stay in the OBS configuration folder; removing the plugin does not delete recordings or your configured destination folders.

## Configure and use

1. Open **Tools → Recording Actions**.
2. Enter a name/alias and an absolute destination folder for each target, or use **Browse**. Empty aliases display `Target 1` / `Target 2`. An empty folder disables that move action.
3. Select **Save**. Missing destination folders are created when an action runs.
4. In **OBS Settings → Hotkeys**, assign shortcuts to the three `Recording Actions` entries. Names use your aliases. If the OBS Hotkeys settings page was already open when an alias changed, reopen it to refresh its labels.
5. Start recording normally, then press the desired shortcut once. Nothing happens when OBS is not recording. Further plugin hotkeys are ignored until the action finishes.

### Destination folders

The example uses **GG** and **Highlights** as aliases for the two move targets. Choose your own names and folders; they are not built-in presets. Logging is optional, and automatic update checks can be disabled here.

![Recording Actions 1.1.0 settings with two destination folders, logging, automatic update checks, and About](docs/images/FolderSettings.png)

### OBS hotkeys

Assign your own key combinations in **OBS Settings → Hotkeys**. The example bindings below are not assigned automatically. Each move entry uses the alias saved in the plugin settings; Delete is a separate action.

![OBS hotkey settings showing Stop and Move to GG, Stop and Move to Highlights, and Stop and Delete](docs/images/Shortcuts.png)

**Delete is permanent. It does not use the Recycle Bin.** Test with disposable recordings first.

**Disable OBS automatic remuxing** under Settings → Advanced → Recording before using these actions. OBS 32.2.2 starts automatic remuxing *after* its recording-stopped event and exposes no remux-completion event. While automatic remuxing is enabled, this plugin ignores its action hotkeys, leaving recording running. It also cancels file processing if that setting changed while stopping. This prevents a race with OBS's remuxer; it never guesses a remuxed filename.

Split recordings: only the final, exact file returned by OBS is affected. Earlier segments remain in the original folder. File extensions are not filtered. If another recording starts during processing, pending work is cancelled where possible; an already completed move/delete cannot be undone.

## Updates and About

Open **Tools → Recording Actions → About** to see the version, license, third-party credits, and project link, or select **Check for updates**.

Automatic update checks are enabled by default and run once per OBS startup. Turn off **Automatically check for updates (GitHub)** in the plugin settings and save to disable them. Checks contact GitHub Releases over HTTPS; no recording files or destination paths are uploaded.

When a newer stable version is available, you can postpone it or download its installer. The plugin checks the downloaded installer against the release's SHA-256 manifest before saving it in your Windows Downloads folder under `RecordingActions`. A folder opens when the download is ready. Close OBS, run the installer, and restart OBS. Installing remains user-controlled; there is no unattended replacement of a loaded plugin.

## Troubleshooting

| Symptom | What to check |
| --- | --- |
| Recording Actions is missing from Tools | Use the Release build with Windows x64 OBS 32.2.2. Check the install folder and restart OBS. Do not install both ProgramData and OBS-folder copies. |
| Raw `Plugin.Name` or `Settings.*` labels | Update to 1.1.0 or newer using the installer. Install all language resources; this version also embeds fallback translations. |
| A move shortcut does nothing | Save an absolute destination folder, assign its OBS hotkey, and disable automatic remuxing. An empty destination disables that target. |
| A shortcut leaves recording running | Check that OBS is recording and automatic remuxing is disabled. The plugin intentionally ignores actions while automatic remuxing is enabled. |
| Alias labels look outdated in OBS | Save the plugin settings, then close and reopen the OBS Hotkeys settings page. |
| A move fails | Enable plugin logs, check the OBS log, destination permissions, free space, and file locks. Failed processing preserves the source. |
| Update checking or downloading fails | Check HTTPS access to GitHub and install the complete package, including the TLS backend. Retry from About or download from Releases. |
| Setup refuses installation | Close OBS, select the OBS folder containing `bin/64bit/obs64.exe`, and remove any duplicate ProgramData plugin copy. |

## File safety and lifecycle

- No file operation occurs before `OBS_FRONTEND_EVENT_RECORDING_STOPPED`.
- The exact `obs_frontend_get_last_recording()` path is captured once. No directory scans, wildcards, or filename inference are used.
- Same-volume moves use a Windows handle rename with replacement disabled. Existing names receive `_1`, `_2`, etc. Moving into the source's own directory is a successful no-op.
- Cross-volume moves create an exclusive new destination, copy all bytes, check byte counts and sizes, flush the destination, then delete the source by its open handle. Failure preserves the source and removes the partial destination when possible. A filesystem/device cleanup failure can leave a partial destination; it is never overwritten on retry.
- Source handles deny writes, rename, and deletion during processing. File identity, size, and last-write time are checked across retries to reject a replaced/changed file.
- Directories, source reparse points, device paths, alternate data streams, and relative paths are rejected.
- Access/sharing/lock errors retry every 500 ms, up to 20 retries after the first attempt. This is about 10 seconds of retry waiting, excluding copy time.
- Work runs outside the UI thread. Shutdown cancels retries and requests cancellation of synchronous worker I/O, then joins the worker before unloading its code. A storage driver that does not promptly complete/cancel I/O can delay shutdown.

## Settings and logs

Settings and hotkeys are stored together in the OBS module configuration directory, normally `%APPDATA%\obs-studio\plugin_config\obs-recording-actions\settings.json`. Portable OBS uses its portable configuration root. Targets are plugin-wide, not per-profile. Writes are atomic through Qt `QSaveFile`; failed saves do not overwrite the previous file. The window remembers its position and size. The dialog uses OBS's Qt theme and standard keyboard-accessible controls.

**Enable logs** is off by default. When enabled, plugin messages appear in OBS's normal log with `[Recording Actions]`. Disable it to suppress all plugin-generated informational, warning, and error messages. File-operation failures never open a popup. Settings validation/save errors appear inline in the settings dialog.

## Build and test

- For builds: Visual Studio 2022 Desktop development with C++, v143 tools, Windows SDK 10.0.20348 or newer, and CMake 3.28 or newer.
- PowerShell 7 for the optional build script. Internet access for the initial OBS/Qt development dependency download; several GB of free disk space.

The build follows the [official OBS Plugin Template](https://github.com/obsproject/obs-plugintemplate) and uses the [OBS Frontend API](https://docs.obsproject.com/reference-frontend-api). The Windows CMake helpers originate from template commit `3e7d7ac3b5342cd7d9b88890b9c70b472d1520fc`, with serial SDK builds and cached-archive checksum verification added. Dependency versions and SHA-256 values are pinned in `buildspec.json`. The plugin version has one source, `buildspec.json`, and is embedded in the DLL and settings window.

From a Visual Studio 2022 developer terminal:

```powershell
cmake --preset windows-x64
cmake --build --preset debug --parallel 1
cmake --build --preset release --parallel 1
ctest --preset release
cpack --config build_x64/CPackConfig.cmake -C Release -B dist
```

Package builds require Inno Setup 6; use `-InnoCompiler <path-to-ISCC.exe>` if it is not in its standard install folder.

The first configure builds the OBS development libraries in Debug and Release. Qt and OBS runtime libraries are supplied by OBS. Packaging includes only the matching Qt Schannel TLS backend under plugin data; do not copy unrelated SDK DLLs into OBS.

Alternatively, build, test, and package in one step:

```powershell
pwsh -File scripts/build.ps1 -Package
pwsh -File scripts/build.ps1 -Configuration Debug
```

For a newer Visual Studio host with v143 installed:

```powershell
pwsh -File scripts/build.ps1 -Generator "Visual Studio 18 2026" -Toolset v143 -BuildDirectory build_local -Package
```

Outputs:

- Release DLL: `build_x64/Release/obs-recording-actions.dll`.
- Debug DLL: `build_x64/Debug/obs-recording-actions.dll` (requires a matching debug Qt/OBS environment; use Release with normal OBS).
- Installer: `dist/obs-recording-actions-1.1.0-windows-x64-setup.exe`.
- Installable ZIP: `dist/obs-recording-actions-1.1.0-windows-x64.zip`.
- Corresponding source ZIP: `dist/obs-recording-actions-1.1.0-source.zip`.
- Download checksums: `dist/SHA256SUMS.txt`.

Tests use disposable fixtures and a simulated OBS frontend; they do not change your OBS profile or recordings. Core tests exercise real file moves/deletes, collisions, replacement races, locks, retries, and cancellation. Cross-volume tests run when the build directory and Windows `TEMP` are on different drives. The integration test uses actual libobs hotkey/data APIs and Qt widgets, with simulated recording events. Test windows may briefly appear. Actual OBS recording/encoder behavior still needs the manual check below.

Format project C++ using the included `.clang-format`:

```powershell
$files = Get-ChildItem src,tests -Recurse -File -Include *.cpp,*.hpp
clang-format --dry-run --Werror $files.FullName
```

If MSBuild reports duplicate `PATH`/`Path` keys, use `scripts/build.ps1`, which normalizes its child environment. Dependency download failures are environment/network failures: configure a working HTTPS proxy or direct connection; do not disable TLS verification. Upstream SDK configuration may emit warnings about unused capture/virtual-camera components; the plugin does not build those components.

## Manual OBS acceptance check

Use disposable recordings and two empty test destinations:

1. Assign all three hotkeys; verify aliases and bindings survive an OBS restart.
2. Record a short MKV; press Target 1 and verify its content and final location. Repeat for Target 2 and another recording format.
3. Verify pressing a second action while stopping does not change the first action. Holding a key must not affect a subsequent recording.
4. Pre-create a destination with the same filename; verify `_1` is created and the existing file is unchanged.
5. Test a destination on another physical volume, then an unwritable destination. The latter must preserve the source.
6. Test Delete with one disposable recording; verify neighboring files remain untouched.
7. Verify no action when idle, no plugin logs when disabled, useful logs when enabled, and normal OBS exit with a pending operation.
8. Check the Tools window under light/dark OBS themes and 100%, 150%, and 200% display scaling.

## License and release status

GPL-2.0-or-later; see `LICENSE` and `data/THIRD_PARTY_NOTICES.md`. The About dialog lists OBS, Qt, and template credits. The package includes the matching Qt Schannel TLS backend for HTTPS; its licenses and corresponding-source links are in `data/THIRD_PARTY_NOTICES.md`.

Published by [Diddlik](https://github.com/Diddlik) at [obs-recording-actions](https://github.com/Diddlik/obs-recording-actions). Version 1.1 includes automatic HTTPS update checks using GitHub Releases, a manual check in About, and an optional SHA-256-verified installer download. Checks run once per OBS startup and can be disabled in plugin settings. Installation remains user-controlled: close OBS and run the downloaded installer. The installer refuses installation or uninstallation while OBS runs and preserves OBS settings. Keep the corresponding source with any binary distribution.

The build script's `-Package` switch also writes `dist/obs-recording-actions-1.1.0-source.zip` containing the buildable project source, tests, documentation, and pinned build helpers.

Local verification on 2026-10-06: Release and Debug builds with MSVC v143 on the Visual Studio 2026 host; core and frontend-simulation test suites pass. Real cross-volume moves were exercised between `F:` and `C:`. The Release DLL also loads against the installed OBS 32.2.2 runtime libraries. Version 1.1.0 passes 34 core and 39 integration checks in Debug and Release. GitHub CI also verifies live HTTPS with the packaged Schannel backend. Full interactive OBS recording, install/uninstall, update download, and display-scaling checks remain manual.

GitHub Actions builds and tests Debug and Release on Windows for pushes to `main` and pull requests. Tags matching the version in `buildspec.json`, such as `v1.1.0`, publish the tested packages with SHA-256 checksums. Release notes are stored in `docs/release-notes/`.

To retry an interrupted publication, run the workflow manually from `main` with `release_tag` set to the existing version tag. It builds that tagged source and uploads the ZIP packages, installer, and checksum file; CPack's staging directories are excluded.
