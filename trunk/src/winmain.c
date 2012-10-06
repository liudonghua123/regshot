/*
    Copyright 1999-2003,2007 TiANWEi
    Copyright 2004 tulipfan
    Copyright 2007 Belogorokhov Youri
    Copyright 2011-2012 Regshot Team

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
#include "version.h"

/*
    Define window title for main with version, revision, etc. (see version.rc.h for title structure)
*/
LPTSTR lpszProgramName = REGSHOT_TITLE;  // tfx  add program titile
LPTSTR lpszAboutRegshot = TEXT("Regshot is a free and open source registry compare utility.\n")
                          TEXT("Version:") REGSHOT_VERSION_NUM_DOTS2 REGSHOT_VERSION_NAME4 TEXT("\n")
                          TEXT("Revision: ") REGSHOT_REVISION_NUM2 REGSHOT_REVISION_NUM_SUFFIX2 TEXT("\n")
                          TEXT("Architecture: ") REGSHOT_VERSION_PLATFORM TEXT("\n")
                          TEXT("Codepage: ") REGSHOT_CODEPAGE TEXT("\n")
                          TEXT("Compiler: ") REGSHOT_VERSION_COMPILER TEXT("\n")
#ifdef REGSHOT_BUILDTYPE
                          TEXT("Build: ") REGSHOT_BUILDTYPE TEXT("\n")
#endif
                          TEXT("\n")
                          REGSHOT_URL TEXT("\n")
                          TEXT("\n")
                          REGSHOT_VERSION_COPYRIGHT TEXT("\n")
                          TEXT("\n");

LPTSTR lpszIniFileName      = TEXT("regshot.ini"); // tfx
LPTSTR lpszLanguageFileName = TEXT("language.ini");

REGSHOT Shot1;
REGSHOT Shot2;

LPTSTR lpszExtDir;
LPTSTR lpszOutputPath;
LPTSTR lpszLastSaveDir;
LPTSTR lpszLastOpenDir;
LPTSTR lpszWindowsDirName;
LPTSTR lpszTempPath;
LPTSTR lpszStartDir;
LPTSTR lpszLanguageIni;  // For language.ini
LPTSTR lpszCurrentTranslator;
LPTSTR lpszRegshotIni;

MSG        msg;          // Windows MSG struct
HWND       hWnd;         // The handle of REGSHOT
HMENU      hMenu;        // The handles of shortcut menus
HMENU      hMenuClear;   // The handles of shortcut menus
BOOL       is1;          // Flag to determine is the 1st shot
RECT       rect;         // Window RECT
BROWSEINFO BrowseInfo1;  // BrowseINFO struct

#ifdef USEHEAPALLOC_DANGER
HANDLE hHeap;  // 1.8.2
#endif


