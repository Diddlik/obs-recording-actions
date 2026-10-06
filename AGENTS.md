# Shared Agent Instructions

This file is the canonical instruction source for Codex, Claude Code, and GitHub Copilot CLI.

## Maintenance

- Keep this file synchronized with the repository's verified behavior.
- Update this file in the same change when build commands, validation steps, architecture constraints, workflows, or conventions change.
- Remove obsolete instructions instead of appending corrections.
- Record only durable facts that are not obvious from the codebase.
- Replace an `OPEN` entry as soon as the decision is made or the fact becomes verifiable.
- Tool-specific instruction files contain only a pointer to this file and genuine tool-specific exceptions.

## First-use onboarding

If any `[TO FILL]` entry remains, complete this onboarding before implementing the user's first task:

1. Inspect the repository and determine every project fact that can be verified from existing files and commands.
2. Do not ask the user for information that can be discovered reliably from the repository.
3. Briefly present the discovered facts, then ask guided questions only for the remaining decisions or unknowns.
4. Ask one focused question or one closely related group of no more than three questions at a time. Explain why each answer matters and offer a recommended option when useful while allowing a free-form answer.
5. Cover the remaining topics in the order of the project facts list: purpose and scope, languages, runtimes and platforms, GUI, build and validation commands, environment requirements, then entry points, architecture, and generated-file constraints.
6. After the user answers, replace the applicable placeholders with concise verified facts, remove entries that do not apply, and record unresolved decisions explicitly as `OPEN` rather than inventing an answer.
7. Summarize what was written to this file, then continue with the user's original task.

If the user cannot be asked (non-interactive run, CI, issue-driven agent, subagent), fill every verifiable fact, mark the rest `OPEN`, and proceed with the task.

## Communication

- Respond to the user in the language the user writes in, including onboarding questions.
- Project artifacts stay in English as defined below.

## Implementation

- Implement only what the current requirement needs.
- Prefer editing existing code over adding files, layers, helpers, or abstractions.
- Use the standard library and existing dependencies before writing custom implementations.
- Do not add speculative extension points, configuration, parameters, or abstractions.
- Add a dependency only when it provides a concrete benefit and does not duplicate existing functionality.
- Preserve validation, security, accessibility, error handling, and data-integrity safeguards.

## Language and comments

- Use English for source code, identifiers, comments, tests, documentation, logs, and commit messages.
- Comments explain why a decision, constraint, workaround, or non-obvious trade-off exists.
- Do not comment what readable code already expresses.
- Prefer clear naming and small functions over explanatory comments.

## Git

- Commit coherent, verified units of work such as a finished feature, fix, or refactoring step, not every individual edit.
- Group related changes into one commit; keep unrelated changes in separate commits.
- Keep commit messages limited to change- and process-related content.
- Never add AI attribution or co-author trailers such as `Co-Authored-By: Copilot`, Claude, Codex, or similar, in commits or pull requests.

## GUI applications

Apply this section when the project has or adds a graphical user interface.

Required features:

- Versioning: one single source for the version number (Semantic Versioning), shown in the UI and embedded in build artifacts.
- About dialog: application name, version, copyright, license, and credits for every third-party library with its license.
- Auto-update: check for updates over HTTPS, verify the update's signature or checksum before installing, let the user postpone or disable automatic checks, and never lose unsaved data during an update.

Design:

- Plan the layout, navigation, and main user flows before implementing screens.
- Follow the platform's current design system: Fluent 2 on Windows, Apple Human Interface Guidelines on macOS and iOS, Material 3 on Android and where no platform system applies.
- Use native or toolkit-standard controls instead of custom widgets.
- Use a consistent spacing grid (multiples of 4 or 8 px), a limited type scale, and the system font unless the brand requires otherwise.
- Support light and dark mode and follow the system setting by default.
- Keep a clear visual hierarchy: one primary action per view, secondary actions visually subordinate, generous whitespace instead of borders and boxes.
- Design empty, loading, and error states; errors say what happened and what the user can do.
- Keep the UI responsive: run long operations in the background with progress indication and cancellation.
- Prefer undo over confirmation dialogs for reversible actions.
- Adapt to window sizes and display scaling (high DPI) without clipping or overlapping content.
- Accessibility: meet WCAG 2.2 AA contrast, provide full keyboard navigation with visible focus, label controls for screen readers, and respect reduced-motion settings.
- Keep animations short and purposeful.
- Keep user-facing strings out of code logic so they can be localized.
- Remember window size, position, and user preferences between sessions.

## Working method

- Inspect the relevant implementation and existing conventions before editing.
- Search for an existing implementation before creating a new one.
- Make the smallest coherent change that fully satisfies the request.
- Preserve unrelated user changes.
- Do not perform unrelated refactoring during a focused change.
- Ask before destructive, irreversible, security-sensitive, or materially out-of-scope actions.

