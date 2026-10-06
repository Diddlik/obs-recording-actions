# Third-party notices

Recording Actions: Copyright (C) 2026 Diddlik and contributors.
Licensed under GPL-2.0-or-later. The full GPL version 2 is included as `LICENSE`.

- OBS Studio, libobs, and OBS Frontend API: OBS Project and contributors, GPL-2.0-or-later. https://github.com/obsproject/obs-studio
- OBS Plugin Template CMake helpers and formatting conventions: OBS Project and contributors, GPL-2.0-or-later. https://github.com/obsproject/obs-plugintemplate
- Qt 6 Core, Gui, Widgets, and Network: The Qt Company Ltd. and contributors. Open-source modules available under LGPL-3.0 / GPL-3.0; this GPL plugin dynamically links to the Qt libraries supplied with OBS. https://www.qt.io/licensing/open-source-lgpl-obligations

The package includes Qt 6.11.1 Schannel TLS backend (`data/qt/tls/qschannelbackend.dll`), Copyright (C) The Qt Company Ltd. and contributors, LGPL-3.0 (with GPL-3.0 alternative). Its matching source is https://download.qt.io/official_releases/qt/6.11/6.11.1/submodules/qtbase-everywhere-src-6.11.1.tar.xz (Qt Network Schannel backend) and the OBS build recipes are https://github.com/obsproject/obs-deps/releases/tag/2026-07-15. Included license texts: `licenses/LGPL-3.0.txt` and `licenses/GPL-3.0.txt`. This dynamically loaded backend can be replaced with a compatible rebuilt version; no restrictions are imposed on modifying or reverse engineering it to debug those modifications. All other Qt and OBS runtime libraries come from OBS and their notices remain part of that installation. The Windows SDK and Visual C++ runtime are Microsoft components supplied by the operating system/OBS prerequisites.