// this new function added by Youri in 1.8.2, for expanding path in browse dialog
int CALLBACK SelectBrowseFolder(HWND hWnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
{
    UNREFERENCED_PARAMETER(lParam);
    if (BFFM_INITIALIZED == uMsg) {
        SendMessage(hWnd, BFFM_SETSELECTION, 1, lpData);
    }
    return 0;
}


//--------------------------------------------------
// Main Dialog Proc
//--------------------------------------------------
BOOL CALLBACK DialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    size_t  nLen;
    //BYTE    nFlag;

    UNREFERENCED_PARAMETER(lParam);

    switch (message) {
        case WM_INITDIALOG:  // Window creation (="program start")
            SendDlgItemMessage(hDlg, IDC_EDITCOMMENT, EM_SETLIMITTEXT, (WPARAM)(COMMENTLENGTH - 1), (LPARAM)0);
            SendDlgItemMessage(hDlg, IDC_EDITPATH, EM_SETLIMITTEXT, (WPARAM)(MAX_PATH - 1), (LPARAM)0);
            SendDlgItemMessage(hDlg, IDC_EDITDIR, EM_SETLIMITTEXT, (WPARAM)(EXTDIRLEN - 1), (LPARAM)0);

            //enlarge some buffer in 201201
            lpszLanguage       = NULL;
            lpszExtDir         = MYALLOC0(EXTDIRLEN * sizeof(TCHAR));      // EXTDIRLEN is actually MAX_PATH * 4
            lpszLanguageIni    = MYALLOC0((MAX_PATH + 1 + _tcslen(lpszLanguageFileName)) * sizeof(TCHAR));   // for language.ini
            lpszRegshotIni     = MYALLOC0((MAX_PATH + 1 + _tcslen(lpszIniFileName)) * sizeof(TCHAR));   // for regshot.ini
            lpszMessage        = MYALLOC0(REGSHOT_MESSAGE_LENGTH * sizeof(TCHAR));  // For status bar text message store
            lpszWindowsDirName = MYALLOC0(MAX_PATH * sizeof(TCHAR));
            lpszTempPath       = MYALLOC0(MAX_PATH * sizeof(TCHAR));
            lpszStartDir       = MYALLOC0(MAX_PATH * sizeof(TCHAR));
            lpszOutputPath     = MYALLOC0(MAX_PATH * sizeof(TCHAR));  // store last save/open hive file dir, +1 for possible change in CompareShots()
            lpgrszLangSection  = NULL;

            ZeroMemory(&Shot1, sizeof(Shot1));
            ZeroMemory(&Shot2, sizeof(Shot2));

            GetWindowsDirectory(lpszWindowsDirName, MAX_PATH);  // length incl. NULL character
            lpszWindowsDirName[MAX_PATH] = (TCHAR)'\0';

            GetTempPath(MAX_PATH, lpszTempPath);  // length incl. NULL character
            lpszTempPath[MAX_PATH] = (TCHAR)'\0';

            //_asm int 3;
            GetCurrentDirectory(MAX_PATH, lpszStartDir);  // length incl. NULL character // fixed in 1.8.2 former version used getcommandline()
            lpszStartDir[MAX_PATH] = (TCHAR)'\0';

            _tcscpy(lpszLanguageIni, lpszStartDir);
            nLen = _tcslen(lpszLanguageIni);
            if (0 < nLen) {
                nLen--;
                if (lpszLanguageIni[nLen] != (TCHAR)'\\') {
                    _tcscat(lpszLanguageIni, TEXT("\\"));
                }
            }
            _tcscat(lpszLanguageIni, lpszLanguageFileName);

            _tcscpy(lpszRegshotIni, lpszStartDir);
            nLen = _tcslen(lpszRegshotIni);
            if (0 < nLen) {
                nLen--;
                if (lpszRegshotIni[nLen] != (TCHAR)'\\') {
                    _tcscat(lpszRegshotIni, TEXT("\\"));
                }
            }
            _tcscat(lpszRegshotIni, lpszIniFileName);

            LoadAvailableLanguagesFromIni(hDlg);
            LoadLanguageFromIni(hDlg);
            SetTextsToDefaultLanguage();
            SetTextsToSelectedLanguage(hDlg);

            SendMessage(hDlg, WM_COMMAND, (WPARAM)IDC_CHECKDIR, (LPARAM)0);

            lpszLastSaveDir = lpszOutputPath;
            lpszLastOpenDir = lpszOutputPath;

            LoadSettingsFromIni(hDlg); // tfx

            return TRUE;

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_1STSHOT:  // Button: 1st Shot
                    is1 = TRUE;  // Popup window messages are for 1st Shot
                    CreateShotPopupMenu();
                    GetWindowRect(GetDlgItem(hDlg, IDC_1STSHOT), &rect);
                    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_LEFTBUTTON, rect.left + 10, rect.top + 10, 0, hDlg, NULL);
                    DestroyMenu(hMenu);
                    return(TRUE);

                case IDC_2NDSHOT:  // Button: 2nd Shot
                    is1 = FALSE;  // Popup window messages are for 2nd Shot
                    CreateShotPopupMenu();
                    GetWindowRect(GetDlgItem(hDlg, IDC_2NDSHOT), &rect);
                    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_LEFTBUTTON, rect.left + 10, rect.top + 10, 0, hDlg, NULL);
                    DestroyMenu(hMenu);
                    return(TRUE);

                case IDM_SHOTONLY:  // Shot Popup Menu: "Shot"
                    if (is1) {
                        Shot(&Shot1);
                    } else {
                        Shot(&Shot2);
                    }
                    return(TRUE);

                case IDM_SHOTSAVE:  // Shot Popup Menu: "Shot and Save..."
                    if (is1) {
                        Shot(&Shot1);
                        SaveHive(&Shot1);
                    } else {
                        Shot(&Shot2);
                        SaveHive(&Shot2);
                    }
                    return(TRUE);

                case IDM_LOAD:  // Shot Popup Menu: "Load..."
                    if (is1) {
                        LoadHive(&Shot1);
                    } else {
                        LoadHive(&Shot2);
                    }
                    //if (is1LoadFromHive || is2LoadFromHive)
                    //  SendMessage(GetDlgItem(hWnd, IDC_CHECKDIR), BM_SETCHECK, (WPARAM)0x00, (LPARAM)0);
                    return(TRUE);

                    /*case IDC_SAVEREG:
                        SaveRegistry(Shot1.lpHKLM, Shot1.lpHKU);
                        return(TRUE);*/

                case IDC_COMPARE:  // Button: Compare
                    EnableWindow(GetDlgItem(hDlg, IDC_COMPARE), FALSE);
                    UI_BeforeClear();
                    CompareShots(&Shot1, &Shot2);
                    ShowWindow(GetDlgItem(hDlg, IDC_PBCOMPARE), SW_HIDE);
                    EnableWindow(GetDlgItem(hDlg, IDC_CLEAR1), TRUE);
                    SetFocus(GetDlgItem(hDlg, IDC_CLEAR1));
                    SendMessage(hDlg, DM_SETDEFID, (WPARAM)IDC_CLEAR1, (LPARAM)0);
                    SetCursor(hSaveCursor);
                    MessageBeep(0xffffffff);
                    return(TRUE);

                case IDC_CLEAR1:  // Button: Clear
                    hMenuClear = CreatePopupMenu();
                    AppendMenu(hMenuClear, MF_STRING, IDM_CLEARALLSHOTS, asLangTexts[iszTextMenuClearAllShots].lpszText);
                    AppendMenu(hMenuClear, MF_MENUBARBREAK, IDM_BREAK, NULL);
                    AppendMenu(hMenuClear, MF_STRING, IDM_CLEARSHOT1, asLangTexts[iszTextMenuClearShot1].lpszText);
                    AppendMenu(hMenuClear, MF_STRING, IDM_CLEARSHOT2, asLangTexts[iszTextMenuClearShot2].lpszText);
                    //AppendMenu(hMenuClear, MF_STRING, IDM_CLEARRESULT, TEXT("Clear comparison result"));
                    SetMenuDefaultItem(hMenuClear, IDM_CLEARALLSHOTS, FALSE);

                    //if (lpHeadFile != NULL)
                    //{
                    //  EnableMenuItem(hMenuClear, IDM_CLEARSHOT1, MF_BYCOMMAND|MF_GRAYED);
                    //  EnableMenuItem(hMenuClear, IDM_CLEARSHOT2, MF_BYCOMMAND|MF_GRAYED);
                    //}
                    //else
                    {
                        if (Shot1.lpHKLM != NULL) {
                            EnableMenuItem(hMenuClear, IDM_CLEARSHOT1, MF_BYCOMMAND | MF_ENABLED);
                        } else {
                            EnableMenuItem(hMenuClear, IDM_CLEARSHOT1, MF_BYCOMMAND | MF_GRAYED);
                        }

                        if (Shot2.lpHKLM != NULL) {
                            EnableMenuItem(hMenuClear, IDM_CLEARSHOT2, MF_BYCOMMAND | MF_ENABLED);
                        } else {
                            EnableMenuItem(hMenuClear, IDM_CLEARSHOT2, MF_BYCOMMAND | MF_GRAYED);
                        }
                    }
                    GetWindowRect(GetDlgItem(hDlg, IDC_CLEAR1), &rect);
                    TrackPopupMenu(hMenuClear, TPM_LEFTALIGN | TPM_LEFTBUTTON, rect.left + 10, rect.top + 10, 0, hDlg, NULL);
                    DestroyMenu(hMenuClear);
                    return(TRUE);

                case IDM_CLEARALLSHOTS:
                    UI_BeforeClear();
                    FreeShot(&Shot1);
                    FreeShot(&Shot2);
                    FreeAllCompareResults();
                    UI_AfterClear();
                    EnableWindow(GetDlgItem(hWnd, IDC_CLEAR1), FALSE);
                    return(TRUE);

                case IDM_CLEARSHOT1:
                    UI_BeforeClear();
                    FreeShot(&Shot1);
                    FreeAllCompareResults();
                    ClearKeyMatchTag(Shot2.lpHKLM);  // we clear Shot2's tag
                    ClearKeyMatchTag(Shot2.lpHKU);
                    ClearHeadFileMatchTag(Shot2.lpHF);
                    UI_AfterClear();
                    return(TRUE);

                case IDM_CLEARSHOT2:
                    UI_BeforeClear();
                    FreeShot(&Shot2);
                    FreeAllCompareResults();
                    ClearKeyMatchTag(Shot1.lpHKLM);  // we clear Shot1's tag
                    ClearKeyMatchTag(Shot1.lpHKU);
                    ClearHeadFileMatchTag(Shot1.lpHF);
                    UI_AfterClear();
                    return(TRUE);

                    /*case IDM_CLEARRESULT:
                        UI_BeforeClear();
                        FreeAllCompareResults();
                        ClearKeyMatchTag(Shot1.lpHKLM);
                        ClearKeyMatchTag(Shot2.lpHKLM);
                        ClearKeyMatchTag(Shot1.lpHKU);
                        ClearKeyMatchTag(Shot2.lpHKU);
                        ClearHeadFileMatchTag(Shot1.lpHF);
                        ClearHeadFileMatchTag(Shot2.lpHF);
                        UI_AfterClear();
                        return(TRUE);*/

                case IDC_CHECKDIR:
                    if (SendMessage(GetDlgItem(hDlg, IDC_CHECKDIR), BM_GETCHECK, (WPARAM)0, (LPARAM)0) == 1) {
                        EnableWindow(GetDlgItem(hDlg, IDC_EDITDIR), TRUE);
                        EnableWindow(GetDlgItem(hDlg, IDC_BROWSE1), TRUE);
                    } else {
                        EnableWindow(GetDlgItem(hDlg, IDC_EDITDIR), FALSE);
                        EnableWindow(GetDlgItem(hDlg, IDC_BROWSE1), FALSE);
                    }
                    return(TRUE);

                case IDC_CANCEL1:  // Button: Quit
                case IDCANCEL:  // Button: Window Close
                    SaveSettingsToIni(hDlg);  // tfx
                    PostQuitMessage(0);
                    return(TRUE);

                case IDC_BROWSE1: {  // Button: Scan dirs
                    LPITEMIDLIST lpidlist;
                    size_t nLenExtDir;

                    BrowseInfo1.hwndOwner = hDlg;
                    BrowseInfo1.pszDisplayName = MYALLOC0(MAX_PATH * sizeof(TCHAR));
                    //BrowseInfo1.lpszTitle = TEXT("Select:");
                    BrowseInfo1.ulFlags = 0;     // 3 lines added in 1.8.2
                    BrowseInfo1.lpfn = NULL;
                    BrowseInfo1.lParam = 0;

                    // remove trailing semicolons
                    nLenExtDir = GetDlgItemText(hDlg, IDC_EDITDIR, lpszExtDir, EXTDIRLEN);  // length incl. NULL character
                    while ((0 < nLenExtDir) && ((TCHAR)';' == lpszExtDir[nLenExtDir - 1])) {
                        nLenExtDir--;
                    }
                    lpszExtDir[nLenExtDir] = (TCHAR)'\0';

                    lpidlist = SHBrowseForFolder(&BrowseInfo1);
                    if (NULL != lpidlist) {
                        SHGetPathFromIDList(lpidlist, BrowseInfo1.pszDisplayName);
                        nLen = _tcslen(BrowseInfo1.pszDisplayName);
                        if (0 < nLen) {
                            size_t nWholeLen;
                            BOOL fSemicolon;

                            fSemicolon = FALSE;
                            nWholeLen = nLenExtDir + nLen;
                            if (0 < nLenExtDir) {
                                fSemicolon = TRUE;
                                nWholeLen++;
                            }

                            if (EXTDIRLEN > nWholeLen) {
                                if (fSemicolon) {
                                    _tcscat(lpszExtDir, TEXT(";"));
                                }
                                _tcscat(lpszExtDir, BrowseInfo1.pszDisplayName);
                            }
                        }
                        MYFREE(lpidlist);
                    }
                    MYFREE(BrowseInfo1.pszDisplayName);

                    SetDlgItemText(hDlg, IDC_EDITDIR, lpszExtDir);
                }
                return(TRUE);

                case IDC_BROWSE2: {  // Button: Output path
                    LPITEMIDLIST lpidlist;

                    BrowseInfo1.hwndOwner = hDlg;
                    BrowseInfo1.pszDisplayName = MYALLOC0(MAX_PATH * sizeof(TCHAR));
                    //BrowseInfo1.lpszTitle = TEXT("Select:");
                    //-----------------
                    // Added by Youri in 1.8.2 ,Thanks!
                    // if you add this code, the browse dialog will be expand path and have button "Create Folder"
                    BrowseInfo1.ulFlags |= 0x0040; // BIF_NEWDIALOGSTYLE;    // button "Create Folder" and resizable
                    BrowseInfo1.lpfn = SelectBrowseFolder;                   // function for expand path
                    BrowseInfo1.lParam = (LPARAM)BrowseInfo1.pszDisplayName;

                    // Initialize selection path
                    GetDlgItemText(hDlg, IDC_EDITPATH, BrowseInfo1.pszDisplayName, MAX_PATH);  // length incl. NULL character
                    //-----------------

                    lpidlist = SHBrowseForFolder(&BrowseInfo1);
                    if (NULL != lpidlist) {
                        SHGetPathFromIDList(lpidlist, BrowseInfo1.pszDisplayName);
                        SetDlgItemText(hDlg, IDC_EDITPATH, BrowseInfo1.pszDisplayName);
                        MYFREE(lpidlist);
                    }

                    MYFREE(BrowseInfo1.pszDisplayName);
                }
                return(TRUE);

                case IDC_COMBOLANGUAGE:  // Combo Box: Language
                    if (CBN_SELCHANGE == HIWORD(wParam)) {  // Only react when user selected something
                        SetTextsToDefaultLanguage();
                        SetTextsToSelectedLanguage(hDlg);
                        return(TRUE);
                    }
                    break;

                case IDC_ABOUT: {  // Button: About
                    LPTSTR   lpszAboutBox;
                    //_asm int 3;
                    lpszAboutBox = MYALLOC0(SIZEOF_ABOUTBOX * sizeof(TCHAR));
                    // it is silly that when wsprintf encounters a NULL string, it will write the whole string to NULL!
                    _sntprintf(lpszAboutBox, SIZEOF_ABOUTBOX, TEXT("%s%s%s%s%s%s\0"), lpszAboutRegshot, TEXT("["), lpszLanguage, TEXT("]"), TEXT(" by: "), lpszCurrentTranslator);
                    lpszAboutBox[SIZEOF_ABOUTBOX - 1] = (TCHAR)'\0'; // safety NULL char
                    MessageBox(hDlg, lpszAboutBox, asLangTexts[iszTextAbout].lpszText, MB_OK);
                    MYFREE(lpszAboutBox);
                    return(TRUE);
                }
            }
    }
    return(FALSE);
}


