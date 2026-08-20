# TidyText — Notepad++ Plugin

A native C++ Notepad++ plugin with two commands:

1. **Non-printing character cleanup** — highlight and/or replace invisible/smart-typography
   characters (NBSP, zero-width spaces, curly quotes, em/en dashes, BOM, stray control chars, etc.)
   with their plain ASCII/correct equivalents.
2. **Send selection to AI** — copies the current selection to the clipboard and opens
   ChatGPT, Claude, or Gemini in your default browser so you can paste it in.

Menu commands (Plugins → TidyText, and right-click → TidyText):
- Highlight Non-Printing Characters
- Clear Highlights
- Replace Non-Printing Characters
- Send Selection to ChatGPT
- Send Selection to Claude
- Send Selection to Gemini

## Project layout

```
src/                  Source files + vendored Notepad++/Scintilla plugin API headers
  PluginDefinition.h/.cpp   Command table wiring
  TidyText.cpp/.rc          DllMain + required plugin exports, version info
  NonPrintingChars.h/.cpp   Feature 1
  SendToAI.h/.cpp           Feature 2
vs.proj/TidyText.vcxproj    MSBuild project (Win32, x64, ARM64)
.github/workflows/build.yml CI: builds all 3 platforms; on a `v*` tag, publishes a GitHub
                             Release with each platform's TidyText.dll zipped
bin/TidyText.dll             Win32 build output
bin64/TidyText.dll           x64 build output
arm64/TidyText.dll           ARM64 build output
```

## Building

Requires the MSVC C++ toolchain (Visual Studio Build Tools 2022, "Desktop development with C++"
workload; the ARM64 target additionally needs the "MSVC ARM64 build tools" component). Build
with:

```
"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe" vs.proj\TidyText.vcxproj /p:Configuration=Release /p:Platform=x64
```

(`Platform` can be `Win32`, `x64`, or `ARM64`.) Output lands in `bin\`, `bin64\`, or `arm64\`
respectively.

CI (`.github/workflows/build.yml`) builds all three platforms on every push/PR, and on a
`v*` tag additionally packages each into a zip and publishes them to a GitHub Release. To cut
a release: bump the version in `src/TidyText.rc` (`VERSION_VALUE`/`VERSION_DIGITALVALUE`),
commit, then `git tag vX.Y.Z && git push --tags`.

## Installing

Copy the DLL matching your Notepad++'s architecture into its own subfolder under Notepad++'s
plugins directory (requires admin rights since Notepad++ is installed under Program Files):

```
C:\Program Files\Notepad++\plugins\TidyText\TidyText.dll
```

Context menu entries need to go in whichever `contextMenu.xml` Notepad++ actually reads —
usually `%APPDATA%\Notepad++\contextMenu.xml` (per-user config), but if a `doLocalConf.xml`
file exists next to `notepad++.exe`, it's the one in the install directory
(`...\Notepad++\contextMenu.xml`) instead. Add these inside `<ScintillaContextMenu>`:

```xml
<Item FolderName="TidyText" PluginEntryName="TidyText" PluginCommandItemName="Highlight Non-Printing Characters" />
<Item FolderName="TidyText" PluginEntryName="TidyText" PluginCommandItemName="Clear Highlights" />
<Item FolderName="TidyText" PluginEntryName="TidyText" PluginCommandItemName="Replace Non-Printing Characters" />
<Item FolderName="TidyText" PluginEntryName="TidyText" PluginCommandItemName="Send Selection to ChatGPT" />
<Item FolderName="TidyText" PluginEntryName="TidyText" PluginCommandItemName="Send Selection to Claude" />
<Item FolderName="TidyText" PluginEntryName="TidyText" PluginCommandItemName="Send Selection to Gemini" />
```

Restart Notepad++ after installing for the plugin to load.

## Extending the character mapping table

The non-printing/typography character table lives in `lookupReplacement()` in
`src/NonPrintingChars.cpp` — it's a plain `switch` on Unicode codepoint, easy to add to.

## Uninstalling

Delete `C:\Program Files\Notepad++\plugins\TidyText\` and remove the `TidyText` entries from
whichever `contextMenu.xml` you edited above.
