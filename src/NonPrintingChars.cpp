#include "PluginInterface.h"
#include "NonPrintingChars.h"
#include <vector>
#include <string>
#include <cstdint>

extern NppData nppData;

namespace {

struct CharMatch
{
    Sci_Position startByte;
    Sci_Position byteLength;
    std::string replacement; // empty means "delete this character"
};

struct DecodedChar
{
    uint32_t cp;
    Sci_Position byteStart;
    Sci_Position byteLength;
};

HWND getCurrentScintilla()
{
    int which = -1;
    ::SendMessage(nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&which);
    if (which == -1)
        return NULL;
    return (which == 0) ? nppData._scintillaMainHandle : nppData._scintillaSecondHandle;
}

// Decodes one UTF-8 codepoint starting at buf[pos] (pos < len).
// Always advances by at least 1 byte, even on malformed input; outLen == 1 for a byte
// that looked like a multi-byte lead but didn't have valid continuation bytes signals failure.
uint32_t decodeUtf8(const unsigned char *buf, size_t len, size_t pos, size_t &outLen)
{
    unsigned char b0 = buf[pos];
    size_t remaining = len - pos;

    auto isCont = [buf, len](size_t i) { return i < len && (buf[i] & 0xC0) == 0x80; };

    if (b0 < 0x80)
    {
        outLen = 1;
        return (uint32_t)b0;
    }
    if ((b0 & 0xE0) == 0xC0 && remaining >= 2 && isCont(pos + 1))
    {
        outLen = 2;
        return (((uint32_t)b0 & 0x1F) << 6) | ((uint32_t)buf[pos + 1] & 0x3F);
    }
    if ((b0 & 0xF0) == 0xE0 && remaining >= 3 && isCont(pos + 1) && isCont(pos + 2))
    {
        outLen = 3;
        return (((uint32_t)b0 & 0x0F) << 12) |
               (((uint32_t)buf[pos + 1] & 0x3F) << 6) |
               ((uint32_t)buf[pos + 2] & 0x3F);
    }
    if ((b0 & 0xF8) == 0xF0 && remaining >= 4 && isCont(pos + 1) && isCont(pos + 2) && isCont(pos + 3))
    {
        outLen = 4;
        return (((uint32_t)b0 & 0x07) << 18) |
               (((uint32_t)buf[pos + 1] & 0x3F) << 12) |
               (((uint32_t)buf[pos + 2] & 0x3F) << 6) |
               ((uint32_t)buf[pos + 3] & 0x3F);
    }

    // Invalid/unsupported lead byte: consume it raw so scanning always progresses.
    outLen = 1;
    return (uint32_t)b0;
}

// Encodes a single codepoint back to UTF-8 bytes.
std::string encodeUtf8(uint32_t cp)
{
    std::string out;
    if (cp <= 0x7Fu)
    {
        out.push_back((char)cp);
    }
    else if (cp <= 0x7FFu)
    {
        out.push_back((char)(0xC0u | (cp >> 6)));
        out.push_back((char)(0x80u | (cp & 0x3Fu)));
    }
    else if (cp <= 0xFFFFu)
    {
        out.push_back((char)(0xE0u | (cp >> 12)));
        out.push_back((char)(0x80u | ((cp >> 6) & 0x3Fu)));
        out.push_back((char)(0x80u | (cp & 0x3Fu)));
    }
    else
    {
        out.push_back((char)(0xF0u | (cp >> 18)));
        out.push_back((char)(0x80u | ((cp >> 12) & 0x3Fu)));
        out.push_back((char)(0x80u | ((cp >> 6) & 0x3Fu)));
        out.push_back((char)(0x80u | (cp & 0x3Fu)));
    }
    return out;
}

// Returns true and fills replacement if this codepoint should be flagged on its own.
bool lookupReplacement(uint32_t cp, std::string &replacement)
{
    switch (cp)
    {
        case 0x00A0u: replacement = " ";   return true; // no-break space
        case 0x00ADu: replacement = "";    return true; // soft hyphen
        case 0x200Bu: replacement = "";    return true; // zero width space
        case 0x200Cu: replacement = "";    return true; // zero width non-joiner
        case 0x200Du: replacement = "";    return true; // zero width joiner
        case 0xFEFFu: replacement = "";    return true; // zero width no-break space / BOM
        case 0x2018u: replacement = "'";   return true; // left single quote
        case 0x2019u: replacement = "'";   return true; // right single quote
        case 0x201Cu: replacement = "\"";  return true; // left double quote
        case 0x201Du: replacement = "\"";  return true; // right double quote
        case 0x2013u: replacement = "-";   return true; // en dash
        case 0x2014u: replacement = "--";  return true; // em dash
        case 0x2026u: replacement = "...";  return true; // ellipsis
        case 0x2028u: replacement = "\n";  return true; // line separator
        case 0x2029u: replacement = "\n";  return true; // paragraph separator
        default:
            if (cp <= 0x1Fu && cp != 0x09u && cp != 0x0Au && cp != 0x0Du)
            {
                replacement = ""; // stray C0 control char
                return true;
            }
            if (cp >= 0x80u && cp <= 0x9Fu)
            {
                replacement = ""; // stray C1 control char (PAD, CCH, ST, OSC, SGC, SCI, ...)
                return true;      // not part of a recoverable mojibake run, but still garbage
            }
            return false;
    }
}

// Byte range to scan: current selection if non-empty, else the whole document.
void getScanRange(HWND sci, Sci_Position &start, Sci_Position &end)
{
    Sci_Position selStart = (Sci_Position)::SendMessage(sci, SCI_GETSELECTIONSTART, 0, 0);
    Sci_Position selEnd = (Sci_Position)::SendMessage(sci, SCI_GETSELECTIONEND, 0, 0);
    if (selEnd > selStart)
    {
        start = selStart;
        end = selEnd;
    }
    else
    {
        start = 0;
        end = (Sci_Position)::SendMessage(sci, SCI_GETLENGTH, 0, 0);
    }
}

std::vector<DecodedChar> decodeRange(const unsigned char *buf, size_t len, Sci_Position baseOffset)
{
    std::vector<DecodedChar> result;
    size_t pos = 0;
    while (pos < len)
    {
        size_t charLen = 1;
        uint32_t cp = decodeUtf8(buf, len, pos, charLen);

        DecodedChar dc;
        dc.cp = cp;
        dc.byteStart = baseOffset + (Sci_Position)pos;
        dc.byteLength = (Sci_Position)charLen;
        result.push_back(dc);

        pos += charLen;
    }
    return result;
}

// Mojibake repair: text that was originally valid UTF-8 (e.g. a curly quote, U+2019)
// but got byte-for-byte reinterpreted as Latin-1/Windows-1252 ends up as 2-4 separate
// "junk" codepoints (each one a former UTF-8 byte, now promoted to its own codepoint,
// e.g. U+00E2, U+0080, U+0099). Reinterpreting those codepoints' low bytes as a raw
// UTF-8 byte stream recovers the original character. Returns the number of source
// codepoints consumed (0 if decoded[i] isn't the start of such a run).
size_t tryMojibakeRepair(const std::vector<DecodedChar> &decoded, size_t i, uint32_t &recoveredCp)
{
    uint32_t lead = decoded[i].cp;
    // Only genuine multi-byte UTF-8 lead bytes (0xC2-0xF4) are candidates.
    if (lead < 0xC2u || lead > 0xF4u)
        return 0;

    unsigned char synth[4];
    size_t available = 0;
    for (size_t k = i; k < decoded.size() && available < 4; ++k)
    {
        if (decoded[k].cp > 0xFFu)
            break;
        synth[available++] = (unsigned char)decoded[k].cp;
    }
    if (available < 2)
        return 0;

    size_t consumed = 1;
    uint32_t cp = decodeUtf8(synth, available, 0, consumed);
    if (consumed < 2) // fell back to the "invalid lead byte" branch: not a real match
        return 0;

    recoveredCp = cp;
    return consumed;
}

std::vector<CharMatch> scanForMatches(HWND sci)
{
    std::vector<CharMatch> matches;

    Sci_Position start = 0, end = 0;
    getScanRange(sci, start, end);
    if (end <= start)
        return matches;

    size_t len = (size_t)(end - start);
    std::vector<char> buf(len + 1);
    Sci_TextRangeFull tr;
    tr.chrg.cpMin = start;
    tr.chrg.cpMax = end;
    tr.lpstrText = buf.data();
    ::SendMessage(sci, SCI_GETTEXTRANGEFULL, 0, (LPARAM)&tr);

    std::vector<DecodedChar> decoded = decodeRange((const unsigned char *)buf.data(), len, start);

    size_t i = 0;
    while (i < decoded.size())
    {
        uint32_t recoveredCp = 0;
        size_t consumed = tryMojibakeRepair(decoded, i, recoveredCp);
        if (consumed > 0)
        {
            std::string replacement;
            if (!lookupReplacement(recoveredCp, replacement))
                replacement = encodeUtf8(recoveredCp);

            Sci_Position totalLen = 0;
            for (size_t k = 0; k < consumed; ++k)
                totalLen += decoded[i + k].byteLength;

            CharMatch m;
            m.startByte = decoded[i].byteStart;
            m.byteLength = totalLen;
            m.replacement = replacement;
            matches.push_back(m);

            i += consumed;
            continue;
        }

        std::string replacement;
        if (lookupReplacement(decoded[i].cp, replacement))
        {
            CharMatch m;
            m.startByte = decoded[i].byteStart;
            m.byteLength = decoded[i].byteLength;
            m.replacement = replacement;
            matches.push_back(m);
        }
        ++i;
    }

    return matches;
}

int g_indicatorID = -1;

int getIndicator(HWND sci)
{
    if (g_indicatorID < 0)
    {
        int startNumber = -1;
        ::SendMessage(nppData._nppHandle, NPPM_ALLOCATEINDICATOR, 1, (LPARAM)&startNumber);
        g_indicatorID = startNumber;
    }
    if (g_indicatorID >= 0)
    {
        ::SendMessage(sci, SCI_INDICSETSTYLE, (WPARAM)g_indicatorID, (LPARAM)INDIC_SQUIGGLE);
        ::SendMessage(sci, SCI_INDICSETFORE, (WPARAM)g_indicatorID, (LPARAM)0x0000FF); // BGR red
    }
    return g_indicatorID;
}

} // namespace

