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

extern LPBYTE lan_dir;
extern LPBYTE lan_file;


//-------------------------------------------------------------
// Routine to get Whole File Name[root dir] from a FILECONTENT
//-------------------------------------------------------------
LPSTR GetWholeFileName(LPFILECONTENT lpFileContent)
{
    LPFILECONTENT lpFC;
    LPSTR   lpName;  // TODO: LPTSTR
    LPSTR   lpTail;  // TODO: LPTSTR
    size_t  nLen;

    nLen = 0;
    for (lpFC = lpFileContent; NULL != lpFC; lpFC = lpFC->lpFatherFile) {
        if (NULL != lpFC->lpFileName) {
            nLen += strlen(lpFC->lpFileName) + 1;  // +1 char for backslash or NULL char  // TODO: _tcslen
        }
    }
    if (0 == nLen) {  // at least create an empty string with NULL char
        nLen++;
    }
    lpName = MYALLOC(nLen * sizeof(TCHAR));

    lpTail = lpName + nLen - 1;
    *lpTail = 0;

    for (lpFC = lpFileContent; NULL != lpFC; lpFC = lpFC->lpFatherFile) {
        if (NULL != lpFC->lpFileName) {
            nLen = strlen(lpFC->lpFileName);
            memcpy(lpTail -= nLen, lpFC->lpFileName, nLen);  // TODO: _tcsncpy
            if (lpTail > lpName) {
                *--lpTail = '\\';    // 0x5c;  // TODO: check if works for Unicode
            }
        }
    }

    return lpName;
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
    LPFILECONTENT lpFileContent
)
{
    //LPSTR   lpTemp;

    if (ISDIR(lpFileContent->fileattr)) {
        //lpTemp = lpFileContent->lpFileName;
        if ((NULL != lpFileContent->lpFileName) && (0 != strcmp(lpFileContent->lpFileName, ".")) && (0 != strcmp(lpFileContent->lpFileName, "..")))  { // tfx   added in 1.7.3 fixed at 1.8.0 we should add here 1.8.0
            //if (*(unsigned short *)lpTemp != 0x002E && !(*(unsigned short *)lpTemp == 0x2E2E && *(lpTemp + 2) == 0x00)) {     // 1.8.2
            LogToMem(typedir, lpcountdir, lpFileContent);
        }
    } else {
        LogToMem(typefile, lpcountfile, lpFileContent);
    }

    if (NULL != lpFileContent->lpFirstSubFile)    {
        GetAllSubFile(TRUE, typedir, typefile, lpcountdir, lpcountfile, lpFileContent->lpFirstSubFile);
    }
    if (TRUE == needbrother) {
        if (NULL != lpFileContent->lpBrotherFile) {
            GetAllSubFile(TRUE, typedir, typefile, lpcountdir, lpcountfile, lpFileContent->lpBrotherFile);
        }
    }
}


