/*
    Copyright 2011-2013 Regshot Team
    Copyright 1999-2003,2007,2011 TiANWEi
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

// Workaround for older Platform/Windows SDKs (e.g. VS 6.0)
#ifndef INVALID_FILE_ATTRIBUTES
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
#endif

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


//-------------------------------------------------------------
// Get whole file name from FILECONTENT
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
            cchName += lpFC->cchFileName + 1;  // +1 char for backslash or NULL char
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
            cchName = lpFC->cchFileName;
            _tcsncpy(lpszTail -= cchName, lpFC->lpszFileName, cchName);
            if (lpszTail > lpszName) {
                *--lpszTail = (TCHAR)'\\';
            }
        }
    }

    return lpszName;
}

//--------------------------------------------------
// Walkthrough lpHF chain and find lpszName matches
//--------------------------------------------------
LPFILECONTENT SearchDirChain(LPTSTR lpszName, LPHEADFILE lpHF)
{
    if (NULL != lpszName) {
        for (; NULL != lpHF; lpHF = lpHF->lpBrotherHF) {
            if ((NULL != lpHF->lpFirstFC)
                    && (NULL != lpHF->lpFirstFC->lpszFileName)
                    && (0 == _tcsicmp(lpszName, lpHF->lpFirstFC->lpszFileName))) {
                return lpHF->lpFirstFC;
            }
        }
    }
    return NULL;
}


//-------------------------------------------------------------
// Routine to walk through all sub tree of current directory [File system]
//-------------------------------------------------------------
VOID LogAllFiles(
    BOOL    fIncludeBrothers,
    DWORD   typedir,
    DWORD   typefile,
    LPDWORD lpcountdir,
    LPDWORD lpcountfile,
    LPFILECONTENT lpStartFC
)
{
    LPFILECONTENT lpFC;

    for (lpFC = lpStartFC; NULL != lpFC; lpFC = lpFC->lpBrotherFC) {
        if (ISDIR(lpFC->nFileAttributes)) {
            if ((NULL != lpFC->lpszFileName) && (0 != _tcscmp(lpFC->lpszFileName, TEXT("."))) && (0 != _tcscmp(lpFC->lpszFileName, TEXT(".."))))  { // tfx   added in 1.7.3 fixed at 1.8.0 we should add here 1.8.0
                LogToMem(typedir, lpcountdir, lpFC);
            }
        } else {
            LogToMem(typefile, lpcountfile, lpFC);
        }

        if (NULL != lpFC->lpFirstSubFC)    {
            LogAllFiles(TRUE, typedir, typefile, lpcountdir, lpcountfile, lpFC->lpFirstSubFC);
        }

        if (!fIncludeBrothers) {
            break;
        }
    }
}


// ----------------------------------------------------------------------
// Free all files
// ----------------------------------------------------------------------
VOID FreeAllFileContents(LPFILECONTENT lpFC)
{
    LPFILECONTENT lpBrotherFC;

    for (; NULL != lpFC; lpFC = lpBrotherFC) {
        // Save pointer in local variable
        lpBrotherFC = lpFC->lpBrotherFC;

        // Free file name
        if (NULL != lpFC->lpszFileName) {
            MYFREE(lpFC->lpszFileName);
        }

        // If the entry has childs, then do a recursive call for the first child
        if (NULL != lpFC->lpFirstSubFC) {
            FreeAllFileContents(lpFC->lpFirstSubFC);
        }

        // Free entry itself
        MYFREE(lpFC);
    }
}


// ----------------------------------------------------------------------
// Free all head files
// ----------------------------------------------------------------------
VOID FreeAllHeadFiles(LPHEADFILE lpHF)
{
    LPHEADFILE lpBrotherHF;

    for (; NULL != lpHF; lpHF = lpBrotherHF) {
        // Save pointer in local variable
        lpBrotherHF = lpHF->lpBrotherHF;

        // If the entry has childs, then do a recursive call for the first child
        if (NULL != lpHF->lpFirstFC) {
            FreeAllFileContents(lpHF->lpFirstFC);
        }

        // Free entry itself
        MYFREE(lpHF);
    }
}


//-------------------------------------------------------------
// File comparison engine (lp1 and lp2 run parallel)
//-------------------------------------------------------------
VOID CompareFiles(LPFILECONTENT lpStartFC1, LPFILECONTENT lpStartFC2)
{
    LPFILECONTENT lpFC1;
    LPFILECONTENT lpFC2;

    // Compare files
    for (lpFC1 = lpStartFC1; NULL != lpFC1; lpFC1 = lpFC1->lpBrotherFC) {
        // Find a matching file for FC1
        for (lpFC2 = lpStartFC2; NULL != lpFC2; lpFC2 = lpFC2->lpBrotherFC) {
            // skip FC2 if already matched
            if (NOMATCH != lpFC2->fFileMatch) {
                continue;
            }
            // skip FC2 if names do *not* match
            if ((NULL == lpFC1->lpszFileName) || (NULL == lpFC2->lpszFileName) || (0 != _tcsicmp(lpFC1->lpszFileName, lpFC2->lpszFileName))) { // 1.8.2 from lstrcmp to strcmp  // 1.9.0 to _tcsicmp
                continue;
            }

            // Two files have the same (non-case-sensitive) name, but we are not sure they are the same, so we compare them!
            if (ISFILE(lpFC1->nFileAttributes) && ISFILE(lpFC2->nFileAttributes)) {
                // Both are files
                if ((lpFC1->nWriteDateTimeLow == lpFC2->nWriteDateTimeLow)
                        && (lpFC1->nWriteDateTimeHigh == lpFC2->nWriteDateTimeHigh)
                        && (lpFC1->nFileSizeLow == lpFC2->nFileSizeLow)
                        && (lpFC1->nFileSizeHigh == lpFC2->nFileSizeHigh)
                        && (lpFC1->nFileAttributes == lpFC2->nFileAttributes)) {
                    // We found a match file!
                    lpFC2->fFileMatch = ISMATCH;
                } else {
                    // We found a dismatch file, they will be logged
                    lpFC2->fFileMatch = ISMODI;
                    LogToMem(FILEMODI, &nFILEMODI, lpFC1);
                }
            } else if (ISDIR(lpFC1->nFileAttributes) && ISDIR(lpFC2->nFileAttributes)) {
                // Both are dirs
                if (lpFC1->nFileAttributes == lpFC2->nFileAttributes) {
                    // Same dir attributes, we compare their subfiles
                    lpFC2->fFileMatch = ISMATCH;
                } else {
                    // Dir attributes changed, they will be logged
                    lpFC2->fFileMatch = ISMODI;
                    LogToMem(DIRMODI, &nDIRMODI, lpFC1);
                }
                CompareFiles(lpFC1->lpFirstSubFC, lpFC2->lpFirstSubFC);
            } else {
                // At least one file of the pair is directory, so we try to determine
                if (ISFILE(lpFC1->nFileAttributes) && ISDIR(lpFC2->nFileAttributes)) {
                    // lpFC1 is file, lpFC2 is dir
                    lpFC1->fFileMatch = ISDEL;
                    LogToMem(FILEDEL, &nFILEDEL, lpFC1);
                    LogAllFiles(FALSE, DIRADD, FILEADD, &nDIRADD, &nFILEADD, lpFC2);
                } else {
                    // lpFC1 is dir, lpFC2 is file
                    LogAllFiles(FALSE, DIRDEL, FILEDEL, &nDIRDEL, &nFILEDEL, lpFC1);
                    lpFC2->fFileMatch = ISADD;
                    LogToMem(FILEADD, &nFILEADD, lpFC2);
                }
            }
            break;
        }

        if (NULL == lpFC2) {
            // lpFC2 looped to the end, that is, we cannot find a lpFC2 matches lpFC1, so lpFC1 is deleted!
            if (ISDIR(lpFC1->nFileAttributes)) {
                LogAllFiles(FALSE, DIRDEL, FILEDEL, &nDIRDEL, &nFILEDEL, lpFC1); // if lpFC1 is dir
            } else {
                LogToMem(FILEDEL, &nFILEDEL, lpFC1);  // if lpFC1 is file
            }
        }
    }

    // We loop to the end, then we do an extra loop of lpFC2 use flag we previous made
    for (lpFC2 = lpStartFC2; NULL != lpFC2; lpFC2 = lpFC2->lpBrotherFC) {
        nComparing++;
        if (NOMATCH == lpFC2->fFileMatch) {
            // We did not find a lpFC1 matches a lpFC2, so lpFC2 is added!
            if (ISDIR(lpFC2->nFileAttributes)) {
                LogAllFiles(FALSE, DIRADD, FILEADD, &nDIRADD, &nFILEADD, lpFC2);
            } else {
                LogToMem(FILEADD, &nFILEADD, lpFC2);
            }
        }
    }

    // Progress bar update
    if (0 != nGettingFile)
        if (nComparing % nGettingFile > nFileStep) {
            nComparing = 0;
            SendDlgItemMessage(hWnd, IDC_PROGBAR, PBM_STEPIT, (WPARAM)0, (LPARAM)0);
        }
}

//-------------------------------------------------------------
// Directory and file comparison engine
//-------------------------------------------------------------
VOID CompareHeadFiles(LPHEADFILE lpStartHF1, LPHEADFILE lpStartHF2)
{
    LPHEADFILE lpHF1;
    LPHEADFILE lpHF2;
    LPFILECONTENT lpFC1;
    LPFILECONTENT lpFC2;

    // first loop
    for (lpHF1 = lpStartHF1; NULL != lpHF1; lpHF1 = lpHF1->lpBrotherHF) {
        lpFC1 = lpHF1->lpFirstFC;
        if (NULL != lpFC1) {
            if (NULL != (lpFC2 = SearchDirChain(lpFC1->lpszFileName, lpStartHF2))) {   // note lpHF2 should not changed here!
                CompareFiles(lpFC1, lpFC2);                              // if found, we do compare
            } else {    // cannot find matched lpFC1 in lpHF2 chain.
                LogAllFiles(FALSE, DIRDEL, FILEDEL, &nDIRDEL, &nFILEDEL, lpFC1);
            }
        }
    }

    // second loop
    for (lpHF2 = lpStartHF2; NULL != lpHF2; lpHF2 = lpHF2->lpBrotherHF) {
        lpFC2 = lpHF2->lpFirstFC;
        if (NULL != lpFC2) {
            if (NULL == (lpFC1 = SearchDirChain(lpFC2->lpszFileName, lpStartHF1))) {   // in the second loop we only find those do not match
                LogAllFiles(FALSE, DIRADD, FILEADD, &nDIRADD, &nFILEADD, lpFC2);
            }
        }
    }
}


// ----------------------------------------------------------------------
// Get file snap shot
// ----------------------------------------------------------------------
VOID GetFilesSnap(LPTSTR lpszFullName, LPFILECONTENT lpFatherFC, LPFILECONTENT *lplpCaller)
{
    LPFILECONTENT lpFC;
    HANDLE hFile;

    // Extra local block to reduce stack usage due to recursive calls
    {
        LPTSTR lpszFindFileName;

        lpszFindFileName = NULL;

        // Get father file data if not already provided
        if (NULL == lpFatherFC) {
            // Check if file is to be generic excluded
            if ((NULL == lpszFullName)
                    || (((TCHAR)'.' == lpszFullName[0])  // fast exclusion for 99% of the cases
                        && ((0 == _tcscmp(lpszFullName, TEXT(".")))
                            || (0 == _tcscmp(lpszFullName, TEXT("..")))))) {
                return;
            }

            // Create new file content
            lpFatherFC = MYALLOC0(sizeof(FILECONTENT));

            // Set file name length
            lpFatherFC->cchFileName = _tcslen(lpszFullName);

            // Check if file is to be excluded
            if (NULL != lprgszFileSkipStrings[0]) {  // only if there is something to exclude
                if (IsInSkipList(lpszFullName, lprgszFileSkipStrings)) {
                    FreeAllFileContents(lpFatherFC);
                    return;
                }
            }

            hFile = FindFirstFile(lpszFullName, &FindData);
            if (INVALID_HANDLE_VALUE != hFile) {
                FindClose(hFile);
            } else {
                // Workaround for some cases in Windows Vista and later
#ifdef _DEBUG
                DWORD nError;
                LPTSTR lpszMessage;

                nError = GetLastError();
                FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                              NULL, nError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR) &lpszMessage, 0, NULL);
                LocalFree(lpszMessage);
#endif

                ZeroMemory(&FindData, sizeof(FindData));

                hFile = CreateFile(lpszFullName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
                if (INVALID_HANDLE_VALUE != hFile) {
                    BY_HANDLE_FILE_INFORMATION FileInformation;
                    BOOL bResult;

                    bResult = GetFileInformationByHandle(hFile, &FileInformation);
                    if (bResult) {
                        FindData.dwFileAttributes = FileInformation.dwFileAttributes;
                        FindData.ftCreationTime = FileInformation.ftCreationTime;
                        FindData.ftLastAccessTime = FileInformation.ftLastAccessTime;
                        FindData.ftLastWriteTime = FileInformation.ftLastWriteTime;
                        FindData.nFileSizeHigh = FileInformation.nFileSizeHigh;
                        FindData.nFileSizeLow = FileInformation.nFileSizeLow;
                    } else {
                        FindData.dwFileAttributes = GetFileAttributes(lpszFullName);
                        if (INVALID_FILE_ATTRIBUTES == FindData.dwFileAttributes) {
                            FindData.dwFileAttributes = 0;
                        }
                        bResult = GetFileTime(hFile, &FindData.ftCreationTime, &FindData.ftLastAccessTime, &FindData.ftLastWriteTime);
                        if (!bResult) {
                            FindData.ftCreationTime.dwLowDateTime = 0;
                            FindData.ftCreationTime.dwHighDateTime = 0;
                            FindData.ftLastAccessTime.dwLowDateTime = 0;
                            FindData.ftLastAccessTime.dwHighDateTime = 0;
                            FindData.ftLastWriteTime.dwLowDateTime = 0;
                            FindData.ftLastWriteTime.dwHighDateTime = 0;
                        }
                        FindData.nFileSizeLow = GetFileSize(hFile, &FindData.nFileSizeHigh);
                        if (INVALID_FILE_SIZE == FindData.nFileSizeLow) {
                            FindData.nFileSizeHigh = 0;
                            FindData.nFileSizeLow = 0;
                        }
                    }
                    CloseHandle(hFile);
                }
            }

            // Write pointer to current file into caller's pointer
            if (NULL != lplpCaller) {
                *lplpCaller = lpFatherFC;
            }

            // Copy file name
            lpFatherFC->lpszFileName = MYALLOC((lpFatherFC->cchFileName + 1) * sizeof(TCHAR));
            _tcscpy(lpFatherFC->lpszFileName, lpszFullName);

            // Copy file data
            lpFatherFC->nWriteDateTimeLow = FindData.ftLastWriteTime.dwLowDateTime;
            lpFatherFC->nWriteDateTimeHigh = FindData.ftLastWriteTime.dwHighDateTime;
            lpFatherFC->nFileSizeLow = FindData.nFileSizeLow;
            lpFatherFC->nFileSizeHigh = FindData.nFileSizeHigh;
            lpFatherFC->nFileAttributes = FindData.dwFileAttributes;

            // Increase file/dir count
            if (ISDIR(lpFatherFC->nFileAttributes)) {
                nGettingDir++;
            } else {
                nGettingFile++;
            }

            // Update counters display
            nGettingTime = GetTickCount();
            if (REFRESHINTERVAL < (nGettingTime - nBASETIME1)) {
                UpdateCounters(asLangTexts[iszTextDir].lpszText, asLangTexts[iszTextFile].lpszText, nGettingDir, nGettingFile);
            }

            // Set "lpFirstSubFC" pointer for storing the first child's pointer
            lplpCaller = &lpFatherFC->lpFirstSubFC;

            // Copy file name to new buffer for directory search
            lpszFindFileName = MYALLOC((lpFatherFC->cchFileName + 4 + 1) * sizeof(TCHAR));  // +4 for "\*.*" search when directory (later in routine)
            _tcscpy(lpszFindFileName, lpszFullName);
        }

        // If the father is a file, then leave
        if (ISFILE(lpFatherFC->nFileAttributes)) {
            if (NULL != lpszFindFileName) {
                MYFREE(lpszFindFileName);
            }
            return;
        }

        // Process all entries of directory
        // a) Create search pattern and start search
        if (NULL == lpszFindFileName) {
            lpszFindFileName = lpszFullName;
        }
        _tcscat(lpszFindFileName, TEXT("\\*.*"));
        hFile = FindFirstFile(lpszFindFileName, &FindData);
        if (lpszFindFileName != lpszFullName) {
            MYFREE(lpszFindFileName);
        }
    }  // End of extra local block
    if (INVALID_HANDLE_VALUE == hFile) {
#ifdef _DEBUG
        DWORD nError;
        LPTSTR lpszMessage;

        nError = GetLastError();
        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                      NULL, nError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR) &lpszMessage, 0, NULL);
        LocalFree(lpszMessage);
#endif
        return;
    }
    // b) process entry then find next
    do {
        lpszFullName = NULL;

        // Check if file is to be generic excluded
        if ((NULL == FindData.cFileName)
                || (((TCHAR)'.' == FindData.cFileName[0])  // fast exclusion for 99% of the cases
                    && ((0 == _tcscmp(FindData.cFileName, TEXT(".")))
                        || (0 == _tcscmp(FindData.cFileName, TEXT("..")))))) {
            continue;  // ignore this entry and continue with next file
        }

        // Create new file content
        // put in a separate var for later use
        lpFC = MYALLOC0(sizeof(FILECONTENT));

        // Set father of current key
        lpFC->lpFatherFC = lpFatherFC;

        // Set file name length
        lpFC->cchFileName = _tcslen(FindData.cFileName);

        // Check if file is to be excluded
        if (NULL != lprgszFileSkipStrings[0]) {  // only if there is something to exclude
            if (IsInSkipList(FindData.cFileName, lprgszFileSkipStrings)) {
                FreeAllFileContents(lpFC);
                continue;  // ignore this entry and continue with next file
            }

            lpFC->lpszFileName = FindData.cFileName;  // borrow for creating whole name
            lpszFullName = GetWholeFileName(lpFC, 4);  // +4 for "\*.*" search when directory (possible recursive call later in iteration)
            lpFC->lpszFileName = NULL;
            if (IsInSkipList(lpszFullName, lprgszFileSkipStrings)) {
                MYFREE(lpszFullName);
                FreeAllFileContents(lpFC);
                continue;  // ignore this entry and continue with next file
            }
        }

        // Write pointer to current file into caller's pointer
        if (NULL != lplpCaller) {
            *lplpCaller = lpFC;
        }

        // Copy file name
        lpFC->lpszFileName = MYALLOC((lpFC->cchFileName + 1) * sizeof(TCHAR));
        _tcscpy(lpFC->lpszFileName, FindData.cFileName);

        // Copy file data
        lpFC->nWriteDateTimeLow = FindData.ftLastWriteTime.dwLowDateTime;
        lpFC->nWriteDateTimeHigh = FindData.ftLastWriteTime.dwHighDateTime;
        lpFC->nFileSizeLow = FindData.nFileSizeLow;
        lpFC->nFileSizeHigh = FindData.nFileSizeHigh;
        lpFC->nFileAttributes = FindData.dwFileAttributes;

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

        // ATTENTION!!! FindData will be INVALID from this point on, due to recursive calls
        // If the entry is a directory, then do a recursive call for it
        // Pass this entry as father and "lpFirstSubFC" pointer for storing the first child's pointer
        if (ISDIR(lpFC->nFileAttributes)) {
            if (NULL == lpszFullName) {
                lpszFullName = GetWholeFileName(lpFC, 4);  // +4 for "\*.*" search (in recursive call)
            }
            GetFilesSnap(lpszFullName, lpFC, &lpFC->lpFirstSubFC);
        }

        if (NULL != lpszFullName) {
            MYFREE(lpszFullName);
        }

        // Set "lpBrotherFC" pointer for storing the next brother's pointer
        lplpCaller = &lpFC->lpBrotherFC;
    } while (FindNextFile(hFile, &FindData));
    FindClose(hFile);
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

                    *lplpHFPrev = lpHF;
                    lplpHFPrev = &lpHF->lpBrotherHF;

                    GetFilesSnap(lpszSubExtDir, NULL, &lpHF->lpFirstFC);
                }

                lpszSubExtDir = &lpszExtDir[i + 1];
            }
        }
    }

    // Update counters display (dirs/files final)
    nGettingTime = GetTickCount();
    UpdateCounters(asLangTexts[iszTextDir].lpszText, asLangTexts[iszTextFile].lpszText, nGettingDir, nGettingFile);
}


// ----------------------------------------------------------------------
// Clear comparison match flags of directories and files
// ----------------------------------------------------------------------
VOID ClearFileMatchFlags(LPFILECONTENT lpFC)
{
    for (; NULL != lpFC; lpFC = lpFC->lpBrotherFC) {
        lpFC->fFileMatch = NOMATCH;
        if (NULL != lpFC->lpFirstSubFC) {
            ClearFileMatchFlags(lpFC->lpFirstSubFC);
        }
    }
}

// ----------------------------------------------------------------------
// Clear comparison match flags in all head files
// ----------------------------------------------------------------------
VOID ClearHeadFileMatchFlags(LPHEADFILE lpHF)
{
    for (; NULL != lpHF; lpHF = lpHF->lpBrotherHF) {
        if (NULL != lpHF->lpFirstFC) {
            ClearFileMatchFlags(lpHF->lpFirstFC);
        }
    }
}


// ----------------------------------------------------------------------
// Save file to HIVE File
//
// This routine is called recursively to store the entries of the file/dir tree
// Therefore temporary vars are put in a local block to reduce stack usage
// ----------------------------------------------------------------------
VOID SaveFiles(LPFILECONTENT lpFC, DWORD nFPFatherFile, DWORD nFPCaller)
{
    DWORD nFPFile;

    for (; NULL != lpFC; lpFC = lpFC->lpBrotherFC) {
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

        // Copy values
        sFC.nWriteDateTimeLow = lpFC->nWriteDateTimeLow;
        sFC.nWriteDateTimeHigh = lpFC->nWriteDateTimeHigh;
        sFC.nFileSizeLow = lpFC->nFileSizeLow;
        sFC.nFileSizeHigh = lpFC->nFileSizeHigh;
        sFC.nFileAttributes = lpFC->nFileAttributes;
        sFC.nChkSum = lpFC->nChkSum;

        // Set file positions of the relatives inside the tree
#ifdef _UNICODE
        sFC.ofsFileName = 0;      // not known yet, may be defined in this call
#endif
#ifndef _UNICODE
        // File name will always be stored behind the structure, so its position is already known
        sFC.ofsFileName = nFPFile + sizeof(sFC);
#endif
        sFC.ofsFirstSubFile = 0;  // not known yet, may be re-written by another recursive call
        sFC.ofsBrotherFile = 0;   // not known yet, may be re-written in another iteration
        sFC.ofsFatherFile = nFPFatherFile;

        // New since file content version 2
        sFC.nFileNameLen = 0;

        // Determine file name length
        if (NULL != lpFC->lpszFileName) {
            sFC.nFileNameLen = (DWORD)lpFC->cchFileName;
            if (0 < sFC.nFileNameLen) {  // otherwise leave it all 0
                sFC.nFileNameLen++;  // account for NULL char
#ifdef _UNICODE
                // File name will always be stored behind the structure, so its position is already known
                sFC.ofsFileName = nFPFile + sizeof(sFC);
#endif
            }
        }

        // Write file content to file
        // Make sure that ALL fields have been initialized/set
        WriteFile(hFileWholeReg, &sFC, sizeof(sFC), &NBW, NULL);

        // Write file name to file
        if (0 < sFC.nFileNameLen) {
            WriteFile(hFileWholeReg, lpFC->lpszFileName, sFC.nFileNameLen * sizeof(TCHAR), &NBW, NULL);
#ifndef _UNICODE
        } else {
            // Write empty string for backward compatibility
            WriteFile(hFileWholeReg, lpszEmpty, 1 * sizeof(TCHAR), &NBW, NULL);
#endif
        }

        // ATTENTION!!! sFC is INVALID from this point on, due to recursive calls
        // If the entry has childs, then do a recursive call for the first child
        // Pass this entry as father and "ofsFirstSubFile" position for storing the first child's position
        if (NULL != lpFC->lpFirstSubFC) {
            SaveFiles(lpFC->lpFirstSubFC, nFPFile, nFPFile + offsetof(SAVEFILECONTENT, ofsFirstSubFile));
        }

        // Set "ofsBrotherFile" position for storing the following brother's position
        nFPCaller = nFPFile + offsetof(SAVEFILECONTENT, ofsBrotherFile);

        // Update progress bar
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
}

//--------------------------------------------------
// Save head file to HIVE file
//--------------------------------------------------
VOID SaveHeadFiles(LPHEADFILE lpHF, DWORD nFPCaller)
{
    DWORD nFPHF;

    // Write all head files and their file contents
    for (; NULL != lpHF; lpHF = lpHF->lpBrotherHF) {
        // Get current file position
        // put in a separate var for later use
        nFPHF = SetFilePointer(hFileWholeReg, 0, NULL, FILE_CURRENT);

        // Write position of current head file in caller's field
        if (0 < nFPCaller) {
            SetFilePointer(hFileWholeReg, nFPCaller, NULL, FILE_BEGIN);
            WriteFile(hFileWholeReg, &nFPHF, sizeof(nFPHF), &NBW, NULL);

            SetFilePointer(hFileWholeReg, nFPHF, NULL, FILE_BEGIN);
        }

        // Initialize head file

        // Set file positions of the relatives inside the tree
        sHF.ofsBrotherHeadFile = 0;   // not known yet, may be re-written in next iteration
        sHF.ofsFirstFileContent = 0;  // not known yet, may be re-written by SaveFiles call

        // Write head file to file
        // Make sure that ALL fields have been initialized/set
        WriteFile(hFileWholeReg, &sHF, sizeof(sHF), &NBW, NULL);

        // Write all file contents of head file
        if (NULL != lpHF->lpFirstFC) {
            SaveFiles(lpHF->lpFirstFC, 0, nFPHF + offsetof(SAVEHEADFILE, ofsFirstFileContent));
        }

        // Set "ofsBrotherHeadFile" position for storing the following brother's position
        nFPCaller = nFPHF + offsetof(SAVEHEADFILE, ofsBrotherHeadFile);
    }
}


// ----------------------------------------------------------------------
// Load file from HIVE file
// ----------------------------------------------------------------------
VOID LoadFiles(DWORD ofsFile, LPFILECONTENT lpFatherFC, LPFILECONTENT *lplpCaller)
{
    LPFILECONTENT lpFC;
    DWORD ofsBrotherFile;

    // Read all files and their sub file contents
    for (; 0 != ofsFile; ofsFile = ofsBrotherFile) {
        // Copy SAVEFILECONTENT to aligned memory block
        CopyMemory(&sFC, (lpFileBuffer + ofsFile), fileheader.nFCSize);

        // Save offset in local variable
        ofsBrotherFile = sFC.ofsBrotherFile;

        // Create new file content
        // put in a separate var for later use
        lpFC = MYALLOC0(sizeof(FILECONTENT));

        // Set father of current file
        lpFC->lpFatherFC = lpFatherFC;

        // Copy file name
        if (fileextradata.bOldFCVersion) {  // old SBCS/MBCS version
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

            lpFC->lpszFileName = MYALLOC(sFC.nFileNameLen * sizeof(TCHAR));
            if (fileextradata.bSameCharSize) {
                _tcsncpy(lpFC->lpszFileName, lpStringBuffer, sFC.nFileNameLen);
            } else {
#ifdef _UNICODE
                MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, (LPCSTR)lpStringBuffer, -1, lpFC->lpszFileName, sFC.nFileNameLen);
#else
                WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK | WC_DEFAULTCHAR, (LPCWSTR)lpStringBuffer, -1, lpFC->lpszFileName, sFC.nFileNameLen, NULL, NULL);
#endif
            }
            lpFC->lpszFileName[sFC.nFileNameLen - 1] = (TCHAR)'\0';  // safety NULL char

            // Set file name length in chars
            lpFC->cchFileName = _tcslen(lpFC->lpszFileName);
        }

        // Check if file is to be generic excluded
        if ((NULL == lpFC->lpszFileName)
                || (((TCHAR)'.' == lpFC->lpszFileName[0])  // fast exclusion for 99% of the cases
                    && ((0 == _tcscmp(lpFC->lpszFileName, TEXT(".")))
                        || (0 == _tcscmp(lpFC->lpszFileName, TEXT("..")))))) {
            FreeAllFileContents(lpFC);
            continue;  // ignore this entry and continue with next brother file
        }

        // Check if file is to be excluded
        if (NULL != lprgszFileSkipStrings[0]) {  // only if there is something to exclude
            LPTSTR lpszFullName;

            if (IsInSkipList(lpFC->lpszFileName, lprgszFileSkipStrings)) {
                FreeAllFileContents(lpFC);
                continue;  // ignore this entry and continue with next file
            }

            lpszFullName = GetWholeFileName(lpFC, 0);
            if (IsInSkipList(lpszFullName, lprgszFileSkipStrings)) {
                MYFREE(lpszFullName);
                FreeAllFileContents(lpFC);
                continue;  // ignore this entry and continue with next brother file
            }
            MYFREE(lpszFullName);
        }

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

        // ATTENTION!!! sFC will be INVALID from this point on, due to recursive calls
        // If the entry has childs, then do a recursive call for the first child
        // Pass this entry as father and "lpFirstSubFC" pointer for storing the first child's pointer
        if (0 != sFC.ofsFirstSubFile) {
            LoadFiles(sFC.ofsFirstSubFile, lpFC, &lpFC->lpFirstSubFC);
        }

        // Set "lpBrotherFC" pointer for storing the next brother's pointer
        lplpCaller = &lpFC->lpBrotherFC;
    }
}


//--------------------------------------------------
// Load head file from HIVE file
//--------------------------------------------------
VOID LoadHeadFiles(DWORD ofsHeadFile, LPHEADFILE *lplpCaller)
{
    LPHEADFILE lpHF;

    // Initialize save structures
    ZeroMemory(&sHF, sizeof(sHF));
    ZeroMemory(&sFC, sizeof(sFC));

    // Read all head files and their file contents
    for (; 0 != ofsHeadFile; ofsHeadFile = sHF.ofsBrotherHeadFile) {
        // Copy SAVEHEADFILE to aligned memory block
        CopyMemory(&sHF, (lpFileBuffer + ofsHeadFile), fileheader.nHFSize);

        // Create new head file
        // put in a separate var for later use
        lpHF = MYALLOC0(sizeof(HEADFILE));

        // Write pointer to current head file into caller's pointer
        if (NULL != lplpCaller) {
            *lplpCaller = lpHF;
        }

        // If the entry has file contents, then do a call for the first file content
        if (0 != sHF.ofsFirstFileContent) {
            LoadFiles(sHF.ofsFirstFileContent, NULL, &lpHF->lpFirstFC);
        }

        // Set "lpBrotherHF" pointer for storing the next brother's pointer
        lplpCaller = &lpHF->lpBrotherHF;
    }
}


//--------------------------------------------------
// Walkthrough head file chain and collect it's first dirname to lpszDir
//--------------------------------------------------
BOOL FindDirChain(LPHEADFILE lpHF, LPTSTR lpszDir, size_t nBufferLen)
{
    size_t      nLen;
    size_t      nWholeLen;
    BOOL        fAddSeparator;
    BOOL        fAddBackslash;

    lpszDir[0] = (TCHAR)'\0';
    nWholeLen = 0;
    for (; NULL != lpHF; lpHF = lpHF->lpBrotherHF) {
        if ((NULL != lpHF->lpFirstFC)
                && (NULL != lpHF->lpFirstFC->lpszFileName)) {
            nLen = lpHF->lpFirstFC->cchFileName;
            if (0 < nLen) {
                fAddSeparator = FALSE;
                if (0 < nWholeLen) {
                    nLen++;
                    fAddSeparator = TRUE;
                }
                fAddBackslash = FALSE;
                if ((TCHAR)':' == lpHF->lpFirstFC->lpszFileName[nLen - 1]) {
                    nLen++;
                    fAddBackslash = TRUE;
                }
                nWholeLen += nLen;
                if (nWholeLen < nBufferLen) {
                    if (fAddSeparator) {
                        _tcscat(lpszDir, TEXT(";"));
                    }
                    _tcscat(lpszDir, lpHF->lpFirstFC->lpszFileName);
                    if (fAddBackslash) {
                        _tcscat(lpszDir, TEXT("\\"));
                    }
                } else {
                    //nWholeLen -= nLen;
                    return FALSE;  // buffer too small, exit for-loop and routine
                }
            }
        }
    }
    return TRUE;  // exit for loop and routine
}


//--------------------------------------------------
// if two dir chains are the same
//--------------------------------------------------
BOOL DirChainMatch(LPHEADFILE lpHF1, LPHEADFILE lpHF2)
{
    TCHAR lpszDir1[EXTDIRLEN];
    TCHAR lpszDir2[EXTDIRLEN];

    FindDirChain(lpHF1, lpszDir1, EXTDIRLEN);  // Length in TCHARs incl. NULL char
    FindDirChain(lpHF2, lpszDir2, EXTDIRLEN);  // Length in TCHARs incl. NULL char

    if (0 != _tcsicmp(lpszDir1, lpszDir2)) {
        return FALSE;
    }

    return TRUE;
}
