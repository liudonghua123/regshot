/*
    Copyright 1999-2003,2007,2011 TiANWEi
    Copyright 2004 tulipfan

    This file is part of Regshot.

    Regshot is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Regshot is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Regshot; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "global.h"
// ISDIR, ISFILE added in 1.8.0
#define ISDIR(x)  ( (x&FILE_ATTRIBUTE_DIRECTORY) != 0 )
#define ISFILE(x) ( (x&FILE_ATTRIBUTE_DIRECTORY) == 0 )
extern LPBYTE lan_dir;
extern LPBYTE lan_file;


//-------------------------------------------------------------
// Routine to get Whole File Name[root dir] from a FILECONTENT
//-------------------------------------------------------------
LPSTR GetWholeFileName(LPFILECONTENT lpFileContent)
{
    LPFILECONTENT lpf;
    LPSTR   lpName;
    LPSTR   lptail;
    size_t  nLen = 0;

    for (lpf = lpFileContent; lpf != NULL; lpf = lpf->lpfatherfile) {
        nLen += strlen(lpf->lpfilename) + 1;
    }
    if (nLen == 0) {
        nLen++;
    }
    lpName = MYALLOC(nLen);

    lptail = lpName + nLen - 1;
    *lptail = 0x00;

    for (lpf = lpFileContent; lpf != NULL; lpf = lpf->lpfatherfile) {
        nLen = strlen(lpf->lpfilename);
        memcpy(lptail -= nLen, lpf->lpfilename, nLen);
        if (lptail > lpName) {
            *--lptail = '\\';    // 0x5c;
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
    LPSTR   lpTemp;

    if (ISDIR(lpFileContent->fileattr)) {
        lpTemp = lpFileContent->lpfilename;
        //if (strcmp(lpFileContent->lpfilename,".") != 0  && strcmp(lpFileContent->lpfilename,"..") != 0) // we should add here 1.8.0
        if (*(unsigned short *)lpTemp != 0x002E && !(*(unsigned short *)lpTemp == 0x2E2E && *(lpTemp + 2) == 0x00)) {     // 1.8.2
            LogToMem(typedir, lpcountdir, lpFileContent);
        }
    } else {
        LogToMem(typefile, lpcountfile, lpFileContent);
    }

    if (lpFileContent->lpfirstsubfile != NULL)     // tfx 避免目录扫描时产生"."和".." added in 1.7.3 fixed at 1.8.0
        //&& strcmp(lpFileContent->lpfirstsubfile->lpfilename,".") != 0    // But, this break the chain! :(. because we store . and .. in scan filecontent!
        //&& strcmp(lpFileContent->lpfirstsubfile->lpfilename,"..") != 0)  // So we should move these two lines above
    {
        GetAllSubFile(TRUE, typedir, typefile, lpcountdir, lpcountfile, lpFileContent->lpfirstsubfile);
    }
    if (needbrother == TRUE) {
        if (lpFileContent->lpbrotherfile != NULL) {
            GetAllSubFile(TRUE, typedir, typefile, lpcountdir, lpcountfile, lpFileContent->lpbrotherfile);
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

    lpTemp = finddata.cFileName; // 1.8

    lpFileContent = MYALLOC0(sizeof(FILECONTENT));
    lpFileContent->lpfilename = MYALLOC0(strlen(finddata.cFileName) + 1);   // must add one!
    strcpy(lpFileContent->lpfilename, finddata.cFileName);
    lpFileContent->writetimelow = finddata.ftLastWriteTime.dwLowDateTime;
    lpFileContent->writetimehigh = finddata.ftLastWriteTime.dwHighDateTime;
    lpFileContent->filesizelow = finddata.nFileSizeLow;
    lpFileContent->filesizehigh = finddata.nFileSizeHigh;
    lpFileContent->fileattr = finddata.dwFileAttributes;
    lpFileContent->lpfatherfile = lpFatherFile;
    lpFatherFile->lpfirstsubfile = lpFileContent;
    lpFileContentTemp = lpFileContent;

    if (ISDIR(lpFileContent->fileattr)) {
        /*
        if (*(unsigned short *)lpTemp != 0x002E && !( *(unsigned short *)lpTemp == 0x2E2E && *(lpTemp + 2) == 0x00 )   // 1.8.2
        00401292   66:8B85 E4FEFFFF MOV AX,WORD PTR SS:[EBP-11C]
        00401299   66:3D 2E00       CMP AX,2E
        0040129D   74 43            JE SHORT regshot.004012E2
        0040129F   66:3D 2E2E       CMP AX,2E2E
        004012A3   75 0A            JNZ SHORT regshot.004012AF
        004012A5   8A85 E6FEFFFF    MOV AL,BYTE PTR SS:[EBP-11A]
        004012AB   84C0             TEST AL,AL
        004012AD   74 33            JE SHORT regshot.004012E2
        // 存在风险,因为如果filename的长度不一定,内存分配不够长度的话代码是会出问题的! 而且从LoadHive过来的也不一定长度.从汇编代码来看还是比较安全的. 我要好好考虑考虑再用.

        */
        //if (lstrcmp(lpFileContent->lpfilename,".") != 0 && lstrcmp(lpFileContent->lpfilename,"..") != 0 // or we can use that
        //use lpTemp may "optimize" some, note, finddata.cfilename should exist!
        //if (!(*lpTemp == '.' && *(lpTemp + 1) == 0x00 ) && !(*lpTemp == '.' && *(lpTemp + 1) == '.' && *(lpTemp + 2) == 0x00 ) // 1.8.0
        //risk! see above.
        if (*(unsigned short *)lpTemp != 0x002E && !(*(unsigned short *)lpTemp == 0x2E2E && *(lpTemp + 2) == 0x00)     // 1.8.2
                && !IsInSkipList(lpFileContent->lpfilename, lplpFileSkipStrings)) { // tfx

            nGettingDir++;
            GetFilesSnap(lpFileContent);
        }
    } else {
        nGettingFile++;
    }


    for (; FindNextFile(filehandle, &finddata) != FALSE;) {
        lpFileContent = MYALLOC0(sizeof(FILECONTENT));
        lpFileContent->lpfilename = MYALLOC0(strlen(finddata.cFileName) + 1);
        strcpy(lpFileContent->lpfilename, finddata.cFileName);
        lpFileContent->writetimelow = finddata.ftLastWriteTime.dwLowDateTime;
        lpFileContent->writetimehigh = finddata.ftLastWriteTime.dwHighDateTime;
        lpFileContent->filesizelow = finddata.nFileSizeLow;
        lpFileContent->filesizehigh = finddata.nFileSizeHigh;
        lpFileContent->fileattr = finddata.dwFileAttributes;
        lpFileContent->lpfatherfile = lpFatherFile;
        lpFileContentTemp->lpbrotherfile = lpFileContent;
        lpFileContentTemp = lpFileContent;

        if (ISDIR(lpFileContent->fileattr)) {
            //if ( !(*lpTemp == '.' && *(lpTemp + 1) == 0x00 ) && !(*lpTemp == '.' && *(lpTemp + 1) == '.' && *(lpTemp + 2) == 0x00 ) // if (lstrcmp(lpFileContent->lpfilename,".") != 0 && lstrcmp(lpFileContent->lpfilename,"..") != 0
            if (*(unsigned short *)lpTemp != 0x002E && !(*(unsigned short *)lpTemp == 0x2E2E && *(lpTemp + 2) == 0x00)    // 1.8.2
                    && !IsInSkipList(lpFileContent->lpfilename, lplpFileSkipStrings)) { // tfx
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

    for (lp1 = lpHead1; lp1 != NULL; lp1 = lp1->lpbrotherfile) {
        for (lp2 = lpHead2; lp2 != NULL; lp2 = lp2->lpbrotherfile) {
            if ((lp2->bfilematch == NOTMATCH) && strcmp(lp1->lpfilename, lp2->lpfilename) == 0) { // 1.8.2 from lstrcmp to strcmp
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
                            CompareFirstSubFile(lp1->lpfirstsubfile, lp2->lpfirstsubfile);
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
    for (lp2 = lpHead2; lp2 != NULL; lp2 = lp2->lpbrotherfile) {
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


//-------------------------------------------------------------
// Routines to free all file tree
//-------------------------------------------------------------
VOID FreeAllFileContent(LPFILECONTENT lpFile)
{
    if (lpFile != NULL) {
        FreeAllFileContent(lpFile->lpfirstsubfile);
        FreeAllFileContent(lpFile->lpbrotherfile);
        MYFREE(lpFile->lpfilename);
        MYFREE(lpFile);
    }
}


//-------------------------------------------------------------
// Routines to free head files
//-------------------------------------------------------------
VOID FreeAllFileHead(LPHEADFILE lp)
{
    if (lp != NULL) {
        FreeAllFileHead(lp->lpnextheadfile);
        FreeAllFileContent(lp->lpfilecontent);
        //FreeAllFileContent(lp->lpfilecontent2);
        MYFREE(lp);
    }

}


/*VOID FreeAllFiles(void)
{
    FreeAllFileHead(lpHeadFile);
    lpHeadFile = NULL;
}*/


//--------------------------------------------------
// File save engine (It is stupid again!) added in 1.8
// 1.8.3 changed some struct
// modi 20111216
//--------------------------------------------------
VOID SaveFileContent(LPFILECONTENT lpFileContent, size_t nFPCurrentFatherFile, DWORD nFPCaller)
{

    size_t  nFPTemp4Write;
    DWORD   nFPHeader;
    DWORD   nFPCurrent;
    DWORD   nLenPlus1;
    int     nPad;
    FILECONTENT sfc;

    nLenPlus1 = (DWORD)strlen(lpFileContent->lpfilename) + 1;                       // Get len+1
    nFPHeader = SetFilePointer(hFileWholeReg, 0, NULL, FILE_CURRENT);               // Save head fp

    sfc.lpfilename = (LPSTR)(nFPHeader + sizeof(FILECONTENT));                      // 1.8.3 11*4 former is 10*4+1
    sfc.writetimelow = lpFileContent->writetimelow;
    sfc.writetimehigh = lpFileContent->writetimehigh;
    sfc.filesizelow = lpFileContent->filesizelow;
    sfc.filesizehigh = lpFileContent->filesizehigh;
    sfc.fileattr = lpFileContent->fileattr;
    sfc.cksum = lpFileContent->cksum;
    sfc.lpfirstsubfile = lpFileContent->lpfirstsubfile;
    sfc.lpbrotherfile = lpFileContent->lpbrotherfile;
    sfc.lpfatherfile = (LPFILECONTENT)nFPCurrentFatherFile;
    sfc.bfilematch = 0;
    WriteFile(hFileWholeReg, &sfc, sizeof(sfc), &NBW, NULL);

/*
    nFPTemp4Write = nFPHeader + sizeof(FILECONTENT);                                            // 1.8.3 11*4 former is 10*4+1
    WriteFile(hFileWholeReg, &nFPTemp4Write, sizeof(nFPTemp4Write), &NBW, NULL);                // Save the location of lpfilename

    WriteFile(hFileWholeReg, (LPBYTE)lpFileContent + sizeof(LPSTR), 6*sizeof(DWORD), &NBW, NULL);   // Write time, size etc. 6*4

    //nFPTemp4Write = (lpFileContent->lpfirstsubfile!=NULL) ? (nFPHeader + 41 + nLenPlus1) : 0;     // We write lpfilename plus a "\0"
    //WriteFile(hFileWholeReg,&nFPTemp4Write,4,&NBW,NULL);                  // Save the location of lpfirstsubfile
    WriteFile(hFileWholeReg, (LPBYTE)lpFileContent + sizeof(LPSTR)+6*sizeof(DWORD), sizeof(LPFILECONTENT)*2, &NBW, NULL);   // Save lpfirstsubfile and lpbrotherfile
    WriteFile(hFileWholeReg, &nFPCurrentFatherFile, sizeof(nFPCurrentFatherFile), &NBW, NULL);                              // Save nFPCurrentFatherKey passed by caller

    lpFileContent->bfilematch=0;
    WriteFile(hFileWholeReg, &(lpFileContent->bfilematch), sizeof(lpFileContent->bfilematch), &NBW, NULL);                  // Clear and save bfilematch
*/
    WriteFile(hFileWholeReg, lpFileContent->lpfilename, nLenPlus1, &NBW, NULL); // Save the current filename

    nPad = (nLenPlus1 % sizeof(int) == 0) ? 0 : (sizeof(int) - nLenPlus1 % sizeof(int));

    nFPTemp4Write = 0;
    if (nPad > 0) {
        WriteFile(hFileWholeReg, &nFPTemp4Write, nPad, &NBW, NULL);         // Save the current filename
    }

    if (lpFileContent->lpfirstsubfile != NULL) {
        // pass this filecontent's position as subfile's fatherfile's position and pass the "lpfirstsubfile field"
        SaveFileContent(lpFileContent->lpfirstsubfile, nFPHeader, nFPHeader + sizeof(LPSTR) + 6 * sizeof(DWORD));
    }

    if (lpFileContent->lpbrotherfile != NULL) {
        // pass this file's fatherfile's position as brother's father and pass "lpbrotherfile field"
        SaveFileContent(lpFileContent->lpbrotherfile, nFPCurrentFatherFile, nFPHeader + sizeof(LPSTR) + 6 * sizeof(DWORD) + sizeof(LPFILECONTENT));
    }

    if (nFPCaller > 0) {    // save position of current file in current father file
        nFPCurrent = SetFilePointer(hFileWholeReg, 0, NULL, FILE_CURRENT);
        SetFilePointer(hFileWholeReg, nFPCaller, NULL, FILE_BEGIN);
        WriteFile(hFileWholeReg, &nFPHeader, sizeof(DWORD), &NBW, NULL);
        SetFilePointer(hFileWholeReg, nFPCurrent, NULL, FILE_BEGIN);
    }

    // Need adjust progress bar para!!
    nSavingFile++;

    if (nGettingFile != 0)
        if (nSavingFile % nGettingFile > nFileStep) {
            nSavingFile = 0;
            SendDlgItemMessage(hWnd, IDC_PBCOMPARE, PBM_STEPIT, (WPARAM)0, (LPARAM)0);
            UpdateWindow(hWnd);
            PeekMessage(&msg, hWnd, WM_ACTIVATE, WM_ACTIVATE, PM_REMOVE);
        }


}

//--------------------------------------------------
// Realign filecontent, called by ReAlignFile()
// modi 20111216
//--------------------------------------------------
VOID ReAlignFileContent(LPFILECONTENT lpFC, size_t nBase)
{

    if (lpFC->lpfilename != NULL) {
        lpFC->lpfilename += nBase;
    }
    if (lpFC->lpfirstsubfile != NULL) {
        lpFC->lpfirstsubfile =(LPFILECONTENT)( (LPBYTE)lpFC->lpfirstsubfile + nBase);
    }
    if (lpFC->lpbrotherfile != NULL) {
        lpFC->lpbrotherfile = (LPFILECONTENT)( (LPBYTE)lpFC->lpbrotherfile + nBase);
    }
    if (lpFC->lpfatherfile != NULL) {
        lpFC->lpfatherfile = (LPFILECONTENT)( (LPBYTE)lpFC->lpfatherfile + nBase);
    }


    nGettingFile++; // just for the progress bar

    if (lpFC->lpfirstsubfile != NULL) {
        ReAlignFileContent(lpFC->lpfirstsubfile, nBase);
    }

    if (lpFC->lpbrotherfile != NULL) {
        ReAlignFileContent(lpFC->lpbrotherfile, nBase);
    }

}


//--------------------------------------------------
// Realign file, walk through chain
// modi 20111216
//--------------------------------------------------
VOID ReAlignFile(LPHEADFILE lpHF, size_t nBase)
{
    LPHEADFILE  lphf;

    for (lphf = lpHF; lphf != NULL; lphf = lphf->lpnextheadfile) {


        if (lphf->lpnextheadfile != NULL) {
            lphf->lpnextheadfile =(LPHEADFILE)( (LPBYTE)lphf->lpnextheadfile + nBase);
        }
        if (lphf->lpfilecontent != NULL) {
            lphf->lpfilecontent = (LPFILECONTENT)( (LPBYTE)lphf->lpfilecontent + nBase);
        }


        if (lphf->lpfilecontent != NULL) {    // I wouldn't find crash bug(loadhive->readfile) in 1.8.0 if I had added it in that version
            ReAlignFileContent(lphf->lpfilecontent, nBase);
        }
    }
}


//--------------------------------------------------
// Walkthrough lpHF and find lpname matches
//--------------------------------------------------
LPFILECONTENT SearchDirChain(LPSTR lpname, LPHEADFILE lpHF)
{
    LPHEADFILE lphf;

    if (lpname != NULL)
        for (lphf = lpHF; lphf != NULL; lphf = lphf->lpnextheadfile) {
            if (lphf->lpfilecontent != NULL && lphf->lpfilecontent->lpfilename != NULL)
                if (_stricmp(lpname, lphf->lpfilecontent->lpfilename) == 0) {
                    return lphf->lpfilecontent;
                }
        }
    return NULL;
}


//--------------------------------------------------
// Walkthrough lpheadfile chain and collect it's first dirname to lpDir
//--------------------------------------------------
VOID FindDirChain(LPHEADFILE lpHF, LPSTR lpDir, size_t nMaxLen)
{
    size_t      nLen;
    LPHEADFILE  lphf;
    *lpDir = 0x00;
    nLen   = 0;

    for (lphf = lpHF; lphf != NULL; lphf = lphf->lpnextheadfile) {
        if (lphf->lpfilecontent != NULL && nLen < nMaxLen) {
            nLen += strlen(lphf->lpfilecontent->lpfilename) + 1;
            strcat(lpDir, lphf->lpfilecontent->lpfilename);
            strcat(lpDir, ";");
        }
    }
}


//--------------------------------------------------
// if two dir chains are the same
//--------------------------------------------------
BOOL DirChainMatch(LPHEADFILE lphf1, LPHEADFILE lphf2)
{
    char lpDir1[EXTDIRLEN + 2];
    char lpDir2[EXTDIRLEN + 2];
    ZeroMemory(lpDir1, sizeof(lpDir1));
    ZeroMemory(lpDir2, sizeof(lpDir2));
    FindDirChain(lphf1, lpDir1, EXTDIRLEN);
    FindDirChain(lphf2, lpDir2, EXTDIRLEN);

    if (_stricmp(lpDir1, lpDir2) != 0) {
        return FALSE;
    } else {
        return TRUE;
    }
}
