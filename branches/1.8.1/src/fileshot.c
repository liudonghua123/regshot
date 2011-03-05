/*
    Copyright 1999-2003,2007 TiANWEi
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
//ISDIR,ISFILE added in 1.80
#define ISDIR(x) ( (x&FILE_ATTRIBUTE_DIRECTORY)!=0 )
#define ISFILE(x) ( (x&FILE_ATTRIBUTE_DIRECTORY)==0 )
extern u_char * lan_dir;
extern u_char * lan_file;

//-------------------------------------------------------------
// Routine to get Whole File Name[root dir] from a FILECONTENT
//-------------------------------------------------------------
LPSTR	GetWholeFileName(LPFILECONTENT lpFileContent)
{
	LPFILECONTENT lpf;
	DWORD	nWholeLen=0,nLen,i;
	LPSTR	lpWholeName=NULL,lptail;
	for(lpf=lpFileContent;lpf!=NULL;lpf=lpf->lpfatherfile)
	{
		nWholeLen=nWholeLen+lstrlen(lpf->lpfilename)+1;
	}
	lpWholeName=GlobalAlloc(LMEM_FIXED,nWholeLen);

	lptail=lpWholeName+nWholeLen-1;
	*lptail=0x00;
	
	for(lpf=lpFileContent;lpf!=NULL;lpf=lpf->lpfatherfile)
	{
		nLen=lstrlen(lpf->lpfilename);
		for(lptail--,i=1;i<=nLen;i++,lptail--)
		{
			*lptail=*((lpf->lpfilename)+nLen-i);
			
		}
		if (lptail>lpWholeName)
			*lptail=0x5c; //"\\"

	}
	return lpWholeName;
}
				

//-------------------------------------------------------------
// Routine to walk through all Sub tree of current Directory [File system]
//-------------------------------------------------------------
VOID	GetAllSubFile(
					  BOOL needbrother,
					  DWORD typedir,DWORD typefile,
					  LPDWORD lpcountdir,LPDWORD lpcountfile,
					  LPFILECONTENT lpFileContent
					  )
{
	if(ISDIR(lpFileContent->fileattr))
	{
		if(lstrcmp(lpFileContent->lpfilename,".")!=0 &&lstrcmp(lpFileContent->lpfilename,"..")!=0) //we should add here 1.8
			LogToMem(typedir,lpcountdir,lpFileContent);
	}
	else
		LogToMem(typefile,lpcountfile,lpFileContent);

	if(lpFileContent->lpfirstsubfile!=NULL) //tfx 避免目录扫描时产生"."和".." added in 1.7.3 fixed at 1.8
		//&&lstrcmp(lpFileContent->lpfirstsubfile->lpfilename,".")!=0 //But, this break the chain! :(. because we store . and .. in scan filecontent!
		//&&lstrcmp(lpFileContent->lpfirstsubfile->lpfilename,"..")!=0) //So we should move these two lines above
	{
		GetAllSubFile(TRUE,typedir,typefile,lpcountdir,lpcountfile,lpFileContent->lpfirstsubfile);
	}
	if(needbrother==TRUE)
	{
		if(lpFileContent->lpbrotherfile!=NULL)
			GetAllSubFile(TRUE,typedir,typefile,lpcountdir,lpcountfile,lpFileContent->lpbrotherfile);
	}

	
}



//------------------------------------------------------------
// File Shot Engine
//------------------------------------------------------------
VOID	GetFilesSnap(LPFILECONTENT lpFatherFile)
{
	LPSTR	lpFilename,lpTemp;
	HANDLE	filehandle;
	WIN32_FIND_DATA	finddata;
	LPFILECONTENT  lpFileContent,lpFileContentTemp;

	lpTemp=GetWholeFileName(lpFatherFile);
	//Not used
	//if(bWinNTDetected)
	//{
	//	lpFilename=GlobalAlloc(LMEM_FIXED,lstrlen(lpTemp)+5+4);
	//	lstrcpy(lpFilename,"\\\\?\\");
	//	lstrcat(lpFilename,lpTemp);
	//}
	//else
	{
		lpFilename=GlobalAlloc(LMEM_FIXED,lstrlen(lpTemp)+5);
		lstrcpy(lpFilename,lpTemp);
	}
	lstrcat(lpFilename,"\\*.*");

	GlobalFree(lpTemp);
	//_asm int 3;
	filehandle=FindFirstFile(lpFilename,&finddata);
	GlobalFree(lpFilename);
	if(filehandle==INVALID_HANDLE_VALUE)
		return;
	
	lpTemp=finddata.cFileName; //1.8

	lpFileContent=GlobalAlloc(LPTR,sizeof(FILECONTENT));
	lpFileContent->lpfilename=GlobalAlloc(LPTR,lstrlen(finddata.cFileName)+1); //must add one!
	lstrcpy(lpFileContent->lpfilename,finddata.cFileName);
	lpFileContent->writetimelow=finddata.ftLastWriteTime.dwLowDateTime;
	lpFileContent->writetimehigh=finddata.ftLastWriteTime.dwHighDateTime;
	lpFileContent->filesizelow=finddata.nFileSizeLow;
	lpFileContent->filesizehigh=finddata.nFileSizeHigh;
	lpFileContent->fileattr=finddata.dwFileAttributes;
	lpFileContent->lpfatherfile=lpFatherFile;
	lpFatherFile->lpfirstsubfile=lpFileContent;
	lpFileContentTemp=lpFileContent;

	if(ISDIR(lpFileContent->fileattr))
	{
		//use lpTemp may "optimize" some, note, finddata.cfilename should exists!
		if(   !(*lpTemp=='.' && *(lpTemp+1)==0x00 ) && !(*lpTemp=='.' && *(lpTemp+1)=='.' && *(lpTemp+2)==0x00 ) //if(lstrcmp(lpFileContent->lpfilename,".")!=0&&lstrcmp(lpFileContent->lpfilename,"..")!=0 //or we can use that
			&&!IsInSkipList(lpFileContent->lpfilename,lpSnapFiles)) //tfx
		{
			
			nGettingDir++;
			GetFilesSnap(lpFileContent);
		}
	}
	else
	{
		nGettingFile++;
	}

	
	for	(;FindNextFile(filehandle,&finddata)!=FALSE;)
	{
		lpFileContent=GlobalAlloc(LPTR,sizeof(FILECONTENT));
		lpFileContent->lpfilename=GlobalAlloc(LPTR,lstrlen(finddata.cFileName)+1);
		lstrcpy(lpFileContent->lpfilename,finddata.cFileName);
		lpFileContent->writetimelow=finddata.ftLastWriteTime.dwLowDateTime;
		lpFileContent->writetimehigh=finddata.ftLastWriteTime.dwHighDateTime;
		lpFileContent->filesizelow=finddata.nFileSizeLow;
		lpFileContent->filesizehigh=finddata.nFileSizeHigh;
		lpFileContent->fileattr=finddata.dwFileAttributes;
		lpFileContent->lpfatherfile=lpFatherFile;
		lpFileContentTemp->lpbrotherfile=lpFileContent;
		lpFileContentTemp=lpFileContent;

		if(ISDIR(lpFileContent->fileattr))
		{
			if(   !(*lpTemp=='.' && *(lpTemp+1)==0x00 ) && !(*lpTemp=='.' && *(lpTemp+1)=='.' && *(lpTemp+2)==0x00 ) //if(lstrcmp(lpFileContent->lpfilename,".")!=0&&lstrcmp(lpFileContent->lpfilename,"..")!=0
				&&!IsInSkipList(lpFileContent->lpfilename,lpSnapFiles)) //tfx
			{
				nGettingDir++;
				GetFilesSnap(lpFileContent);
			}
		}
		else
		{
			nGettingFile++;
		}

	}
	FindClose(filehandle);

	nGettingTime=GetTickCount();
	if ((nGettingTime-nBASETIME1)>REFRESHINTERVAL)
	{
		ShowCounters(lan_dir,lan_file,nGettingDir,nGettingFile);
	}

	return ;
}


//-------------------------------------------------------------
// File Compare Engine (lp1 and lp2 run parallel)
//-------------------------------------------------------------
VOID	CompareFirstSubFile(LPFILECONTENT lpHead1,LPFILECONTENT lpHead2)
{
	LPFILECONTENT	lp1,lp2;
	for(lp1=lpHead1;lp1!=NULL;lp1=lp1->lpbrotherfile)
	{
		for(lp2=lpHead2;lp2!=NULL;lp2=lp2->lpbrotherfile)
		{
			if((lp2->bfilematch==NOTMATCH)&&lstrcmp(lp1->lpfilename,lp2->lpfilename)==0)
			{	//Two 'FILE's have same name,but we are not sure they are the same,so we compare it!
				if( ISFILE(lp1->fileattr) && ISFILE(lp2->fileattr) )
					////(lp1->fileattr&FILE_ATTRIBUTE_DIRECTORY)!=FILE_ATTRIBUTE_DIRECTORY&&(lp2->fileattr&FILE_ATTRIBUTE_DIRECTORY)!=FILE_ATTRIBUTE_DIRECTORY)
				{	
					//Lp1 is file,lp2 is file
					if(lp1->writetimelow==lp2->writetimelow&&lp1->writetimehigh==lp2->writetimehigh&&
						lp1->filesizelow==lp2->filesizelow&&lp1->filesizehigh==lp2->filesizehigh&&lp1->fileattr==lp2->fileattr)
					{ //We found a match file!
						lp2->bfilematch=ISMATCH;
					}
					else
					{
						//We found a dismatch file ,they will be logged
						lp2->bfilematch=ISMODI;
						LogToMem(FILEMODI,&nFILEMODI,lp1);
					}
					
				}
				else
				{
					//At least one file of the pair is directory,so we try to determine
					if( ISDIR(lp1->fileattr) && ISDIR(lp2->fileattr))
						////(lp1->fileattr&FILE_ATTRIBUTE_DIRECTORY)==FILE_ATTRIBUTE_DIRECTORY&&(lp2->fileattr&FILE_ATTRIBUTE_DIRECTORY)==FILE_ATTRIBUTE_DIRECTORY)
					{	
						//The two 'FILE's are all dirs
						if(lp1->fileattr==lp2->fileattr)
						{	//Same attributs of dirs,we compare their subfiles
							lp2->bfilematch=ISMATCH;
							CompareFirstSubFile(lp1->lpfirstsubfile,lp2->lpfirstsubfile);
						}
						else
						{   //Dir attributes changed,they will be logged
							lp2->bfilematch=ISMODI;
							LogToMem(DIRMODI,&nDIRMODI,lp1);
						}
						//break;
					}
					else
					{
						//One of the 'FILE's is dir,but which one?
						if( ISFILE(lp1->fileattr) && ISDIR(lp2->fileattr) )
							////(lp1->fileattr&FILE_ATTRIBUTE_DIRECTORY)!=FILE_ATTRIBUTE_DIRECTORY&&(lp2->fileattr&FILE_ATTRIBUTE_DIRECTORY)==FILE_ATTRIBUTE_DIRECTORY)
						{ //lp1 is file,lp2 is dir
							lp1->bfilematch=ISDEL;
							LogToMem(FILEDEL,&nFILEDEL,lp1);
							GetAllSubFile(FALSE,DIRADD,FILEADD,&nDIRADD,&nFILEADD,lp2);
						}
						else
						{	
							//Lp1 is dir,lp2 is file
							lp2->bfilematch=ISADD;
							LogToMem(FILEADD,&nFILEADD,lp2);
							GetAllSubFile(FALSE,DIRDEL,FILEDEL,&nDIRDEL,&nFILEDEL,lp1);
						}
					}
				}
				break;
			}
		}
		if(lp2==NULL)
		{
			//lp2 looped to the end,that is,we can not find a lp2 matches lp1,so lp1 is  deleted!
			if(ISDIR(lp1->fileattr))
				GetAllSubFile(FALSE,DIRDEL,FILEDEL,&nDIRDEL,&nFILEDEL,lp1); //if lp1 is dir
			else
				LogToMem(FILEDEL,&nFILEDEL,lp1); //if lp1 is file
		}
	}
	//We loop to the end,then we do an extra loop of lp2 use flag we previous make
	for(lp2=lpHead2;lp2!=NULL;lp2=lp2->lpbrotherfile)
	{
		nComparing++;
		if(lp2->bfilematch==NOTMATCH)
		{   //We did not find a lp1 matches a lp2,so lp2 is added!
			if(ISDIR(lp2->fileattr))
				GetAllSubFile(FALSE,DIRADD,FILEADD,&nDIRADD,&nFILEADD,lp2);
			else
				LogToMem(FILEADD,&nFILEADD,lp2);
		}
	}
	// Progress bar update
	if (nGettingFile!=0)
	if (nComparing%nGettingFile>nFileStep)
	{
		nComparing=0;
		SendDlgItemMessage(hWnd,IDC_PBCOMPARE,PBM_STEPIT,(WPARAM)0,(LPARAM)0);
	}

}

//-------------------------------------------------------------
// Routines to Free All File tree
//-------------------------------------------------------------
VOID FreeAllFileContent(LPFILECONTENT lpFile)
{
	if(lpFile!=NULL)
	{
		FreeAllFileContent(lpFile->lpfirstsubfile);
		FreeAllFileContent(lpFile->lpbrotherfile);
		GlobalFree(lpFile->lpfilename);
		GlobalFree(lpFile);
	}
}

//-------------------------------------------------------------
// Routines to Free Head Files
//-------------------------------------------------------------
VOID FreeAllFileHead(LPHEADFILE lp)
{
	if(lp!=NULL)
	{
		FreeAllFileHead(lp->lpnextheadfile);
		FreeAllFileContent(lp->lpfilecontent);
		//FreeAllFileContent(lp->lpfilecontent2);
		GlobalFree(lp);
	}

}
/*
VOID FreeAllFiles(void)
{
	FreeAllFileHead(lpHeadFile);
	lpHeadFile=NULL;
}
*/

