---
id: updates
title: Install and update
description: Use the portable or installed edition update flow.
section: Using Reader
order: 180
parent: overview
revision: 3
---

## Editions {#updates}

Extract the complete portable ZIP into a writable folder and keep `portable.flag`; its settings/cache remain in `data`. The per-user installer normally uses `%LOCALAPPDATA%/Programs/CyCodeReader` and stores preferences separately. It does not change file associations or install the optional Python SDK. Do not mix portable and installed directories.

Public release builds check GitHub Releases in the background at startup by default on new installations. A newer compatible version opens the Updates panel. Use **Download update**, then **Close and install update** after reviewing the version. Existing preferences are preserved; **Updates → Check for updates at startup** enables or disables future startup checks. Manual checks remain available in this panel. There is no separate update download to choose on GitHub. Development/preparation builds may disable checks. No private GitHub credentials are collected by Reader's updater. Review the available version and checksum-verified download before accepting an update.

Portable updates use their compatible ZIP/native helper; installed editions use their installer path. Save or discard documents and close other instances when requested. Cancel preserves the running work. Application closure/restart and update acceptance are explicit actions; opening this manual never installs anything. Linux/macOS updating is not provided.
