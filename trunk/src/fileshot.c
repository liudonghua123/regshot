/*
    Copyright 1999-2003,2007,2011 TiANWEi
    Copyright 2004 tulipfan
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

// ISDIR, ISFILE added in 1.8.0
#define ISDIR(x)  ( (x&FILE_ATTRIBUTE_DIRECTORY) != 0 )
#define ISFILE(x) ( (x&FILE_ATTRIBUTE_DIRECTORY) == 0 )

SAVEFILECONTENT sFC;
SAVEHEADFILE sHF;

// Some DWORDs used to show the progress bar and etc
DWORD nGettingFile;
DWORD nGettingDir;
DWORD nSavingFile;

WIN32_FIND_DATA FindData;


// ----------------------------------------------------------------------
// Free all files
// ----------------------------------------------------------------------
VOID FreeAllFileContent(LPFILECONTENT lpFC)
{
    if (NULL != lpFC) {
        if (NULL != lpFC->lpszFileName) {
            MYFREE(lpFC->lpszFileName);
        }
        FreeAllFileContent(lpFC->lpFirstSubFC);
        FreeAllFileContent(lpFC->lpBrotherFC);
        MYFREE(lpFC);
    }
}


// ----------------------------------------------------------------------
// Free all head files
// ----------------------------------------------------------------------
VOID FreeAllFileHead(LPHEADFILE lpHF)
{
    if (NULL != lpHF) {
        FreeAllFileContent(lpHF->lpFirstFC);
        FreeAllFileHead(lpHF->lpBrotherHF);
        MYFREE(lpHF);
    }
}


//-------------------------------------------------------------
// Get whole file name [root dir] from FILECONTENT
//-------------------------------------------------------------
LPTSTR GetWholeFileName(LPFILECONTENT lpStartFC, size_t cchExtra)
{
    LPFILECONTENT lpFC;
    LPTSTR lpszName;
    LPTSTR lpszTail;
    size_t cchName;

    cchName = 0;
    for (lpFC = lpStartFC; NULL != lpFC; lpFC = lpFC->lpFatherFC) {
        if (NULL != lpFC->lpszFileName) {
            cchName += _tcslen(lpFC->lpszFileName) + 1;  // +1 char for backslash or NULL char
        }
    }
    if (0 == cchName) {  // at least create an empty string with NULL char
        cchName++;
    }

    lpszName = MYALLOC((cchName + cchExtra) * sizeof(TCHAR));

    lpszTail = &lpszName[cchName - 1];
    lpszTail[0] = (TCHAR)'\0';

    for (lpFC = lpStartFC; NULL != lpFC; lpFC = lpFC->lpFatherFC) {
        if (NULL != lpFC->lpszFileName) {
            cchName = _tcslen(lpFC->lpszFileName);
            _tcsncpy(lpszTail -= cchName, lpFC->lpszFileName, cchName);
            if (lpszTail > lpszName) {
                *--lpszTail = (TCHAR)'\\';
            }
        }
    }

    return lpszName;
}


//-------------------------------------------------------------
// Routine to walk through all sub tree of current directory [File system]
//-------------------------------------------------------------
VOID GetAllSubFile(
    BOOL    needbrother,
    DWORD   typedir,
    DWORD   typefile,
    LPDWORD lpcountdir,
    LPDWORD lpcountfile,
    LPFILECONTENT lpFC
)
{
    //LPTSTR   lpTemp;

    if (ISDIR(lpFC->nFileAttributes)) {
        //lpTemp = lpFC->lpszFileName;
        if ((NULL != lpFC->lpszFileName) && (0 != _tcscmp(lpFC->lpszFileName, TEXT("."))) && (0 != _tcscmp(lpFC->lpszFileName, TEXT(".."))))  { // tfx   added in 1.7.3 fixed at 1.8.0 we should add here 1.8.0
            //if (*(unsigned short *)lpTemp != 0x002E && !(*(unsigned short *)lpTemp == 0x2E2E && *(lpTemp + 2) == 0x00)) {     // 1.8.2
            LogToMem(typedir, lpcountdir, lpFC);
        }
    } else {
        LogToMem(typefile, lpcountfile, lpFC);
    }

    if (NULL != lpFC->lpFirstSubFC)    {
        GetAllSubFile(TRUE, typedir, typefile, lpcountdir, lpcountfile, lpFC->lpFirstSubFC);
    }
    if (TRUE == needbrother) {
        if (NULL != lpFC->lpBrotherFC) {
            GetAllSubFile(TRUE, typedir, typefile, lpcountdir, lpcountfile, lpFC->lpBrotherFC);
        }
    }
}


// ----------------------------------------------------------------------
// Get file snap shot
// ----------------------------------------------------------------------
LPFILECONTENT GetFilesSnap(LPTSTR lpszName, LPWIN32_FIND_DATA lpFindData, LPFILECONTENT lpFatherFC, LPFILECONTENT *lplpCaller)
{
    LPFILECONTENT lpFC;
    LPFILECONTENT *lplpFCPrev;
    HANDLE hFile;

    // Check if file is to be generic excluded
    if ((NULL == lpszName)
            || (0 == _tcscmp(lpszName, TEXT(".")))
            || (0 == _tcscmp(lpszName, TEXT("..")))) {
        return NULL;
    }

    // Create new file content
    // put in a separate var for later use
    lpFC = MYALLOC0(sizeof(FILECONTENT));
    ZeroMemory(lpFC, sizeof(FILECONTENT));

    // Set father of current key
    lpFC->lpFatherFC = lpFatherFC;

    // Extra local block to reduce stack usage due to recursive calls
    {
        LPTSTR lpszFindFileName;
        size_t cchName;

        // Set file name
        cchName = _tcslen(lpszName) + 1;
        lpFC->lpszFileName = MYALLOC0(cchName * sizeof(TCHAR));
        _tcscpy(lpFC->lpszFileName, lpszName);

        // Check if file is to be excluded
        lpszFindFileName = GetWholeFileName(lpFC, 4);  // +4 for "\*.*" search when directory
        if (IsInSkipList(lpszFindFileName, lprgszFileSkipStrings)) {
            MYFREE(lpszFindFileName);
            FreeAllFileContent(lpFC);
            return NULL;
        }

        // Write pointer to current file into caller's pointer
        if (NULL != lplpCaller) {
            *lplpCaller = lpFC;
        }

        // Get file data if not already provide
        if (NULL == lpFindData) {
            lpFindData = &FindData;
            hFile = FindFirstFile(lpszFindFileName, lpFindData);
            if (hFile == INVALID_HANDLE_VALUE) {
                nGettingFile++;  // count as file
                MYFREE(lpszFindFileName);
                return lpFC;
            }
            FindClose(hFile);
        }

        // Set file data
        lpFC->nWriteDateTimeLow = lpFindData->ftLastWriteTime.dwLowDateTime;
        lpFC->nWriteDateTimeHigh = lpFindData->ftLastWriteTime.dwHighDateTime;
        lpFC->nFileSizeLow = lpFindData->nFileSizeLow;
        lpFC->nFileSizeHigh = lpFindData->nFileSizeHigh;
        lpFC->nFileAttributes = lpFindData->dwFileAttributes;

        // Increase file/dir count
        if (ISDIR(lpFC->nFileAttributes)) {
            nGettingDir++;
        } else {
            nGettingFile++;
        }

        // Update counters display
        nGettingTime = GetTickCount();
        if (REFRESHINTERVAL < (nGettingTime - nBASETIME1)) {
            UpdateCounters(asLangTexts[iszTextDir].lpszText, asLangTexts[iszTextFile].lpszText, nGettingDir, nGettingFile);
        }

        // When file then leave
        if (ISFILE(lpFC->nFileAttributes)) {
            MYFREE(lpszFindFileName);
            return lpFC;
        }

        // ATTENTION!!! Content of lpFindData is INVALID from this point on, due to recursive calls

        // Find all file entries of directory
        _tcscat(lpszFindFileName, TEXT("\\*.*"));
        hFile = FindFirstFile(lpszFindFileName, &FindData);
        MYFREE(lpszFindFileName);
        if (hFile == INVALID_HANDLE_VALUE) {
            return lpFC;
        }
    }

    // Process all directory entries
    lplpFCPrev = &lpFC->lpFirstSubFC;
    do {
        LPFILECONTENT lpFCSub;

        lpFCSub = GetFilesSnap(lpFindData->cFileName, &FindData, lpFC, lplpFCPrev);
        if (NULL != lpFCSub) {
            lplpFCPrev = &lpFCSub->lpBrotherFC;
        }
    } while (FindNextFile(hFile, &FindData) != FALSE);
    FindClose(hFile);

    return lpFC;
}


// ----------------------------------------------------------------------
// File Shot Engine
// ----------------------------------------------------------------------
VOID FileShot(LPREGSHOT lpShot)
{
    UINT cchExtDir;

    cchExtDir = GetDlgItemText(hWnd, IDC_EDITDIR, lpszExtDir, EXTDIRLEN);  // length incl. NULL character
    if (0 < cchExtDir) {
        LPHEADFILE *lplpHFPrev;
        LPTSTR lpszSubExtDir;
        size_t i;

        lplpHFPrev = &lpShot->lpHF;
        lpszSubExtDir = lpszExtDir;
        for (i = 0; i <= cchExtDir; i++) {
            // Split each directory in string
            if (((TCHAR)';' == lpszExtDir[i]) || ((TCHAR)'\0' == lpszExtDir[i])) {
                LPHEADFILE lpHF;
                size_t j;

                lpszExtDir[i] = (TCHAR)'\0';
                j = i;

                // remove all trailing backslashes
                while ((0 < j) && ((TCHAR)'\\' == lpszExtDir[--j])) {
                    lpszExtDir[j] = (TCHAR)'\0';
                }

                // if anything is left then process this directory
                if ((0 < j) && ((TCHAR)'\0' != lpszExtDir[j])) {
                    lpHF = MYALLOC0(sizeof(HEADFILE));
                    ZeroMemory(lpHF, sizeof(HEADFILE));

                    *lplpHFPrev = lpHF;
                    lplpHFPrev = &lpHF->lpBrotherHF;

                    GetFilesSnap(lpszSubExtDir, NULL, NULL, &lpHF->lpFirstFC);
                }

                lpszSubExtDir = &lpszExtDir[i + 1];
            }
        }
    }

    // Update counters display (dirs/files final)
    nGettingTime = GetTickCount();
    UpdateCounters(asLangTexts[iszTextDir].lpszText, asLangTexts[iszTextFile].lpszText, nGettingDir, nGettingFile);
}


//-------------------------------------------------------------
// File comparison engine (lp1 and lp2 run parallel)
//-------------------------------------------------------------
VOID CompareFirstSubFile(LPFILECONTENT lpFCHead1, LPFILECONTENT lpFCHead2)
{
    LPFILECONTENT lpFC1;
    LPFILECONTENT lpFC2;

    for (lpFC1 = lpFCHead1; lpFC1 != NULL; lpFC1 = lpFC1->lpBrotherFC) {
        for (lpFC2 = lpFCHead2; lpFC2 != NULL; lpFC2 = lpFC2->lpBrotherFC) {
            if ((lpFC2->fComparison == NOTMATCH) && _tcscmp(lpFC1->lpszFileName, lpFC2->lpszFileName) == 0) { // 1.8.2 from lstrcmp to strcmp
                // Two files have the same name, but we are not sure they are the same, so we compare them!
                if (ISFILE(lpFC1->nFileAttributes) && ISFILE(lpFC2->nFileAttributes))
                    //(lpFC1->nFileAttributes&FILE_ATTRIBUTE_DIRECTORY) != FILE_ATTRIBUTE_DIRECTORY && (lpFC2->nFileAttributes&FILE_ATTRIBUTE_DIRECTORY) != FILE_ATTRIBUTE_DIRECTORY)
                {
                    // Lp1 is file, lpFC2 is file
                    if (lpFC1->nWriteDateTimeLow == lpFC2->nWriteDateTimeLow && lpFC1->nWriteDateTimeHigh == lpFC2->nWriteDateTimeHigh &&
                            lpFC1->nFileSizeLow == lpFC2->nFileSizeLow && lpFC1->nFileSizeHigh == lpFC2->nFileSizeHigh && lpFC1->nFileAttributes == lpFC2->nFileAttributes) {
                        // We found a match file!
                        lpFC2->fComparison = ISMATCH;
                    } else {
                        // We found a dismatch file, they will be logged
                        lpFC2->fComparison = ISMODI;
                        LogToMem(FILEMODI, &nFILEMODI, lpFC1);
                    }

                } else {
                    // At least one file of the pair is directory, so we try to determine
                    if (ISDIR(lpFC1->nFileAttributes) && ISDIR(lpFC2->nFileAttributes))
                        // (lpFC1->nFileAttributes&FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY && (lpFC2->nFileAttributes&FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY)
                    {
                        // The two 'FILE's are all dirs
                        if (lpFC1->nFileAttributes == lpFC2->nFileAttributes) {
                            // Same dir attributes, we compare their subfiles
                            lpFC2->fComparison = ISMATCH;
                            CompareFirstSubFile(lpFC1->lpFirstSubFC, lpFC2->lpFirstSubFC);
                        } else {
                            // Dir attributes changed, they will be logged
                            lpFC2->fComparison = ISMODI;
                            LogToMem(DIRMODI, &nDIRMODI, lpFC1);
                        }
                        //break;
                    } else {
                        // One of the 'FILE's is dir, but which one?
                        if (ISFILE(lpFC1->nFileAttributes) && ISDIR(lpFC2->nFileAttributes))
                            //(lpFC1->nFileAttributes&FILE_ATTRIBUTE_DIRECTORY) != FILE_ATTRIBUTE_DIRECTORY && (lpFC2->nFileAttributes&FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY)
                        {
                            // lpFC1 is file, lpFC2 is dir
                            lpFC1->fComparison = ISDEL;
                            LogToMem(FILEDEL, &nFILEDEL, lpFC1);
                            GetAllSubFile(FALSE, DIRADD, FILEADD, &nDIRADD, &nFILEADD, lpFC2);
                        } else {
                            // lpFC1 is dir, lpFC2 is file
                            lpFC2->fComparison = ISADD;
                            LogToMem(FILEADD, &nFILEADD, lpFC2);
                            GetAllSubFile(FALSE, DIRDEL, FILEDEL, &nDIRDEL, &nFILEDEL, lpFC1);
                        }
                    }
                }
                break;
            }
        }

        if (lpFC2 == NULL) {
            // lpFC2 looped to the end, that is, we can not find a lpFC2 matches lpFC1, so lpFC1 is deleted!
            if (ISDIR(lpFC1->nFileAttributes)) {
                GetAllSubFile(FALSE, DIRDEL, FILEDEL, &nDIRDEL, &nFILEDEL, lpFC1); // if lpFC1 is dir
            } else {
                LogToMem(FILEDEL, &nFILEDEL, lpFC1);  // if lpFC1 is file
            }
        }
    }

    // We loop to the end, then we do an extra loop of lpFC2 use flag we previous made
    for (lpFC2 = lpFCHead2; lpFC2 != NULL; lpFC2 = lpFC2->lpBrotherFC) {
        nComparing++;
        if (lpFC2->fComparison == NOTMATCH) {
            // We did not find a lpFC1 matches a lpFC2, so lpFC2 is added!
            if (ISDIR(lpFC2->nFileAttributes)) {
                GetAllSubFile(FALSE, DIRADD, FILEADD, &nDIRADD, &nFILEADD, lpFC2);
            } else {
                LogToMem(FILEADD, &nFILEADD, lpFC2);
            }
        }
    }

    // Progress bar update
    if (nGettingFile != 0)
        if (nComparing % nGettingFile > nFileStep) {
            nComparing = 0;
            SendDlgItemMessage(hWnd, IDC_PROGBAR, PBM_STEPIT, (WPARAM)0, (LPARAM)0);
        }
}


// ----------------------------------------------------------------------
// Clear comparison match flags in all files
// ----------------------------------------------------------------------
VOID ClearFileContentMatchTag(LPFILECONTENT lpFC)
{
    if (NULL != lpFC) {
        lpFC->fComparison = 0;
        ClearFileContentMatchTag(lpFC->lpFirstSubFC);
        ClearFileContentMatchTag(lpFC->lpBrotherFC);
    }
}

// ----------------------------------------------------------------------
// Clear comparison match flags in all head files
// ----------------------------------------------------------------------
VOID ClearHeadFileMatchTag(LPHEADFILE lpStartHF)
{
    LPHEADFILE lpHF;

    for (lpHF = lpStartHF; NULL != lpHF; lpHF = lpHF->lpBrotherHF) {
        ClearFileContentMatchTag(lpHF->lpFirstFC);
    }
}


// ----------------------------------------------------------------------
// Save file to HIVE File
//
// This routine is called recursively to store the entries of the file/dir tree
// Therefore temporary vars are put in a local block to reduce stack usage
// ----------------------------------------------------------------------
VOID SaveFileContent(LPFILECONTENT lpFC, DWORD nFPFatherFile, DWORD nFPCaller)
{
    DWORD nFPFile;

    // Get current file position
    // put in a separate var for later use
    nFPFile = SetFilePointer(hFileWholeReg, 0, NULL, FILE_CURRENT);

    // Write position of current file in caller's field
    if (0 < nFPCaller) {
        SetFilePointer(hFileWholeReg, nFPCaller, NULL, FILE_BEGIN);
        WriteFile(hFileWholeReg, &nFPFile, sizeof(nFPFile), &NBW, NULL);

        SetFilePointer(hFileWholeReg, nFPFile, NULL, FILE_BEGIN);
    }

    // Initialize file content
    ZeroMemory(&sFC, sizeof(sFC));

    // Copy values
    sFC.nWriteDateTimeLow = lpFC->nWriteDateTimeLow;
    sFC.nWriteDateTimeHigh = lpFC->nWriteDateTimeHigh;
    sFC.nFileSizeLow = lpFC->nFileSizeLow;
    sFC.nFileSizeHigh = lpFC->nFileSizeHigh;
    sFC.nFileAttributes = lpFC->nFileAttributes;
    sFC.nChkSum = lpFC->nChkSum;

    // Set file positions of the relatives inside the tree
    sFC.ofsFileName = 0;      // not known yet, may be re-written in this call
    sFC.ofsFirstSubFile = 0;  // not known yet, may be re-written by another recursive call
    sFC.ofsBrotherFile = 0;   // not known yet, may be re-written by another recursive call
    sFC.ofsFatherFile = nFPFatherFile;

    // New since file content version 2
    sFC.nFileNameLen = 0;
    if (NULL != lpFC->lpszFileName) {
        sFC.nFileNameLen = (DWORD)_tcslen(lpFC->lpszFileName);
#ifdef _UNICODE
        sFC.nFileNameLen++;  // account for NULL char
        // File name will always be stored behind the structure, so its position is already known
        sFC.ofsFileName = nFPFile + sizeof(sFC);
#endif
    }
#ifndef _UNICODE
    sFC.nFileNameLen++;  // account for NULL char
    // File name will always be stored behind the structure, so its position is already known
    sFC.ofsFileName = nFPFile + sizeof(sFC);
#endif

    // Write file content to file
    // Make sure that ALL fields have been initialized/set
    WriteFile(hFileWholeReg, &sFC, sizeof(sFC), &NBW, NULL);

    // Write file name to file
    if (NULL != lpFC->lpszFileName) {
        WriteFile(hFileWholeReg, lpFC->lpszFileName, sFC.nFileNameLen * sizeof(TCHAR), &NBW, NULL);
#ifndef _UNICODE
    } else {
        // Write empty string for backward compatibility
        WriteFile(hFileWholeReg, lpszEmpty, sFC.nFileNameLen * sizeof(TCHAR), &NBW, NULL);
#endif
    }

    // ATTENTION!!! sFC is INVALID from this point on, due to recursive calls

    // If the entry has childs, then do a recursive call for the first child
    // Pass this entry as father and "ofsFirstSubFile" position for storing the first child's position
    if (NULL != lpFC->lpFirstSubFC) {
        SaveFileContent(lpFC->lpFirstSubFC, nFPFile, nFPFile + offsetof(SAVEFILECONTENT, ofsFirstSubFile));
    }

    // If the entry has a following brother, then do a recursive call for the following brother
    // Pass father as father and "ofsBrotherFile" position for storing the next brother's position
    if (NULL != lpFC->lpBrotherFC) {
        SaveFileContent(lpFC->lpBrotherFC, nFPFatherFile, nFPFile + offsetof(SAVEFILECONTENT, ofsBrotherFile));
    }

    // TODO: Need to adjust progress bar para!!
    nSavingFile++;
    if (0 != nGettingFile) {
        if (nSavingFile % nGettingFile > nFileStep) {
            nSavingFile = 0;
            SendDlgItemMessage(hWnd, IDC_PROGBAR, PBM_STEPIT, (WPARAM)0, (LPARAM)0);
            UpdateWindow(hWnd);
            PeekMessage(&msg, hWnd, WM_ACTIVATE, WM_ACTIVATE, PM_REMOVE);
        }
    }
}

//--------------------------------------------------
// Save head file to HIVE file
//--------------------------------------------------
VOID SaveHeadFile(LPHEADFILE lpStartHF, DWORD nFPCaller)
{
    LPHEADFILE lpHF;
    DWORD nFPHFCaller;
    DWORD nFPHF;

    // Write all head files and their file contents
    nFPHFCaller = nFPCaller;
    for (lpHF = lpStartHF; NULL != lpHF; lpHF = lpHF->lpBrotherHF) {
        nFPHF = SetFilePointer(hFileWholeReg, 0, NULL, FILE_CURRENT);

        // Write position of current head file in caller's field
        if (0 < nFPHFCaller) {
            SetFilePointer(hFileWholeReg, nFPHFCaller, NULL, FILE_BEGIN);
            WriteFile(hFileWholeReg, &nFPHF, sizeof(nFPHF), &NBW, NULL);

            SetFilePointer(hFileWholeReg, nFPHF, NULL, FILE_BEGIN);
        }
        nFPHFCaller = nFPHF + offsetof(SAVEHEADFILE, ofsBrotherHeadFile);

        // Initialize head file
        ZeroMemory(&sHF, sizeof(sHF));

        // Write head file to file
        // Make sure that ALL fields have been initialized/set
        WriteFile(hFileWholeReg, &sHF, sizeof(sHF), &NBW, NULL);

        // Write all file contents of head file
        if (NULL != lpHF->lpFirstFC) {
            SaveFileContent(lpHF->lpFirstFC, 0, nFPHF + offsetof(SAVEHEADFILE, ofsFirstFileContent));
        }
    }
}


// ----------------------------------------------------------------------
// Load file from HIVE file
// ----------------------------------------------------------------------
VOID LoadFile(DWORD ofsFileContent, LPFILECONTENT lpFatherFC, LPFILECONTENT *lplpCaller)
{
    LPFILECONTENT lpFC;
    DWORD ofsFirstSubFile;
    DWORD ofsBrotherFile;
    BOOL fIgnore;

    fIgnore = FALSE;

    // Copy SAVEFILECONTENT to aligned memory block
    ZeroMemory(&sFC, sizeof(sFC));
    CopyMemory(&sFC, (lpFileBuffer + ofsFileContent), fileheader.nFCSize);

    // Create new file content
    // put in a separate var for later use
    lpFC = MYALLOC0(sizeof(FILECONTENT));
    ZeroMemory(lpFC, sizeof(FILECONTENT));

    // Set father of current file
    lpFC->lpFatherFC = lpFatherFC;

    // Copy file name
    if (FILECONTENT_VERSION_2 > fileheader.nFCVersion) {  // old SBCS/MBCS version
        sFC.nFileNameLen = (DWORD)strlen((const char *)(lpFileBuffer + sFC.ofsFileName));
        if (0 < sFC.nFileNameLen) {
            sFC.nFileNameLen++;  // account for NULL char
        }
    }
    if (0 < sFC.nFileNameLen) {  // otherwise leave it NULL
        // Copy string to an aligned memory block
        nSourceSize = sFC.nFileNameLen * fileheader.nCharSize;
        nStringBufferSize = AdjustBuffer(&lpStringBuffer, nStringBufferSize, nSourceSize, REGSHOT_BUFFER_BLOCK_BYTES);
        ZeroMemory(lpStringBuffer, nStringBufferSize);
        CopyMemory(lpStringBuffer, (lpFileBuffer + sFC.ofsFileName), nSourceSize);

        lpFC->lpszFileName = MYALLOC0(sFC.nFileNameLen * sizeof(TCHAR));
        if (sizeof(TCHAR) == fileheader.nCharSize) {
            _tcscpy(lpFC->lpszFileName, lpStringBuffer);
        } else {
#ifdef _UNICODE
            MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, (LPCSTR)lpStringBuffer, -1, lpFC->lpszFileName, sFC.nFileNameLen);
#else
            WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK | WC_DEFAULTCHAR, (LPCWSTR)lpStringBuffer, -1, lpFC->lpszFileName, sFC.nFileNameLen, NULL, NULL);
#endif
        }
    }

    // Check if file is to be generic excluded
    if ((NULL == lpFC->lpszFileName)
            || (0 == _tcscmp(lpFC->lpszFileName, TEXT(".")))
            || (0 == _tcscmp(lpFC->lpszFileName, TEXT("..")))) {
        FreeAllFileContent(lpFC);
        fIgnore = TRUE;
    }

    if (!fIgnore) {
        // Write pointer to current file into caller's pointer
        if (NULL != lplpCaller) {
            *lplpCaller = lpFC;
        }

        // Copy file data
        lpFC->nWriteDateTimeLow = sFC.nWriteDateTimeLow;
        lpFC->nWriteDateTimeHigh = sFC.nWriteDateTimeHigh;
        lpFC->nFileSizeLow = sFC.nFileSizeLow;
        lpFC->nFileSizeHigh = sFC.nFileSizeHigh;
        lpFC->nFileAttributes = sFC.nFileAttributes;
        lpFC->nChkSum = sFC.nChkSum;

        // Increase file/dir count
        if (ISDIR(lpFC->nFileAttributes)) {
            nGettingDir++;
        } else {
            nGettingFile++;
        }

        // Update counters display
        nGettingTime = GetTickCount();
        if (REFRESHINTERVAL < (nGettingTime - nBASETIME1)) {
            UpdateCounters(asLangTexts[iszTextDir].lpszText, asLangTexts[iszTextFile].lpszText, nGettingDir, nGettingFile);
        }
    }

    // Save offsets in local variables
    ofsFirstSubFile = sFC.ofsFirstSubFile;
    ofsBrotherFile = sFC.ofsBrotherFile;

    // ATTENTION!!! sFC is INVALID from this point on, due to recursive calls

    // If the entry has childs, then do a recursive call for the first child
    // Pass this entry as father and "lpFirstSubFC" pointer for storing the first child's pointer
    if ((!fIgnore) && (0 != ofsFirstSubFile)) {
        LoadFile(ofsFirstSubFile, lpFC, &lpFC->lpFirstSubFC);
    }

    // If the entry has a following brother, then do a recursive call for the following brother
    // Pass father as father and "lpBrotherFC" pointer for storing the next brother's pointer
    if (0 != ofsBrotherFile) {
        if (!fIgnore) {
            LoadFile(ofsBrotherFile, lpFatherFC, &lpFC->lpBrotherFC);
        } else {
            LoadFile(ofsBrotherFile, lpFatherFC, lplpCaller);
        }
    }
}


//--------------------------------------------------
// Load head file from HIVE file
//--------------------------------------------------
VOID LoadHeadFile(DWORD ofsHeadFile, LPHEADFILE *lplpCaller)
{
    LPHEADFILE *lplpHFCaller;
    LPHEADFILE lpHF;

    // Read all head files and their file contents
    lplpHFCaller = lplpCaller;
    for (; 0 != ofsHeadFile; ofsHeadFile = sHF.ofsBrotherHeadFile) {
        // Copy SAVEHEADFILE to aligned memory block
        ZeroMemory(&sHF, sizeof(sHF));
        CopyMemory(&sHF, (lpFileBuffer + ofsHeadFile), fileheader.nHFSize);

        // Create new head file
        // put in a separate var for later use
        lpHF = MYALLOC0(sizeof(SAVEHEADFILE));
        ZeroMemory(lpHF, sizeof(SAVEHEADFILE));

        // Write pointer to current head file into caller's pointer
        if (NULL != lplpHFCaller) {
            *lplpHFCaller = lpHF;
        }
        lplpHFCaller = &lpHF->lpBrotherHF;

        // If the entry has file contents, then do a call for the first file content
        if (0 != sHF.ofsFirstFileContent) {
            LoadFile(sHF.ofsFirstFileContent, NULL, &lpHF->lpFirstFC);
        }
    }
}


//--------------------------------------------------
// Walkthrough lpHF chain and find lpszName matches
//--------------------------------------------------
LPFILECONTENT SearchDirChain(LPTSTR lpszName, LPHEADFILE lpStartHF)
{
    LPHEADFILE lpHF;

    if (NULL != lpszName) {
        for (lpHF = lpStartHF; NULL != lpHF; lpHF = lpHF->lpBrotherHF) {
            if ((NULL != lpHF->lpFirstFC)
                    && (NULL != lpHF->lpFirstFC->lpszFileName)
                    && (0 == _tcsicmp(lpszName, lpHF->lpFirstFC->lpszFileName))) {
                return lpHF->lpFirstFC;
            }
        }
    }
    return NULL;
}

//--------------------------------------------------
// Walkthrough lpHF chain and collect it's first dirname to lpszDir
//--------------------------------------------------
VOID FindDirChain(LPHEADFILE lpStartHF, LPTSTR lpszDir, size_t nBufferLen)
{
    LPHEADFILE  lpHF;
    size_t      nLen;
    size_t      nWholeLen;
    BOOL        fAddBackslash;
    BOOL        fAddSeparator;

    lpszDir[0] = (TCHAR)'\0';
    nWholeLen = 0;
    for (lpHF = lpStartHF; NULL != lpHF; lpHF = lpHF->lpBrotherHF) {
        if ((NULL != lpHF->lpFirstFC)
                && (NULL != lpHF->lpFirstFC->lpszFileName)) {
            nLen = _tcslen(lpHF->lpFirstFC->lpszFileName);
            if (nLen > 0) {
                fAddBackslash = FALSE;
                if ((TCHAR)':' == lpHF->lpFirstFC->lpszFileName[nLen - 1]) {
                    nLen++;
                    fAddBackslash = TRUE;
                }
                fAddSeparator = FALSE;
                if (nWholeLen > 0) {
                    nLen++;
                    fAddSeparator = TRUE;
                }
                if ((nWholeLen + nLen) < nBufferLen) {
                    nWholeLen += nLen;
                    if (fAddSeparator) {
                        _tcscat(lpszDir, TEXT(";"));
                    }
                    _tcscat(lpszDir, lpHF->lpFirstFC->lpszFileName);
                    if (fAddBackslash) {
                        _tcscat(lpszDir, TEXT("\\"));
                    }
                } else {
                    break;
                }
            }
        }
    }
}


//--------------------------------------------------
// if two dir chains are the same
//--------------------------------------------------
BOOL DirChainMatch(LPHEADFILE lpHF1, LPHEADFILE lpHF2)
{
    TCHAR lpszDir1[EXTDIRLEN];
    TCHAR lpszDir2[EXTDIRLEN];

    ZeroMemory(lpszDir1, sizeof(lpszDir1));
    ZeroMemory(lpszDir2, sizeof(lpszDir2));

    FindDirChain(lpHF1, lpszDir1, EXTDIRLEN);  // Length in TCHARs incl. NULL char
    FindDirChain(lpHF2, lpszDir2, EXTDIRLEN);  // Length in TCHARs incl. NULL char

    if (0 != _tcsicmp(lpszDir1, lpszDir2)) {
        return FALSE;
    } else {
        return TRUE;
    }
}
