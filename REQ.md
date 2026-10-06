# OBS Recording Actions Plugin

## Goal

Implement a native OBS Studio plugin for Windows that provides configurable hotkeys for handling a recording after it is stopped.

The plugin must be generic and must **not contain game-specific concepts or hard-coded category names** such as `GG's`, `Highlights`, `Funny`, etc.

Instead, users configure destination folders and assign a custom **name / alias** to each destination.

Example:

```text
Alias: Highlights
Folder: D:\Videos\Highlights
```

or:

```text
Alias: Client A
Folder: D:\Recordings\Client-A
```

or:

```text
Alias: YouTube
Folder: D:\Content\YouTube
```

The plugin replaces an existing Python OBS script and must not require Python or any external runtime.

Use C++ and the official OBS Studio Plugin Template / OBS Frontend API.

---

# Target Platform

Initial scope:

- Windows 10/11 x64
- Current stable OBS Studio
- Visual Studio 2022
- CMake
- Native OBS plugin
- C++17 or newer

Do not implement Linux or macOS support in version 1, but keep filesystem and core logic portable where reasonably possible.

---

# Core Concept

Version 1 supports:

- **Move Target 1**
- **Move Target 2**
- **Delete**

Each move target consists of:

```text
Alias
Destination Folder
```

Example configuration:

```text
Move Target 1
Alias: GG
Folder: D:\Recordings\GG

Move Target 2
Alias: Highlights
Folder: D:\Recordings\Highlights
```

Another user could configure:

```text
Move Target 1
Alias: Work
Folder: E:\OBS\Work

Move Target 2
Alias: YouTube
Folder: E:\OBS\YouTube
```

There must be no business logic tied to the alias.

The alias is only a user-friendly display name.

---

# Hotkeys

The plugin must register three OBS frontend hotkeys.

Conceptually:

```text
Recording Actions: Stop + Move to <Target 1 Alias>
Recording Actions: Stop + Move to <Target 2 Alias>
Recording Actions: Stop + Delete
```

Example with aliases configured as:

```text
Target 1 Alias = GG
Target 2 Alias = Highlights
```

the user should see:

```text
Recording Actions: Stop + Move to GG
Recording Actions: Stop + Move to Highlights
Recording Actions: Stop + Delete
```

If dynamically updating OBS hotkey descriptions after changing an alias is technically problematic, use stable fallback names:

```text
Recording Actions: Stop + Move to Target 1
Recording Actions: Stop + Move to Target 2
Recording Actions: Stop + Delete
```

but show the configured alias clearly inside the plugin settings.

Prefer dynamic hotkey names if the OBS API supports this cleanly and safely.

Hotkey assignments must persist across OBS restarts.

Hotkeys are configured through:

```text
OBS -> Settings -> Hotkeys
```

---

# Functional Requirements

## 1. Stop + Move to Target 1

When the corresponding hotkey is pressed:

1. Check whether OBS is currently recording.
2. If no recording is active:
   - do nothing.
3. Check that Target 1 has a valid configured destination.
4. Set the pending action to `MoveTarget1`.
5. Stop the current OBS recording.
6. Wait until OBS reports that recording has fully stopped.
7. Retrieve the exact path of the completed recording.
8. Move the file into the configured Target 1 directory.

Example:

```text
Target 1 Alias:
YouTube

Target 1 Folder:
D:\Content\YouTube
```

Result:

```text
D:\OBS\2026-10-06_15-20-00.mkv

->

D:\Content\YouTube\2026-10-06_15-20-00.mkv
```

---

# 2. Stop + Move to Target 2

Same behavior as Target 1, but use the configured Target 2 destination.

Example:

```text
Target 2 Alias:
Archive

Target 2 Folder:
D:\Content\Archive
```

---

# 3. Stop + Delete

When the configured hotkey is pressed:

1. Check whether OBS is currently recording.
2. Set the pending action to `Delete`.
3. Stop recording.
4. Wait until OBS reports that recording has fully stopped.
5. Retrieve the exact recording path.
6. Permanently delete that file.

Do not use the Windows Recycle Bin in version 1.

---

# Important Recording Handling

Never move or delete a recording immediately after requesting recording stop.

The plugin must wait until OBS reports that recording has fully stopped.

Expected flow:

```text
Hotkey
   |
   v
Set Pending Action
   |
   v
Request OBS Recording Stop
   |
   v
OBS finishes writing / muxing
   |
   v
Recording Stopped Event
   |
   v
Get Exact Last Recording Path
   |
   v
Move / Delete
```

Use the appropriate OBS Frontend API for:

- checking recording state
- stopping recording
- receiving frontend events
- retrieving the last recording path
- registering frontend hotkeys

---

# State Management

Use an internal state such as:

```cpp
enum class PendingAction
{
    None,
    MoveTarget1,
    MoveTarget2,
    Delete
};
```

Only one recording action may be pending at a time.

If another hotkey is pressed while an operation is already pending:

```text
ignore it
```

Do not:

- execute another operation
- switch the pending action
- display a popup
- display a modal warning

This is important for Stream Decks, keyboards or other devices that may accidentally trigger a command twice.

---

# Configurable Move Targets

Version 1 contains exactly two configurable move targets.

Represent them internally using a reusable structure rather than hard-coded variables.

Recommended structure:

```cpp
struct MoveTarget
{
    std::string alias;
    std::filesystem::path directory;
};
```

For example:

```cpp
MoveTarget target1;
MoveTarget target2;
```

Avoid names such as:

```cpp
ggsFolder
highlightsFolder
```

because categories must remain generic.

Prefer naming such as:

```cpp
target1
target2

moveTarget1
moveTarget2

destination1
destination2
```

---

# Settings

The plugin must provide a configuration UI inside OBS.

Preferred entry:

```text
Tools -> Recording Actions
```

The settings should look approximately like:

```text
Recording Actions

Move Target 1

Name / Alias:
[________________]

Destination Folder:
[________________] [Browse]


Move Target 2

Name / Alias:
[________________]

Destination Folder:
[________________] [Browse]


General

[ ] Enable logs
```

---

# Alias Requirements

Each move target must have a configurable user-facing alias.

Examples:

```text
Highlights
Archive
YouTube
Client A
Good Takes
Project Alpha
Review
Keep
Training
```

Aliases:

- are purely display names
- must not influence filesystem logic
- do not need to match directory names
- should support Unicode
- should be persisted with the OBS configuration

An alias may contain:

```text
spaces
apostrophes
hyphens
Unicode characters
```

Example:

```text
Alias:
Best Takes

Folder:
D:\OBS\Sorted\01
```

This is valid.

---

# Default Aliases

Use neutral defaults.

Recommended:

```text
Target 1
Target 2
```

Do not use game-specific defaults.

---

# Empty Alias Behavior

If an alias is empty, display:

```text
Target 1
```

or:

```text
Target 2
```

as the fallback name.

An empty alias must never break the plugin.

---

# Destination Folder Handling

If a configured destination directory does not exist:

create it automatically.

For example:

```cpp
std::filesystem::create_directories(...)
```

If creation fails:

- abort the move operation
- preserve the source recording
- do not show a modal popup
- log the error only when logging is enabled

Never delete the original recording when a move fails.

---

# Filename Collision Handling

Never overwrite an existing recording.

Example:

Target already contains:

```text
2026-10-06_15-20-00.mkv
```

The plugin should generate:

```text
2026-10-06_15-20-00_1.mkv
```

then:

```text
2026-10-06_15-20-00_2.mkv
```

etc.

The existing destination file must never be overwritten.

Prefer this deterministic naming scheme:

```text
original-name_1.ext
original-name_2.ext
original-name_3.ext
```

---

# Cross-Volume Move Handling

The source recording and target folder may be on different drives.

Example:

```text
Source:
C:\OBS\Recording.mkv

Destination:
D:\Archive
```

The implementation must support this correctly.

Do not assume that a filesystem rename operation alone is sufficient.

The move implementation should correctly support:

```text
same drive
different drive
different Windows volume
```

When required:

```text
copy file
verify operation succeeded
remove original
```

The original recording must only be deleted after the destination file has been successfully created.

---

# File Lock / Retry Handling

OBS or Windows may still hold the file briefly after the recording-stopped event.

Implement retry behavior.

Suggested configuration:

```text
Retry interval:
500 ms

Maximum retries:
20

Maximum total wait:
approximately 10 seconds
```

Retry on temporary errors such as:

```text
permission denied
sharing violation
file in use
```

Do not freeze the OBS UI.

Do not perform:

```cpp
Sleep(10000);
```

on the OBS UI thread.

Use a timer or another non-blocking mechanism.

---

# Supported Recording Formats

The plugin must not make assumptions about the recording format.

It must support any recording filename returned by OBS, including:

```text
.mkv
.mp4
.mov
.flv
.ts
```

Use the exact path returned by OBS.

---

# Logging

Provide:

```text
Enable Logs
```

Default:

```text
false
```

Create helper functions such as:

```cpp
logInfo(...)
logWarning(...)
logError(...)
```

Every plugin-generated log message must respect the setting.

Conceptually:

```cpp
if (!settings.enableLogs)
    return;
```

When logs are disabled:

- no info logs
- no warning logs
- no error logs from this plugin
- no log popup caused by the plugin
- no modal message boxes

The plugin should silently ignore normal situations such as:

```text
hotkey pressed while OBS is not recording
second hotkey press while operation is already pending
```

