---
id: plugins
title: Manage extensions
description: Inspect commands, options and syntax contributions.
section: Using Reader
order: 120
parent: overview
revision: 2
---

## Extensions {#extensions}

Open Extensions to inspect loaded plugins, declared commands, languages, options and separate load errors. Configure options through their typed fields; invalid values are refused. Commands that return text update the live buffer and do not save it. Inspect a transformation before saving important files.

Install manifests and matching optional native libraries in `plugins` beside the application, then restart. The supplied text-tools example adds sort/trim commands and CyConfig syntax; the native example and Git provider demonstrate other contributions. There is no marketplace, downloader, hot reload or native-code sandbox.

## Development {#development}

Use Reader's ABI 1, not CyCode's different plugin ABI or Notepad++ plugins. See the [public native contract](doc:plugin-api#contract), [syntax definitions](doc:syntax-plugins#definitions) and [comparison providers](doc:comparison-api#contract). Native libraries execute trusted code with the process's privileges and must return promptly.