/*BOOL SetPrivilege(HANDLE hToken, LPCSTR pString, BOOL bEnablePrivilege)
{
    TOKEN_PRIVILEGES    tp;
    LUID    luid;
    TOKEN_PRIVILEGES    tpPrevious;
    DWORD   cbSize = sizeof(TOKEN_PRIVILEGES);

    if (!LookupPrivilegeValue(NULL,pString,&luid))
        return FALSE;
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = 0;
    if (!AdjustTokenPrivileges(hToken,FALSE,&tp,sizeof(TOKEN_PRIVILEGES),&tpPrevious,&cbSize))
        return FALSE;
    tpPrevious.PrivilegeCount = 1;
    tpPrevious.Privileges[0].Luid = luid;
    if (bEnablePrivilege)
        tpPrevious.Privileges[0].Attributes| = (SE_PRIVILEGE_ENABLED);
    else
        tpPrevious.Privileges[0].Attributes^ = ((tpPrevious.Privileges[0].Attributes)&(SE_PRIVILEGE_ENABLED));
    if (!AdjustTokenPrivileges(hToken,FALSE,&tpPrevious,cbSize,NULL,NULL))
        return FALSE;
    return TRUE;
}*/


int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                     LPTSTR lpszCmdLine, int nCmdShow)
{

    /*
    BOOL    bWinNTDetected;
    HANDLE  hToken = 0;
    OSVERSIONINFO winver;
    winver.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    GetVersionEx(&winver);
    bWinNTDetected = (winver.dwPlatformId == VER_PLATFORM_WIN32_NT) ? TRUE : FALSE;
    //hWndMonitor be created first for the multilanguage interface.

    //FARPROC       lpfnDlgProc;
    //lpfnDlgProc = MakeProcInstance((FARPROC)DialogProc,hInstance);    // old style of create dialogproc
    */
    UNREFERENCED_PARAMETER(lpszCmdLine);
    UNREFERENCED_PARAMETER(hPrevInstance);

#ifdef USEHEAPALLOC_DANGER
    hHeap = GetProcessHeap(); // 1.8.2
#endif

    hWnd = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, (DLGPROC)DialogProc);

    SetClassLongPtr(hWnd, GCLP_HICON, (LONG_PTR)LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MAINICON)));

    SetWindowText(hWnd, lpszProgramName);  // tfx set program title to lpszProgramName to avoid edit resource file
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);
    //SetPriorityClass(hInstance,31);
    /*if (bWinNTDetected)
      {
          if (OpenProcessToken(GetCurrentProcess(),TOKEN_ADJUST_PRIVILEGES|TOKEN_QUERY,&hToken) == TRUE)
          {
              if (SetPrivilege(hToken,"SeSystemProfilePrivilege",TRUE) == TRUE)
              {
                  MessageBox(hWnd,"We are in system level,enjoy!","Info:",MB_OK);
              }
              CloseHandle(hToken);
          }
      }*/
    while (GetMessage(&msg, NULL, (WPARAM)NULL, (LPARAM)NULL)) {
        if (!IsDialogMessage(hWnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    return(int)(msg.wParam);
}
