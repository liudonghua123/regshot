/*
    Copyright 2011-2013 Regshot Team
    Copyright 1999-2003,2007 TiANWEi
    Copyright 2004 tulipfan

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

LPTSTR lpszMessage;

HCURSOR hHourGlass = NULL;   // Handle of cursor
HCURSOR hSaveCursor = NULL;  // Handle of cursor

// Some vars used to update the "status bar"
DWORD nCurrentTime;
DWORD nStartTime;
DWORD nLastTime;

// Some vars used to update the progress bar
DWORD cCurrent;
DWORD cEnd;


// ----------------------------------------------------------------------
// Displays/Hides counters in the "status bar" of the window
// ----------------------------------------------------------------------
VOID UI_ShowHideCounters(int nCmdShow)  // 1.8.2
{
    ShowWindow(GetDlgItem(hWnd, IDC_TEXTCOUNT1), nCmdShow);
    ShowWindow(GetDlgItem(hWnd, IDC_TEXTCOUNT2), nCmdShow);
    ShowWindow(GetDlgItem(hWnd, IDC_TEXTCOUNT3), nCmdShow);
    UpdateWindow(hWnd);
}


// ----------------------------------------------------------------------
// Displays/Hides progress bar in the "status bar" of the window
// ----------------------------------------------------------------------
VOID UI_ShowHideProgressBar(int nCmdShow)
{
    ShowWindow(GetDlgItem(hWnd, IDC_PROGBAR), nCmdShow);
    UpdateWindow(hWnd);
}


// ----------------------------------------------------------------------
// Resets and displays counters in the "status bar" of the window
// ----------------------------------------------------------------------
VOID UI_InitCounters(VOID)
{
    nCurrentTime = 0;
    nStartTime = GetTickCount();
    nLastTime = nStartTime;

    UI_ShowHideProgressBar(SW_HIDE);
    _tcscpy(lpszMessage, TEXT(" "));  // clear the counters
    SendDlgItemMessage(hWnd, IDC_TEXTCOUNT1, WM_SETTEXT, (WPARAM)0, (LPARAM)lpszMessage);
    SendDlgItemMessage(hWnd, IDC_TEXTCOUNT2, WM_SETTEXT, (WPARAM)0, (LPARAM)lpszMessage);
    SendDlgItemMessage(hWnd, IDC_TEXTCOUNT3, WM_SETTEXT, (WPARAM)0, (LPARAM)lpszMessage);
    UI_ShowHideCounters(SW_SHOW);
}


// ----------------------------------------------------------------------
// Resets and displays progress bar in the "status bar" of the window
// ----------------------------------------------------------------------
VOID UI_InitProgressBar(VOID)
{
    cCurrent = 0;
    nCurrentTime = 0;
    nStartTime = GetTickCount();
    nLastTime = nStartTime;

    UI_ShowHideCounters(SW_HIDE);  // 1.8.2
    SendDlgItemMessage(hWnd, IDC_PROGBAR, PBM_SETRANGE32, (WPARAM)0, (LPARAM)MAXPBPOSITION);
    SendDlgItemMessage(hWnd, IDC_PROGBAR, PBM_SETPOS, (WPARAM)0, (LPARAM)0);
    UI_ShowHideProgressBar(SW_SHOW);
}


// ----------------------------------------------------------------------
// Update counters in the "status bar" of the window
// ----------------------------------------------------------------------
VOID UI_UpdateCounters(LPTSTR lpszTitle1, LPTSTR lpszTitle2, DWORD nCount1, DWORD nCount2)
{
    // Remember current time for next update interval
    nLastTime = nCurrentTime;

    // Update "status bar"
    _sntprintf(lpszMessage, REGSHOT_MESSAGE_LENGTH, TEXT("%s %us%03ums\0"), asLangTexts[iszTextTime].lpszText, (nCurrentTime - nStartTime) / 1000, (nCurrentTime - nStartTime) % 1000);
    lpszMessage[REGSHOT_MESSAGE_LENGTH - 1] = (TCHAR)'\0';  // safety NULL char
    SendDlgItemMessage(hWnd, IDC_TEXTCOUNT3, WM_SETTEXT, (WPARAM)0, (LPARAM)lpszMessage);

    _sntprintf(lpszMessage, REGSHOT_MESSAGE_LENGTH, TEXT("%s %u\0"), lpszTitle1, nCount1);
    lpszMessage[REGSHOT_MESSAGE_LENGTH - 1] = (TCHAR)'\0';  // safety NULL char
    SendDlgItemMessage(hWnd, IDC_TEXTCOUNT1, WM_SETTEXT, (WPARAM)0, (LPARAM)lpszMessage);

    _sntprintf(lpszMessage, REGSHOT_MESSAGE_LENGTH, TEXT("%s %u\0"), lpszTitle2, nCount2);
    lpszMessage[REGSHOT_MESSAGE_LENGTH - 1] = (TCHAR)'\0';  // safety NULL char
    SendDlgItemMessage(hWnd, IDC_TEXTCOUNT2, WM_SETTEXT, (WPARAM)0, (LPARAM)lpszMessage);

    // Refresh window display
    UpdateWindow(hWnd);
    PeekMessage(&msg, hWnd, WM_ACTIVATE, WM_ACTIVATE, PM_REMOVE);
}


// ----------------------------------------------------------------------
// Update progress bar in the "status bar" of the window
// ----------------------------------------------------------------------
VOID UI_UpdateProgressBar(VOID)
{
    DWORD nPBPos;

    // Remember current time for next update interval
    nLastTime = nCurrentTime;

    // Update "status bar"
    if (0 != cEnd) {
        nPBPos = (DWORD)(cCurrent * (__int64)MAXPBPOSITION / cEnd);
        SendDlgItemMessage(hWnd, IDC_PROGBAR, PBM_SETPOS, (WPARAM)nPBPos, (LPARAM)0);

        // Refresh window display
        UpdateWindow(hWnd);
        PeekMessage(&msg, hWnd, WM_ACTIVATE, WM_ACTIVATE, PM_REMOVE);
    }
}


// ----------------------------------------------------------------------
// Prepare the GUI for the shot about to be taken
// ----------------------------------------------------------------------
VOID UI_BeforeShot(DWORD nID)
{
    hHourGlass = LoadCursor(NULL, IDC_WAIT);
    hSaveCursor = SetCursor(hHourGlass);
    EnableWindow(GetDlgItem(hWnd, nID), FALSE);
}


//--------------------------------------------------
// Reset the GUI after the shot has been taken
//--------------------------------------------------
VOID UI_AfterShot(VOID)
{
    DWORD nIDDefault;

    if (!Shot1.fFilled) {
        nIDDefault = IDC_1STSHOT;
    } else if (!Shot2.fFilled) {
        nIDDefault = IDC_2NDSHOT;
    } else {
        nIDDefault = IDC_COMPARE;
    }
    EnableWindow(GetDlgItem(hWnd, IDC_CLEAR1), TRUE);
    EnableWindow(GetDlgItem(hWnd, nIDDefault), TRUE);
    SendMessage(hWnd, DM_SETDEFID, (WPARAM)nIDDefault, (LPARAM)0);
    SetFocus(GetDlgItem(hWnd, nIDDefault));
    SetCursor(hSaveCursor);
    MessageBeep(0xffffffff);
}


//--------------------------------------------------
// Prepare the GUI for clearing
//--------------------------------------------------
VOID UI_BeforeClear(VOID)
{
    //EnableWindow(GetDlgItem(hWnd,IDC_CLEAR1),FALSE);
    hHourGlass = LoadCursor(NULL, IDC_WAIT);
    hSaveCursor = SetCursor(hHourGlass);
    UI_ShowHideCounters(SW_HIDE);
    UpdateWindow(hWnd);
}


//--------------------------------------------------
// Reset the GUI after the clearing
//--------------------------------------------------
VOID UI_AfterClear(VOID)
{
    DWORD nIDDefault;
    //BOOL fChk;  // used for file scan disable

    nIDDefault = 0;
    if (!Shot1.fFilled) {
        nIDDefault = IDC_1STSHOT;
    } else if (!Shot2.fFilled) {
        nIDDefault = IDC_2NDSHOT;
    }
    EnableWindow(GetDlgItem(hWnd, nIDDefault), TRUE);
    EnableWindow(GetDlgItem(hWnd, IDC_COMPARE), FALSE);

    //fChk = FALSE;
    if ((!Shot1.fFilled) && (!Shot2.fFilled)) {
        EnableWindow(GetDlgItem(hWnd, IDC_2NDSHOT), FALSE);
        EnableWindow(GetDlgItem(hWnd, IDC_CLEAR1), FALSE);
        //fChk = TRUE;
    }

    //EnableWindow(GetDlgItem(hWnd, IDC_CHECKDIR), fChk);  // Not used in 1.8; we only enable fChk when clear all
    //SendMessage(hWnd, WM_COMMAND, (WPARAM)IDC_CHECKDIR, (LPARAM)0);

    SetFocus(GetDlgItem(hWnd, nIDDefault));
    SendMessage(hWnd, DM_SETDEFID, (WPARAM)nIDDefault, (LPARAM)0);
    SetCursor(hSaveCursor);
    MessageBeep(0xffffffff);
}


//--------------------------------------------------
// Show popup menu for Shot buttons
//--------------------------------------------------
VOID UI_CreateShotPopupMenu(VOID)
{
    hMenu = CreatePopupMenu();

    AppendMenu(hMenu, MF_STRING, IDM_SHOTONLY, asLangTexts[iszTextMenuShot].lpszText);
    AppendMenu(hMenu, MF_STRING, IDM_SHOTSAVE, asLangTexts[iszTextMenuShotSave].lpszText);
    AppendMenu(hMenu, MF_SEPARATOR, IDM_BREAK, NULL);
    AppendMenu(hMenu, MF_STRING, IDM_LOAD, asLangTexts[iszTextMenuLoad].lpszText);

    SetMenuDefaultItem(hMenu, IDM_SHOTONLY, FALSE);
}


//--------------------------------------------------
// Show popup menu for Clear button
//--------------------------------------------------
VOID UI_CreateClearPopupMenu(VOID)
{
    hMenu = CreatePopupMenu();

    AppendMenu(hMenu, MF_STRING, IDM_CLEARALL, asLangTexts[iszTextMenuClearAll].lpszText);
    AppendMenu(hMenu, MF_MENUBARBREAK, IDM_BREAK, NULL);
    AppendMenu(hMenu, MF_STRING, IDM_CLEARSHOT1, asLangTexts[iszTextMenuClearShot1].lpszText);
    AppendMenu(hMenu, MF_STRING, IDM_CLEARSHOT2, asLangTexts[iszTextMenuClearShot2].lpszText);
    //AppendMenu(hMenu, MF_STRING, IDM_CLEARRESULT, TEXT("Clear comparison result"));

    if (!Shot1.fFilled) {
        EnableMenuItem(hMenu, IDM_CLEARSHOT1, MF_BYCOMMAND | MF_GRAYED);
    }
    if (!Shot2.fFilled) {
        EnableMenuItem(hMenu, IDM_CLEARSHOT2, MF_BYCOMMAND | MF_GRAYED);
    }
    /*
    if (!Comparison.fFilled) {
        EnableMenuItem(hMenu, IDM_CLEARRESULT, MF_BYCOMMAND | MF_GRAYED);
    }
    */

    SetMenuDefaultItem(hMenu, IDM_CLEARALL, FALSE);
}