void highlightNonPrintingChars()
{
    HWND sci = getCurrentScintilla();
    if (!sci)
        return;

    int indicator = getIndicator(sci);
    if (indicator < 0)
        return;

    std::vector<CharMatch> matches = scanForMatches(sci);

    ::SendMessage(sci, SCI_SETINDICATORCURRENT, (WPARAM)indicator, 0);
    for (const CharMatch &m : matches)
        ::SendMessage(sci, SCI_INDICATORFILLRANGE, (WPARAM)m.startByte, (LPARAM)m.byteLength);

    TCHAR msg[128];
    wsprintf(msg, TEXT("Found and highlighted %d non-printing/mis-encoded character(s)."), (int)matches.size());
    ::MessageBox(nppData._nppHandle, msg, TEXT("TidyText"), MB_OK | MB_ICONINFORMATION);
}

void clearNonPrintingHighlights()
{
    HWND sci = getCurrentScintilla();
    if (!sci)
        return;

    int indicator = getIndicator(sci);
    if (indicator < 0)
        return;

    Sci_Position len = (Sci_Position)::SendMessage(sci, SCI_GETLENGTH, 0, 0);
    ::SendMessage(sci, SCI_SETINDICATORCURRENT, (WPARAM)indicator, 0);
    ::SendMessage(sci, SCI_INDICATORCLEARRANGE, 0, (LPARAM)len);
}

void replaceNonPrintingChars()
{
    HWND sci = getCurrentScintilla();
    if (!sci)
        return;

    std::vector<CharMatch> matches = scanForMatches(sci);
    if (matches.empty())
    {
        ::MessageBox(nppData._nppHandle, TEXT("No non-printing or mis-encoded characters found."), TEXT("TidyText"), MB_OK | MB_ICONINFORMATION);
        return;
    }

    // Replace back-to-front so earlier byte offsets stay valid as replacement lengths differ.
    for (std::vector<CharMatch>::reverse_iterator it = matches.rbegin(); it != matches.rend(); ++it)
    {
        ::SendMessage(sci, SCI_SETTARGETRANGE, (WPARAM)it->startByte, (LPARAM)(it->startByte + it->byteLength));
        ::SendMessage(sci, SCI_REPLACETARGET, (WPARAM)it->replacement.size(), (LPARAM)it->replacement.c_str());
    }

    TCHAR msg[128];
    wsprintf(msg, TEXT("Replaced %d non-printing/mis-encoded character(s)."), (int)matches.size());
    ::MessageBox(nppData._nppHandle, msg, TEXT("TidyText"), MB_OK | MB_ICONINFORMATION);
}
