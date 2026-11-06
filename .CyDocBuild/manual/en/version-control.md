---
id: version-control
title: Compare files and Git revisions
description: Review read-only snapshots of live tabs and repository data.
section: Using Reader
order: 110
parent: overview
revision: 2
---

## Compare {#providers}

Compare selects two live tabs, an external disk file or the last saved version. Unsaved text is included. Choose side-by-side/unified layout, search and previous/next change. Both sides share the scrolling canvas. Source documents and disk files are unchanged. Refresh stale snapshots after editing.

## Git comparisons {#git}

Install Git in PATH and keep the bundled Git file comparisons plugin enabled. Its menu detects the active file's local repository. Compare the buffer with HEAD/index or review HEAD-to-index staged changes. The code/line context menu opens file or selection history, following renames, with differences against a parent, buffer or another selected revision.

These are local read-only operations. Reader does not stage, commit, push or pull. Git context and comparison providers use the [plugin contract](doc:comparison-api#contract); missing Git, untracked files and unsupported content are reported explicitly.