When logging is enabled, useful messages may include:

```text
[Recording Actions] Stop requested: Target 1 (YouTube)

[Recording Actions] Recording finished:
D:\OBS\recording.mkv

[Recording Actions] Moving to target "YouTube"

[Recording Actions] Move completed:
D:\YouTube\recording.mkv
```

---

# Persistence

Persist:

```text
Target 1 Alias
Target 1 Folder

Target 2 Alias
Target 2 Folder

Enable Logs

Hotkey assignments
```

All values must survive:

```text
OBS restart
plugin reload
Windows restart
```

Use OBS configuration APIs where appropriate.

---

# Internal Configuration Model

Recommended:

```cpp
struct MoveTarget
{
    std::string alias;
    std::filesystem::path directory;
};

struct PluginSettings
{
    MoveTarget target1;
    MoveTarget target2;

    bool enableLogs = false;
};
```

Do not spread configuration across many global variables.

---

# Suggested Plugin Architecture

Prefer a manager class such as:

```cpp
class RecordingActions
{
public:
    bool initialize();
    void shutdown();

    void stopAndMoveToTarget1();
    void stopAndMoveToTarget2();
    void stopAndDelete();

    void handleFrontendEvent(obs_frontend_event event);

private:
    PendingAction pendingAction = PendingAction::None;

    PluginSettings settings;

    void startAction(PendingAction action);
    void processLastRecording();

    bool moveRecording(
        const std::filesystem::path& source,
        const MoveTarget& target);

    bool deleteRecording(
        const std::filesystem::path& source);
};
```

Improve this architecture if a cleaner solution is appropriate.

---

# Project Structure

Prefer something similar to:

```text
obs-recording-actions/
│
├── CMakeLists.txt
├── buildspec.json
│
├── src/
│   ├── plugin-main.cpp
│   ├── recording-actions.cpp
│   ├── recording-actions.hpp
│   ├── settings.cpp
│   └── settings.hpp
│
├── data/
│   └── locale/
│       └── en-US.ini
│
└── README.md
```

Avoid unnecessary complexity.

---

# Localization

Do not hard-code UI strings throughout the implementation.

Use OBS localization resources where appropriate.

Example:

```text
Plugin.Name="Recording Actions"

Settings.Target1="Move Target 1"
Settings.Target2="Move Target 2"

Settings.Alias="Name / Alias"
Settings.Directory="Destination Folder"

Settings.EnableLogs="Enable Logs"

Hotkey.Target1="Stop + Move to Target 1"
Hotkey.Target2="Stop + Move to Target 2"
Hotkey.Delete="Stop + Delete"
```

This should make additional languages easy to add later.

---

# Thread Safety

Avoid unsafe shared state.

If asynchronous filesystem work or retry handling introduces multiple execution contexts, protect shared state appropriately.

Do not call OBS APIs from arbitrary worker threads unless the relevant OBS API explicitly allows it.

OBS-related operations should remain on an appropriate OBS/frontend thread.

Filesystem operations may be separated where reasonable.

---

# OBS Lifecycle

During plugin load:

```text
initialize configuration

load persisted settings

register frontend event callback

register Target 1 hotkey

register Target 2 hotkey

register Delete hotkey

restore saved hotkey bindings
```

During unload:

```text
cancel retry operations

remove event callbacks where required

release resources

persist settings if required
```

OBS must shut down normally even when a recording operation is pending.

---

# Safety Requirements

## Move

Never overwrite an existing file.

Never delete the source file unless the destination operation completed successfully.

If anything fails:

```text
preserve original recording
```

## Delete

Delete only the exact recording path returned by OBS.

Delete only when:

```cpp
pendingAction == PendingAction::Delete
```

After processing:

```text
clear pending action
clear cached recording path
clear retry state
```

Never:

```text
delete directories
use wildcards
delete multiple recordings
guess which recording should be deleted
```

---

# Hotkey Behavior

Only act on key-down.

Equivalent:

```cpp
if (!pressed)
    return;
```

Holding the key must not continuously execute the action.

If another recording action is already pending:

```text
return immediately
```

---

# Version 1 Scope

Implement:

```text
2 configurable move targets
1 delete action
custom aliases
custom folders
hotkeys
settings persistence
hotkey persistence
logging toggle
collision protection
retry handling
cross-volume moves
```

Do NOT implement:

```text
automatic recording restart
Replay Buffer management
streaming actions
history database
undo
Recycle Bin
recording preview
Stream Deck-specific plugin
network upload
cloud upload
Linux support
macOS support
```

---

# Extensibility

Although version 1 has exactly two move targets, design the code so that adding more targets later is straightforward.

Avoid logic such as:

```cpp
if (alias == "Highlights")
```