//------------------------------------------------------------
// File Shot Engine
//------------------------------------------------------------
VOID GetFilesSnap(LPFILECONTENT lpFatherFile)
{
    LPSTR   lpFilename;
    LPSTR   lpTemp;
    HANDLE  filehandle;
    WIN32_FIND_DATA finddata;
    LPFILECONTENT   lpFileContent;
    LPFILECONTENT   lpFileContentTemp;

    lpTemp = GetWholeFileName(lpFatherFile);
    //Not used
    //if (bWinNTDetected)
    //{
    //  lpFilename = MYALLOC(strlen(lpTemp) + 5 + 4);
    //  strcpy(lpFilename,"\\\\?\\");
    //  strcat(lpFilename,lpTemp);
    //}
    //else
    {
        lpFilename = MYALLOC(strlen(lpTemp) + 5);
        strcpy(lpFilename, lpTemp);
    }
    strcat(lpFilename, "\\*.*");

    MYFREE(lpTemp);
    //_asm int 3;
    filehandle = FindFirstFile(lpFilename, &finddata);
    MYFREE(lpFilename);
    if (filehandle == INVALID_HANDLE_VALUE) {
        return;
    }

    //lpTemp = finddata.cFileName; // 1.8

    lpFileContent = MYALLOC0(sizeof(FILECONTENT));
    lpFileContent->lpFileName = MYALLOC0(strlen(finddata.cFileName) + 1);   // must add one!
    strcpy(lpFileContent->lpFileName, finddata.cFileName);
    lpFileContent->writetimelow = finddata.ftLastWriteTime.dwLowDateTime;
    lpFileContent->writetimehigh = finddata.ftLastWriteTime.dwHighDateTime;
    lpFileContent->filesizelow = finddata.nFileSizeLow;
    lpFileContent->filesizehigh = finddata.nFileSizeHigh;
    lpFileContent->fileattr = finddata.dwFileAttributes;
    lpFileContent->lpFatherFile = lpFatherFile;
    lpFatherFile->lpFirstSubFile = lpFileContent;
    lpFileContentTemp = lpFileContent;

    if (ISDIR(lpFileContent->fileattr)) {
        if ((NULL != lpFileContent->lpFileName)
                && (0 != strcmp(lpFileContent->lpFileName, "."))
                && (0 != strcmp(lpFileContent->lpFileName, ".."))
                && !IsInSkipList(lpFileContent->lpFileName, lplpFileSkipStrings)) {  // tfx
            nGettingDir++;
            GetFilesSnap(lpFileContent);
        }
    } else {
        nGettingFile++;
    }

    for (; FindNextFile(filehandle, &finddata) != FALSE;) {
        lpFileContent = MYALLOC0(sizeof(FILECONTENT));
        lpFileContent->lpFileName = MYALLOC0(strlen(finddata.cFileName) + 1);
        strcpy(lpFileContent->lpFileName, finddata.cFileName);
        lpFileContent->writetimelow = finddata.ftLastWriteTime.dwLowDateTime;
        lpFileContent->writetimehigh = finddata.ftLastWriteTime.dwHighDateTime;
        lpFileContent->filesizelow = finddata.nFileSizeLow;
        lpFileContent->filesizehigh = finddata.nFileSizeHigh;
        lpFileContent->fileattr = finddata.dwFileAttributes;
        lpFileContent->lpFatherFile = lpFatherFile;
        lpFileContentTemp->lpBrotherFile = lpFileContent;
        lpFileContentTemp = lpFileContent;

        if (ISDIR(lpFileContent->fileattr)) {
            if ((NULL != lpFileContent->lpFileName)
                    && (0 != strcmp(lpFileContent->lpFileName, "."))
                    && (0 != strcmp(lpFileContent->lpFileName, ".."))
                    && !IsInSkipList(lpFileContent->lpFileName, lplpFileSkipStrings)) {  // tfx
                nGettingDir++;
                GetFilesSnap(lpFileContent);
            }
        } else {
            nGettingFile++;
        }

    }
    FindClose(filehandle);

    nGettingTime = GetTickCount();
    if ((nGettingTime - nBASETIME1) > REFRESHINTERVAL) {
        UpdateCounters(lan_dir, lan_file, nGettingDir, nGettingFile);
    }

    return ;
}


