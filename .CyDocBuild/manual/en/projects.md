---
id: projects
title: Files, folders and windows
description: Keep independent file sets without loading an IDE project.
section: Using Reader
order: 80
parent: overview
revision: 2
---

## File sets {#projects}

Reader owns open documents rather than a solution/build model. Open files directly and use the sidebar or Ctrl+P to select a tab. Detaching an editor tab or Move to new window creates a separate native window. Start another application process for an independent set of files.

## Unreal text files {#unreal}

C++/Unreal syntax modes highlight source and macro-style tokens. `.uproject`, `.uplugin` and `.sln` remain text documents. Reader does not run UnrealBuildTool, generate a compile database or edit Blueprint assets. Use CyCode for those project services.

## Example {#example}

Open `Source/main.cpp`, a shader and a configuration file together. Search their live text with Ctrl+Shift+F and compare two tabs. None of this requires a compiler or a project import.
