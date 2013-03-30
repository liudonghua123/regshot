/*
    Copyright 2011-2013 Regshot Team
    Copyright 1999-2003,2007,2011 TiANWEi
    Copyright 2007 Belogorokhov Youri

    This file is part of Regshot.

    Regshot is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 2.1 of the License, or
    (at your option) any later version.

    Regshot is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with Regshot.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "global.h"

// Some strings used to write to HTML or TEXT file
LPTSTR lpszCRLF             = TEXT("\r\n");  // {0x0d,0x0a,0x00};
LPTSTR lpszTextLine         = TEXT("\r\n----------------------------------\r\n");
LPTSTR lpszHTML_BR          = TEXT("<BR>\r\n");
LPTSTR lpszHTMLBegin        = TEXT("<HTML>\r\n");
LPTSTR lpszHTMLEnd          = TEXT("</HTML>\r\n");
LPTSTR lpszHTMLHeadBegin    = TEXT("<HEAD>\r\n");
LPTSTR lpszHTML_CType       =
#ifdef _UNICODE
    TEXT("<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-16\">\r\n");
#else
    TEXT("<meta http-equiv=\"Content-Type\" content=\"text/html; charset=ISO-8859-1\">\r\n");
#endif
LPTSTR lpszHTMLHeadEnd      = TEXT("</HEAD>\r\n");
LPTSTR lpszHTMLTd1Begin     = TEXT("<TR><TD BGCOLOR=\"#669999\" ALIGN=\"LEFT\"><FONT COLOR=\"WHITE\"><B>");
LPTSTR lpszHTMLTd1End       = TEXT("</B></FONT></TD></TR>\r\n");
LPTSTR lpszHTMLTd2Begin     = TEXT("<TR><TD NOWRAP><FONT COLOR=\"BLACK\">");
LPTSTR lpszHTMLTd2End       = TEXT("</FONT></TD></TR>\r\n");
// color idea got from HANDLE(Youri) at wgapatcher.ru :) 1.8
LPTSTR lpszHTML_CSS         = TEXT("<STYLE TYPE = \"text/css\">td{font-family:\"Tahoma\";font-size:9pt}\
tr{font-size:9pt}body{font-size:9pt}\
.o{background:#E0F0E0}.n{background:#FFFFFF}</STYLE>\r\n");  // 1.8.2 from e0e0e0 to e0f0e0 by Charles
LPTSTR lpszHTMLBodyBegin    = TEXT("<BODY BGCOLOR=\"#FFFFFF\" TEXT=\"#000000\" LINK=\"#C8C8C8\">\r\n");
LPTSTR lpszHTMLBodyEnd      = TEXT("</BODY>\r\n");
LPTSTR lpszHTMLTableBegin   = TEXT("<TABLE BORDER=\"0\" WIDTH=\"480\">\r\n");
LPTSTR lpszHTMLTableEnd     = TEXT("</TABLE>\r\n");
LPTSTR lpszHTMLSpan1        = TEXT("<SPAN CLASS=\"o\">");
LPTSTR lpszHTMLSpan2        = TEXT("<SPAN CLASS=\"n\">");
LPTSTR lpszHTMLSpanEnd      = TEXT("</SPAN>");
LPTSTR lpszHTMLWebSiteBegin = TEXT("<FONT COLOR=\"#888888\">Created with <A HREF=\"http://sourceforge.net/projects/regshot/\">");
LPTSTR lpszHTMLWebSiteEnd   = TEXT("</A></FONT><BR>\r\n");

HANDLE hFile;  // Handle of file regshot use


// ----------------------------------------------------------------------
// Several routines to write to an output file
// ----------------------------------------------------------------------

// ----------------------------------------------------------------------
VOID WriteTableHead(LPTSTR lpszText, DWORD nCount, BOOL fAsHTML)
{
    TCHAR szCount[17];

    _sntprintf(szCount, 17, TEXT("%u\0"), nCount);
    szCount[16] = (TCHAR)'\0';  // saftey NULL char

    if (fAsHTML) {
        WriteFile(hFile, lpszHTML_BR, (DWORD)(_tcslen(lpszHTML_BR) * sizeof(TCHAR)), &NBW, NULL);
        WriteFile(hFile, lpszHTMLTableBegin, (DWORD)(_tcslen(lpszHTMLTableBegin) * sizeof(TCHAR)), &NBW, NULL);
        WriteFile(hFile, lpszHTMLTd1Begin, (DWORD)(_tcslen(lpszHTMLTd1Begin) * sizeof(TCHAR)), &NBW, NULL);
    } else {
        WriteFile(hFile, lpszTextLine, (DWORD)(_tcslen(lpszTextLine) * sizeof(TCHAR)), &NBW, NULL);
    }

    WriteFile(hFile, lpszText, (DWORD)(_tcslen(lpszText) * sizeof(TCHAR)), &NBW, NULL);
    WriteFile(hFile, TEXT(" "), (DWORD)(1 * sizeof(TCHAR)), &NBW, NULL);
    WriteFile(hFile, szCount, (DWORD)(_tcslen(szCount) * sizeof(TCHAR)), &NBW, NULL);

    if (fAsHTML) {
        WriteFile(hFile, lpszHTMLTd1End, (DWORD)(_tcslen(lpszHTMLTd1End) * sizeof(TCHAR)), &NBW, NULL);
        WriteFile(hFile, lpszHTMLTableEnd, (DWORD)(_tcslen(lpszHTMLTableEnd) * sizeof(TCHAR)), &NBW, NULL);
    } else {
        WriteFile(hFile, lpszTextLine, (DWORD)(_tcslen(lpszTextLine) * sizeof(TCHAR)), &NBW, NULL);
    }
}

// ----------------------------------------------------------------------
VOID WritePart(DWORD nActionType, LPCOMPRESULT lpStartCR, BOOL fAsHTML, BOOL fUseColor)
{
    size_t nCharsToWrite;
    size_t nCharsToGo;
    LPTSTR lpszResult;
    LPTSTR lpszResultTemp;
    LPCOMPRESULT lpCR;
    BOOL fColor;  // color flip-flop flag
    DWORD cbHTMLSpan1;
    DWORD cbHTMLSpan2;
    DWORD cbHTMLSpanEnd;
    DWORD cbHTML_BR;
    DWORD cbCRLF;
    int i;

    cbHTMLSpan1 = 0;
    cbHTMLSpan2 = 0;
    cbHTMLSpanEnd = 0;
    cbHTML_BR = 0;
    cbCRLF = 0;

    if (fAsHTML) {
        WriteFile(hFile, lpszHTMLTableBegin, (DWORD)(_tcslen(lpszHTMLTableBegin) * sizeof(TCHAR)), &NBW, NULL);
        WriteFile(hFile, lpszHTMLTd2Begin, (DWORD)(_tcslen(lpszHTMLTd2Begin) * sizeof(TCHAR)), &NBW, NULL);
        if (fUseColor) {
            cbHTMLSpan1 = (DWORD)(_tcslen(lpszHTMLSpan1) * sizeof(TCHAR));
            cbHTMLSpan2 = (DWORD)(_tcslen(lpszHTMLSpan2) * sizeof(TCHAR));
            cbHTMLSpanEnd = (DWORD)(_tcslen(lpszHTMLSpanEnd) * sizeof(TCHAR));
        }
        cbHTML_BR = (DWORD)(_tcslen(lpszHTML_BR) * sizeof(TCHAR));
    } else {
        cbCRLF = (DWORD)(_tcslen(lpszCRLF) * sizeof(TCHAR));
    }

    fColor = FALSE;
    for (lpCR = lpStartCR; NULL != lpCR; lpCR = lpCR->lpNextCR) {
        for (i = 0; i < 2; i++) {
            lpszResult = NULL;
            if (0 == i) {
                if (NULL == lpCR->lpContentOld) {
                    continue;
                }
                lpszResult = ResultToString(nActionType, lpCR->lpContentOld);
            }
            if (1 == i) {
                if (NULL == lpCR->lpContentNew) {
                    continue;
                }
                lpszResult = ResultToString(nActionType, lpCR->lpContentNew);
            }
            if (NULL == lpszResult) {
                continue;
            }

            if (fAsHTML) {
                // 1.8.0: zebra/flip-flop colors
                if (fUseColor) {
                    if (!fColor) {
                        WriteFile(hFile, lpszHTMLSpan1, cbHTMLSpan1, &NBW, NULL);
                    } else {
                        WriteFile(hFile, lpszHTMLSpan2, cbHTMLSpan2, &NBW, NULL);
                    }
                }
            }

            lpszResultTemp = lpszResult;
            for (nCharsToGo = _tcslen(lpszResult); 0 < nCharsToGo;) {
                nCharsToWrite = nCharsToGo;
                if (HTMLWRAPLENGTH < nCharsToWrite) {
                    nCharsToWrite = HTMLWRAPLENGTH;
                }

                WriteFile(hFile, lpszResultTemp, (DWORD)(nCharsToWrite * sizeof(TCHAR)), &NBW, NULL);
                lpszResultTemp += nCharsToWrite;
                nCharsToGo -= nCharsToWrite;

                if (0 == nCharsToGo) {
                    break;  // skip newline
                }

                if (fAsHTML) {
                    WriteFile(hFile, lpszHTML_BR, cbHTML_BR, &NBW, NULL);
                } else {
                    WriteFile(hFile, lpszCRLF, cbCRLF, &NBW, NULL);
                }
            }
            MYFREE(lpszResult);

            if (fAsHTML) {
                if (fUseColor) {
                    WriteFile(hFile, lpszHTMLSpanEnd, cbHTMLSpanEnd, &NBW, NULL);
                }
                WriteFile(hFile, lpszHTML_BR, cbHTML_BR, &NBW, NULL);
            } else {
                WriteFile(hFile, lpszCRLF, cbCRLF, &NBW, NULL);
            }
        }

        // 1.8.0: zebra/flip-flop colors
        if (fUseColor) {
            fColor = !fColor;
        }

        // Increase count
        cCurrent++;

        // Update progress bar display
        if (0 != cEnd) {
            nCurrentTime = GetTickCount();
            if (REFRESHINTERVAL < (nCurrentTime - nLastTime)) {
                UI_UpdateProgressBar();
            }
        }
    }

    if (fAsHTML) {
        WriteFile(hFile, lpszHTMLTd2End, (DWORD)(_tcslen(lpszHTMLTd2End) * sizeof(TCHAR)), &NBW, NULL);
        WriteFile(hFile, lpszHTMLTableEnd, (DWORD)(_tcslen(lpszHTMLTableEnd) * sizeof(TCHAR)), &NBW, NULL);
    }
}

// ----------------------------------------------------------------------
VOID WriteTitle(LPTSTR lpszTitle, LPTSTR lpszValue, BOOL fAsHTML)
{
    if (fAsHTML) {
        WriteFile(hFile, lpszHTMLTableBegin, (DWORD)(_tcslen(lpszHTMLTableBegin) * sizeof(TCHAR)), &NBW, NULL);
        WriteFile(hFile, lpszHTMLTd1Begin, (DWORD)(_tcslen(lpszHTMLTd1Begin) * sizeof(TCHAR)), &NBW, NULL);
    }

    WriteFile(hFile, lpszTitle, (DWORD)(_tcslen(lpszTitle) * sizeof(TCHAR)), &NBW, NULL);
    WriteFile(hFile, TEXT(" "), (DWORD)(1 * sizeof(TCHAR)), &NBW, NULL);
    WriteFile(hFile, lpszValue, (DWORD)(_tcslen(lpszValue) * sizeof(TCHAR)), &NBW, NULL);

    if (fAsHTML) {
        WriteFile(hFile, lpszHTMLTd1End, (DWORD)(_tcslen(lpszHTMLTd1End) * sizeof(TCHAR)), &NBW, NULL);
        WriteFile(hFile, lpszHTMLTableEnd, (DWORD)(_tcslen(lpszHTMLTableEnd) * sizeof(TCHAR)), &NBW, NULL);
    } else {
        WriteFile(hFile, lpszCRLF, (DWORD)(_tcslen(lpszCRLF) * sizeof(TCHAR)), &NBW, NULL);
    }
}

// ----------------------------------------------------------------------
VOID WriteHTMLBegin(void)
{
    WriteFile(hFile, lpszHTMLBegin, (DWORD)(_tcslen(lpszHTMLBegin) * sizeof(TCHAR)), &NBW, NULL);
    WriteFile(hFile, lpszHTMLHeadBegin, (DWORD)(_tcslen(lpszHTMLHeadBegin) * sizeof(TCHAR)), &NBW, NULL);
    WriteFile(hFile, lpszHTML_CType, (DWORD)(_tcslen(lpszHTML_CType) * sizeof(TCHAR)), &NBW, NULL);
    WriteFile(hFile, lpszHTML_CSS, (DWORD)(_tcslen(lpszHTML_CSS) * sizeof(TCHAR)), &NBW, NULL);
    WriteFile(hFile, lpszHTMLHeadEnd, (DWORD)(_tcslen(lpszHTMLHeadEnd) * sizeof(TCHAR)), &NBW, NULL);
    WriteFile(hFile, lpszHTMLBodyBegin, (DWORD)(_tcslen(lpszHTMLBodyBegin) * sizeof(TCHAR)), &NBW, NULL);

    WriteFile(hFile, lpszHTMLWebSiteBegin, (DWORD)(_tcslen(lpszHTMLWebSiteBegin) * sizeof(TCHAR)), &NBW, NULL);
    WriteFile(hFile, lpszProgramName, (DWORD)(_tcslen(lpszProgramName) * sizeof(TCHAR)), &NBW, NULL);
    WriteFile(hFile, lpszHTMLWebSiteEnd, (DWORD)(_tcslen(lpszHTMLWebSiteEnd) * sizeof(TCHAR)), &NBW, NULL);
}

// ----------------------------------------------------------------------
VOID WriteHTMLEnd(void)
{
    WriteFile(hFile, lpszHTMLBodyEnd, (DWORD)(_tcslen(lpszHTMLBodyEnd) * sizeof(TCHAR)), &NBW, NULL);
    WriteFile(hFile, lpszHTMLEnd, (DWORD)(_tcslen(lpszHTMLEnd) * sizeof(TCHAR)), &NBW, NULL);
}

// ----------------------------------------------------------------------
VOID WriteHTML_BR(void)
{
    WriteFile(hFile, lpszHTML_BR, (DWORD)(_tcslen(lpszHTML_BR) * sizeof(TCHAR)), &NBW, NULL);
}