//-------------------------------------------------------------
// File comparison engine (lp1 and lp2 run parallel)
//-------------------------------------------------------------
VOID CompareFirstSubFile(LPFILECONTENT lpHead1, LPFILECONTENT lpHead2)
{
    LPFILECONTENT   lp1;
    LPFILECONTENT   lp2;

    for (lp1 = lpHead1; lp1 != NULL; lp1 = lp1->lpBrotherFile) {
        for (lp2 = lpHead2; lp2 != NULL; lp2 = lp2->lpBrotherFile) {
            if ((lp2->bfilematch == NOTMATCH) && strcmp(lp1->lpFileName, lp2->lpFileName) == 0) { // 1.8.2 from lstrcmp to strcmp
                // Two files have the same name, but we are not sure they are the same, so we compare them!
                if (ISFILE(lp1->fileattr) && ISFILE(lp2->fileattr))
                    //(lp1->fileattr&FILE_ATTRIBUTE_DIRECTORY) != FILE_ATTRIBUTE_DIRECTORY && (lp2->fileattr&FILE_ATTRIBUTE_DIRECTORY) != FILE_ATTRIBUTE_DIRECTORY)
                {
                    // Lp1 is file, lp2 is file
                    if (lp1->writetimelow == lp2->writetimelow && lp1->writetimehigh == lp2->writetimehigh &&
                            lp1->filesizelow == lp2->filesizelow && lp1->filesizehigh == lp2->filesizehigh && lp1->fileattr == lp2->fileattr) {
                        // We found a match file!
                        lp2->bfilematch = ISMATCH;
                    } else {
                        // We found a dismatch file, they will be logged
                        lp2->bfilematch = ISMODI;
                        LogToMem(FILEMODI, &nFILEMODI, lp1);
                    }

                } else {
                    // At least one file of the pair is directory, so we try to determine
                    if (ISDIR(lp1->fileattr) && ISDIR(lp2->fileattr))
                        // (lp1->fileattr&FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY && (lp2->fileattr&FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY)
                    {
                        // The two 'FILE's are all dirs
                        if (lp1->fileattr == lp2->fileattr) {
                            // Same dir attributes, we compare their subfiles
                            lp2->bfilematch = ISMATCH;
                            CompareFirstSubFile(lp1->lpFirstSubFile, lp2->lpFirstSubFile);
                        } else {
                            // Dir attributes changed, they will be logged
                            lp2->bfilematch = ISMODI;
                            LogToMem(DIRMODI, &nDIRMODI, lp1);
                        }
                        //break;
                    } else {
                        // One of the 'FILE's is dir, but which one?
                        if (ISFILE(lp1->fileattr) && ISDIR(lp2->fileattr))
                            //(lp1->fileattr&FILE_ATTRIBUTE_DIRECTORY) != FILE_ATTRIBUTE_DIRECTORY && (lp2->fileattr&FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY)
                        {
                            // lp1 is file, lp2 is dir
                            lp1->bfilematch = ISDEL;
                            LogToMem(FILEDEL, &nFILEDEL, lp1);
                            GetAllSubFile(FALSE, DIRADD, FILEADD, &nDIRADD, &nFILEADD, lp2);
                        } else {
                            // lp1 is dir, lp2 is file
                            lp2->bfilematch = ISADD;
                            LogToMem(FILEADD, &nFILEADD, lp2);
                            GetAllSubFile(FALSE, DIRDEL, FILEDEL, &nDIRDEL, &nFILEDEL, lp1);
                        }
                    }
                }
                break;
            }
        }

        if (lp2 == NULL) {
            // lp2 looped to the end, that is, we can not find a lp2 matches lp1, so lp1 is deleted!
            if (ISDIR(lp1->fileattr)) {
                GetAllSubFile(FALSE, DIRDEL, FILEDEL, &nDIRDEL, &nFILEDEL, lp1); // if lp1 is dir
            } else {
                LogToMem(FILEDEL, &nFILEDEL, lp1);  // if lp1 is file
            }
        }
    }

    // We loop to the end, then we do an extra loop of lp2 use flag we previous made
    for (lp2 = lpHead2; lp2 != NULL; lp2 = lp2->lpBrotherFile) {
        nComparing++;
        if (lp2->bfilematch == NOTMATCH) {
            // We did not find a lp1 matches a lp2, so lp2 is added!
            if (ISDIR(lp2->fileattr)) {
                GetAllSubFile(FALSE, DIRADD, FILEADD, &nDIRADD, &nFILEADD, lp2);
            } else {
                LogToMem(FILEADD, &nFILEADD, lp2);
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
VOID ClearFileContentMatchTag(LPFILECONTENT lpFileContent)
{
    if (NULL != lpFileContent) {
        lpFileContent->bfilematch = 0;
        ClearFileContentMatchTag(lpFileContent->lpFirstSubFile);
        ClearFileContentMatchTag(lpFileContent->lpBrotherFile);
    }
}

// ----------------------------------------------------------------------
// Clear comparison match flags in all head files
// ----------------------------------------------------------------------
VOID ClearHeadFileMatchTag(LPHEADFILE lpHeadFile)
{
    LPHEADFILE lpHFTemp;

    for (lpHFTemp = lpHeadFile; NULL != lpHFTemp; lpHFTemp = lpHFTemp->lpBrotherHeadFile) {
        ClearFileContentMatchTag(lpHFTemp->lpFirstFile);
    }
}


// ----------------------------------------------------------------------
// Free all files
// ----------------------------------------------------------------------
VOID FreeAllFileContent(LPFILECONTENT lpFileContent)
{
    if (NULL != lpFileContent) {
        if (NULL != lpFileContent->lpFileName) {
            MYFREE(lpFileContent->lpFileName);
        }
        FreeAllFileContent(lpFileContent->lpFirstSubFile);
        FreeAllFileContent(lpFileContent->lpBrotherFile);
        MYFREE(lpFileContent);
    }
}

// ----------------------------------------------------------------------
// Free all head files
// ----------------------------------------------------------------------
VOID FreeAllFileHead(LPHEADFILE lpHeadFile)
{
    if (NULL != lpHeadFile) {
        FreeAllFileContent(lpHeadFile->lpFirstFile);
        FreeAllFileHead(lpHeadFile->lpBrotherHeadFile);
        MYFREE(lpHeadFile);
    }
}


// ----------------------------------------------------------------------
// Save file to HIVE File
//
// This routine is called recursively to store the entries of the file/dir tree
// Therefore temporary vars are put in a local block to reduce stack usage
// ----------------------------------------------------------------------
VOID SaveFileContent(LPFILECONTENT lpFileContent, DWORD nFPFatherFile, DWORD nFPCaller)
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
    sFC.writetimelow = lpFileContent->writetimelow;
    sFC.writetimehigh = lpFileContent->writetimehigh;
    sFC.filesizelow = lpFileContent->filesizelow;
    sFC.filesizehigh = lpFileContent->filesizehigh;
    sFC.fileattr = lpFileContent->fileattr;
    sFC.chksum = lpFileContent->chksum;

    // Set file positions of the relatives inside the tree
    sFC.ofsFileName = 0;      // not known yet, may be re-written in this call
    sFC.ofsFirstSubFile = 0;  // not known yet, may be re-written by another recursive call
    sFC.ofsBrotherFile = 0;   // not known yet, may be re-written by another recursive call
    sFC.ofsFatherFile = nFPFatherFile;

    // New since file content version 2
    sFC.nFileNameLen = 0;
    if (NULL != lpFileContent->lpFileName) {
        sFC.nFileNameLen = (DWORD)_tcslen(lpFileContent->lpFileName);
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
    if (NULL != lpFileContent->lpFileName) {
        WriteFile(hFileWholeReg, lpFileContent->lpFileName, sFC.nFileNameLen * sizeof(TCHAR), &NBW, NULL);
#ifndef _UNICODE
    } else {
        // Write empty string for backward compatibility
        WriteFile(hFileWholeReg, szEmpty, sFC.nFileNameLen * sizeof(TCHAR), &NBW, NULL);
#endif
    }

    // ATTENTION!!! sFC is INVALID from this point on, due to recursive calls

    // If the entry has childs, then do a recursive call for the first child
    // Pass this entry as father and "ofsFirstSubFile" position for storing the first child's position
    if (NULL != lpFileContent->lpFirstSubFile) {
        SaveFileContent(lpFileContent->lpFirstSubFile, nFPFile, nFPFile + offsetof(SAVEFILECONTENT, ofsFirstSubFile));
    }

    // If the entry has a following brother, then do a recursive call for the following brother
    // Pass father as father and "ofsBrotherFile" position for storing the next brother's position
    if (NULL != lpFileContent->lpBrotherFile) {
        SaveFileContent(lpFileContent->lpBrotherFile, nFPFatherFile, nFPFile + offsetof(SAVEFILECONTENT, ofsBrotherFile));
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
VOID SaveHeadFile(LPHEADFILE lpHF, DWORD nFPCaller)
{
    LPHEADFILE lpHFTemp;
    DWORD nFPHF;

    // Write all head files and their file contents
    for (lpHFTemp = lpHF; NULL != lpHFTemp; lpHFTemp = lpHFTemp->lpBrotherHeadFile) {
        nFPHF = SetFilePointer(hFileWholeReg, 0, NULL, FILE_CURRENT);

        // Write position of current head file in caller's field
        if (0 < nFPCaller) {
            SetFilePointer(hFileWholeReg, nFPCaller, NULL, FILE_BEGIN);
            WriteFile(hFileWholeReg, &nFPHF, sizeof(nFPHF), &NBW, NULL);

            SetFilePointer(hFileWholeReg, nFPHF, NULL, FILE_BEGIN);
        }

        // Initialize head file
        ZeroMemory(&sHF, sizeof(sHF));

        // Write head file to file
        // Make sure that ALL fields have been initialized/set
        WriteFile(hFileWholeReg, &sHF, sizeof(sHF), &NBW, NULL);

        // Write all file contents of head file
        if (NULL != lpHFTemp->lpFirstFile) {
            SaveFileContent(lpHFTemp->lpFirstFile, 0, nFPHF + offsetof(SAVEHEADFILE, ofsFirstFileContent));
        }

        nFPCaller = nFPHF + offsetof(SAVEHEADFILE, ofsBrotherHeadFile);
    }
}


// ----------------------------------------------------------------------
// Load file from HIVE file
// ----------------------------------------------------------------------
VOID LoadFile(DWORD ofsFileContent, LPFILECONTENT lpFatherFile, LPFILECONTENT *lplpCaller)
{
    LPFILECONTENT lpFile;
    DWORD ofsFirstSubFile;
    DWORD ofsBrotherFile;

    // Copy SAVEFILECONTENT to aligned memory block
    ZeroMemory(&sFC, sizeof(sFC));
    CopyMemory(&sFC, (lpFileBuffer + ofsFileContent), fileheader.nFCSize);

    // Create new file content
    // put in a separate var for later use
    lpFile = MYALLOC0(sizeof(FILECONTENT));
    ZeroMemory(lpFile, sizeof(FILECONTENT));

    // Write pointer to current file into caller's pointer
    if (NULL != lplpCaller) {
        *lplpCaller = lpFile;
    }

    // Set father of current file
    lpFile->lpFatherFile = lpFatherFile;

    // Copy values
    lpFile->writetimelow = sFC.writetimelow;
    lpFile->writetimehigh = sFC.writetimehigh;
    lpFile->filesizelow = sFC.filesizelow;
    lpFile->filesizehigh = sFC.filesizehigh;
    lpFile->fileattr = sFC.fileattr;
    lpFile->chksum = sFC.chksum;

    // Copy file name
    if (FILECONTENT_VERSION_2 > fileheader.nFCVersion) {
        sFC.nFileNameLen = (DWORD)strlen((const char *)(lpFileBuffer + sFC.ofsFileName));
        if (0 < sFC.nFileNameLen) {
            sFC.nFileNameLen++;  // account for NULL char
        }
    }
    if (0 < sFC.nFileNameLen) {  // otherwise leave it NULL
        // Copy string to an aligned memory block
        nSourceSize = sFC.nFileNameLen * fileheader.nCharSize;
        nBufferSize = AdjustBuffer(&lpszBuffer, nBufferSize, nSourceSize, REGSHOT_STRING_BUFFER_BYTES);
        ZeroMemory(lpszBuffer, nBufferSize);
        CopyMemory(lpszBuffer, (lpFileBuffer + sFC.ofsFileName), nSourceSize);

        lpFile->lpFileName = MYALLOC0(sFC.nFileNameLen * sizeof(TCHAR));
        if (sizeof(TCHAR) == fileheader.nCharSize) {
            _tcscpy(lpFile->lpFileName, (LPTSTR)lpszBuffer);
        } else {
#ifdef _UNICODE
            MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, (LPCSTR)lpszBuffer, -1, lpFile->lpFileName, sFC.nFileNameLen);
#else
            WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK | WC_DEFAULTCHAR, (LPCWSTR)lpszBuffer, -1, lpFile->lpFileName, sFC.nFileNameLen, NULL, NULL);
#endif
        }
    }

    ofsFirstSubFile = sFC.ofsFirstSubFile;
    ofsBrotherFile = sFC.ofsBrotherFile;

    if (ISDIR(lpFile->fileattr)) {
        nGettingDir++;
    } else {
        nGettingFile++;
    }

    nGettingTime = GetTickCount();
    if ((nGettingTime - nBASETIME1) > REFRESHINTERVAL) {
        UpdateCounters(lan_dir, lan_file, nGettingDir, nGettingFile);
    }

    // ATTENTION!!! sFC is INVALID from this point on, due to recursive calls

    // If the entry has childs, then do a recursive call for the first child
    // Pass this entry as father and "lpFirstSubFile" pointer for storing the first child's pointer
    if (0 != ofsFirstSubFile) {
        LoadFile(ofsFirstSubFile, lpFile, &lpFile->lpFirstSubFile);
    }

    // If the entry has a following brother, then do a recursive call for the following brother
    // Pass father as father and "lpBrotherFile" pointer for storing the next brother's pointer
    if (0 != ofsBrotherFile) {
        LoadFile(ofsBrotherFile, lpFatherFile, &lpFile->lpBrotherFile);
    }
}

//--------------------------------------------------
// Load head file from HIVE file
//--------------------------------------------------
VOID LoadHeadFile(DWORD ofsHeadFile, LPHEADFILE *lplpCaller)
{
    LPHEADFILE lpHF;

    // Read all head files and their file contents
    for (; 0 != ofsHeadFile; ofsHeadFile = sHF.ofsBrotherHeadFile) {
        // Copy SAVEHEADFILE to aligned memory block
        ZeroMemory(&sHF, sizeof(sHF));
        CopyMemory(&sHF, (lpFileBuffer + ofsHeadFile), fileheader.nHFSize);

        // Create new head file
        // put in a separate var for later use
        lpHF = MYALLOC0(sizeof(SAVEHEADFILE));
        ZeroMemory(lpHF, sizeof(SAVEHEADFILE));

        // Write pointer to current head file into caller's pointer
        if (NULL != lplpCaller) {
            *lplpCaller = lpHF;
        }

        // If the entry has file contents, then do a call for the first file content
        if (0 != sHF.ofsFirstFileContent) {
            LoadFile(sHF.ofsFirstFileContent, NULL, &lpHF->lpFirstFile);
        }

        lplpCaller = &lpHF->lpBrotherHeadFile;
    }
}


//--------------------------------------------------
// Walkthrough lpHF and find lpname matches
//--------------------------------------------------
LPFILECONTENT SearchDirChain(LPSTR lpName, LPHEADFILE lpHF)
{
    LPHEADFILE lpHFTemp;

    if (NULL != lpName) {
        for (lpHFTemp = lpHF; NULL != lpHFTemp; lpHFTemp = lpHFTemp->lpBrotherHeadFile) {
            if ((NULL != lpHFTemp->lpFirstFile)
                    && (NULL != lpHFTemp->lpFirstFile->lpFileName)
                    && (0 == _stricmp(lpName, lpHFTemp->lpFirstFile->lpFileName))) {
                return lpHFTemp->lpFirstFile;
            }
        }
    }
    return NULL;
}

//--------------------------------------------------
// Walkthrough lpFILES chain and collect it's first dirname to lpDir
//--------------------------------------------------
VOID FindDirChain(LPHEADFILE lpHF, LPSTR lpDir, size_t nMaxLen)
{
    LPHEADFILE  lpHFTemp;
    size_t      nLen;

    *lpDir = 0;
    nLen = 0;
    for (lpHFTemp = lpHF; NULL != lpHFTemp; lpHFTemp = lpHFTemp->lpBrotherHeadFile) {
        if ((NULL != lpHFTemp->lpFirstFile)
                && (NULL != lpHFTemp->lpFirstFile->lpFileName)
                && (nLen < nMaxLen)) {
            nLen += strlen(lpHFTemp->lpFirstFile->lpFileName) + 1;
            strcat(lpDir, lpHFTemp->lpFirstFile->lpFileName);
            strcat(lpDir, TEXT(";"));
        }
    }
}


//--------------------------------------------------
// if two dir chains are the same
//--------------------------------------------------
BOOL DirChainMatch(LPHEADFILE lphf1, LPHEADFILE lphf2)
{
    char lpDir1[EXTDIRLEN + 4];
    char lpDir2[EXTDIRLEN + 4];
    ZeroMemory(lpDir1, sizeof(lpDir1));
    ZeroMemory(lpDir2, sizeof(lpDir2));
    FindDirChain(lphf1, lpDir1, EXTDIRLEN);
    FindDirChain(lphf2, lpDir2, EXTDIRLEN);

    if (0 != _stricmp(lpDir1, lpDir2)) {
        return FALSE;
    } else {
        return TRUE;
    }
}