//--------------------------------------------------
//File Save Engine (It is stupid again!) added in 1.8
//--------------------------------------------------
VOID	SaveFileContent(LPFILECONTENT lpFileContent, DWORD nFPCurrentFatherFile,DWORD nFPCaller)
{

	DWORD	nFPHeader,nFPCurrent,nFPTemp4Write,nLenPlus1;
	
	
	nLenPlus1=lstrlen(lpFileContent->lpfilename)+1;											//get len+1
	nFPHeader=SetFilePointer(hFileWholeReg,0,NULL,FILE_CURRENT);							//save head fp
	nFPTemp4Write=nFPHeader+41;								//10*4+1
	WriteFile(hFileWholeReg,&nFPTemp4Write,4,&NBW,NULL);					//save location of lpfilename
	WriteFile(hFileWholeReg,(LPBYTE)lpFileContent+4,24,&NBW,NULL);	//Write time,size etc. 6*4

	//nFPTemp4Write=(lpFileContent->lpfirstsubfile!=NULL) ? (nFPHeader+41+nLenPlus1):0;			//We write lpfilename plus a "\0"
	//WriteFile(hFileWholeReg,&nFPTemp4Write,4,&NBW,NULL);					//save location of lpfirstsubfile
	WriteFile(hFileWholeReg,(LPBYTE)lpFileContent+28,8,&NBW,NULL);			//save lpfirstsubfile and lpbrotherfile
	WriteFile(hFileWholeReg,&nFPCurrentFatherFile,4,&NBW,NULL);				//save nFPCurrentFatherKey passed by caller
	nFPTemp4Write=0;
	WriteFile(hFileWholeReg,&nFPTemp4Write,1,&NBW,NULL);					//clear and save bfilematch
	WriteFile(hFileWholeReg,lpFileContent->lpfilename,nLenPlus1,&NBW,NULL);	//Save the current filename
	
	
	if(lpFileContent->lpfirstsubfile!=NULL)
	{
		//pass this filecontent's position as subfile's fatherfile's position and pass the "lpfirstsubfile field"
		SaveFileContent(lpFileContent->lpfirstsubfile,nFPHeader,nFPHeader+28); 
	}
		
	if(lpFileContent->lpbrotherfile!=NULL)
	{
		// pass this file's fatherfile's position as brother's father and pass "lpbrotherfile field"
		SaveFileContent(lpFileContent->lpbrotherfile,nFPCurrentFatherFile,nFPHeader+32);
	}

	if(nFPCaller>0) //save position of current file in current father file
	{
		nFPCurrent=SetFilePointer(hFileWholeReg,0,NULL,FILE_CURRENT);
		SetFilePointer(hFileWholeReg,nFPCaller,NULL,FILE_BEGIN);
		WriteFile(hFileWholeReg,&nFPHeader,4,&NBW,NULL);
		SetFilePointer(hFileWholeReg,nFPCurrent,NULL,FILE_BEGIN);
	}
	//Need adjust progress bar para!!
	nSavingFile++;
	if (nGettingFile!=0)
	if (nSavingFile%nGettingFile>nFileStep)
	{
		nSavingFile=0;
		SendDlgItemMessage(hWnd,IDC_PBCOMPARE,PBM_STEPIT,(WPARAM)0,(LPARAM)0);
		UpdateWindow(hWnd);
		PeekMessage(&msg,hWnd,WM_ACTIVATE,WM_ACTIVATE,PM_REMOVE);
	}
	

}
//--------------------------------------------------
//ReAlign filecontent,called by realignfile()
//--------------------------------------------------
VOID ReAlignFileContent(LPFILECONTENT lpFC,DWORD nBase)
{
	LPDWORD lp;
	lp=(LPDWORD)lpFC;
	if((*lp)!=0) (*lp)+=nBase;	lp+=7;
	if((*lp)!=0) (*lp)+=nBase;	lp++;
	if((*lp)!=0) (*lp)+=nBase;	lp++;
	if((*lp)!=0) (*lp)+=nBase;
	nGettingFile++; // just for the progress bar
	if(lpFC->lpfirstsubfile!=NULL)
		ReAlignFileContent(lpFC->lpfirstsubfile,nBase);
	if(lpFC->lpbrotherfile!=NULL)
		ReAlignFileContent(lpFC->lpbrotherfile,nBase);

}
//--------------------------------------------------
//ReAlign file,walkthrough chain
//--------------------------------------------------
VOID ReAlignFile(LPHEADFILE lpHF,DWORD nBase)
{
	LPDWORD lp;
	LPHEADFILE lphf;
	for(lphf=lpHF;lphf!=NULL;lphf=lphf->lpnextheadfile)
	{
		lp=(LPDWORD)lphf;
		if((*lp)!=0) (*lp)+=nBase;lp++;
		if((*lp)!=0) (*lp)+=nBase;
		if(lphf->lpfilecontent!=NULL) //I wouldn't find crash bug(loadhive->readfile) in 1.8.0 if I had added it in that version
			ReAlignFileContent(lphf->lpfilecontent,nBase);
	}
}


