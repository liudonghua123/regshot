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


//-------------------------------------------------------------
// Get whole file name [root dir] from FILECONTENT
//-------------------------------------------------------------
LPTSTR GetWholeFileName(LPFILECONTENT lpStartFC)
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

    lpszName = MYALLOC(cchName * sizeof(TCHAR));

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

    if (ISDIR(lpFC->fileattr)) {
        //lpTemp = lpFC->lpFileName;
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


//------------------------------------------------------------
// File Shot Engine
//------------------------------------------------------------
VOID GetFilesSnap(LPFILECONTENT lpFatherFC)
{
    LPTSTR   lpFilename;
    LPTSTR   lpTemp;
    HANDLE  filehandle;
    WIN32_FIND_DATA finddata;
    LPFILECONTENT   lpFC;
    LPFILECONTENT   lpFCTemp;

    lpTemp = GetWholeFileName(lpFatherFC);
    //Not used
    //if (bWinNTDetected)
    //{
    //  lpFilename = MYALLOC(_tcslen(lpTemp) + 5 + 4);
    //  _tcscpy(lpFilename,"\\\\?\\");
    //  _tcscat(lpFilename,lpTemp);
    //}
    //else
    {
        lpFilename = MYALLOC(_tcslen(lpTemp) + 5);
        _tcscpy(lpFilename, lpTemp);
    }
    _tcscat(lpFilename, TEXT("\\*.*"));

    MYFREE(lpTemp);
    //_asm int 3;
    filehandle = FindFirstFile(lpFilename, &finddata);
    MYFREE(lpFilename);
    if (filehandle == INVALID_HANDLE_VALUE) {
        return;
    }

    //lpTemp = finddata.cFileName; // 1.8

    lpFC = MYALLOC0(sizeof(FILECONTENT));
    lpFC->lpszFileName = MYALLOC0(_tcslen(finddata.cFileName) + 1);   // must add one!
    _tcscpy(lpFC->lpszFileName, finddata.cFileName);
    lpFC->writetimelow = finddata.ftLastWriteTime.dwLowDateTime;
    lpFC->writetimehigh = finddata.ftLastWriteTime.dwHighDateTime;
    lpFC->filesizelow = finddata.nFileSizeLow;
    lpFC->filesizehigh = finddata.nFileSizeHigh;
    lpFC->fileattr = finddata.dwFileAttributes;
    lpFC->lpFatherFC = lpFatherFC;
    lpFatherFC->lpFirstSubFC = lpFC;
    lpFCTemp = lpFC;

    if (ISDIR(lpFC->fileattr)) {
        if ((NULL != lpFC->lpszFileName)
                && (0 != _tcscmp(lpFC->lpszFileName, TEXT(".")))
                && (0 != _tcscmp(lpFC->lpszFileName, TEXT("..")))
                && !IsInSkipList(lpFC->lpszFileName, lprgszFileSkipStrings)) {  // tfx
            nGettingDir++;
            GetFilesSnap(lpFC);
        }
    } else {
        nGettingFile++;
    }

    for (; FindNextFile(filehandle, &finddata) != FALSE;) {
        lpFC = MYALLOC0(sizeof(FILECONTENT));
        lpFC->lpszFileName = MYALLOC0(_tcslen(finddata.cFileName) + 1);
        _tcscpy(lpFC->lpszFileName, finddata.cFileName);
        lpFC->writetimelow = finddata.ftLastWriteTime.dwLowDateTime;
        lpFC->writetimehigh = finddata.ftLastWriteTime.dwHighDateTime;
        lpFC->filesizelow = finddata.nFileSizeLow;
        lpFC->filesizehigh = finddata.nFileSizeHigh;
        lpFC->fileattr = finddata.dwFileAttributes;
        lpFC->lpFatherFC = lpFatherFC;
        lpFCTemp->lpBrotherFC = lpFC;
        lpFCTemp = lpFC;

        if (ISDIR(lpFC->fileattr)) {
            if ((NULL != lpFC->lpszFileName)
                    && (0 != _tcscmp(lpFC->lpszFileName, TEXT(".")))
                    && (0 != _tcscmp(lpFC->lpszFileName, TEXT("..")))
                    && !IsInSkipList(lpFC->lpszFileName, lprgszFileSkipStrings)) {  // tfx
                nGettingDir++;
                GetFilesSnap(lpFC);
            }
        } else {
            nGettingFile++;
        }

    }
    FindClose(filehandle);

    nGettingTime = GetTickCount();
    if (REFRESHINTERVAL < (nGettingTime - nBASETIME1)) {
        UpdateCounters(asLangTexts[iszTextDir].lpszText, asLangTexts[iszTextFile].lpszText, nGettingDir, nGettingFile);
    }

    return ;
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
            if ((lpFC2->bfilematch == NOTMATCH) && _tcscmp(lpFC1->lpszFileName, lpFC2->lpszFileName) == 0) { // 1.8.2 from lstrcmp to strcmp
                // Two files have the same name, but we are not sure they are the same, so we compare them!
                if (ISFILE(lpFC1->fileattr) && ISFILE(lpFC2->fileattr))
                    //(lpFC1->fileattr&FILE_ATTRIBUTE_DIRECTORY) != FILE_ATTRIBUTE_DIRECTORY && (lpFC2->fileattr&FILE_ATTRIBUTE_DIRECTORY) != FILE_ATTRIBUTE_DIRECTORY)
                {
                    // Lp1 is file, lpFC2 is file
                    if (lpFC1->writetimelow == lpFC2->writetimelow && lpFC1->writetimehigh == lpFC2->writetimehigh &&
                            lpFC1->filesizelow == lpFC2->filesizelow && lpFC1->filesizehigh == lpFC2->filesizehigh && lpFC1->fileattr == lpFC2->fileattr) {
                        // We found a match file!
                        lpFC2->bfilematch = ISMATCH;
                    } else {
                        // We found a dismatch file, they will be logged
                        lpFC2->bfilematch = ISMODI;
                        LogToMem(FILEMODI, &nFILEMODI, lpFC1);
                    }

                } else {
                    // At least one file of the pair is directory, so we try to determine
                    if (ISDIR(lpFC1->fileattr) && ISDIR(lpFC2->fileattr))
                        // (lpFC1->fileattr&FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY && (lpFC2->fileattr&FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY)
                    {
                        // The two 'FILE's are all dirs
                        if (lpFC1->fileattr == lpFC2->fileattr) {
                            // Same dir attributes, we compare their subfiles
                            lpFC2->bfilematch = ISMATCH;
                            CompareFirstSubFile(lpFC1->lpFirstSubFC, lpFC2->lpFirstSubFC);
                        } else {
                            // Dir attributes changed, they will be logged
                            lpFC2->bfilematch = ISMODI;
                            LogToMem(DIRMODI, &nDIRMODI, lpFC1);
                        }
                        //break;
                    } else {
                        // One of the 'FILE's is dir, but which one?
                        if (ISFILE(lpFC1->fileattr) && ISDIR(lpFC2->fileattr))
                            //(lpFC1->fileattr&FILE_ATTRIBUTE_DIRECTORY) != FILE_ATTRIBUTE_DIRECTORY && (lpFC2->fileattr&FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY)
                        {
                            // lpFC1 is file, lpFC2 is dir
                            lpFC1->bfilematch = ISDEL;
                            LogToMem(FILEDEL, &nFILEDEL, lpFC1);
                            GetAllSubFile(FALSE, DIRADD, FILEADD, &nDIRADD, &nFILEADD, lpFC2);
                        } else {
                            // lpFC1 is dir, lpFC2 is file
                            lpFC2->bfilematch = ISADD;
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
            if (ISDIR(lpFC1->fileattr)) {
                GetAllSubFile(FALSE, DIRDEL, FILEDEL, &nDIRDEL, &nFILEDEL, lpFC1); // if lpFC1 is dir
            } else {
                LogToMem(FILEDEL, &nFILEDEL, lpFC1);  // if lpFC1 is file
            }
        }
    }

    // We loop to the end, then we do an extra loop of lpFC2 use flag we previous made
    for (lpFC2 = lpFCHead2; lpFC2 != NULL; lpFC2 = lpFC2->lpBrotherFC) {
        nComparing++;
        if (lpFC2->bfilematch == NOTMATCH) {
            // We did not find a lpFC1 matches a lpFC2, so lpFC2 is added!
            if (ISDIR(lpFC2->fileattr)) {
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
            SendDlgItemMessage(hWnd, IDC_PBCOMPARE, PBM_STEPIT, (WPARAM)0, (LPARAM)0);
        }
}


// ----------------------------------------------------------------------
// Clear comparison match flags in all files
// ----------------------------------------------------------------------
VOID ClearFileContentMatchTag(LPFILECONTENT lpFC)
{
    if (NULL != lpFC) {
        lpFC->bfilematch = 0;
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
    sFC.writetimelow = lpFC->writetimelow;
    sFC.writetimehigh = lpFC->writetimehigh;
    sFC.filesizelow = lpFC->filesizelow;
    sFC.filesizehigh = lpFC->filesizehigh;
    sFC.fileattr = lpFC->fileattr;
    sFC.chksum = lpFC->chksum;

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
            SendDlgItemMessage(hWnd, IDC_PBCOMPARE, PBM_STEPIT, (WPARAM)0, (LPARAM)0);
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

    // Copy SAVEFILECONTENT to aligned memory block
    ZeroMemory(&sFC, sizeof(sFC));
    CopyMemory(&sFC, (lpFileBuffer + ofsFileContent), fileheader.nFCSize);

    // Create new file content
    // put in a separate var for later use
    lpFC = MYALLOC0(sizeof(FILECONTENT));
    ZeroMemory(lpFC, sizeof(FILECONTENT));

    // Write pointer to current file into caller's pointer
    if (NULL != lplpCaller) {
        *lplpCaller = lpFC;
    }

    // Set father of current file
    lpFC->lpFatherFC = lpFatherFC;

    // Copy values
    lpFC->writetimelow = sFC.writetimelow;
    lpFC->writetimehigh = sFC.writetimehigh;
    lpFC->filesizelow = sFC.filesizelow;
    lpFC->filesizehigh = sFC.filesizehigh;
    lpFC->fileattr = sFC.fileattr;
    lpFC->chksum = sFC.chksum;

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

    ofsFirstSubFile = sFC.ofsFirstSubFile;
    ofsBrotherFile = sFC.ofsBrotherFile;

    if (ISDIR(lpFC->fileattr)) {
        nGettingDir++;
    } else {
        nGettingFile++;
    }

    nGettingTime = GetTickCount();
    if (REFRESHINTERVAL < (nGettingTime - nBASETIME1)) {
        UpdateCounters(asLangTexts[iszTextDir].lpszText, asLangTexts[iszTextFile].lpszText, nGettingDir, nGettingFile);
    }

    // ATTENTION!!! sFC is INVALID from this point on, due to recursive calls

    // If the entry has childs, then do a recursive call for the first child
    // Pass this entry as father and "lpFirstSubFC" pointer for storing the first child's pointer
    if (0 != ofsFirstSubFile) {
        LoadFile(ofsFirstSubFile, lpFC, &lpFC->lpFirstSubFC);
    }

    // If the entry has a following brother, then do a recursive call for the following brother
    // Pass father as father and "lpBrotherFC" pointer for storing the next brother's pointer
    if (0 != ofsBrotherFile) {
        LoadFile(ofsBrotherFile, lpFatherFC, &lpFC->lpBrotherFC);
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

    lpszDir[0] = (TCHAR)'\0';
    nWholeLen = 0;
    for (lpHF = lpStartHF; NULL != lpHF; lpHF = lpHF->lpBrotherHF) {
        if ((NULL != lpHF->lpFirstFC)
                && (NULL != lpHF->lpFirstFC->lpszFileName)) {
            nLen = _tcslen(lpHF->lpFirstFC->lpszFileName);
            if (nLen > 0) {
                if (nWholeLen > 0) {
                    nLen++;  // account for semicolon
                }
                if ((nWholeLen + nLen) < nBufferLen) {
                    if (nWholeLen > 0) {
                        _tcscat(lpszDir, TEXT(";"));
                    }
                    nWholeLen += nLen;
                    _tcscat(lpszDir, lpHF->lpFirstFC->lpszFileName);
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

    FindDirChain(lpHF1, lpszDir1, EXTDIRLEN);	// Length in TCHARs incl. NULL char
    FindDirChain(lpHF2, lpszDir2, EXTDIRLEN);	// Length in TCHARs incl. NULL char

    if (0 != _tcsicmp(lpszDir1, lpszDir2)) {
        return FALSE;
    } else {
        return TRUE;
    }
}