## Verification

- Run the narrowest relevant test, build, lint, format, or executable check after the last change.
- Add or update tests for non-trivial behavior changes and bug fixes.
- Do not claim success without current verification evidence.
- Report what was verified and what could not be verified.
- Distinguish product failures from environment, permission, network, and tooling failures.

## Security

- Never commit, print, store, or document secrets, tokens, credentials, or private keys.
- Validate data at user, file, environment, process, and network boundaries.
- Preserve authentication, authorization, escaping, permission checks, and safe defaults.
- Do not weaken security controls to make tests or local execution pass.

## Project facts

- Purpose and scope: Native OBS Recording Actions v1: exactly two configurable stop-and-move targets and one permanent stop-and-delete hotkey; REQ.md is the feature specification.
- Primary languages and runtimes: C++17; OBS Frontend API and Qt 6 Widgets and Network supplied by OBS. No Python or external runtime at plugin runtime.
- Platform-specific constraints: Windows 10/11 x64 only; current stable OBS is 32.2.2 (verified 2026-10-06). Windows file handles must prevent replacement races and overwrite.
- GUI toolkit, update mechanism, and distribution: Qt 6 modeless Tools settings and About dialogs using the OBS theme. ZIP contains the plugin DLL, PDB, locales, matching Qt Schannel TLS backend and license notices; scripts/build.ps1 also packages corresponding source. Publisher: Diddlik; repository and releases: https://github.com/Diddlik/obs-recording-actions. English and German locales are also embedded as fallback. Automatic HTTPS update checks use GitHub Releases, are disableable, and downloads require confirmation plus SHA-256 manifest verification. The Inno Setup installer refuses updates/uninstall while OBS runs; users close OBS and run setup themselves.
- CI and release: `.github/workflows/build.yml` builds/tests Debug and Release on windows-2022 for main pushes and pull requests. Matching `v*.*.*` tags publish ZIP packages, the Inno Setup installer and SHA256SUMS.txt after tests pass; release notes live in docs/release-notes. Manual dispatch with release_tag builds that existing tag and can safely retry interrupted asset publication without moving the tag.
- Build command: `pwsh -File scripts/build.ps1 -Package` (VS 2022). Local host: add `-Generator "Visual Studio 18 2026" -Toolset v143 -BuildDirectory build_local`. The script normalizes duplicate PATH/Path environment keys. SDK builds are serial. Package builds require Inno Setup 6; pass -InnoCompiler for a nonstandard compiler path.
- Test command: `ctest --test-dir build_x64 -C Release --output-on-failure`. Core tests use real disposable files; integration tests use real Qt/libobs with simulated frontend events. Cross-volume coverage requires build and TEMP on different drives.
- Lint and format command: In PowerShell, run `clang-format --dry-run --Werror (Get-ChildItem src,tests -Recurse -File -Include *.cpp,*.hpp).FullName`; `.clang-format` follows the OBS template. Compile warnings are errors in the build script.
- Local run command: Load the built DLL and locale resources in OBS Studio; installed OBS is C:/Program Files/obs-studio/bin/64bit/obs64.exe. Do not alter the user OBS configuration for automated tests.
- Required environment: CMake 3.28+, Visual Studio 2022 C++ tools or compatible newer host with v143, Windows SDK, matching OBS development libraries and Qt 6 SDK, and Inno Setup 6 for installers.
- Important entry points: `src/plugin-main.cpp` owns the module; `recording-actions.cpp` handles UI/hotkeys/lifecycle; `settings.cpp` handles atomic JSON persistence; `file-actions.cpp` owns Windows handle operations.
- Architecture constraints: OBS calls and manager state stay on the frontend UI thread; a cancellable worker owns filesystem operations. Target configuration is snapshotted per action; overlapping hotkeys are ignored. Automatic remuxing blocks actions because OBS starts it after STOPPED. Settings use OBS JSON plus QSaveFile (atomic replacement without ACL-copy rights). Never infer recording filenames.
- Generated files: Build trees, downloaded dependencies, generated version/resource files, test fixtures and packages are generated; exclude them from source control.
- Files or directories not to edit manually: Preserve .claude/, graft/, and tool instruction adapters. Do not edit generated dependency/build outputs as project source.

Do not begin implementation while `[TO FILL]` entries remain. Follow the guided onboarding above instead.

## Definition of done

A change is complete when:

- The requested behavior is implemented.
- Relevant verification passes after the final edit.
- Appropriate error paths and edge cases are handled.
- Documentation and this file reflect changed behavior or workflows.
- No unrelated files, abstractions, or dependencies were introduced.
- Remaining limitations and unverified points are stated clearly.