//--------------------------------------------------
// Walkthrough lpHF and find lpname matches
//--------------------------------------------------
LPFILECONTENT SearchDirChain(LPSTR lpname,LPHEADFILE lpHF)
{
	LPHEADFILE lphf;
	if(lpname!=NULL)
	for(lphf=lpHF;lphf!=NULL;lphf=lphf->lpnextheadfile)
	{
		if(lphf->lpfilecontent!=NULL && lphf->lpfilecontent->lpfilename!=NULL)
			if(lstrcmpi(lpname,lphf->lpfilecontent->lpfilename)==0)
				return lphf->lpfilecontent;
	}
	return NULL;
}

//--------------------------------------------------
// Walkthrough lpheadfile chain and collect it's first dirname to lpDir
//--------------------------------------------------
VOID FindDirChain(LPHEADFILE lpHF,LPSTR lpDir,int nMaxLen)
{
	int nLen;
	LPHEADFILE lphf;
	*lpDir=0x00;
	nLen=0;
	for(lphf=lpHF;lphf!=NULL;lphf=lphf->lpnextheadfile)
	{
		if(lphf->lpfilecontent!=NULL && nLen < nMaxLen)
		{
			nLen+=lstrlen(lphf->lpfilecontent->lpfilename)+1;
			strcat(lpDir,lphf->lpfilecontent->lpfilename);
			strcat(lpDir,";");
		}
	}
}


//--------------------------------------------------
// if two dir chains are the same
//--------------------------------------------------
BOOL DirChainMatch(LPHEADFILE lphf1,LPHEADFILE lphf2)
{
	char lpDir1[EXTDIRLEN+2];
	char lpDir2[EXTDIRLEN+2];
	ZeroMemory(lpDir1,sizeof(lpDir1));
	ZeroMemory(lpDir2,sizeof(lpDir2));
	FindDirChain(lphf1,lpDir1,EXTDIRLEN);
	FindDirChain(lphf2,lpDir2,EXTDIRLEN);
	if(lstrcmpi(lpDir1,lpDir2)!=0)
		return FALSE;
	else
		return TRUE;
}

