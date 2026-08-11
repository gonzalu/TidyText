#include "PluginDefinition.h"
#include "menuCmdID.h"
#include "NonPrintingChars.h"
#include "SendToAI.h"

//
// The plugin data that Notepad++ needs
//
FuncItem funcItem[nbFunc];

//
// The data of Notepad++ that you can use in your plugin commands
//
NppData nppData;

//
// Initialize your plugin data here
// It will be called while plugin loading
void pluginInit(HANDLE /*hModule*/)
{
}

//
// Here you can do the clean up, save the parameters (if any) for the next session
//
void pluginCleanUp()
{
}

//
// Initialization of your plugin commands
// You should fill your plugins commands here
void commandMenuInit()
{
    setCommand(0, TEXT("Highlight Non-Printing Characters"), highlightNonPrintingChars, NULL, false);
    setCommand(1, TEXT("Clear Highlights"), clearNonPrintingHighlights, NULL, false);
    setCommand(2, TEXT("Replace Non-Printing Characters"), replaceNonPrintingChars, NULL, false);

    // separator: a FuncItem with a NULL function pointer renders as a menu separator
    lstrcpy(funcItem[3]._itemName, TEXT("-"));
    funcItem[3]._pFunc = NULL;
    funcItem[3]._init2Check = false;
    funcItem[3]._pShKey = NULL;

    setCommand(4, TEXT("Send Selection to ChatGPT"), sendSelectionToChatGPT, NULL, false);
    setCommand(5, TEXT("Send Selection to Claude"), sendSelectionToClaude, NULL, false);
    setCommand(6, TEXT("Send Selection to Gemini"), sendSelectionToGemini, NULL, false);
}

//
// Here you can do the clean up (especially for the shortcut)
//
void commandMenuCleanUp()
{
	// Don't forget to deallocate your shortcut here
}


//
// This function help you to initialize your plugin commands
//
bool setCommand(size_t index, TCHAR *cmdName, PFUNCPLUGINCMD pFunc, ShortcutKey *sk, bool check0nInit)
{
    if (index >= nbFunc)
        return false;

    if (!pFunc)
        return false;

    lstrcpy(funcItem[index]._itemName, cmdName);
    funcItem[index]._pFunc = pFunc;
    funcItem[index]._init2Check = check0nInit;
    funcItem[index]._pShKey = sk;

    return true;
}
