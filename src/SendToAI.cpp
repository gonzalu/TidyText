#include "PluginInterface.h"
#include "SendToAI.h"
#include <shellapi.h>
#include <vector>
#include <cstring>

extern NppData nppData;

namespace {

HWND getCurrentScintilla()
{
    int which = -1;
    ::SendMessage(nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&which);
    if (which == -1)
        return NULL;
    return (which == 0) ? nppData._scintillaMainHandle : nppData._scintillaSecondHandle;
}

// Copies the current selection (UTF-8, as Scintilla stores it) to the Windows
// clipboard as Unicode text, then opens the given URL in the default browser.
void copySelectionAndOpen(const wchar_t *url)
{
    HWND sci = getCurrentScintilla();
    if (!sci)
        return;

    Sci_Position len = (Sci_Position)::SendMessage(sci, SCI_GETSELTEXT, 0, (LPARAM)NULL);
    if (len <= 1) // just the terminating NUL, i.e. empty selection
    {
        ::MessageBox(nppData._nppHandle, TEXT("No text selected."), TEXT("TidyText"), MB_OK | MB_ICONINFORMATION);
        return;
    }

    std::vector<char> utf8Buf((size_t)len);
    ::SendMessage(sci, SCI_GETSELTEXT, 0, (LPARAM)utf8Buf.data());

    int wideLen = ::MultiByteToWideChar(CP_UTF8, 0, utf8Buf.data(), -1, NULL, 0);
    if (wideLen <= 0)
        return;

    std::vector<wchar_t> wideBuf((size_t)wideLen);
    ::MultiByteToWideChar(CP_UTF8, 0, utf8Buf.data(), -1, wideBuf.data(), wideLen);

    if (::OpenClipboard(nppData._nppHandle))
    {
        ::EmptyClipboard();

        HGLOBAL hMem = ::GlobalAlloc(GMEM_MOVEABLE, (SIZE_T)wideLen * sizeof(wchar_t));
        if (hMem)
        {
            void *dest = ::GlobalLock(hMem);
            if (dest)
            {
                memcpy(dest, wideBuf.data(), (size_t)wideLen * sizeof(wchar_t));
                ::GlobalUnlock(hMem);
                ::SetClipboardData(CF_UNICODETEXT, hMem);
            }
            else
            {
                ::GlobalFree(hMem);
            }
        }

        ::CloseClipboard();
    }

    ::ShellExecuteW(NULL, L"open", url, NULL, NULL, SW_SHOWNORMAL);
}

} // namespace

void sendSelectionToChatGPT()
{
    copySelectionAndOpen(L"https://chatgpt.com/");
}

void sendSelectionToClaude()
{
    copySelectionAndOpen(L"https://claude.ai/new");
}

void sendSelectionToGemini()
{
    copySelectionAndOpen(L"https://gemini.google.com/app");
}
