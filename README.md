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
vs.proj/TidyText.vcxproj    MSBuild project (x64 only, matches Notepad++'s architecture)
bin64/TidyText.dll          Build output
```

## Building

Requires the MSVC C++ toolchain (Visual Studio Build Tools 2022, "Desktop development with C++"
workload). Build with:

```
"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe" vs.proj\TidyText.vcxproj /p:Configuration=Release /p:Platform=x64
```

Output: `bin64\TidyText.dll`.

## Installing

Copy the DLL into its own subfolder under Notepad++'s plugins directory (requires admin rights
since Notepad++ is installed under Program Files):

```
C:\Program Files\Notepad++\plugins\TidyText\TidyText.dll
```

Context menu entries are configured in `C:\Program Files\Notepad++\contextMenu.xml` under
`<ScintillaContextMenu>`:

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
`contextMenu.xml`.