or any other alias-dependent behavior.

Aliases are data only.

A future version could support:

```text
unlimited configurable destinations
```

for example:

```text
Target:
YouTube

Target:
Archive

Target:
Client A

Target:
Review

Target:
Training
```

Do not implement the dynamic target list yet, but avoid architecture that would make this unnecessarily difficult.

---

# Potential Future Features

The architecture should permit future actions such as:

```text
Stop + Move + Start New Recording

Move Last Recording

Delete Last Recording

Duplicate Recording to Target

Unlimited Move Targets

Target-specific Hotkeys

Target-specific Icons

Notifications

Recycle Bin instead of permanent delete

Stream Deck integration
```

Do not implement these in version 1.

---

# Build Requirements

Use official OBS plugin-template conventions where possible.

The repository must be buildable using:

```text
Windows 11
Visual Studio 2022
CMake
OBS development dependencies
```

README must explain:

1. prerequisites
2. CMake configuration
3. Debug build
4. Release build
5. output DLL location
6. plugin installation into OBS
7. configuring move targets
8. assigning aliases
9. assigning hotkeys
10. enabling/disabling logs

---

# Code Quality

Requirements:

- modern C++
- RAII
- `std::filesystem`
- no raw owning pointers
- minimal global state
- generic naming
- no game-specific concepts
- reusable `MoveTarget` model
- Unicode-safe path handling on Windows
- comments only where valuable
- clear separation between OBS integration and filesystem operations
- no unnecessary external dependencies
- no unnecessary abstraction

Do not over-engineer the first version.

---

# Acceptance Criteria

## Scenario 1 – Target 1

Configuration:

```text
Target 1 Alias:
YouTube

Target 1 Folder:
D:\Content\YouTube
```

Given:

```text
OBS is recording
```

When the Target 1 hotkey is triggered.

Then:

```text
recording stops

OBS finalizes the file

file is moved to:
D:\Content\YouTube

original recording is removed only after successful move
```

---

# Scenario 2 – Target 2

Configuration:

```text
Target 2 Alias:
Archive

Target 2 Folder:
D:\Archive
```

When the Target 2 hotkey is triggered:

```text
recording stops
file is finalized
file is moved to D:\Archive
```

---

# Scenario 3 – Delete

When:

```text
Stop + Delete
```

is triggered:

```text
recording stops
OBS finalizes recording
exact completed recording is deleted
```

---

# Scenario 4 – Aliases

Given:

```text
Target 1 Alias = Client A
```

the plugin UI should display the alias clearly.

Where technically feasible, the corresponding hotkey should also be represented as:

```text
Stop + Move to Client A
```

No filesystem behavior should depend on the alias.

---

# Scenario 5 – Double Press

Given the first hotkey already started recording shutdown.

When the user presses another Recording Actions hotkey.

Then:

```text
nothing happens
pending action remains unchanged
no popup appears
no duplicate operation occurs
```

---

# Scenario 6 – No Recording

Given OBS is not recording.

When any plugin hotkey is triggered.

Then:

```text
nothing happens
no popup appears
OBS remains stable
```

---

# Scenario 7 – Existing Destination Filename

Given:

```text
D:\Archive\recording.mkv
```

already exists.

Moving another:

```text
recording.mkv
```

must result in:

```text
recording_1.mkv
```

The existing file must remain untouched.

---

# Scenario 8 – Invalid Destination

Given the destination cannot be created or written to.

Then:

```text
the source recording remains untouched

the file is not lost

no modal popup appears

an error is logged only if Enable Logs is active
```

---

# Scenario 9 – Different Drives

Given:

```text
Recording:
C:\OBS\recording.mkv

Target:
D:\Videos\Archive
```

the move must succeed even though source and destination are on different volumes.

---

# Codex Implementation Task

Create the complete buildable plugin repository.

Do not produce only sample code, snippets or pseudocode.

Implement the full version-1 feature set described in this specification.

Produce:

```text
complete C++ source code

CMake configuration

OBS plugin metadata

localization file

settings implementation

hotkey implementation

recording event handling

safe filesystem handling

README

build instructions

installation instructions
```

Use current OBS Frontend API and official OBS plugin-template conventions.

After implementation:

1. build the project
2. fix compiler errors
3. inspect OBS lifecycle handling
4. inspect hotkey persistence
5. inspect settings persistence
6. inspect custom alias handling
7. inspect double-hotkey behavior
8. inspect destination collision handling
9. inspect cross-volume move behavior
10. inspect move failure safety
11. inspect delete safety
12. inspect retry behavior
13. verify logging can be fully disabled
14. ensure there are no game-specific names or assumptions remaining

Do not stop after scaffolding.

The expected result is a working native OBS Studio plugin suitable for direct installation and testing on Windows x64.