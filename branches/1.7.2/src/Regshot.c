/*************************************************
 * Chinese notes about the source code release   *
 * [Sorry for the non-Chinese reader,I must use  *
 * my mother language to explain something ]     *
 * ***********************************************
 
 * 2003/10/15, 将这早应该发布的源码终于发布了,似
 * 乎应该称其为"该死的"源码! 原因是它照现在的我来
 * 看写得实在是不怎么样,只是小巧而已,也许我应该叫
 * 它为"regshit" :))
 
 * regshot在1.4版做过重大的调整,重新写了引擎,就是
 * 现在用的,后来又加入过9x的实时监控(vxd),后来又
 * 因为某些原因去除,后来又加入了语言选择,后来由于
 * 一次分区误操作,丢失了一些代码,所幸有备份,但再
 * 升级的念头就没有了. 详见whatsnew.txt
 
 * 我希望有程序员可以由此源码启发,写出若干控件,无论
 * 是vb,还是vc,或是我喜欢的bc,造福大家,如果您能将
 * 其做为免费的东西的话就太好了.
 
 * 无论如何,人类的激情,怨恨,恋爱,自私,创造等等都是
 * 过眼云烟......
 * 飞出去,去迎接更广阔的未来
 
 * TiANWEi at http://www.digitalnuke.com/
 
 * Thanks to all of you!
 
/*************************************************
 *  RegShot Win9x,NT Registry compare tool       *
 *  http://regshot.yeah.net  spring_w@163.com    *
 *   latest version: 1.61 2002/03/30             *
 *************************************************
 Regshot is a free small utility that can compare
 registry changes.The tech of RegShot is based on 
 Windows Registry APIs:
 RegOpenKeyEx()
 RegEnumValue()
 RegEnumKeyEx()
 RegQureyInfoKey() ...

 The most important routines in Regshot are :
 CompareFirstSubKey()
 GetRegistrySnap() ...

 For the multilanguage to work prop,the definations
 IN resource.h must be in their order
 
 Due to TAB size mismatch, the document is best 
 viewed at MS VC 4.2 or MS VC 6.0
 
 ==============
 Tab size=4
 Indent size=4
 Keep tabs
 ==============
 
 The shortcomings are listed at the end of this DOC.
*/

#include <windows.h>
#include <shlobj.h>
#include "resource.h"

#define WIN32_LEAN_AND_MEAN //tfx 减少程序文件大小
#pragma comment(linker,"/FILEALIGN:0x200 /MERGE:.data=.text /MERGE:.rdata=.text /SECTION:.text,EWR /IGNORE:4078")

//#define DEBUGLOG
//Some definations
#define SIZEOFREG_DWORD	4		//In current windows ,reg_dword's size =4 
#define NOTMATCH		0		//Define modification type in compare results
#define ISMATCH			1
#define ISDEL			2
#define ISADD			3
#define ISMODI			4

#define KEYADD			1
#define KEYDEL			2
#define VALADD			3
#define VALDEL			4
#define VALMODI			5
#define FILEADD			6
#define FILEDEL			7
#define FILEMODI		8
#define DIRADD			9
#define DIRDEL			10
#define DIRMODI			11

#define REFRESHINTERVAL	110		//Define progress refresh rate
#define MAXPBPOSITION	100		//Define progress bar length
#define COMMENTLENGTH	50		//Define commentfield length on the MainForm
#define HTMLWRAPLENGTH	1000	//Define html out put wrap length
#define MAXAMOUNTOFFILE	10000	//Define out put file counts
#define	EXTDIRLEN	MAX_PATH*3	//Define Searching Directory field length
#define COMPUTERNAMELEN	64		//Define COMPUTER name length
#define HIVEBEGINOFFSET	512		//Hive file out put header computerlen*2+sizeof(systemtime)+32 must <hivebeginoffset!!!!!!!!!!!!!!


//Some definations of MutiLanguage strings [Free space length]
#define SIZEOF_LANGUAGE_SECTIONNAMES_BUFFER 2048
#define SIZEOF_SINGLE_LANGUAGENAME	64
#define SIZEOF_FREESTRINGS 16384
#define SIZEOF_ABOUTBOX 2048


//This is main struct I used for Windows Registry Key 
struct	_KEYCONTENT
{					
	LPSTR	lpkeyname;							//Pointer to Key Name
	struct	_VALUECONTENT FAR * lpfirstvalue;	//Pointer to Key's first Value
	struct	_KEYCONTENT	FAR * lpfirstsubkey;	//Pointer to Key's first Sub Key
	struct	_KEYCONTENT	FAR * lpbrotherkey;		//Pointer to Key's brother
	struct	_KEYCONTENT	FAR * lpfatherkey;		//Pointer to Key's father
	BYTE	bkeymatch;							//Flag used at comparing

};

//This is main struct I used for Windows Registry Value
struct	_VALUECONTENT
{
	DWORD	typecode;							//Type of Value [DWORD,STRING...]
	DWORD	datasize;							//Value Data size in bytes
	LPSTR	lpvaluename;						//Pointer to Value Name
	LPBYTE	lpvaluedata;						//Pointer to Value Data
	struct	_VALUECONTENT FAR * lpnextvalue;	//Pointer to Value's brother
	struct	_KEYCONTENT	FAR * lpfatherkey;		//Pointer to Value's father[Key]
	BYTE	bvaluematch;						//Flag used at comparing
};

//This is main struct I used for Windows File System
struct	_FILECONTENT
{
	LPSTR	lpfilename;							//Pointer to File Name
	DWORD	writetimelow;						//File write time [LOW  DWORD]
	DWORD	writetimehigh;						//File write time [HIGH DWORD]
	DWORD	filesizelow;						//File size	 [LOW  DWORD]
	DWORD	filesizehigh;						//File size	 [HIGH DWORD]
	DWORD	fileattr;							//File Attributes
	struct	_FILECONTENT FAR * lpfirstsubfile;	//Pointer to Files[DIRS] first sub file
	struct	_FILECONTENT FAR * lpbrotherfile;	//Pointer to Files[DIRS] brother
	struct	_FILECONTENT FAR * lpfatherfile;	//Pointer to Files father
	BYTE	bfilematch;							//Flag used at comparing
};

// This is the struct I use for compare result  Output 
struct  _COMRESULT
{
	LPSTR	lpresult;							//Pointer to result string
	struct	_COMRESULT FAR * lpnextresult;		//Pointer to next _COMRESULT
};

typedef struct _KEYCONTENT KEYCONTENT,FAR * LPKEYCONTENT;
typedef struct _VALUECONTENT VALUECONTENT,FAR * LPVALUECONTENT;
typedef struct _COMRESULT COMRESULT,FAR * LPCOMRESULT;
typedef struct _FILECONTENT FILECONTENT,FAR * LPFILECONTENT;

//That is the struct I use for file tree compare
struct	_HEADFILE
{
	LPFILECONTENT	lpHeadFile1;				//Pointer to header file at 1st shot
	LPFILECONTENT	lpHeadFile2;				//Pointer to header file at 2nd shot
	struct _HEADFILE	FAR *	lpnextheadfile;	//Pointer to next headfile struc
};
typedef	struct	_HEADFILE	HEADFILE,FAR * LPHEADFILE;




MSG				msg;							//Windows MSG struct
HWND			hWnd;							//The handle of REGSHOT
HMENU			hMenu,hMenuClear;				//The handles of shortcut menus
HANDLE			hFile,hFileWholeReg;			//Handle of file regshot use
HCURSOR			hHourGlass;						//Handle of cursor
HCURSOR			hSaveCursor;					//Handle of cursor
BOOL			is1;							//Flag to determine is the 1st shot
BOOL			is1LoadFromHive,is2LoadFromHive;//Flag to determine are shots load from hive files
RECT			rect;							//Window RECT
FILETIME		ftLastWrite;					//Filetime struct
BROWSEINFO		BrowseInfo1;					//BrowseINFO struct
OPENFILENAME	opfn;							//Openfilename struct

//Pointers to compare result [see above.]
LPCOMRESULT	lpKEYADD;							
LPCOMRESULT	lpKEYDEL;
LPCOMRESULT	lpVALADD;
LPCOMRESULT	lpVALDEL;
LPCOMRESULT	lpVALMODI;
LPCOMRESULT	lpFILEADD;
LPCOMRESULT	lpFILEDEL;
LPCOMRESULT	lpFILEMODI;
LPCOMRESULT	lpDIRADD;
LPCOMRESULT	lpDIRDEL;
LPCOMRESULT	lpDIRMODI;


LPCOMRESULT	lpKEYADDHEAD;
LPCOMRESULT	lpKEYDELHEAD;
LPCOMRESULT	lpVALADDHEAD;
LPCOMRESULT	lpVALDELHEAD;
LPCOMRESULT	lpVALMODIHEAD;
LPCOMRESULT	lpFILEADDHEAD;
LPCOMRESULT	lpFILEDELHEAD;
LPCOMRESULT	lpFILEMODIHEAD;
LPCOMRESULT	lpDIRADDHEAD;
LPCOMRESULT	lpDIRDELHEAD;
LPCOMRESULT	lpDIRMODIHEAD;

//Number of Modification detected
DWORD	nKEYADD;
DWORD	nKEYDEL;
DWORD	nVALADD;
DWORD	nVALDEL;
DWORD	nVALMODI;
DWORD	nFILEADD;
DWORD	nFILEDEL;
DWORD	nFILEMODI;
DWORD	nDIRADD;
DWORD	nDIRDEL;
DWORD	nDIRMODI;


//Some DWORD used to show the progress bar and etc
DWORD	nGettingValue;
DWORD	nGettingKey,nComparing,nComparingStep,nSavingKey,nSavingStep;
DWORD	nGettingTime,nBASETIME,nBASETIME1;
DWORD	nGettingFile,nGettingDir;
DWORD	nMask=0xf7fd; //not used now ,but should be added
DWORD	nRegMessageCount=0;
DWORD	NumberOfBytesWritten;

//Pointers to Registry Key
LPKEYCONTENT	lpHeadLocalMachine1;		//Pointer to HKEY_LOCAL_MACHINE	1
LPKEYCONTENT	lpHeadLocalMachine2;		//Pointer to HKEY_LOCAL_MACHINE 2
LPKEYCONTENT	lpHeadUsers1;				//Pointer to HKEY_USERS 1
LPKEYCONTENT	lpHeadUsers2;				//Pointer to HKEY_USERS 1
LPHEADFILE		lpHeadFile;					//Pointer to Headfile
LPSTR			lpTempHive1,lpTempHive2;	//Pointer for load hive files
LPSTR			lpComputerName1,lpComputerName2,lpUserName1,lpUserName2;
SYSTEMTIME FAR * lpSystemtime1,* lpSystemtime2;

//Some pointers need to allocate enought space to working
LPSTR	lpKeyName,lpMESSAGE,lpExtDir,lpOutputpath,lpLastSaveDir,lpLastOpenDir,lpCurrentLanguage;
LPSTR	lpWindowsDirName,lpTempPath,lpStartDir,lpIni,lpFreeStrings,lpCurrentTranslator;

LPSTR	lpProgramDir; //tfx 定义
#define SIZEOF_REGSHOT	65535
#define MAXREGSHOT		100
LPDWORD lpSnapRegs, lpSnapFiles;
LPSTR	lpRegshotIni;
LPSTR   lpSnapRegsStr,lpSnapFilesStr,lpSnapKey,lpSnapReturn;
LPSTR	REGSHOTINI			="regshot.ini";
LPSTR	INI_SETUP			="Setup";
LPSTR	INI_FLAG			="Flag";
LPSTR	INI_EXTDIR			="ExtDir";
LPSTR	INI_OUTDIR			="OutDir";
LPSTR	INI_SKIPREGKEY		="SkipRegKey";
LPSTR	INI_SKIPDIR			="SkipDir";

LPSTR	REGSHOTDATFILE		="rgst152.dat";
LPSTR	REGSHOTLANGUAGEFILE	="language.ini";
LPSTR	lpSectionCurrent	="CURRENT";
LPSTR	lpItemTranslator	="Translator";
LPSTR	lpMyName			="[Original]";
LPSTR	lpDefaultLanguage	="English";
//LPSTR	USERSSTRING			="HKEY_USERS";
//LPSTR	LOCALMACHINESTRING	="HKEY_LOCAL_MACHINE";
LPSTR	USERSSTRING			="HKU";
LPSTR	LOCALMACHINESTRING	="HKLM";
LPSTR	lpRegSaveFileHeader	="REGSHOTHIVE";
LPDWORD	ldwTempStrings;

//This is the dimension for MultiLanguage Default Strings[English]
unsigned char str_default[][22]=
{
"Keys:",
"Values:",
"Dirs:",
"Files:",
"Time:",
"Keys added:",
"Keys deleted:",
"Values added:",
"Values deleted:",
"Values modified:",
"Files added:",
"Files deleted:",
"Files[attr]modified:",
"Folders added:",
"Folders deleted:",
"Folders[attr]changed:",
"Total changes:",
"Comments:",
"Datetime:",
"Computer:",
"Username:",
"About",
"Error",
"Error call ex-viewer",
"Error create file",
"Error open file",
"Error move fp",
"&1st shot",
"&2nd shot",
"c&Ompare",
"&Clear",
"&Quit",
"&About",
"&Monitor",
"Compare logs save as:",
"Output path:",
"Add comment into log:",
"Plain &TXT",
"&HTML document",
"&Scan dir1[;dir2;...]",
"&Shot",
"Shot and Sa&ve...",
"Loa&d...",
"&Clear All",
"Clear &1st shot",
"Clear &2nd shot"
};

//This is the Pointer to Language Strings
LPSTR	str_key;
LPSTR	str_value;
LPSTR	str_dir;
LPSTR	str_file;
LPSTR	str_time;
LPSTR	str_keyadd;
LPSTR	str_keydel;
LPSTR	str_valadd;
LPSTR	str_valdel;
LPSTR   str_valmodi;
LPSTR	str_fileadd;
LPSTR	str_filedel;
LPSTR	str_filemodi;
LPSTR	str_diradd;
LPSTR	str_dirdel;
LPSTR	str_dirmodi;
LPSTR	str_total;
LPSTR	str_comments;
LPSTR	str_datetime;
LPSTR	str_computer;
LPSTR	str_username;
LPSTR	str_about;
LPSTR	str_error;
LPSTR   str_errorexecviewer;
LPSTR	str_errorcreatefile;
LPSTR	str_erroropenfile;
LPSTR	str_errormovefp;
LPSTR	str_menushot;
LPSTR	str_menushotsave;
LPSTR	str_menuload;
LPSTR	str_menuclearallshots;
LPSTR	str_menuclearshot1;
LPSTR	str_menuclearshot2;



//Some strings use to write to HTML or TEXT file
LPSTR	str_line	="\r\n----------------------------------\r\n";
LPSTR	lpBR		="<BR>";
LPSTR	lpHTMLbegin	="<HTML>";
LPSTR	lpHTMLover	="</HTML>";
LPSTR	lpHEADbegin	="<HEAD>";
LPSTR	lpHEADover	="</HEAD>";
LPSTR	lpTd1Begin	="<TR><TD BGCOLOR=669999 ALIGN=LEFT><FONT COLOR=WHITE><B>";
LPSTR	lpTd2Begin	="<TR><TD NOWRAP><FONT COLOR=BLACK>";
LPSTR	lpTd1Over	="</B></FONT></TD></TR>";
LPSTR	lpTd2Over	="</FONT></TD></TR>";
LPSTR	lpstyle		="<STYLE TYPE=\"text/css\">td{font-family:\"Tahoma\";font-size:9pt}tr{font-size:9pt}body{font-size:9pt}</STYLE>";
LPSTR	lpBodyBegin	="<BODY BGCOLOR=FFFFFF TEXT=000000 LINK=C8C8C8>";
LPSTR	lpBodyOver	="</BODY>";
LPSTR	lpTableBegin="<TABLE BORDER=0 WIDTH=480>";
LPSTR	lpTableOver	="</TABLE>";
LPSTR	str_website	="<FONT COLOR=C8C8C8>Bug reports to:<A HREF=\"http://regshot.yeah.net/\">regshot.yeah.net</FONT></A>";
LPSTR	str_aboutme	="Regshot 1.7\n(c) Copyright 2000-2002\tTiANWEi\n(c) Copyright 2004-2004\ttulipfan\n\nhttp://regshot.yeah.net/\nhttp://regshot.ist.md/ [Thanks Alexander Romanenko!]\n\n";
LPSTR	str_lognote	="Regshot 1.7\r\n";
LPSTR	str_prgname ="Regshot 1.7"; //tfx 程序标题

unsigned char lpCR[]={0x0d,0x0a,0x00};
unsigned char strDefResPre[]="~res";
unsigned char str_filter[]={"Regshot hive files [*.hiv]\0*.hiv\0All files\0*.*\0\0"};

//Former definations used at Dynamic Monitor Engine.Not Used NOW
//#define	DIOCPARAMSSIZE	20	//4+4+4+8 bytes DIOCParams size!
//#define	MAXLISTBOXLEN	1024
//#define	RING3TDLEN		8	//ring3 td name length
//LPSTR	str_errorini="Error create Dialog!";
//INT		tabarray[]={40,106,426,466};		// the tabstops is the len addup!
//BOOL	bWinNTDetected;
//UINT			WM_REGSHOT=0;

#ifdef	DEBUGLOG
LPSTR	lstrdb1;
#endif





//-------------------------------------------------------------
//Regshot error message routine[simple]
//-------------------------------------------------------------
void	MyErrorMessage(HANDLE hDialog,LPSTR note)
{
	MessageBox(hDialog,note,str_error,MB_ICONHAND);
}

//-------------------------------------------------------------
//Roution to debug,NOT used!
//-------------------------------------------------------------
#ifdef  DEBUGLOG
void	DebugLog(LPSTR filename,LPSTR lpstr,HWND hDlg,BOOL bisCR)
{
	DWORD	length;
	DWORD	nPos;
	
	hFile = CreateFile(filename,GENERIC_READ | GENERIC_WRITE,FILE_SHARE_READ | FILE_SHARE_WRITE,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
	if( hFile == INVALID_HANDLE_VALUE) 
		MyErrorMessage(hDlg,str_errorcreatefile);
	else
	{
		nPos=SetFilePointer(hFile,0,NULL,FILE_END);
		if(nPos==0xFFFFFFFF)
			MyErrorMessage(hDlg,str_errormovefp);
		else
		{
		
			length=lstrlen(lpstr);
			WriteFile(hFile,lpstr,length,&NumberOfBytesWritten,NULL);
			if(NumberOfBytesWritten!=length)
			{
				//MyErrorMessage(hDlg,str_errorwritefile);
				
			}
			if(bisCR==TRUE)
			WriteFile(hFile,lpCR,2,&NumberOfBytesWritten,NULL);
		}
	}
	CloseHandle(hFile);
}
#endif

//-------------------------------------------------------------
//Routine wasted
//-------------------------------------------------------------
/*LPVOID	MyHeapAlloc(DWORD type,DWORD size)
{
	if((bTurboMode==FALSE)&&((lpMyHeap+size)<(lpMyHeap+MYHEAPSIZE)))
	{
		lpMyHeap=lpMyHeap+size;
		if(type==LPTR)
			ZeroMemory(lpMyHeap,size);
	}
	else
	{
		lpMyHeap=GlobalAlloc(type,size);
	}
	return lpMyHeap;
}
 */


//-------------------------------------------------------------
//Routine to get Whole File Name[root dir] from a FILECONTENT
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
//Routine to Get Whole Key Name from KEYCONTENT
//-------------------------------------------------------------
LPSTR	GetWholeKeyName(LPKEYCONTENT lpKeyContent)
{
	LPKEYCONTENT lpf;
	DWORD	nWholeLen=0,nLen,i;
	LPSTR	lpWholeName=NULL,lptail;
	for(lpf=lpKeyContent;lpf!=NULL;lpf=lpf->lpfatherkey)
	{
		nWholeLen=nWholeLen+lstrlen(lpf->lpkeyname)+1;
	}
	lpWholeName=GlobalAlloc(LMEM_FIXED,nWholeLen);

	lptail=lpWholeName+nWholeLen-1;
	*lptail=0x00;
	for(lpf=lpKeyContent;lpf!=NULL;lpf=lpf->lpfatherkey)
	{
		nLen=lstrlen(lpf->lpkeyname);
		for(lptail--,i=1;i<=nLen;i++,lptail--)
		{
			*lptail=*((lpf->lpkeyname)+nLen-i);
			
		}
		if (lptail>lpWholeName)
			*lptail=0x5c; //"\\"

	}
	return lpWholeName;
}


//-------------------------------------------------------------
//Routine to Get Whole VALUE Name from VALUECONTENT
//-------------------------------------------------------------
LPSTR	GetWholeValueName(LPVALUECONTENT lpValueContent)
{
	LPKEYCONTENT lpf;
	DWORD	nWholeLen=0,nLen,i;
	LPSTR	lpWholeName=NULL,lptail;
	nLen=lstrlen(lpValueContent->lpvaluename);
	nWholeLen=nLen+1;
	for(lpf=lpValueContent->lpfatherkey;lpf!=NULL;lpf=lpf->lpfatherkey)
	{
		nWholeLen=nWholeLen+lstrlen(lpf->lpkeyname)+1;
	}
	lpWholeName=GlobalAlloc(LMEM_FIXED,nWholeLen);
	lptail=lpWholeName+nWholeLen-1;
	*lptail=0x00;

	for(lptail--,i=1;i<=nLen;i++,lptail--)
	{
		*lptail=*((lpValueContent->lpvaluename)+nLen-i);
	}
	*lptail=0x5c; //"\\"
	for(lpf=lpValueContent->lpfatherkey;lpf!=NULL;lpf=lpf->lpfatherkey)
	{
		nLen=lstrlen(lpf->lpkeyname);
		for(lptail--,i=1;i<=nLen;i++,lptail--)
		{
			*lptail=*((lpf->lpkeyname)+nLen-i);
		}
		if (lptail>lpWholeName)
			*lptail=0x5c; //"\\"

	}
	return lpWholeName;
}

//-------------------------------------------------------------
//Routine Trans VALUECONTENT.data[which in binary] into strings
//-------------------------------------------------------------
LPSTR	TransData(LPVALUECONTENT lpValueContent,DWORD type)
{
	LPSTR	lpvaluedata=NULL;
	DWORD	c,size=lpValueContent->datasize;
	switch(type)
	{

		case	REG_SZ:
		//case	REG_EXPAND_SZ: Not used any more,they all included in [default]
				lpvaluedata=GlobalAlloc(LPTR,size+5); //5 is enough
				lstrcpy(lpvaluedata,": \"");
				if(lpValueContent->lpvaluedata!=NULL) //added in 1.62? not compiled
					lstrcat(lpvaluedata,lpValueContent->lpvaluedata);
				lstrcat(lpvaluedata,"\"");
				//wsprintf has a bug that can not print string too long one time!);
				//wsprintf(lpvaluedata,"%s%s%s",": \"",lpValueContent->lpvaluedata,"\"");
				break;
		case	REG_MULTI_SZ:
				//Be sure to add below line outside of following "if",
				//for that GlobalFree(lp) must had lp already located!
				lpvaluedata=GlobalAlloc(LPTR,size+5);//5 is enough
				for(c=0;c<size;c++)
				{
					if (*((LPBYTE)(lpValueContent->lpvaluedata+c))==0)
					{
						if (*((LPBYTE)(lpValueContent->lpvaluedata+c+1))!=0)
							*((LPBYTE)(lpValueContent->lpvaluedata+c))=0x20; ////???????
						else
						   break;
					}
				}
				//*((LPBYTE)(lpValueContent->lpvaluedata+size))=0x00; // for some illegal multisz
				lstrcpy(lpvaluedata,": '");
				lstrcat(lpvaluedata,lpValueContent->lpvaluedata);
				lstrcat(lpvaluedata,"'");
				//wsprintf(lpvaluedata,"%s%s%s",": \"",lpValueContent->lpvaluedata,"\"");
				break;
		case	REG_DWORD:
		//case	REG_DWORD_BIG_ENDIAN: Not used any more,they all included in [default]
				lpvaluedata=GlobalAlloc(LPTR,13); //13 is enough
				wsprintf(lpvaluedata,"%s%08X",": 0x",*(LPDWORD)(lpValueContent->lpvaluedata));
				break;
		default	:
				lpvaluedata=GlobalAlloc(LPTR,3*(size+1)); //3*(size+1) is enough
				*lpvaluedata=0x3a;
				//for the resttype lengthofvaluedata doesn't contains the 0!
				for	(c=0;c<size;c++)
					wsprintf(lpvaluedata+3*c+1," %02X",*(lpValueContent->lpvaluedata+c));
	}
	return lpvaluedata;
}


//-------------------------------------------------------------
//Routine to Get Whole Value Data from VALUECONTENT
//-------------------------------------------------------------
LPSTR	GetWholeValueData(LPVALUECONTENT lpValueContent)
{
	LPSTR	lpvaluedata=NULL;
	DWORD	c,size=lpValueContent->datasize;
	switch(lpValueContent->typecode)
	{

		case	REG_SZ:
		case	REG_EXPAND_SZ:
				if(lpValueContent->lpvaluedata!=NULL) //added in 1.62 not compiled
				{
				if(size==(DWORD)lstrlen(lpValueContent->lpvaluedata)+1)
					lpvaluedata=TransData(lpValueContent,REG_SZ);
				else
					lpvaluedata=TransData(lpValueContent,REG_BINARY);
				}
				else
					lpvaluedata=TransData(lpValueContent,REG_SZ);
				break;
		case	REG_MULTI_SZ:
				if(*((LPBYTE)(lpValueContent->lpvaluedata))!=0x00)
				{
					for(c=0;;c++)
					{
						if (*((LPWORD)(lpValueContent->lpvaluedata+c))==0)
						   break;
					}
					if(size==c+2)
						lpvaluedata=TransData(lpValueContent,REG_MULTI_SZ);
					else
						lpvaluedata=TransData(lpValueContent,REG_BINARY);
				}
				else
					lpvaluedata=TransData(lpValueContent,REG_BINARY);
				break;
		case	REG_DWORD:
		case	REG_DWORD_BIG_ENDIAN:
				if(size==SIZEOFREG_DWORD)
					lpvaluedata=TransData(lpValueContent,REG_DWORD);
				else
					lpvaluedata=TransData(lpValueContent,REG_BINARY);
				break;
		default	:
				lpvaluedata=TransData(lpValueContent,REG_BINARY);	
	}
	return lpvaluedata;
}


//-------------------------------------------------------------
//Routine to create new compare result,distribute to different lp???MODI
//-------------------------------------------------------------

void	CreateNewResult(DWORD actiontype,LPDWORD lpcount,LPSTR lpresult)
{
	LPCOMRESULT	lpnew;
	lpnew=(LPCOMRESULT)GlobalAlloc(LPTR,sizeof(COMRESULT));
	lpnew->lpresult=lpresult;
	
	switch(actiontype)
	{
		case KEYADD:
				*lpcount==0 ? (lpKEYADDHEAD=lpnew):(lpKEYADD->lpnextresult=lpnew);
				lpKEYADD=lpnew;
				break;
		case KEYDEL:
				*lpcount==0 ? (lpKEYDELHEAD=lpnew):(lpKEYDEL->lpnextresult=lpnew);
				lpKEYDEL=lpnew;
				break;
		case VALADD:
				*lpcount==0 ? (lpVALADDHEAD=lpnew):(lpVALADD->lpnextresult=lpnew);
				lpVALADD=lpnew;
				break;
		case VALDEL:
				*lpcount==0 ? (lpVALDELHEAD=lpnew):(lpVALDEL->lpnextresult=lpnew);
				lpVALDEL=lpnew;
				break;
		case VALMODI:
				*lpcount==0 ? (lpVALMODIHEAD=lpnew):(lpVALMODI->lpnextresult=lpnew);
				lpVALMODI=lpnew;
				break;
		case FILEADD:
				*lpcount==0 ? (lpFILEADDHEAD=lpnew):(lpFILEADD->lpnextresult=lpnew);
				lpFILEADD=lpnew;
				break;
		case FILEDEL:
				*lpcount==0 ? (lpFILEDELHEAD=lpnew):(lpFILEDEL->lpnextresult=lpnew);
				lpFILEDEL=lpnew;
				break;
		case FILEMODI:
				*lpcount==0 ? (lpFILEMODIHEAD=lpnew):(lpFILEMODI->lpnextresult=lpnew);
				lpFILEMODI=lpnew;
				break;
		case DIRADD:
				*lpcount==0 ? (lpDIRADDHEAD=lpnew):(lpDIRADD->lpnextresult=lpnew);
				lpDIRADD=lpnew;
				break;
		case DIRDEL:
				*lpcount==0 ? (lpDIRDELHEAD=lpnew):(lpDIRDEL->lpnextresult=lpnew);
				lpDIRDEL=lpnew;
				break;
		case DIRMODI:
				*lpcount==0 ? (lpDIRMODIHEAD=lpnew):(lpDIRMODI->lpnextresult=lpnew);
				lpDIRMODI=lpnew;
				break;

	}
	(*lpcount)++;
}


//-------------------------------------------------------------
//Write compare results into memory and call CreateNewResult()
//-------------------------------------------------------------
void	LogToMem(DWORD actiontype,LPDWORD lpcount,LPVOID lp)
{
	LPSTR	lpname,lpdata,lpall;
	if(actiontype==KEYADD||actiontype==KEYDEL)
	{
		lpname=GetWholeKeyName(lp);
		CreateNewResult(actiontype,lpcount,lpname);
	}

	else
	{
		if(actiontype==VALADD||actiontype==VALDEL||actiontype==VALMODI)
		{
		
			lpname=GetWholeValueName(lp);
			lpdata=GetWholeValueData(lp);
			lpall=GlobalAlloc(LMEM_FIXED,lstrlen(lpname)+lstrlen(lpdata)+2);
			//do not use:wsprintf(lpall,"%s%s",lpname,lpdata); !!! strlen limit!
			lstrcpy(lpall,lpname);
			lstrcat(lpall,lpdata);
			GlobalFree(lpname);
			GlobalFree(lpdata);
			CreateNewResult(actiontype,lpcount,lpall);
		}
		else
		{
			lpname=GetWholeFileName(lp);
			CreateNewResult(actiontype,lpcount,lpname);
		}

	}
}


//-------------------------------------------------------------
//Routine to walk through sub keytree of current Key
//-------------------------------------------------------------
void	GetAllSubName(
					  BOOL needbrother,
					  DWORD typekey,DWORD typevalue,
					  LPDWORD lpcountkey,LPDWORD lpcountvalue,
					  LPKEYCONTENT lpKeyContent
					  )
{

	LPVALUECONTENT lpv;
	LogToMem(typekey,lpcountkey,lpKeyContent);
	
	if(lpKeyContent->lpfirstsubkey!=NULL)
	{
		GetAllSubName(TRUE,typekey,typevalue,lpcountkey,lpcountvalue,lpKeyContent->lpfirstsubkey);
	}
		
	if(needbrother==TRUE)
		if(lpKeyContent->lpbrotherkey!=NULL)
		{
			GetAllSubName(TRUE,typekey,typevalue,lpcountkey,lpcountvalue,lpKeyContent->lpbrotherkey);
		}

	for(lpv=lpKeyContent->lpfirstvalue;lpv!=NULL;lpv=lpv->lpnextvalue)
	{
		LogToMem(typevalue,lpcountvalue,lpv);
	}
}

//-------------------------------------------------------------
//Routine to walk through all values of current key
//-------------------------------------------------------------
void	GetAllValue(DWORD typevalue,LPDWORD lpcountvalue,LPKEYCONTENT lpKeyContent)
{
	LPVALUECONTENT lpv;
	for(lpv=lpKeyContent->lpfirstvalue;lpv!=NULL;lpv=lpv->lpnextvalue)
	{
		LogToMem(typevalue,lpcountvalue,lpv);
	}
}

//-------------------------------------------------------------
//Routine to walk through all Sub tree of current Directory [File system]
//-------------------------------------------------------------
void	GetAllSubFile(
					  BOOL needbrother,
					  DWORD typedir,DWORD typefile,
					  LPDWORD lpcountdir,LPDWORD lpcountfile,
					  LPFILECONTENT lpFileContent
					  )
{

	if((lpFileContent->fileattr&FILE_ATTRIBUTE_DIRECTORY)==FILE_ATTRIBUTE_DIRECTORY)
		LogToMem(typedir,lpcountdir,lpFileContent);
	else
		LogToMem(typefile,lpcountfile,lpFileContent);

	if(lpFileContent->lpfirstsubfile!=NULL //tfx 避免目录扫描时产生"."和".."
		&&lstrcmp(lpFileContent->lpfirstsubfile->lpfilename,".")!=0
		&&lstrcmp(lpFileContent->lpfirstsubfile->lpfilename,"..")!=0)
	{
		GetAllSubFile(TRUE,typedir,typefile,lpcountdir,lpcountfile,lpFileContent->lpfirstsubfile);
	}
	if(needbrother==TRUE)
	{
		if(lpFileContent->lpbrotherfile!=NULL)
			GetAllSubFile(TRUE,typedir,typefile,lpcountdir,lpcountfile,lpFileContent->lpbrotherfile);
	}

	
}

//-------------------------------------------------------------
//Routine to Free All Compare Results [Release Memory]
//-------------------------------------------------------------
void	FreeAllCom(LPCOMRESULT lpComResult)
{
	LPCOMRESULT lp,lpold;
	for(lp=lpComResult;lp!=NULL;)
	{
		if(lp->lpresult!=NULL)
			GlobalFree(lp->lpresult);
		lpold=lp;
		lp=lp->lpnextresult;
		GlobalFree(lpold);
	}
	
}

//-------------------------------------------------------------
//Routine to Free All Keys and Values
//-------------------------------------------------------------
void FreeAllKey(LPKEYCONTENT lpKey)
{
	LPVALUECONTENT lpv,lpvold;
	if(lpKey!=NULL)
	{
		FreeAllKey(lpKey->lpfirstsubkey);
		FreeAllKey(lpKey->lpbrotherkey);
		for(lpv=lpKey->lpfirstvalue;lpv!=NULL;)
		{
			GlobalFree(lpv->lpvaluename);
			if(lpv->lpvaluedata!=NULL)
				GlobalFree(lpv->lpvaluedata);
			lpvold=lpv;
			lpv=lpv->lpnextvalue;
			GlobalFree(lpvold);
		}
		GlobalFree(lpKey->lpkeyname);
		GlobalFree(lpKey);
	}

}

//-------------------------------------------------------------
//Routines to Free All File tree
//-------------------------------------------------------------
void FreeAllFileContent(LPFILECONTENT lpFile)
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
//Routines to Free Head Files
//-------------------------------------------------------------
void FreeAllFileHead(LPHEADFILE lp)
{
	if(lp!=NULL)
	{
		FreeAllFileHead(lp->lpnextheadfile);
		FreeAllFileContent(lp->lpHeadFile1);
		FreeAllFileContent(lp->lpHeadFile2);
		GlobalFree(lp);
	}

}


//-------------------------------------------------------------
//Clear Flag previous made by Compare Routine for the next compare
//-------------------------------------------------------------
VOID ClearKeyMatchTag(LPKEYCONTENT lpKey)
{
	LPVALUECONTENT lpv;
	if(lpKey!=NULL)
	{
		lpKey->bkeymatch=0;
		for(lpv=lpKey->lpfirstvalue;lpv!=NULL;lpv=lpv->lpnextvalue)
		{
			lpv->bvaluematch=0;
		}
		
		if(lpKey->lpfirstsubkey!=NULL)
		{
			ClearKeyMatchTag(lpKey->lpfirstsubkey);
		}
		
		if(lpKey->lpbrotherkey!=NULL)
		{
			ClearKeyMatchTag(lpKey->lpbrotherkey);
		}
	}
}

//////////////////////////////////////
void FreeAllKeyContent1(void)
{

	if(is1LoadFromHive)
	{
		GlobalFree(lpTempHive1);
		lpTempHive1=NULL;
	}
	else
	{
		FreeAllKey(lpHeadLocalMachine1);
		FreeAllKey(lpHeadUsers1);
	}
	lpHeadLocalMachine1=NULL;lpHeadUsers1=NULL;
	*lpComputerName1=0;*lpUserName1=0;

}
void FreeAllKeyContent2(void)
{
	
	if(is2LoadFromHive)
	{
		GlobalFree(lpTempHive2);
		lpTempHive2=NULL;
	}
	else
	{
	 	FreeAllKey(lpHeadLocalMachine2);
		FreeAllKey(lpHeadUsers2);
	}
	lpHeadLocalMachine2=NULL;lpHeadUsers2=NULL;
	*lpComputerName2=0;*lpUserName2=0;

}	
void FreeAllCompareResults(void)
{	
	FreeAllCom(lpKEYADDHEAD);
	FreeAllCom(lpKEYDELHEAD);
	FreeAllCom(lpVALADDHEAD);
	FreeAllCom(lpVALDELHEAD);
	FreeAllCom(lpVALMODIHEAD);
	FreeAllCom(lpFILEADDHEAD);
	FreeAllCom(lpFILEDELHEAD);
	FreeAllCom(lpFILEMODIHEAD);
	FreeAllCom(lpDIRADDHEAD);
	FreeAllCom(lpDIRDELHEAD);
	FreeAllCom(lpDIRMODIHEAD);
	

	nKEYADD=0;nKEYDEL=0;nVALADD=0;nVALDEL=0;nVALMODI=0;nFILEADD=0;nFILEDEL=0;nFILEMODI=0;nDIRADD=0;nDIRDEL=0;nDIRMODI=0;
	lpKEYADDHEAD=NULL;lpKEYDELHEAD=NULL;lpVALADDHEAD=NULL;lpVALDELHEAD=NULL;lpVALMODIHEAD=NULL;
	lpFILEADDHEAD=NULL;lpFILEDELHEAD=NULL;lpFILEMODIHEAD=NULL;lpDIRADDHEAD=NULL;lpDIRDELHEAD=NULL;lpDIRMODIHEAD=NULL;
}
void FreeAllFiles(void)
{
	FreeAllFileHead(lpHeadFile);
	lpHeadFile=NULL;
}


//-------------------------------------------------------------
//That is the Main Routine of File Compare Engine!!
//-------------------------------------------------------------
void	CompareFirstSubFile(LPFILECONTENT lpHead1,LPFILECONTENT lpHead2)
{
	LPFILECONTENT	lp1,lp2;
	for(lp1=lpHead1;lp1!=NULL;lp1=lp1->lpbrotherfile)
	{
		for(lp2=lpHead2;lp2!=NULL;lp2=lp2->lpbrotherfile)
		{
			if((lp2->bfilematch==NOTMATCH)&&lstrcmp(lp1->lpfilename,lp2->lpfilename)==0)
			{	//Two 'FILE's have same name,but I am not sure they are same,so we compare it!
				if((lp1->fileattr&FILE_ATTRIBUTE_DIRECTORY)!=FILE_ATTRIBUTE_DIRECTORY&&(lp2->fileattr&FILE_ATTRIBUTE_DIRECTORY)!=FILE_ATTRIBUTE_DIRECTORY)
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
					if((lp1->fileattr&FILE_ATTRIBUTE_DIRECTORY)==FILE_ATTRIBUTE_DIRECTORY&&(lp2->fileattr&FILE_ATTRIBUTE_DIRECTORY)==FILE_ATTRIBUTE_DIRECTORY)
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
						if((lp1->fileattr&FILE_ATTRIBUTE_DIRECTORY)!=FILE_ATTRIBUTE_DIRECTORY&&(lp2->fileattr&FILE_ATTRIBUTE_DIRECTORY)==FILE_ATTRIBUTE_DIRECTORY)
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
			if((lp1->fileattr&FILE_ATTRIBUTE_DIRECTORY)==FILE_ATTRIBUTE_DIRECTORY)
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
			if((lp2->fileattr&FILE_ATTRIBUTE_DIRECTORY)==FILE_ATTRIBUTE_DIRECTORY)
				GetAllSubFile(FALSE,DIRADD,FILEADD,&nDIRADD,&nFILEADD,lp2);
			else
				LogToMem(FILEADD,&nFILEADD,lp2);
		}
	}
	// Progress bar update
	if (nGettingFile!=0)
	if (nComparing%nGettingFile>nComparingStep)
	{
		nComparing=0;
		SendDlgItemMessage(hWnd,IDC_PBCOMPARE,PBM_STEPIT,(WPARAM)0,(LPARAM)0);
	}

}
				
				

//-------------------------------------------------------------
//That is the Main Point of Registry Compare Engine
//-------------------------------------------------------------

VOID * CompareFirstSubKey(LPKEYCONTENT lpHead1,LPKEYCONTENT lpHead2)
{
	LPKEYCONTENT	lp1,lp2;
	LPVALUECONTENT	lpvalue1,lpvalue2;
	DWORD	i;
	
	for(lp1=lpHead1;lp1!=NULL;lp1=lp1->lpbrotherkey)
	{
		for(lp2=lpHead2;lp2!=NULL;lp2=lp2->lpbrotherkey)
		{
			if((lp2->bkeymatch==NOTMATCH)&&lstrcmp(lp1->lpkeyname,lp2->lpkeyname)==0)
			{	//Same key found! we compare their values and their subkeys!

				lp2->bkeymatch=ISMATCH;
				if (lp1->lpfirstvalue==NULL&&lp2->lpfirstvalue!=NULL)
				{	//Key1 has no values,so lpvalue2 is added! we find all values belongs to lp2!
					GetAllValue(VALADD,&nVALADD,lp2);
				}
				else
				{
					if (lp1->lpfirstvalue!=NULL&&lp2->lpfirstvalue==NULL)
					{	//Key2 has no values,so lpvalue1 is delted! we find all values belongs to lp1!
						GetAllValue(VALDEL,&nVALDEL,lp1);
					}
					else
					{	//Two keys all has values,so we loop them

						for(lpvalue1=lp1->lpfirstvalue;lpvalue1!=NULL;lpvalue1=lpvalue1->lpnextvalue)
						{
							for(lpvalue2=lp2->lpfirstvalue;lpvalue2!=NULL;lpvalue2=lpvalue2->lpnextvalue)
							{	//Loop lp2 to find a value matchs lp1's
								if((lpvalue2->bvaluematch==NOTMATCH)&&(lpvalue1->typecode==lpvalue2->typecode))
								{	//Same valuedata type
									if(lstrcmp(lpvalue1->lpvaluename,lpvalue2->lpvaluename)==0)
									{	//Same valuename
										if(lpvalue1->datasize==lpvalue2->datasize)
										{	//Same size of valuedata
											for(i=0;i<lpvalue1->datasize;i++)
											{
												if (*((lpvalue1->lpvaluedata)+i)!=*((lpvalue2->lpvaluedata)+i))
													break;
											}
											if (i==lpvalue1->datasize)
											{	//Same valuedata,keys are the same!
												
												lpvalue2->bvaluematch=ISMATCH;
												break;//Be sure not to do lp2==NULL
											}
											else
											{	//Valuedata not match due to data mismatch!,we found a modified valuedata!*****
												lpvalue2->bvaluematch=ISMODI;
												LogToMem(VALMODI,&nVALMODI,lpvalue1);
												LogToMem(VALMODI,&nVALMODI,lpvalue2);
												nVALMODI--;
												break;
											}
										}
										else
										{	//Waluedata does not match due to size,we found a modified valuedata!******
											lpvalue2->bvaluematch=ISMODI;
											LogToMem(VALMODI,&nVALMODI,lpvalue1);
											LogToMem(VALMODI,&nVALMODI,lpvalue2);
											nVALMODI--;
											break;
										}
									}
								}
							}
							if(lpvalue2==NULL)
							{	//We found a value in lp1 but not in lp2,we found a deleted value*****
								LogToMem(VALDEL,&nVALDEL,lpvalue1);
							}
						}
						//After we loop to end,we do extra loop use flag we previouse made to get added values
						for(lpvalue2=lp2->lpfirstvalue;lpvalue2!=NULL;lpvalue2=lpvalue2->lpnextvalue)
						{
							if(lpvalue2->bvaluematch!=ISMATCH&&lpvalue2->bvaluematch!=ISMODI)
							{	//We found a value in lp2's but not in lp1's,we found a added value****
								LogToMem(VALADD,&nVALADD,lpvalue2);
					
							}
						}
					}
				}
				
				//////////////////////////////////////////////////////////////
				//After we walk through the values above,we now try to loop the sub keys of current key
				if(lp1->lpfirstsubkey==NULL&&lp2->lpfirstsubkey!=NULL)
				{	//lp2's firstsubkey added!
					GetAllSubName(TRUE,KEYADD,VALADD,&nKEYADD,&nVALADD,lp2->lpfirstsubkey);
				}
				if(lp1->lpfirstsubkey!=NULL&&lp2->lpfirstsubkey==NULL)
				{	//lp1's firstsubkey deleted!
					GetAllSubName(TRUE,KEYDEL,VALDEL,&nKEYDEL,&nVALDEL,lp1->lpfirstsubkey);
				}
				if(lp1->lpfirstsubkey!=NULL&&lp2->lpfirstsubkey!=NULL)
					CompareFirstSubKey(lp1->lpfirstsubkey,lp2->lpfirstsubkey);
				break;
			}
		}
		if(lp2==NULL)
		{	//We did not find a lp2 matches a lp1,so lp1 is deleted!
			GetAllSubName(FALSE,KEYDEL,VALDEL,&nKEYDEL,&nVALDEL,lp1);
		}

	}
	
	//After we loop to end,we do extra loop use flag we previouse made to get added keys
	for(lp2=lpHead2;lp2!=NULL;lp2=lp2->lpbrotherkey) //->lpbrotherkey
	{
		nComparing++;
		if(lp2->bkeymatch==NOTMATCH)
		{   //We did not find a lp1 matches a lp2,so lp2 is added!
			GetAllSubName(FALSE,KEYADD,VALADD,&nKEYADD,&nVALADD,lp2);
		}
	}

  	// Progress bar update
	if (nGettingKey!=0)
	if (nComparing%nGettingKey>nComparingStep)
	{
		nComparing=0;
		SendDlgItemMessage(hWnd,IDC_PBCOMPARE,PBM_STEPIT,(WPARAM)0,(LPARAM)0);
	}

	return NULL;
}

//------------------------------------------------------------
//Several routines to write to output file
//------------------------------------------------------------
void	WriteHead(LPSTR lpstr,DWORD count,BOOL isHTML)
{
	unsigned char lpcount[8];
	wsprintf(lpcount,"%d",count);
	if(isHTML==TRUE)
	{
		WriteFile(hFile,lpBR,lstrlen(lpBR),&NumberOfBytesWritten,NULL);
		WriteFile(hFile,lpTableBegin,lstrlen(lpTableBegin),&NumberOfBytesWritten,NULL);
		WriteFile(hFile,lpTd1Begin,lstrlen(lpTd1Begin),&NumberOfBytesWritten,NULL);
	}
	else
		WriteFile(hFile,str_line,lstrlen(str_line),&NumberOfBytesWritten,NULL);
	WriteFile(hFile,lpstr,lstrlen(lpstr),&NumberOfBytesWritten,NULL);
	WriteFile(hFile,lpcount,lstrlen(lpcount),&NumberOfBytesWritten,NULL);
	if(isHTML==TRUE)
	{
		WriteFile(hFile,lpTd1Over,lstrlen(lpTd1Over),&NumberOfBytesWritten,NULL);
		WriteFile(hFile,lpTableOver,lstrlen(lpTableOver),&NumberOfBytesWritten,NULL);
	}
	else
		WriteFile(hFile,str_line,lstrlen(str_line),&NumberOfBytesWritten,NULL);
}

//------------------------------------------------------------
void	WritePart(LPCOMRESULT lpcomhead,BOOL isHTML)
{
	DWORD n,nLen;
	LPBYTE lpstr;
	LPCOMRESULT lp;
	if(isHTML)
	{
		WriteFile(hFile,lpTableBegin,lstrlen(lpTableBegin),&NumberOfBytesWritten,NULL);
		WriteFile(hFile,lpTd2Begin,lstrlen(lpTd2Begin),&NumberOfBytesWritten,NULL);
	}

	for(lp=lpcomhead;lp!=NULL;lp=lp->lpnextresult)
	{
		nLen=lstrlen(lp->lpresult);
		lpstr=lp->lpresult;
		for(n=0;nLen>0;)
		{
			nLen<HTMLWRAPLENGTH? (n=nLen):(n=HTMLWRAPLENGTH);

			WriteFile(hFile,lpstr,n,&NumberOfBytesWritten,NULL);
			lpstr=lpstr+n;
			nLen=nLen-n;
			//WriteFile(hFile,lp->lpresult,lstrlen(lp->lpresult),&NumberOfBytesWritten,NULL);
			if(isHTML)
				WriteFile(hFile,lpBR,lstrlen(lpBR),&NumberOfBytesWritten,NULL);
			//else
			//	WriteFile(hFile,lpCR,2,&NumberOfBytesWritten,NULL);
			// for some reason,txt don't wrap anymore since 1.50e,check below!
		}
		if(!isHTML)
			WriteFile(hFile,lpCR,2,&NumberOfBytesWritten,NULL); //this!

	}
	if(isHTML)
	{
		WriteFile(hFile,lpTd2Over,lstrlen(lpTd2Over),&NumberOfBytesWritten,NULL);
		WriteFile(hFile,lpTableOver,lstrlen(lpTableOver),&NumberOfBytesWritten,NULL);
	}

}
//------------------------------------------------------------
void	WriteTitle(LPSTR lph,LPSTR lpb,BOOL isHTML)
{
	if(isHTML)
	{
		WriteFile(hFile,lpTableBegin,lstrlen(lpTableBegin),&NumberOfBytesWritten,NULL);
		WriteFile(hFile,lpTd1Begin,lstrlen(lpTd1Begin),&NumberOfBytesWritten,NULL);
	}
	WriteFile(hFile,lph,lstrlen(lph),&NumberOfBytesWritten,NULL);
	WriteFile(hFile,lpb,lstrlen(lpb),&NumberOfBytesWritten,NULL);
	if(isHTML)
	{
		WriteFile(hFile,lpTd1Over,lstrlen(lpTd1Over),&NumberOfBytesWritten,NULL);
		WriteFile(hFile,lpTableOver,lstrlen(lpTableOver),&NumberOfBytesWritten,NULL);
	}
	else
		WriteFile(hFile,lpCR,2,&NumberOfBytesWritten,NULL);
}
//------------------------------------------------------------
//routine to replace invalid chars in comment fields
//------------------------------------------------------------

BOOL ReplaceInValidFileName(LPSTR lpf)
{
	LPSTR	lpInvalid="\\/:*?\"<>|";
	DWORD	i,j,nLen;
	BOOL	bNoSpace=FALSE;
	nLen=lstrlen(lpf);
	for(i=0;i<nLen;i++)
	{
		for(j=0;j<9;j++)
		{
			if (*(lpf+i)==*(lpInvalid+j))
				*(lpf+i)=0x2D;// check for invalid chars and replace it (return FALSE;)
			else
				if(*(lpf+i)!=0x20&&*(lpf+i)!=0x09) //At least one non-space,non-tab char needed!
					bNoSpace=TRUE;

		}
	}
	return bNoSpace;
}
				
//////////////////////////////////////////////////////////////////
VOID PreShowProgressBar(VOID)
{
	nSavingKey=0;
	nSavingStep=nGettingKey/100;
	nComparing=0;
	nComparingStep=nGettingKey/100;
	SendDlgItemMessage(hWnd,IDC_PBCOMPARE,PBM_SETRANGE,(WPARAM)0,MAKELPARAM(0,MAXPBPOSITION));
	SendDlgItemMessage(hWnd,IDC_PBCOMPARE,PBM_SETPOS,(WPARAM)0,(LPARAM)0);
	SendDlgItemMessage(hWnd,IDC_PBCOMPARE,PBM_SETSTEP,(WPARAM)1,(LPARAM)0);
	ShowWindow(GetDlgItem(hWnd,IDC_PBCOMPARE),SW_SHOW);
}

/**/

//------------------------------------------------------------
//routine to call Compare Engine
//------------------------------------------------------------
BOOL RegCompare(HWND hDlg)
{
	BOOL	isHTML;//,bSaveWithCommentName;
	LPSTR	lpstrcomp,lpExt,lpDestFileName;
	DWORD	buffersize=2048,nTotal,filetail=0,nLengthofStr;
	LPHEADFILE	lphf;
	FILETIME ftime1,ftime2;

	PreShowProgressBar();

	
	SystemTimeToFileTime(lpSystemtime1,&ftime1);
	SystemTimeToFileTime(lpSystemtime2,&ftime2);
	if(CompareFileTime(&ftime1,&ftime2)<=0)
	{
		CompareFirstSubKey(lpHeadLocalMachine1,lpHeadLocalMachine2);
		CompareFirstSubKey(lpHeadUsers1,lpHeadUsers2);
	}
	else
	{
		CompareFirstSubKey(lpHeadLocalMachine2,lpHeadLocalMachine1);
		CompareFirstSubKey(lpHeadUsers2,lpHeadUsers1);
	}


	
	SendDlgItemMessage(hDlg,IDC_PBCOMPARE,PBM_SETPOS,(WPARAM)0,(LPARAM)0);

	nComparingStep=nGettingFile/100;
	
	if(!is1LoadFromHive&&!is2LoadFromHive)
	for(lphf=lpHeadFile;lphf!=NULL;lphf=lphf->lpnextheadfile)
	{
		if(lphf->lpHeadFile1!=NULL&&lphf->lpHeadFile2!=NULL)
		{
			CompareFirstSubFile(lphf->lpHeadFile1,lphf->lpHeadFile2);
		}

	}
	SendDlgItemMessage(hDlg,IDC_PBCOMPARE,PBM_SETPOS,(WPARAM)MAXPBPOSITION,(LPARAM)0);

	
	if(SendMessage(GetDlgItem(hDlg,IDC_RADIO1),BM_GETCHECK,(WPARAM)0,(LPARAM)0)==1)
	{
		isHTML=FALSE;
		lpExt=".txt";
	}
	else
	{
		isHTML=TRUE;
		lpExt=".htm";
	}
	
	lpDestFileName=GlobalAlloc(LPTR,MAX_PATH*2);
	lpstrcomp=GlobalAlloc(LPTR,buffersize); //buffersize must> commentlength+10 .txt 0000
	GetDlgItemText(hDlg,IDC_EDITCOMMENT,lpstrcomp,COMMENTLENGTH);
	GetDlgItemText(hDlg,IDC_EDITPATH,lpOutputpath,MAX_PATH);
	
	nLengthofStr=lstrlen(lpOutputpath);

	if (nLengthofStr>0&&*(lpOutputpath+nLengthofStr-1)!='\\')
	{
		*(lpOutputpath+nLengthofStr)='\\';
		*(lpOutputpath+nLengthofStr+1)=0x00; //bug found by "itschy" <itschy@lycos.de> 1.61d->1.61e
		nLengthofStr++;
	}
	lstrcpy(lpDestFileName,lpOutputpath);

	//bSaveWithCommentName=TRUE;
	if	(ReplaceInValidFileName(lpstrcomp))
		lstrcat(lpDestFileName,lpstrcomp);
	else
		lstrcat(lpDestFileName,strDefResPre);
	
	nLengthofStr=lstrlen(lpDestFileName);
	lstrcat(lpDestFileName,lpExt);
	hFile = CreateFile(lpDestFileName,GENERIC_READ | GENERIC_WRITE,FILE_SHARE_READ | FILE_SHARE_WRITE,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
	if( hFile == INVALID_HANDLE_VALUE) 
	{
		for (filetail=0;filetail<MAXAMOUNTOFFILE;filetail++)
		{
			wsprintf(lpDestFileName+nLengthofStr,"%04d",filetail);
			//*(lpDestFileName+nLengthofStr+4)=0x00;
			lstrcpy(lpDestFileName+nLengthofStr+4,lpExt);

			hFile = CreateFile(lpDestFileName,GENERIC_READ | GENERIC_WRITE,FILE_SHARE_READ | FILE_SHARE_WRITE,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
			if( hFile == INVALID_HANDLE_VALUE) 
			{
				if (GetLastError()==ERROR_FILE_EXISTS)	//My God! I use stupid ERROR_ALREADY_EXISTS first!!
					continue;
				else
				{
					MyErrorMessage(hDlg,str_errorcreatefile);
					return FALSE;
				}
			}
			else
			{
				break;
			}
		}
		if (filetail>=MAXAMOUNTOFFILE)
		{
			MyErrorMessage(hDlg,str_errorcreatefile);
			return FALSE;
		}

	}

		if(isHTML==TRUE)
		{
			WriteFile(hFile,lpHTMLbegin,lstrlen(lpHTMLbegin),&NumberOfBytesWritten,NULL);
			WriteFile(hFile,lpHEADbegin,lstrlen(lpHEADbegin),&NumberOfBytesWritten,NULL);
			//WriteFile(hFile,lpTITLEbegin,lstrlen(lpTITLEbegin),&NumberOfBytesWritten,NULL);
		}
		if(isHTML==TRUE)
		{
			//WriteFile(hFile,lpTITLEover,lstrlen(lpTITLEover),&NumberOfBytesWritten,NULL);
			WriteFile(hFile,lpstyle,lstrlen(lpstyle),&NumberOfBytesWritten,NULL);
			WriteFile(hFile,lpHEADover,lstrlen(lpHEADover),&NumberOfBytesWritten,NULL);
			WriteFile(hFile,lpBodyBegin,lstrlen(lpBodyBegin),&NumberOfBytesWritten,NULL);
		}
		WriteFile(hFile,str_lognote,lstrlen(str_lognote),&NumberOfBytesWritten,NULL);

		//_asm int 3;
		GetDlgItemText(hDlg,IDC_EDITCOMMENT,lpstrcomp,COMMENTLENGTH);
		WriteTitle(str_comments,lpstrcomp,isHTML);

		
		wsprintf(lpstrcomp,"%d%s%d%s%d %02d%s%02d%s%02d %s %d%s%d%s%d %02d%s%02d%s%02d",
			lpSystemtime1->wYear,"/",
			lpSystemtime1->wMonth,"/",
			lpSystemtime1->wDay,
			lpSystemtime1->wHour,":",
			lpSystemtime1->wMinute,":",
			lpSystemtime1->wSecond," , ",
			lpSystemtime2->wYear,"/",
			lpSystemtime2->wMonth,"/",
			lpSystemtime2->wDay,
			lpSystemtime2->wHour,":",
			lpSystemtime2->wMinute,":",
			lpSystemtime2->wSecond

			);
			
		WriteTitle(str_datetime,lpstrcomp,isHTML);


		
		*lpstrcomp=0x00; //ZeroMemory(lpstrcomp,buffersize);
		//GetComputerName(lpstrcomp,&buffersize);
		lstrcpy(lpstrcomp,lpComputerName1);
		lstrcat(lpstrcomp," , ");
		lstrcat(lpstrcomp,lpComputerName2);
		WriteTitle(str_computer,lpstrcomp,isHTML);

		*lpstrcomp=0x00;//ZeroMemory(lpstrcomp,buffersize);
		//GetUserName(lpstrcomp,&buffersize);
		lstrcpy(lpstrcomp,lpUserName1);
		lstrcat(lpstrcomp," , ");
		lstrcat(lpstrcomp,lpUserName2);

		WriteTitle(str_username,lpstrcomp,isHTML);

		GlobalFree(lpstrcomp);

		//Write keydel part
		if(nKEYDEL!=0)
		{
			WriteHead(str_keydel,nKEYDEL,isHTML);
			WritePart(lpKEYDELHEAD,isHTML);
		}
		//Write keyadd part
		if(nKEYADD!=0)
		{
			WriteHead(str_keyadd,nKEYADD,isHTML);
			WritePart(lpKEYADDHEAD,isHTML);
		}
		//Write valdel part
		if(nVALDEL!=0)
		{
			WriteHead(str_valdel,nVALDEL,isHTML);
			WritePart(lpVALDELHEAD,isHTML);
		}
		//Write valadd part
		if(nVALADD!=0)
		{
			WriteHead(str_valadd,nVALADD,isHTML);
			WritePart(lpVALADDHEAD,isHTML);
		}
		//Write valmodi part
		if(nVALMODI!=0)
		{
			WriteHead(str_valmodi,nVALMODI,isHTML);
			WritePart(lpVALMODIHEAD,isHTML);
		}
		//Write file add part
		if(nFILEADD!=0)
		{
			WriteHead(str_fileadd,nFILEADD,isHTML);
			WritePart(lpFILEADDHEAD,isHTML);
		}
		//Write file del part
		if(nFILEDEL!=0)
		{
			WriteHead(str_filedel,nFILEDEL,isHTML);
			WritePart(lpFILEDELHEAD,isHTML);
		}
		//Write file modi part
		if(nFILEMODI!=0)
		{
			WriteHead(str_filemodi,nFILEMODI,isHTML);
			WritePart(lpFILEMODIHEAD,isHTML);
		}
		//Write directory add part
		if(nDIRADD!=0)
		{
			WriteHead(str_diradd,nDIRADD,isHTML);
			WritePart(lpDIRADDHEAD,isHTML);
		}
		//Write directory del part
		if(nDIRDEL!=0)
		{
			WriteHead(str_dirdel,nDIRDEL,isHTML);
			WritePart(lpDIRDELHEAD,isHTML);
		}
		//Write directory modi part
		if(nDIRMODI!=0)
		{
			WriteHead(str_dirmodi,nDIRMODI,isHTML);
			WritePart(lpDIRMODIHEAD,isHTML);
		}

		nTotal=nKEYADD+nKEYDEL+nVALADD+nVALDEL+nVALMODI+nFILEADD+nFILEDEL+nFILEMODI+nDIRADD+nDIRDEL+nDIRMODI;
		if(isHTML==TRUE)
		{
			WriteFile(hFile,lpBR,lstrlen(lpBR),&NumberOfBytesWritten,NULL);
		}
		WriteHead(str_total,nTotal,isHTML);
		if(isHTML==TRUE)
		{
			WriteFile(hFile,str_website,lstrlen(str_website),&NumberOfBytesWritten,NULL);
			WriteFile(hFile,lpBodyOver,lstrlen(lpBodyOver),&NumberOfBytesWritten,NULL);
			WriteFile(hFile,lpHTMLover,lstrlen(lpHTMLover),&NumberOfBytesWritten,NULL);
		}

	
		CloseHandle(hFile);
		
		if((DWORD)ShellExecute(hDlg,"open",lpDestFileName,NULL,NULL,SW_SHOW)<=32)
			MyErrorMessage(hDlg,str_errorexecviewer);
		GlobalFree(lpDestFileName);


return TRUE;
}

//////////////////////////////////////////////////////////////////
void	BusyRoutine(LPSTR title1,LPSTR title2,DWORD count1,DWORD count2)
{
	//nGettingTime=GetTickCount();
	nBASETIME1=nGettingTime;
	wsprintf(lpMESSAGE,"%s%d%s%d%s",str_time,(nGettingTime-nBASETIME)/1000,"s",(nGettingTime-nBASETIME)%1000,"ms");
	SendDlgItemMessage(hWnd,IDC_TEXTCOUNT3,WM_SETTEXT,(WPARAM)0,(LPARAM)lpMESSAGE);
	wsprintf(lpMESSAGE,"%s%d",title1,count1);
	SendDlgItemMessage(hWnd,IDC_TEXTCOUNT1,WM_SETTEXT,(WPARAM)0,(LPARAM)lpMESSAGE);
	wsprintf(lpMESSAGE,"%s%d",title2,count2);
	SendDlgItemMessage(hWnd,IDC_TEXTCOUNT2,WM_SETTEXT,(WPARAM)0,(LPARAM)lpMESSAGE);

	UpdateWindow(hWnd);
	PeekMessage(&msg,hWnd,WM_ACTIVATE,WM_ACTIVATE,PM_REMOVE);
	//SetForegroundWindow(hWnd);
}

/**/

//------------------------------------------------------------
//routine to call File compare engine
//------------------------------------------------------------
void	GetFilesSnap(HWND hDlg,LPFILECONTENT lpFatherFile)
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
	lpFileContent=GlobalAlloc(LPTR,sizeof(FILECONTENT));

	lpFileContent->lpfilename=GlobalAlloc(LPTR,lstrlen(finddata.cFileName)+1);
	lstrcpy(lpFileContent->lpfilename,finddata.cFileName);
	lpFileContent->writetimelow=finddata.ftLastWriteTime.dwLowDateTime;
	lpFileContent->writetimehigh=finddata.ftLastWriteTime.dwHighDateTime;
	lpFileContent->filesizelow=finddata.nFileSizeLow;
	lpFileContent->filesizehigh=finddata.nFileSizeHigh;
	lpFileContent->fileattr=finddata.dwFileAttributes;
	lpFileContent->lpfatherfile=lpFatherFile;
	lpFatherFile->lpfirstsubfile=lpFileContent;
	lpFileContentTemp=lpFileContent;
	if((lpFileContent->fileattr&FILE_ATTRIBUTE_DIRECTORY)==FILE_ATTRIBUTE_DIRECTORY)
	{
		if(lstrcmp(lpFileContent->lpfilename,".")!=0&&lstrcmp(lpFileContent->lpfilename,"..")!=0
			&&!IsInSkipList(lpFileContent->lpfilename,lpSnapFiles)) //tfx
		{
			
			nGettingDir++;
			GetFilesSnap(hDlg,lpFileContent);
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
		if((lpFileContent->fileattr&FILE_ATTRIBUTE_DIRECTORY)==FILE_ATTRIBUTE_DIRECTORY)
		{
			if(lstrcmp(lpFileContent->lpfilename,".")!=0&&lstrcmp(lpFileContent->lpfilename,"..")!=0
				&&!IsInSkipList(lpFileContent->lpfilename,lpSnapFiles)) //tfx
			{
				nGettingDir++;
				GetFilesSnap(hDlg,lpFileContent);
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
		BusyRoutine(str_dir,str_file,nGettingDir,nGettingFile);
	}

	return ;
}


/**/
//------------------------------------------------------------
//Routine to SCAN THE REGISTRY!!
//------------------------------------------------------------
VOID	*	GetRegistrySnap(hDlg,hkey,lpFatherKeyContent)
HWND	hDlg;	
HKEY	hkey;
LPKEYCONTENT	lpFatherKeyContent;

{
    
	HKEY  Subhkey;
	DWORD i,NTr;
	DWORD TypeCode;
	DWORD LengthOfKeyName;
	DWORD LengthOfValueName;
	DWORD LengthOfValueData;
	DWORD LengthOfLongestValueName;
	DWORD LengthOfLongestValueData;
	DWORD LengthOfLongestSubkeyName;
	LPSTR lpValueName;
	LPSTR lpValueData;
	LPKEYCONTENT	lpKeyContent,lpKeyContentLast;
	LPVALUECONTENT	lpValueContent,lpValueContentLast;

	//To detemine MAX length
	if(RegQueryInfoKey(
		hkey,
		NULL,						//lpClassName_nouse,
		NULL,						//&nClassName_nouse_length,
		NULL,
		NULL,						//&NumberOfSubkeys,
		&LengthOfLongestSubkeyName, //chars
		NULL,						//&nClassName_nouse_longestlength,
		NULL,						//&NumberOfValue,
		&LengthOfLongestValueName,	//chars
		&LengthOfLongestValueData,	//bytes
		NULL,						//&nSecurity_length_nouse,
		NULL						//&ftLastWrite
		)!=ERROR_SUCCESS)
		return NULL;
	LengthOfLongestSubkeyName =LengthOfLongestSubkeyName*2+3; //yeah,may be x+1 is enought! x=chars
	LengthOfLongestValueName  =LengthOfLongestValueName*2+3; //yeah,may be x+1 is enought! x=chars
	LengthOfLongestValueData  =LengthOfLongestValueData+1;
	lpValueName=GlobalAlloc(LMEM_FIXED,LengthOfLongestValueName);
	lpValueData=GlobalAlloc(LMEM_FIXED,LengthOfLongestValueData);

	//Get Values
	for(i=0;;i++)
	{
		
		*(LPBYTE)lpValueName=(BYTE)0x00;//That's the bug in 2000! thanks zhangl@digiark.com!
		*(LPBYTE)lpValueData=(BYTE)0x00;
		//DebugBreak();
		LengthOfValueName=LengthOfLongestValueName;
		LengthOfValueData=LengthOfLongestValueData;
		NTr=RegEnumValue(hkey,i,lpValueName,&LengthOfValueName,NULL,&TypeCode,lpValueData,&LengthOfValueData);
		if	(NTr==ERROR_NO_MORE_ITEMS)
			break;
		else 
		{
			if	(NTr!=ERROR_SUCCESS)
			{
				continue;
			}
		}

#ifdef DEBUGLOG		
		DebugLog("debug_trytogetvalue.log","trying:",hDlg,FALSE);
		DebugLog("debug_trytogetvalue.log",lpValueName,hDlg,TRUE);
#endif

		lpValueContent=GlobalAlloc(LPTR,sizeof(VALUECONTENT));
		//I had done if(i==0) in 1.50b- ! thanks fisttk@21cn.com and non-standard 
		if(lpFatherKeyContent->lpfirstvalue==NULL)
			lpFatherKeyContent->lpfirstvalue=lpValueContent;
		else
			lpValueContentLast->lpnextvalue=lpValueContent;
		lpValueContentLast=lpValueContent;
		lpValueContent->typecode=TypeCode;
		lpValueContent->datasize=LengthOfValueData;
		lpValueContent->lpfatherkey=lpFatherKeyContent;
		lpValueContent->lpvaluename=GlobalAlloc(LMEM_FIXED,lstrlen(lpValueName)+1);
		lstrcpy(lpValueContent->lpvaluename,lpValueName);

		if(LengthOfValueData!=0)
		{
			lpValueContent->lpvaluedata=GlobalAlloc(LMEM_FIXED,LengthOfValueData);
			CopyMemory(lpValueContent->lpvaluedata,lpValueData,LengthOfValueData);
			//	*(lpValueContent->lpvaluedata+LengthOfValueData)=0x00;
		}
		nGettingValue++;

#ifdef DEBUGLOG		
		lstrdb1=GlobalAlloc(LPTR,100);
		wsprintf(lstrdb1,"LGVN:%08d LGVD:%08d VN:%08d VD:%08d",LengthOfLongestValueName,LengthOfLongestValueData,LengthOfValueName,LengthOfValueData);
		DebugLog("debug_valuenamedata.log",lstrdb1,hDlg,TRUE);
		DebugLog("debug_valuenamedata.log",GetWholeValueName(lpValueContent),hDlg,FALSE);
		DebugLog("debug_valuenamedata.log",GetWholeValueData(lpValueContent),hDlg,TRUE);
		//DebugLog("debug_valuenamedata.log",":",hDlg,FALSE);
		//DebugLog("debug_valuenamedata.log",lpValueData,hDlg,TRUE);
		GlobalFree(lstrdb1);

#endif
	}

	GlobalFree(lpValueName);
	GlobalFree(lpValueData);
	
	for(i=0;;i++)
	{
		LengthOfKeyName=LengthOfLongestSubkeyName;
		*(LPBYTE)lpKeyName=(BYTE)0x00;
		NTr=RegEnumKeyEx(hkey,i,lpKeyName,&LengthOfKeyName,NULL,NULL,NULL,&ftLastWrite);
		if	(NTr==ERROR_NO_MORE_ITEMS)
			break;
		else 
		{
			if	(NTr!=ERROR_SUCCESS)
			{
				continue;
			}
		}
		lpKeyContent=GlobalAlloc(LPTR,sizeof(KEYCONTENT));
		if	(lpFatherKeyContent->lpfirstsubkey==NULL)
			lpFatherKeyContent->lpfirstsubkey=lpKeyContent;
		else
			lpKeyContentLast->lpbrotherkey=lpKeyContent;
		lpKeyContentLast=lpKeyContent;
		lpKeyContent->lpkeyname=GlobalAlloc(LMEM_FIXED,lstrlen(lpKeyName)+1);
		lstrcpy(lpKeyContent->lpkeyname,lpKeyName);
		lpKeyContent->lpfatherkey=lpFatherKeyContent;
		//DebugLog("debug_getkey.log",lpKeyName,hDlg,TRUE);

#ifdef DEBUGLOG		
		lstrdb1=GlobalAlloc(LPTR,100);
		wsprintf(lstrdb1,"LGKN:%08d KN:%08d",LengthOfLongestSubkeyName,LengthOfKeyName);
		DebugLog("debug_key.log",lstrdb1,hDlg,TRUE);
		DebugLog("debug_key.log",GetWholeKeyName(lpKeyContent),hDlg,TRUE);
		GlobalFree(lstrdb1);

#endif
		
		nGettingKey++;

		if(IsInSkipList(lpKeyName,lpSnapRegs)||RegOpenKeyEx(hkey,lpKeyName,0,KEY_QUERY_VALUE|KEY_ENUMERATE_SUB_KEYS,&Subhkey)!=ERROR_SUCCESS)
		{ //tfx
			continue;
		}
		GetRegistrySnap(hDlg,Subhkey,lpKeyContent);
		RegCloseKey(Subhkey);
	}
	
	nGettingTime=GetTickCount();
	if ((nGettingTime-nBASETIME1)>REFRESHINTERVAL)
	{
		BusyRoutine(str_key,str_value,nGettingKey,nGettingValue);
	}


	return NULL;
}

	
/*	TimerProc I used to update toolbar,not used now!
	
BOOL	CALLBACK	TimerProc(hDlg,message,wParam,lParam)
HWND	hDlg;	 
UINT	message; 
WPARAM	wParam; 
LPARAM	lParam; 
{
//	case	WM_TIMER:
			//DebugBreak();
			nGettingTime++;
			wsprintf(lpMESSAGE,"%d",nGettingTime);
			SendMessage(GetDlgItem(hDlg,IDC_TEXTCOUNT3),WM_SETTEXT,(WPARAM)0,(LPARAM)lpMESSAGE);
			return(TRUE);
}
*/	

//--------------------------------------------------
//Routines not so important
//--------------------------------------------------
VOID	OnlyShotPre(DWORD id)
{
	hHourGlass=LoadCursor(NULL,IDC_WAIT);
	hSaveCursor=SetCursor(hHourGlass);
	EnableWindow(GetDlgItem(hWnd,id),FALSE);
	EnableWindow(GetDlgItem(hWnd,IDC_CHECKDIR),FALSE);
	EnableWindow(GetDlgItem(hWnd,IDC_EDITDIR),FALSE);
}
VOID	OnlyShotDone(VOID)
{
	DWORD	iddef;
	if(lpHeadLocalMachine1==NULL)
		iddef=IDC_1STSHOT;
	else if(lpHeadLocalMachine2==NULL)
		iddef=IDC_2NDSHOT;
	else
		iddef=IDC_COMPARE;
	EnableWindow(GetDlgItem(hWnd,IDC_CLEAR1),TRUE);
	EnableWindow(GetDlgItem(hWnd,iddef),TRUE);
	SendMessage(hWnd,DM_SETDEFID,(WPARAM)iddef,(LPARAM)0);
	SetFocus(GetDlgItem(hWnd,iddef));
	SetCursor(hSaveCursor);					
	MessageBeep(0xffffffff);
}
VOID	PreClear(VOID)
{
	//EnableWindow(GetDlgItem(hWnd,IDC_CLEAR1),FALSE);
	hHourGlass=LoadCursor(NULL,IDC_WAIT);
	hSaveCursor=SetCursor(hHourGlass);

	ShowWindow(GetDlgItem(hWnd,IDC_TEXTCOUNT1),SW_HIDE);
	ShowWindow(GetDlgItem(hWnd,IDC_TEXTCOUNT2),SW_HIDE);
	ShowWindow(GetDlgItem(hWnd,IDC_TEXTCOUNT3),SW_HIDE);
	UpdateWindow(hWnd);

}
VOID	AfterClear(VOID)
{
	DWORD iddef;
	BOOL  bChk; //used for file scan disable
	if(lpHeadLocalMachine1==NULL)
		iddef=IDC_1STSHOT;
	else if(lpHeadLocalMachine2==NULL)
		iddef=IDC_2NDSHOT;
	EnableWindow(GetDlgItem(hWnd,iddef),TRUE);
	EnableWindow(GetDlgItem(hWnd,IDC_COMPARE),FALSE);
	
	if(lpHeadLocalMachine1==NULL&&lpHeadLocalMachine2==NULL)
	{
		EnableWindow(GetDlgItem(hWnd,IDC_2NDSHOT),FALSE);
		EnableWindow(GetDlgItem(hWnd,IDC_CLEAR1),FALSE);
		bChk=TRUE;
	}
	else
		bChk=FALSE;

	EnableWindow(GetDlgItem(hWnd,IDC_CHECKDIR),bChk); //we only enable chk when clear all
	SendMessage(hWnd,WM_COMMAND,(WPARAM)IDC_CHECKDIR,(LPARAM)0);

	SetFocus(GetDlgItem(hWnd,iddef));
	SendMessage(hWnd,DM_SETDEFID,(WPARAM)iddef,(LPARAM)0);
	SetCursor(hSaveCursor);					
	MessageBeep(0xffffffff);
}


//----------------------------------------------------------------------------------------------------
//2 Routines to save registry to Hive file,It is rather stupid,see the end of this doc!
//----------------------------------------------------------------------------------------------------
VOID	SaveRegKey(LPKEYCONTENT lpKeyContent, DWORD nFPCurrentFatherKey,DWORD nFPCaller)
{

	DWORD	nFPHeader,nFPCurrent,nFPTemp4Write,nLenPlus1;
	LPVALUECONTENT lpv;
	
	
	nLenPlus1=lstrlen(lpKeyContent->lpkeyname)+1;											//get len+1
	nFPHeader=SetFilePointer(hFileWholeReg,0,NULL,FILE_CURRENT);							//save head fp
	nFPTemp4Write=nFPHeader+21;								//5*4+1
	WriteFile(hFileWholeReg,&nFPTemp4Write,4,&NumberOfBytesWritten,NULL);					//save location of lpkeyname
	nFPTemp4Write=(lpKeyContent->lpfirstvalue!=NULL) ? (nFPHeader+21+nLenPlus1):0;			//We write lpkeyname plus a "\0"
	WriteFile(hFileWholeReg,&nFPTemp4Write,4,&NumberOfBytesWritten,NULL);					//save location of lpfirstvalue
	WriteFile(hFileWholeReg,(LPBYTE)lpKeyContent+8,8,&NumberOfBytesWritten,NULL);			//save lpfirstsubkey and lpbrotherkey
	WriteFile(hFileWholeReg,&nFPCurrentFatherKey,4,&NumberOfBytesWritten,NULL);				//save nFPCurrentFatherKey passed by caller
	nFPTemp4Write=0;
	WriteFile(hFileWholeReg,&nFPTemp4Write,1,&NumberOfBytesWritten,NULL);					//clear and save bkeymatch
	WriteFile(hFileWholeReg,lpKeyContent->lpkeyname,nLenPlus1,&NumberOfBytesWritten,NULL);	//Save the current keyname
	
	

	//Save the sub-value of current KeyContent
	for(lpv=lpKeyContent->lpfirstvalue;lpv!=NULL;lpv=lpv->lpnextvalue)
	{
		nLenPlus1=lstrlen(lpv->lpvaluename)+1;
		nFPCurrent=SetFilePointer(hFileWholeReg,0,NULL,FILE_CURRENT);						//save  fp
		WriteFile(hFileWholeReg,(LPBYTE)lpv,8,&NumberOfBytesWritten,NULL);
		nFPTemp4Write=nFPCurrent+25;														//6*4+1
		WriteFile(hFileWholeReg,&nFPTemp4Write,4,&NumberOfBytesWritten,NULL);				//save location of lpvaluename
		nFPTemp4Write=(lpv->datasize>0)?(nFPCurrent+25+nLenPlus1):0;						//if no lpvaluedata,we write 0
		WriteFile(hFileWholeReg,&nFPTemp4Write,4,&NumberOfBytesWritten,NULL);				//save location of lpvaluedata
		nFPTemp4Write=(lpv->lpnextvalue!=NULL)?(nFPCurrent+25+nLenPlus1+lpv->datasize):0;	//if no nextvalue we write 0
		WriteFile(hFileWholeReg,&nFPTemp4Write,4,&NumberOfBytesWritten,NULL);				//save location of next subvalue
		nFPTemp4Write=nFPHeader;
		WriteFile(hFileWholeReg,&nFPTemp4Write,4,&NumberOfBytesWritten,NULL);				//save location of current key
		nFPTemp4Write=0;
		WriteFile(hFileWholeReg,&nFPTemp4Write,1,&NumberOfBytesWritten,NULL);				//clear and save bvaluematch
		WriteFile(hFileWholeReg,lpv->lpvaluename,nLenPlus1,&NumberOfBytesWritten,NULL);		//save lpvaluename
		WriteFile(hFileWholeReg,lpv->lpvaluedata,lpv->datasize,&NumberOfBytesWritten,NULL); //save lpvaluedata

	}
	
	if(lpKeyContent->lpfirstsubkey!=NULL)
	{
		//pass this keycontent's position as subkey's fatherkey's position and pass the "lpfirstsubkey field"
		SaveRegKey(lpKeyContent->lpfirstsubkey,nFPHeader,nFPHeader+8); 
	}
		
	if(lpKeyContent->lpbrotherkey!=NULL)
	{
		// pass this key's fatherkey's position as brother's father and pass "lpbrotherkey field"
		SaveRegKey(lpKeyContent->lpbrotherkey,nFPCurrentFatherKey,nFPHeader+12);
	}

	if(nFPCaller>0) //save position of current key in current father key
	{
		nFPCurrent=SetFilePointer(hFileWholeReg,0,NULL,FILE_CURRENT);
		SetFilePointer(hFileWholeReg,nFPCaller,NULL,FILE_BEGIN);
		WriteFile(hFileWholeReg,&nFPHeader,4,&NumberOfBytesWritten,NULL);
		SetFilePointer(hFileWholeReg,nFPCurrent,NULL,FILE_BEGIN);
	}
	nSavingKey++;
	if (nGettingKey!=0)
	if (nSavingKey%nGettingKey>nSavingStep)
	{
		nSavingKey=0;
		SendDlgItemMessage(hWnd,IDC_PBCOMPARE,PBM_STEPIT,(WPARAM)0,(LPARAM)0);
		UpdateWindow(hWnd);
		PeekMessage(&msg,hWnd,WM_ACTIVATE,WM_ACTIVATE,PM_REMOVE);
	}

}
/////////////////////////////////////////////////////////
VOID	SaveRegistry(LPKEYCONTENT lpKeyHLM,LPKEYCONTENT lpKeyUSER,LPSTR computer,LPSTR user,LPVOID time)
{
	DWORD nFPcurrent;
	if(lpKeyHLM!=NULL||lpKeyUSER!=NULL)
	{

		opfn.lStructSize=sizeof(opfn);
		opfn.hwndOwner=hWnd;
		opfn.lpstrFilter=str_filter;
		opfn.lpstrFile=GlobalAlloc(LPTR,MAX_PATH+1);
		opfn.nMaxFile=MAX_PATH*2;
		opfn.lpstrInitialDir=lpLastSaveDir;
		opfn.lpstrDefExt="hiv";
		opfn.Flags=OFN_OVERWRITEPROMPT|OFN_HIDEREADONLY;
		if(GetSaveFileName(&opfn))
		{					
			hFileWholeReg = CreateFile(opfn.lpstrFile,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
			if( hFileWholeReg!= INVALID_HANDLE_VALUE) 
			{

				PreClear();
				PreShowProgressBar();
				WriteFile(hFileWholeReg,lpRegSaveFileHeader,lstrlen(lpRegSaveFileHeader),&NumberOfBytesWritten,NULL); //save lpvaluedata

				//Save the position of H_L_M
				nFPcurrent=HIVEBEGINOFFSET; //computerlen*2+sizeof(systemtime)+32 must <hivebeginoffset
				SetFilePointer(hFileWholeReg,16,NULL,FILE_BEGIN);
				WriteFile(hFileWholeReg,&nFPcurrent,4,&NumberOfBytesWritten,NULL);

				SetFilePointer(hFileWholeReg,HIVEBEGINOFFSET,NULL,FILE_BEGIN);


				if(lpKeyHLM!=NULL)
					SaveRegKey(lpKeyHLM,0,0);
				
				//Save the position of hkeyUsr
				nFPcurrent=SetFilePointer(hFileWholeReg,0,NULL,FILE_CURRENT);
				SetFilePointer(hFileWholeReg,20,NULL,FILE_BEGIN);
				WriteFile(hFileWholeReg,&nFPcurrent,4,&NumberOfBytesWritten,NULL);
				SetFilePointer(hFileWholeReg,nFPcurrent,NULL,FILE_BEGIN);

				if(lpKeyUSER!=NULL)
					SaveRegKey(lpKeyUSER,0,0);
				
				SetFilePointer(hFileWholeReg,32,NULL,FILE_BEGIN);
				WriteFile(hFileWholeReg,computer,lstrlen(computer)+1,&NumberOfBytesWritten,NULL);
				SetFilePointer(hFileWholeReg,COMPUTERNAMELEN+32,NULL,FILE_BEGIN);
				WriteFile(hFileWholeReg,user,lstrlen(user)+1,&NumberOfBytesWritten,NULL);
				SetFilePointer(hFileWholeReg,COMPUTERNAMELEN*2+32,NULL,FILE_BEGIN);
				WriteFile(hFileWholeReg,time,sizeof(SYSTEMTIME),&NumberOfBytesWritten,NULL);

				ShowWindow(GetDlgItem(hWnd,IDC_PBCOMPARE),SW_HIDE);

				SetCursor(hSaveCursor);					
				MessageBeep(0xffffffff);
				CloseHandle(hFileWholeReg);
			}
			else
				MyErrorMessage(hWnd,str_errorcreatefile);

		}
		*(opfn.lpstrFile+opfn.nFileOffset)=0x00;
		lstrcpy(lpLastSaveDir,opfn.lpstrFile);
		GlobalFree(opfn.lpstrFile);
	}
}

//--------------------------------------------------
//After Loading from hive file, data must be realigned! 
//See the note at the end of the source code
//--------------------------------------------------
VOID ReAlign(LPKEYCONTENT lpKey,DWORD nBase)
{
	LPDWORD lp;
	LPVALUECONTENT lpv;
	lp=(LPDWORD)lpKey;
	
	if((*lp)!=0) (*lp)+=nBase;	lp++;
	if((*lp)!=0) (*lp)+=nBase;	lp++;
	if((*lp)!=0) (*lp)+=nBase;	lp++;
	if((*lp)!=0) (*lp)+=nBase;	lp++;
	if((*lp)!=0) (*lp)+=nBase;

	for(lpv=lpKey->lpfirstvalue;lpv!=NULL;lpv=lpv->lpnextvalue)
	{
		lp=(LPDWORD)lpv+2;
		if((*lp)!=0) (*lp)+=nBase;	lp++;
		if((*lp)!=0) (*lp)+=nBase;	lp++;
		if((*lp)!=0) (*lp)+=nBase;	lp++;
		if((*lp)!=0) (*lp)+=nBase;
	}
		
	if(lpKey->lpfirstsubkey!=NULL)
	{
		ReAlign(lpKey->lpfirstsubkey,nBase);
	}
		
	if(lpKey->lpbrotherkey!=NULL)
	{
		ReAlign(lpKey->lpbrotherkey,nBase);
	}
}

//----------------------------------------------------------------------------------------------------
//Load Registry From HIVE file,After that,We should realign the data in memory!
//----------------------------------------------------------------------------------------------------
BOOL LoadRegistry(LPKEYCONTENT FAR * lplpKeyHLM,LPKEYCONTENT FAR * lplpKeyUSER,LPSTR lpHive)
{
	DWORD	nFileSize,nOffSet=0,nBase;
	BOOL	bRet=FALSE;
	opfn.lStructSize=sizeof(opfn);
	opfn.hwndOwner=hWnd;
	opfn.lpstrFilter=str_filter;
	opfn.lpstrFile=GlobalAlloc(LPTR,MAX_PATH+1);
	opfn.nMaxFile=MAX_PATH*2;
	opfn.lpstrInitialDir=lpLastOpenDir;
	opfn.Flags=OFN_FILEMUSTEXIST|OFN_HIDEREADONLY;
	opfn.lpstrDefExt="hiv";
	if(GetOpenFileName(&opfn))
	{					
		hFileWholeReg=CreateFile(opfn.lpstrFile,GENERIC_READ ,FILE_SHARE_READ ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
		if (hFileWholeReg!=INVALID_HANDLE_VALUE)
		{
			lpHive=GlobalAlloc(LPTR,16);
			ReadFile(hFileWholeReg,lpHive,16,&NumberOfBytesWritten,NULL);
			if(lstrcmp(lpRegSaveFileHeader,lpHive)!=0)
			{
				MyErrorMessage(hWnd,"It is not a valid Regshot hive file!");
				bRet=FALSE;
			}
			else
			{
				if(is1)
					OnlyShotPre(IDC_1STSHOT);
				else
					OnlyShotPre(IDC_2NDSHOT);
				nFileSize=GetFileSize(hFileWholeReg,NULL);
				GlobalFree(lpHive);

				lpHive=GlobalAlloc(LMEM_FIXED,nFileSize);
				nBase=(DWORD)lpHive;
				ReadFile(hFileWholeReg,&nOffSet,4,&NumberOfBytesWritten,NULL);
				*lplpKeyHLM=(LPKEYCONTENT)(nBase+nOffSet);
				ReadFile(hFileWholeReg,&nOffSet,4,&NumberOfBytesWritten,NULL);
				*lplpKeyUSER=(LPKEYCONTENT)(nBase+nOffSet);
				SetFilePointer(hFileWholeReg,0,NULL,FILE_BEGIN);
				ReadFile(hFileWholeReg,lpHive,nFileSize,&NumberOfBytesWritten,NULL);
				ReAlign(*lplpKeyHLM,nBase);
				ReAlign(*lplpKeyUSER,nBase);

				if(is1)
				{
					lpComputerName1=lpHive+32;
					lpUserName1=lpHive+COMPUTERNAMELEN+32;
					lpSystemtime1=(SYSTEMTIME FAR *)(lpHive+COMPUTERNAMELEN*2+32);
				}
				else
				{
					lpComputerName2=lpHive+32;
					lpUserName2=lpHive+COMPUTERNAMELEN+32;
					lpSystemtime2=(SYSTEMTIME FAR *)(lpHive+COMPUTERNAMELEN*2+32);
				}

				OnlyShotDone();
				bRet=TRUE;

			}
			CloseHandle(hFileWholeReg);
		}
		else
		{
			MyErrorMessage(hWnd,str_erroropenfile);
			bRet=FALSE;
		}

		
	}
	else
		bRet=FALSE;;

	*(opfn.lpstrFile+opfn.nFileOffset)=0x00;
	lstrcpy(lpLastOpenDir,opfn.lpstrFile);

	GlobalFree(opfn.lpstrFile);
	return(bRet);

}



//--------------------------------------------------
// Routines that show multi language,not so important
//--------------------------------------------------
BOOL	GetLanguageType(HWND hDlg)
{
	DWORD	nReturn;
	BOOL	bRet;
	LPSTR	lp;
	LPSTR	lpSectionNames=GlobalAlloc(LPTR,SIZEOF_LANGUAGE_SECTIONNAMES_BUFFER);
	//LPSTR	lpCurrentLanguage=GlobalAlloc(LPTR,SIZEOF_SINGLE_LANGUAGENAME);

	
	nReturn=GetPrivateProfileSectionNames(lpSectionNames,SIZEOF_LANGUAGE_SECTIONNAMES_BUFFER,lpIni);
	if (nReturn>1)
	{
		bRet=TRUE;
		for(lp=lpSectionNames;*lp!=0;lp=lp+lstrlen(lp)+1)
		{
			if(lstrcmpi(lp,lpSectionCurrent)!=0)
			SendDlgItemMessage(hDlg,IDC_COMBOLANGUAGE,CB_ADDSTRING,(WPARAM)0,(LPARAM)lp);
		}
		GetPrivateProfileString(lpSectionCurrent,lpSectionCurrent,
							lpDefaultLanguage,lpCurrentLanguage,16,lpIni);

		nReturn=SendDlgItemMessage(hDlg,IDC_COMBOLANGUAGE,CB_FINDSTRINGEXACT,(WPARAM)0,(LPARAM)lpCurrentLanguage);
		if (nReturn!=CB_ERR)
		{
			bRet=TRUE;
			SendDlgItemMessage(hDlg,IDC_COMBOLANGUAGE,CB_SETCURSEL,(WPARAM)nReturn,(LPARAM)0);
		}
		else
			bRet=FALSE;

	}
	else
		bRet=FALSE;


	GlobalFree(lpSectionNames);
	//GlobalFree(lpCurrentLanguage);
	return bRet;
	
}
//--------------------------------------------------
// Routines that show multi language,not so important
//--------------------------------------------------

VOID	GetDefaultStrings(VOID)
{
	//_asm int 3
	str_key				=str_default[0];
	str_value			=str_default[1];
	str_dir				=str_default[2];
	str_file			=str_default[3];
	str_time			=str_default[4];
	str_keyadd			=str_default[5];
	str_keydel			=str_default[6];
	str_valadd			=str_default[7];
	str_valdel			=str_default[8];
	str_valmodi			=str_default[9];
	str_fileadd			=str_default[10];
	str_filedel			=str_default[11];
	str_filemodi		=str_default[12];
	str_diradd			=str_default[13];
	str_dirdel			=str_default[14];
	str_dirmodi			=str_default[15];
	str_total			=str_default[16];
	str_comments		=str_default[17];
	str_datetime		=str_default[18];
	str_computer		=str_default[19];
	str_username		=str_default[20];
	str_about			=str_default[21];
	str_error			=str_default[22];
	str_errorexecviewer	=str_default[23];
	str_errorcreatefile	=str_default[24];
	str_erroropenfile	=str_default[25];
	str_errormovefp		=str_default[26];
	str_menushot		=str_default[40];
	str_menushotsave	=str_default[41];
	str_menuload		=str_default[42];
	str_menuclearallshots=str_default[43];
	str_menuclearshot1	=str_default[44];
	str_menuclearshot2	=str_default[45];


}
//--------------------------------------------------
// Routines that show multi language,not so important
//--------------------------------------------------

VOID	PointToNewStrings(VOID)
{
	LPDWORD	lp=ldwTempStrings;
	str_key				=(LPSTR)(*lp);lp++;
	str_value			=(LPSTR)(*lp);lp++;
	str_dir				=(LPSTR)(*lp);lp++;
	str_file			=(LPSTR)(*lp);lp++;
	str_time			=(LPSTR)(*lp);lp++;
	str_keyadd			=(LPSTR)(*lp);lp++;
	str_keydel			=(LPSTR)(*lp);lp++;
	str_valadd			=(LPSTR)(*lp);lp++;
	str_valdel			=(LPSTR)(*lp);lp++;
	str_valmodi			=(LPSTR)(*lp);lp++;
	str_fileadd			=(LPSTR)(*lp);lp++;
	str_filedel			=(LPSTR)(*lp);lp++;
	str_filemodi		=(LPSTR)(*lp);lp++;
	str_diradd			=(LPSTR)(*lp);lp++;
	str_dirdel			=(LPSTR)(*lp);lp++;
	str_dirmodi			=(LPSTR)(*lp);lp++;
	str_total			=(LPSTR)(*lp);lp++;
	str_comments		=(LPSTR)(*lp);lp++;
	str_datetime		=(LPSTR)(*lp);lp++;
	str_computer		=(LPSTR)(*lp);lp++;
	str_username		=(LPSTR)(*lp);lp++;
	str_about			=(LPSTR)(*lp);lp++;
	str_error			=(LPSTR)(*lp);lp++;
	str_errorexecviewer	=(LPSTR)(*lp);lp++;
	str_errorcreatefile	=(LPSTR)(*lp);lp++;
	str_erroropenfile	=(LPSTR)(*lp);lp++;
	str_errormovefp		=(LPSTR)(*lp);lp+=14;
	str_menushot		=(LPSTR)(*lp);lp++;
	str_menushotsave	=(LPSTR)(*lp);lp++;
	str_menuload		=(LPSTR)(*lp);lp++;
	str_menuclearallshots=(LPSTR)(*lp);lp++;
	str_menuclearshot1	=(LPSTR)(*lp);lp++;
	str_menuclearshot2	=(LPSTR)(*lp);

}
//--------------------------------------------------
// Routines that show multi language,not so important
//--------------------------------------------------
LPSTR	FindMatchedPos(LPSTR lpMaster,LPSTR lp,DWORD size)
{
	DWORD	i,j,nsizelp;
	nsizelp=lstrlen(lp);
	if (size<=nsizelp||nsizelp<1)
		return NULL;
	
	for(i=0;i<size-nsizelp;i++)
	{
		for(j=0;j<nsizelp;j++)
		{
			if(*(lp+j)!=*(lpMaster+i+j))
				break;
		}
		//_asm int 3;
		if(j==nsizelp)
			return lpMaster+i+nsizelp;
	}
	return NULL;
		
}
				
//--------------------------------------------------
// Routines that show multi language,not so important
//--------------------------------------------------
BOOL	GetLanguageStrings(HWND hDlg)
{
	DWORD	nIndex,i;
	BOOL	bRet;
	//LPSTR	lpCurrentLanguage=GlobalAlloc(LPTR,16);
	LPSTR	lpIniKey=GlobalAlloc(LPTR,8);
	LPSTR	lpReturn;
	LPDWORD lp;

	nIndex=SendDlgItemMessage(hDlg,IDC_COMBOLANGUAGE,CB_GETCURSEL,(WPARAM)0,(LPARAM)0);
	if (nIndex!=CB_ERR)
	{
		
		
		SendDlgItemMessage(hDlg,IDC_COMBOLANGUAGE,CB_GETLBTEXT,(WPARAM)nIndex,(LPARAM)lpCurrentLanguage);
		WritePrivateProfileString(lpSectionCurrent,lpSectionCurrent,lpCurrentLanguage,lpIni);
		ZeroMemory(lpFreeStrings,SIZEOF_FREESTRINGS);
		GetPrivateProfileSection(lpCurrentLanguage,lpFreeStrings,SIZEOF_FREESTRINGS,lpIni);
		for(i=1,lp=ldwTempStrings;i<47;i++)
		{
			
			wsprintf(lpIniKey,"%d%s",i,"="); 
			//pointer returned was pointed to char just after "="
			if((lpReturn=FindMatchedPos(lpFreeStrings,lpIniKey,SIZEOF_FREESTRINGS))!=NULL)
			{
				//_asm int 3;
				*(lp+i-1)=(DWORD)lpReturn;
			}
			else
				*(lp+i-1)=(DWORD)str_default[i-1];
			
			if(i>=28&&i<41&&i!=34)
			{
				SetDlgItemText(hDlg,ID_BASE+3+i-28,(LPSTR)(*(lp+i-1)));
			}


		}
		
		lpReturn=FindMatchedPos(lpFreeStrings,lpItemTranslator,SIZEOF_FREESTRINGS);
		lpCurrentTranslator=(lpReturn!=NULL)?(lpReturn+1):lpMyName;
		PointToNewStrings();
		bRet=TRUE;
	}
	else
		bRet=FALSE;
	//GlobalFree(lpCurrentLanguage);
	GlobalFree(lpIniKey);
	return bRet;
}

BOOL SetSnapRegs(HWND hDlg) //tfx 保存信息到配置文件
{
	BYTE nFlag;
	LPSTR lpString;

	nFlag=(BYTE)(SendMessage(GetDlgItem(hDlg,IDC_RADIO1),BM_GETCHECK,(WPARAM)0,(LPARAM)0)
		|SendMessage(GetDlgItem(hDlg,IDC_RADIO2),BM_GETCHECK,(WPARAM)0,(LPARAM)0)<<1
		|SendMessage(GetDlgItem(hDlg,IDC_CHECKDIR),BM_GETCHECK,(WPARAM)0,(LPARAM)0)<<2);

	lpString=GlobalAlloc(LMEM_ZEROINIT,EXTDIRLEN+2);
	sprintf(lpString,"%s=%d",INI_FLAG,nFlag);
	WritePrivateProfileSection(INI_SETUP,lpString,lpRegshotIni);

	if(GetDlgItemText(hDlg,IDC_EDITDIR,lpString,EXTDIRLEN+2)!=0)
		WritePrivateProfileString(INI_SETUP,INI_EXTDIR,lpString,lpRegshotIni);

	if(GetDlgItemText(hDlg,IDC_EDITPATH,lpString,MAX_PATH)!=0)
		WritePrivateProfileString(INI_SETUP,INI_OUTDIR,lpString,lpRegshotIni);

	GlobalFree(lpString);
	GlobalFree(lpRegshotIni);
	GlobalFree(lpSnapRegsStr);
	GlobalFree(lpSnapFilesStr);
	GlobalFree(lpSnapKey);
	GlobalFree(lpSnapReturn);

	return TRUE;
}

BOOL GetSnapRegs(HWND hDlg) //tfx 取配置文件信息
{
	int i;
	BYTE nFlag;

	lpSnapKey=GlobalAlloc(LMEM_FIXED,20);

	lpSnapRegs=GlobalAlloc(LMEM_ZEROINIT,sizeof(LPSTR)*MAXREGSHOT);
	lpSnapRegsStr=GlobalAlloc(LMEM_ZEROINIT,SIZEOF_REGSHOT);
	if(GetPrivateProfileSection(INI_SKIPREGKEY,lpSnapRegsStr,SIZEOF_REGSHOT,lpRegshotIni)>0)
	{
		for(i=0;i<MAXREGSHOT-1;i++)
		{
			wsprintf(lpSnapKey,"%d%s",i,"=");
			if((lpSnapReturn=FindMatchedPos(lpSnapRegsStr,lpSnapKey,SIZEOF_REGSHOT))!=NULL)
			{
				*(lpSnapRegs+i)=(DWORD)lpSnapReturn;
				//dwSnapFiles++;
			}
			else
			{
				break;
			}
		}
	}

	lpSnapFiles=GlobalAlloc(LMEM_ZEROINIT,sizeof(LPSTR)*MAXREGSHOT);
	lpSnapFilesStr=GlobalAlloc(LMEM_ZEROINIT,SIZEOF_REGSHOT);
	if(GetPrivateProfileSection(INI_SKIPDIR,lpSnapFilesStr,SIZEOF_REGSHOT,lpRegshotIni))
	{
		for(i=0;i<MAXREGSHOT-1;i++)
		{
			wsprintf(lpSnapKey,"%d%s",i,"=");
			if((lpSnapReturn=FindMatchedPos(lpSnapFilesStr,lpSnapKey,SIZEOF_REGSHOT))!=NULL)
			{
				*(lpSnapFiles+i)=(DWORD)lpSnapReturn;
				//dwSnapFiles++;
			}
			else
			{
				break;
			}
		}
	}

	nFlag=(BYTE)GetPrivateProfileInt(INI_SETUP,INI_FLAG,0,lpRegshotIni);
	if(nFlag!=0)
	{
		SendMessage(GetDlgItem(hDlg,IDC_RADIO1),BM_SETCHECK,(WPARAM)(nFlag&0x01),(LPARAM)0);
		SendMessage(GetDlgItem(hDlg,IDC_RADIO2),BM_SETCHECK,(WPARAM)((nFlag&0x01)^0x01),(LPARAM)0);
		SendMessage(GetDlgItem(hDlg,IDC_CHECKDIR),BM_SETCHECK,(WPARAM)((nFlag&0x04)>>1),(LPARAM)0);
	}
	else
	{
		SendMessage(GetDlgItem(hDlg,IDC_RADIO1),BM_SETCHECK,(WPARAM)0x01,(LPARAM)0);
		SendMessage(GetDlgItem(hDlg,IDC_RADIO2),BM_SETCHECK,(WPARAM)0x00,(LPARAM)0);
		SendMessage(GetDlgItem(hDlg,IDC_CHECKDIR),BM_SETCHECK,(WPARAM)0x00,(LPARAM)0);
	}

	if(GetPrivateProfileString(INI_SETUP,INI_EXTDIR,NULL,lpExtDir,MAX_PATH,lpRegshotIni)!=0)
		SetDlgItemText(hDlg,IDC_EDITDIR,lpExtDir);
	else
	{
		SetDlgItemText(hDlg,IDC_EDITDIR,lpWindowsDirName);
/*		lpProgramDir=GlobalAlloc(LPTR,MAX_PATH); //tfx 暂时不用了
		if(SUCCEEDED(SHGetSpecialFolderPath(NULL,lpProgramDir,0x0026,0)))
		{
			lstrcat(lpProgramDir,";");
			lstrcat(lpProgramDir,lpWindowsDirName);
			SetDlgItemText(hDlg,IDC_EDITDIR,lpProgramDir);
		}
		else
			SetDlgItemText(hDlg,IDC_EDITDIR,lpWindowsDirName);
		GlobalFree(lpProgramDir);
*/	}

	if(GetPrivateProfileString(INI_SETUP,INI_OUTDIR,NULL,lpOutputpath,MAX_PATH,lpRegshotIni)!=0)
		SetDlgItemText(hDlg,IDC_EDITPATH,lpOutputpath);
	else
		SetDlgItemText(hDlg,IDC_EDITPATH,lpTempPath);

	SendMessage(hDlg,WM_COMMAND,(WPARAM)IDC_CHECKDIR,(LPARAM)0);

	return TRUE;
}

BOOL IsInSkipList(LPSTR lpSnap, LPDWORD lpSkipList) //tfx 跳过黑名单
{
	int i;
	for(i=0;i<=MAXREGSHOT-1&&(LPSTR)(*(lpSkipList+i))!=NULL;i++)
	{
		if(lstrcmpi(lpSnap, (LPSTR)*(lpSkipList+i))==0)
		{
			return TRUE;
		}
	}
	return FALSE;
}

VOID	OnlyShot1(VOID)
{
	UINT	nLengthofStr;	
	lpHeadLocalMachine1=(LPKEYCONTENT)GlobalAlloc(LPTR,sizeof(KEYCONTENT));
	lpHeadUsers1=(LPKEYCONTENT)GlobalAlloc(LPTR,sizeof(KEYCONTENT));
	
	lpHeadLocalMachine1->lpkeyname=GlobalAlloc(LMEM_FIXED,lstrlen(LOCALMACHINESTRING)+1);
	lpHeadUsers1->lpkeyname=GlobalAlloc(LMEM_FIXED,lstrlen(USERSSTRING)+1);
	lstrcpy(lpHeadLocalMachine1->lpkeyname,LOCALMACHINESTRING);
	lstrcpy(lpHeadUsers1->lpkeyname,USERSSTRING);

	OnlyShotPre(IDC_1STSHOT);

					
	ShowWindow(GetDlgItem(hWnd,IDC_TEXTCOUNT1),SW_SHOW);
	ShowWindow(GetDlgItem(hWnd,IDC_TEXTCOUNT2),SW_SHOW);
	ShowWindow(GetDlgItem(hWnd,IDC_TEXTCOUNT3),SW_SHOW);
	
	nGettingKey=2;nGettingValue=0;nGettingTime=0;nGettingFile=0,nGettingDir=0;
	nBASETIME=GetTickCount();
	nBASETIME1=nBASETIME;
					
	GetRegistrySnap(hWnd,HKEY_LOCAL_MACHINE,lpHeadLocalMachine1);
	GetRegistrySnap(hWnd,HKEY_USERS,lpHeadUsers1);
	nGettingTime=GetTickCount();
	BusyRoutine(str_key,str_value,nGettingKey,nGettingValue);
	if(SendMessage(GetDlgItem(hWnd,IDC_CHECKDIR),BM_GETCHECK,(WPARAM)0,(LPARAM)0)==1)
	{
		DWORD nSubExtDirLen,i;
		LPSTR lpSubExtDir;
		LPHEADFILE lphf,lphftemp;

		GetDlgItemText(hWnd,IDC_EDITDIR,lpExtDir,EXTDIRLEN+2);
		nLengthofStr=lstrlen(lpExtDir);

		lphf=lpHeadFile;
		lpSubExtDir=lpExtDir;
						
		if(nLengthofStr>0)
		for(i=0;i<=nLengthofStr;i++)
						{
							//This is the stupid File Name Detect Routine,[seperate with ";"]
							if(*(lpExtDir+i)==0x3b||*(lpExtDir+i)==0x00)
							{
								*(lpExtDir+i)=0x00;
								
								if(*(lpExtDir+i-1)=='\\'&&i>0)
									*(lpExtDir+i-1)=0x00;

								if(*lpSubExtDir!=0x00)
								{
									lphf=(LPHEADFILE)GlobalAlloc(LPTR,sizeof(HEADFILE));
									if(lpHeadFile==NULL)
										lpHeadFile=lphf;
									else
										lphftemp->lpnextheadfile=lphf;

									lphftemp=lphf;	
									lphf->lpHeadFile1=(LPFILECONTENT)GlobalAlloc(LPTR,sizeof(FILECONTENT));
									lphf->lpHeadFile2=(LPFILECONTENT)GlobalAlloc(LPTR,sizeof(FILECONTENT));

									nSubExtDirLen=lstrlen(lpSubExtDir)+1;
									lphf->lpHeadFile1->lpfilename=GlobalAlloc(LMEM_FIXED,nSubExtDirLen);
									lphf->lpHeadFile2->lpfilename=GlobalAlloc(LMEM_FIXED,nSubExtDirLen);

									lstrcpy(lphf->lpHeadFile1->lpfilename,lpSubExtDir);
									lstrcpy(lphf->lpHeadFile2->lpfilename,lpSubExtDir);

									lphf->lpHeadFile1->fileattr=FILE_ATTRIBUTE_DIRECTORY;
									lphf->lpHeadFile2->fileattr=FILE_ATTRIBUTE_DIRECTORY;

									GetFilesSnap(hWnd,lphf->lpHeadFile1);
									nGettingTime=GetTickCount();
									BusyRoutine(str_dir,str_file,nGettingDir,nGettingFile);
								}
								lpSubExtDir=lpExtDir+i+1;

							}

						}
					}

	NumberOfBytesWritten=COMPUTERNAMELEN;
	GetSystemTime(lpSystemtime1);
	GetComputerName(lpComputerName1,&NumberOfBytesWritten);
	GetUserName(lpUserName1,&NumberOfBytesWritten);

	OnlyShotDone();
					

}

// -----------------------------
VOID	OnlyShot2(VOID)
{
	lpHeadLocalMachine2=(LPKEYCONTENT)GlobalAlloc(LPTR,sizeof(KEYCONTENT));
	lpHeadUsers2=(LPKEYCONTENT)GlobalAlloc(LPTR,sizeof(KEYCONTENT));

	lpHeadLocalMachine2->lpkeyname=GlobalAlloc(LMEM_FIXED,lstrlen(LOCALMACHINESTRING)+1);
	lpHeadUsers2->lpkeyname=GlobalAlloc(LMEM_FIXED,lstrlen(USERSSTRING)+1);
	lstrcpy(lpHeadLocalMachine2->lpkeyname,LOCALMACHINESTRING);
	lstrcpy(lpHeadUsers2->lpkeyname,USERSSTRING);
	
	OnlyShotPre(IDC_2NDSHOT);
	
	nGettingKey=2;nGettingValue=0;nGettingTime=0;nGettingFile=0,nGettingDir=0;
	nBASETIME=GetTickCount();
	nBASETIME1=nBASETIME;

	ShowWindow(GetDlgItem(hWnd,IDC_TEXTCOUNT1),SW_SHOW);
	ShowWindow(GetDlgItem(hWnd,IDC_TEXTCOUNT2),SW_SHOW);
	ShowWindow(GetDlgItem(hWnd,IDC_TEXTCOUNT3),SW_SHOW);

	GetRegistrySnap(hWnd,HKEY_LOCAL_MACHINE,lpHeadLocalMachine2);
	GetRegistrySnap(hWnd,HKEY_USERS,lpHeadUsers2);
	nGettingTime=GetTickCount();
	BusyRoutine(str_key,str_value,nGettingKey,nGettingValue);

	if(SendMessage(GetDlgItem(hWnd,IDC_CHECKDIR),BM_GETCHECK,(WPARAM)0,(LPARAM)0)==1)
	{
		LPHEADFILE lphf;
		for(lphf=lpHeadFile;lphf!=NULL;lphf=lphf->lpnextheadfile)
		{
			//_asm int 3;
			GetFilesSnap(hWnd,lphf->lpHeadFile2);
			nGettingTime=GetTickCount();
			BusyRoutine(str_dir,str_file,nGettingDir,nGettingFile);
		}
	}

					
	
	NumberOfBytesWritten=COMPUTERNAMELEN;
	GetSystemTime(lpSystemtime2);
	GetComputerName(lpComputerName2,&NumberOfBytesWritten);
	GetUserName(lpUserName2,&NumberOfBytesWritten);
	OnlyShotDone();				
}

//--------------------------------------------------
//Show popup shortcut menu
//--------------------------------------------------
VOID CreateShotPopupMenu(VOID)
{
	hMenu=CreatePopupMenu();
	AppendMenu(hMenu,MF_STRING,IDM_ONLYSHOT,str_menushot);
	AppendMenu(hMenu,MF_STRING,IDM_SHOTSAVE,str_menushotsave);
	AppendMenu(hMenu,MF_SEPARATOR,IDM_BREAK,NULL);
	AppendMenu(hMenu,MF_STRING,IDM_LOAD,str_menuload);
	SetMenuDefaultItem(hMenu,IDM_ONLYSHOT,FALSE);
}

//--------------------------------------------------
//Main Dialog Proc
//--------------------------------------------------

BOOL	CALLBACK	DialogProc(hDlg,message,wParam,lParam)
HWND	hDlg;	 
UINT	message; 
WPARAM	wParam; 
LPARAM	lParam; 
{					    
	UINT	nLengthofStr;
//	BYTE	nFlag;

	switch(message)
	{
	case	WM_INITDIALOG:
			
			SendDlgItemMessage(hDlg,IDC_EDITCOMMENT,EM_SETLIMITTEXT,(WPARAM)COMMENTLENGTH,(LPARAM)0);
			SendDlgItemMessage(hDlg,IDC_EDITPATH,EM_SETLIMITTEXT,(WPARAM)MAX_PATH,(LPARAM)0);
			SendDlgItemMessage(hDlg,IDC_EDITDIR,EM_SETLIMITTEXT,(WPARAM)EXTDIRLEN,(LPARAM)0);
			lpExtDir=GlobalAlloc(LPTR,(EXTDIRLEN+2));
			lpOutputpath=GlobalAlloc(LPTR,MAX_PATH+5);
			lpCurrentLanguage=GlobalAlloc(LPTR,SIZEOF_SINGLE_LANGUAGENAME);
			lpKeyName=GlobalAlloc(LPTR,MAX_PATH+1);
			lpMESSAGE=GlobalAlloc(LPTR,100);
			lpStartDir=GlobalAlloc(LPTR,MAX_PATH+1);
			lpWindowsDirName=GlobalAlloc(LPTR,MAX_PATH+5);
			lpTempPath=GlobalAlloc(LPTR,MAX_PATH+2);
			lpComputerName1=GlobalAlloc(LPTR,COMPUTERNAMELEN);
			lpComputerName2=GlobalAlloc(LPTR,COMPUTERNAMELEN);
			lpUserName1=GlobalAlloc(LPTR,COMPUTERNAMELEN);
			lpUserName2=GlobalAlloc(LPTR,COMPUTERNAMELEN);
			lpSystemtime1=GlobalAlloc(LPTR,sizeof(SYSTEMTIME));
			lpSystemtime2=GlobalAlloc(LPTR,sizeof(SYSTEMTIME));
			lpCurrentTranslator=lpMyName;

			GetWindowsDirectory(lpWindowsDirName,MAX_PATH);
			nLengthofStr=lstrlen(lpWindowsDirName);
			if (nLengthofStr>0&&*(lpWindowsDirName+nLengthofStr-1)=='\\')
				*(lpWindowsDirName+nLengthofStr-1)=0x00;
			GetTempPath(MAX_PATH,lpTempPath);

			lpStartDir=GetCommandLine();
			lpIni=strrchr(lpStartDir,'\\');lpStartDir++;*++lpIni=0x0;

			lpIni=GlobalAlloc(LPTR,MAX_PATH*2);
			lstrcpy(lpIni,lpStartDir);
			lstrcat(lpIni,REGSHOTLANGUAGEFILE);

			lpFreeStrings=GlobalAlloc(LMEM_FIXED,SIZEOF_FREESTRINGS);
			ldwTempStrings=GlobalAlloc(LPTR,4*60); //max is 60 strings

			if(GetLanguageType(hDlg))
				GetLanguageStrings(hDlg);
			else
				GetDefaultStrings();
			
/*			//To get rgst152.dat which is the ini file of regshot,but it should  be a standard ini file in future!
			hFile = CreateFile(REGSHOTDATFILE,GENERIC_READ | GENERIC_WRITE,FILE_SHARE_READ | FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
			if( hFile != INVALID_HANDLE_VALUE) 
			{
				if((ReadFile(hFile,&nFlag,1,&NumberOfBytesWritten,NULL)==TRUE)&&NumberOfBytesWritten==1)
				{
					SendMessage(GetDlgItem(hDlg,IDC_RADIO1),BM_SETCHECK,(WPARAM)(nFlag&0x01),(LPARAM)0);
					SendMessage(GetDlgItem(hDlg,IDC_RADIO2),BM_SETCHECK,(WPARAM)((nFlag&0x01)^0x01),(LPARAM)0);
					SendMessage(GetDlgItem(hDlg,IDC_CHECKDIR),BM_SETCHECK,(WPARAM)((nFlag&0x02)>>1),(LPARAM)0);
					//SendMessage(GetDlgItem(hDlg,IDC_CHECKTURBO),BM_SETCHECK,(WPARAM)((nFlag&0x02)>1),(LPARAM)0);
					//SendMessage(hDlg,WM_COMMAND,(WPARAM)(((DWORD)(BN_CLICKED)<<16)|(DWORD)((nFlag&0x01==1)?IDC_RADIO1:IDC_RADIO2)),(LPARAM)0);
					//SendMessage(GetDlgItem(hDlg,IDC_CHECKAUTOCOMPARE),BM_SETCHECK,(WPARAM)((nFlag&0x02)>1),(LPARAM)0);
					//SendMessage(GetDlgItem(hDlg,IDC_CHECKWRITECONTENT),BM_SETCHECK,(WPARAM)((nFlag&0x04)>>2),(LPARAM)0);
					//SendMessage(GetDlgItem(hDlg,IDC_CHECKINI),BM_SETCHECK,(WPARAM)((nFlag&0x08)>>3),(LPARAM)0);
				}
				ReadFile(hFile,&nMask,4,&NumberOfBytesWritten,NULL);

				if((ReadFile(hFile,&nLengthofStr,sizeof(nLengthofStr),&NumberOfBytesWritten,NULL)==TRUE)
					&&NumberOfBytesWritten==sizeof(nLengthofStr)&&nLengthofStr!=0)
				{
					
					if((ReadFile(hFile,lpExtDir,nLengthofStr,&NumberOfBytesWritten,NULL)==TRUE)&&NumberOfBytesWritten==nLengthofStr)
					{
						SetDlgItemText(hDlg,IDC_EDITDIR,lpExtDir);
					}
					else
						SetDlgItemText(hDlg,IDC_EDITDIR,lpWindowsDirName);

				}
				else
					SetDlgItemText(hDlg,IDC_EDITDIR,lpWindowsDirName);
				
				//the output temppath
				if((ReadFile(hFile,&nLengthofStr,sizeof(nLengthofStr),&NumberOfBytesWritten,NULL)==TRUE)
					&&NumberOfBytesWritten==sizeof(nLengthofStr)&&nLengthofStr!=0)
				{
					
					if((ReadFile(hFile,lpOutputpath,nLengthofStr,&NumberOfBytesWritten,NULL)==TRUE)&&NumberOfBytesWritten==nLengthofStr)
					{
						SetDlgItemText(hDlg,IDC_EDITPATH,lpOutputpath);
					}
					else
						SetDlgItemText(hDlg,IDC_EDITPATH,lpTempPath);

				}
				else
					SetDlgItemText(hDlg,IDC_EDITPATH,lpTempPath);


				CloseHandle(hFile);
			}
			else
			{
				SendMessage(GetDlgItem(hDlg,IDC_RADIO1),BM_SETCHECK,(WPARAM)0x01,(LPARAM)0);
				SendMessage(GetDlgItem(hDlg,IDC_RADIO2),BM_SETCHECK,(WPARAM)0x00,(LPARAM)0);
				SendMessage(GetDlgItem(hDlg,IDC_CHECKDIR),BM_SETCHECK,(WPARAM)0x00,(LPARAM)0);
				SetDlgItemText(hDlg,IDC_EDITDIR,lpWindowsDirName);
				SetDlgItemText(hDlg,IDC_EDITPATH,lpTempPath);
				//SendMessage(GetDlgItem(hDlg,IDC_CHECKTURBO),BM_SETCHECK,(WPARAM)0,(LPARAM)0);
			}
*/			//EnableWindow(GetDlgItem(hDlg,IDC_CHECKWRITECONTENT),FALSE);
			SendMessage(hDlg,WM_COMMAND,(WPARAM)IDC_CHECKDIR,(LPARAM)0);

			lpLastSaveDir=lpOutputpath;
			lpLastOpenDir=lpOutputpath;
			
			lpRegshotIni=GlobalAlloc(LPTR,MAX_PATH);
			lstrcpy(lpRegshotIni,lpStartDir);
			if (*(lpRegshotIni+lstrlen(lpRegshotIni)-1)!='\\')
				lstrcat(lpRegshotIni,"\\");
			lstrcat(lpRegshotIni,REGSHOTINI);

			GetSnapRegs(hDlg);
			
			return TRUE;
			
	case	WM_COMMAND:
			switch(LOWORD(wParam))
			{
			case	IDC_1STSHOT:
					CreateShotPopupMenu();
					is1=TRUE;
					GetWindowRect(GetDlgItem(hDlg,IDC_1STSHOT),&rect);
					TrackPopupMenu(hMenu,TPM_LEFTALIGN|TPM_LEFTBUTTON,rect.left+10,rect.top+10,0,hDlg,NULL);
					DestroyMenu(hMenu);
					
					return(TRUE);
			case	IDC_2NDSHOT:
					CreateShotPopupMenu();
					is1=FALSE;
					GetWindowRect(GetDlgItem(hDlg,IDC_2NDSHOT),&rect);
					TrackPopupMenu(hMenu,TPM_LEFTALIGN|TPM_LEFTBUTTON,rect.left+10,rect.top+10,0,hDlg,NULL);
					DestroyMenu(hMenu);
					return(TRUE);
			case	IDM_ONLYSHOT:
					if(is1)
					{
						is1LoadFromHive=FALSE;
						OnlyShot1();
					}
					else
					{
						is2LoadFromHive=FALSE;
						OnlyShot2();
					}

					return(TRUE);
			case	IDM_SHOTSAVE:
					if(is1)
					{
						is1LoadFromHive=FALSE;
						OnlyShot1();
						SaveRegistry(lpHeadLocalMachine1,lpHeadUsers1,lpComputerName1,lpUserName1,lpSystemtime1); //I might use a struct in future!
					}
					else
					{
						is2LoadFromHive=FALSE;
						OnlyShot2();
						SaveRegistry(lpHeadLocalMachine2,lpHeadUsers2,lpComputerName2,lpUserName2,lpSystemtime2);
					}

					return(TRUE);

			case	IDM_LOAD:
					if(is1)
						is1LoadFromHive=LoadRegistry(&lpHeadLocalMachine1,&lpHeadUsers1,lpTempHive1);
					else
						is2LoadFromHive=LoadRegistry(&lpHeadLocalMachine2,&lpHeadUsers2,lpTempHive2);

					if(is1LoadFromHive||is2LoadFromHive)
						SendMessage(GetDlgItem(hWnd,IDC_CHECKDIR),BM_SETCHECK,(WPARAM)0x00,(LPARAM)0);

					return(TRUE);
					
			//case	IDC_SAVEREG:
					//SaveRegistry(lpHeadLocalMachine1,lpHeadUsers1);
			//		return(TRUE);
			case	IDC_COMPARE:
					/*
					hHourGlass=LoadCursor(NULL,IDC_WAIT);
					hSaveCursor=SetCursor(hHourGlass);
					ShowWindow(GetDlgItem(hDlg,IDC_TEXTCOUNT1),SW_HIDE);
					ShowWindow(GetDlgItem(hDlg,IDC_TEXTCOUNT2),SW_HIDE);
					ShowWindow(GetDlgItem(hDlg,IDC_TEXTCOUNT3),SW_HIDE);
					//ShowWindow(GetDlgItem(hDlg,IDC_PBCOMPARE),SW_SHOW);
					
					*/
					EnableWindow(GetDlgItem(hDlg,IDC_COMPARE),FALSE);
					PreClear();
					RegCompare(hDlg);
					ShowWindow(GetDlgItem(hDlg,IDC_PBCOMPARE),SW_HIDE);
					EnableWindow(GetDlgItem(hDlg,IDC_CLEAR1),TRUE);
					SetFocus(GetDlgItem(hDlg,IDC_CLEAR1));
					SendMessage(hDlg,DM_SETDEFID,(WPARAM)IDC_CLEAR1,(LPARAM)0);
					SetCursor(hSaveCursor);					
					MessageBeep(0xffffffff);
					return(TRUE);
			case	IDC_CLEAR1:
					hMenuClear=CreatePopupMenu();
					AppendMenu(hMenuClear,MF_STRING,IDM_CLEARALLSHOTS,str_menuclearallshots);
					AppendMenu(hMenuClear,MF_MENUBARBREAK,IDM_BREAK,NULL);
					AppendMenu(hMenuClear,MF_STRING,IDM_CLEARSHOT1,str_menuclearshot1);
					AppendMenu(hMenuClear,MF_STRING,IDM_CLEARSHOT2,str_menuclearshot2);
					SetMenuDefaultItem(hMenuClear,IDM_CLEARALLSHOTS,FALSE);
			
					
					//if(SendMessage(GetDlgItem(hDlg,IDC_CHECKDIR),BM_GETCHECK,(WPARAM)0,(LPARAM)0)==1||
					//	is1LoadFromHive||is2LoadFromHive)
					if(lpHeadFile!=NULL)
					{
						EnableMenuItem(hMenuClear,IDM_CLEARSHOT1,MF_BYCOMMAND|MF_GRAYED);
						EnableMenuItem(hMenuClear,IDM_CLEARSHOT2,MF_BYCOMMAND|MF_GRAYED);
					}
					else
					{
						if(lpHeadLocalMachine1!=NULL)
							EnableMenuItem(hMenuClear,IDM_CLEARSHOT1,MF_BYCOMMAND|MF_ENABLED);
						else
							EnableMenuItem(hMenuClear,IDM_CLEARSHOT1,MF_BYCOMMAND|MF_GRAYED);

						if(lpHeadLocalMachine2!=NULL)
							EnableMenuItem(hMenuClear,IDM_CLEARSHOT2,MF_BYCOMMAND|MF_ENABLED);
						else
							EnableMenuItem(hMenuClear,IDM_CLEARSHOT2,MF_BYCOMMAND|MF_GRAYED);
					}
					GetWindowRect(GetDlgItem(hDlg,IDC_CLEAR1),&rect);
					TrackPopupMenu(hMenuClear,TPM_LEFTALIGN|TPM_LEFTBUTTON,rect.left+10,rect.top+10,0,hDlg,NULL);
					DestroyMenu(hMenuClear);
					return(TRUE);
			case	IDM_CLEARALLSHOTS:
					PreClear();
					FreeAllKeyContent1();
					FreeAllKeyContent2();
					FreeAllCompareResults();
					FreeAllFiles();
					AfterClear();
					EnableWindow(GetDlgItem(hWnd,IDC_CLEAR1),FALSE);
					return(TRUE);
			case	IDM_CLEARSHOT1:
					PreClear();
					FreeAllKeyContent1();
					FreeAllCompareResults();
					
					ClearKeyMatchTag(lpHeadLocalMachine2); //we clear shot2's tag
					ClearKeyMatchTag(lpHeadUsers2);
					AfterClear();
					return(TRUE);
			case	IDM_CLEARSHOT2:
					PreClear();
					FreeAllKeyContent2();
					FreeAllCompareResults();
					ClearKeyMatchTag(lpHeadLocalMachine1); //we clear shot1's tag
					ClearKeyMatchTag(lpHeadUsers1);
					AfterClear();
					return(TRUE);

			case	IDC_CHECKDIR:
					if(SendMessage(GetDlgItem(hDlg,IDC_CHECKDIR),BM_GETCHECK,(WPARAM)0,(LPARAM)0)==1)
					{
						EnableWindow(GetDlgItem(hDlg,IDC_EDITDIR),TRUE);
						EnableWindow(GetDlgItem(hDlg,IDC_BROWSE1),TRUE);
					}
					else
					{
						EnableWindow(GetDlgItem(hDlg,IDC_EDITDIR),FALSE);
						EnableWindow(GetDlgItem(hDlg,IDC_BROWSE1),FALSE);
					}
					return(TRUE);
			case	IDC_CANCEL1:
			case	IDCANCEL:
/*					SetCurrentDirectory(lpStartDir);
					hFile = CreateFile(REGSHOTDATFILE,GENERIC_READ | GENERIC_WRITE,FILE_SHARE_READ | FILE_SHARE_WRITE,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
					if( hFile != INVALID_HANDLE_VALUE) 
					{
						
						nFlag=(BYTE)(SendMessage(GetDlgItem(hDlg,IDC_RADIO1),BM_GETCHECK,(WPARAM)0,(LPARAM)0)|
							SendMessage(GetDlgItem(hDlg,IDC_CHECKDIR),BM_GETCHECK,(WPARAM)0,(LPARAM)0)<<1);
						WriteFile(hFile,&nFlag,1,&NumberOfBytesWritten,NULL);
						WriteFile(hFile,&nMask,4,&NumberOfBytesWritten,NULL);
						nLengthofStr = GetDlgItemText(hDlg,IDC_EDITDIR,lpExtDir,EXTDIRLEN+2);
						WriteFile(hFile,&nLengthofStr,sizeof(nLengthofStr),&NumberOfBytesWritten,NULL);
						WriteFile(hFile,lpExtDir,nLengthofStr,&NumberOfBytesWritten,NULL);
						nLengthofStr = GetDlgItemText(hDlg,IDC_EDITPATH,lpOutputpath,MAX_PATH);
						WriteFile(hFile,&nLengthofStr,sizeof(nLengthofStr),&NumberOfBytesWritten,NULL);
						WriteFile(hFile,lpOutputpath,nLengthofStr,&NumberOfBytesWritten,NULL);

						CloseHandle(hFile);
					}
*/	
					SetSnapRegs(hDlg);
					PostQuitMessage(0);
					return(TRUE);

			case	IDC_BROWSE1:
				{

					LPITEMIDLIST lpidlist;
					DWORD	nWholeLen;
					BrowseInfo1.hwndOwner=hDlg;
					BrowseInfo1.pszDisplayName=GlobalAlloc(LPTR,MAX_PATH+1);
					//BrowseInfo1.lpszTitle="Select:";
					lpidlist=SHBrowseForFolder(&BrowseInfo1);
					if (lpidlist!=NULL)
					{
						SHGetPathFromIDList(lpidlist,BrowseInfo1.pszDisplayName);
						nLengthofStr = GetDlgItemText(hDlg,IDC_EDITDIR,lpExtDir,EXTDIRLEN+2);
						nWholeLen=nLengthofStr+lstrlen(BrowseInfo1.pszDisplayName);
						
						if (nWholeLen<EXTDIRLEN+1)
						{
							lstrcat(lpExtDir,";");
							lstrcat(lpExtDir,BrowseInfo1.pszDisplayName);

						}
						else
							lstrcpy(lpExtDir,BrowseInfo1.pszDisplayName);
						
						SetDlgItemText(hDlg,IDC_EDITDIR,lpExtDir);
						GlobalFree(lpidlist);
					}
					
					GlobalFree(BrowseInfo1.pszDisplayName);
				}
				return(TRUE);
					
			case	IDC_BROWSE2:
				{

					LPITEMIDLIST lpidlist;
					BrowseInfo1.hwndOwner=hDlg;
					BrowseInfo1.pszDisplayName=GlobalAlloc(LPTR,MAX_PATH+1);
					//BrowseInfo1.lpszTitle="Select:";
					lpidlist=SHBrowseForFolder(&BrowseInfo1);
					if (lpidlist!=NULL)
					{
						SHGetPathFromIDList(lpidlist,BrowseInfo1.pszDisplayName);
						SetDlgItemText(hDlg,IDC_EDITPATH,BrowseInfo1.pszDisplayName);
						GlobalFree(lpidlist);
					}
					
					GlobalFree(BrowseInfo1.pszDisplayName);
				}
				return(TRUE);
			case	IDC_COMBOLANGUAGE:
					GetLanguageStrings(hDlg);
					return(TRUE);
					
			case	IDC_ABOUT:
					{	
					LPSTR	lpAboutBox;
					//_asm int 3;
					lpAboutBox=GlobalAlloc(LPTR,SIZEOF_ABOUTBOX);
					//it is silly that when wsprintf encounter a NULL strings, it will write the whole string to NULL!
					wsprintf(lpAboutBox,"%s%s%s%s%s%s",str_aboutme,"[",(lstrlen(lpCurrentLanguage)==0)?lpDefaultLanguage:lpCurrentLanguage,"]"," by:",lpCurrentTranslator);
					MessageBox(hDlg,lpAboutBox,str_about,MB_OK);
					GlobalFree(lpAboutBox);
					return(TRUE);
					}
			}

	}
	return(FALSE);
}
/*
BOOL	SetPrivilege(HANDLE hToken,LPCTSTR pString,BOOL bEnablePrivilege)
{
	TOKEN_PRIVILEGES	tp;
	LUID	luid;
	TOKEN_PRIVILEGES	tpPrevious;
	DWORD	cbSize=sizeof(TOKEN_PRIVILEGES);
	if	(!LookupPrivilegeValue(NULL,pString,&luid))
		return FALSE;
	tp.PrivilegeCount=1;
	tp.Privileges[0].Luid=luid;
	tp.Privileges[0].Attributes=0;
	if	(!AdjustTokenPrivileges(hToken,FALSE,&tp,sizeof(TOKEN_PRIVILEGES),&tpPrevious,&cbSize))
		return FALSE;
	tpPrevious.PrivilegeCount=1;
	tpPrevious.Privileges[0].Luid=luid;
	if	(bEnablePrivilege)
		tpPrevious.Privileges[0].Attributes|=(SE_PRIVILEGE_ENABLED);
	else
		tpPrevious.Privileges[0].Attributes^=((tpPrevious.Privileges[0].Attributes)&(SE_PRIVILEGE_ENABLED));
	if	(!AdjustTokenPrivileges(hToken,FALSE,&tpPrevious,cbSize,NULL,NULL))
		return FALSE;
	return TRUE;
}

*/


//////////////////////////////////////////////////////////////////
int		PASCAL WinMain(hInstance,hPrevInstance,lpszCmdLine,nCmdShow)
HINSTANCE	hInstance;
HINSTANCE  hPrevInstance;
LPSTR	lpszCmdLine;
int		nCmdShow;
{
	/*
	BOOL	bWinNTDetected;
	HANDLE	hToken=0;
	OSVERSIONINFO winver;
	winver.dwOSVersionInfoSize=sizeof(OSVERSIONINFO);
	GetVersionEx(&winver);
	bWinNTDetected=(winver.dwPlatformId==VER_PLATFORM_WIN32_NT) ? TRUE : FALSE;
	//hWndMonitor be created first for the multilanguage interface.
	
	//FARPROC		lpfnDlgProc;     
	//lpfnDlgProc	=	MakeProcInstance((FARPROC)DialogProc,hInstance); //old style of create dialogproc
	*/			
			

	hWnd=CreateDialog(hInstance,MAKEINTRESOURCE(IDD_DIALOG1),NULL,(WNDPROC)DialogProc);
	
	SetClassLong(hWnd,GCL_HICON,(LONG)LoadIcon(hInstance,MAKEINTRESOURCE(IDI_ICON1)));

	SetWindowText(hWnd, str_prgname); //tfx 设置程序标题为str_prgname，避免修改资源文件
	ShowWindow(hWnd,nCmdShow);
	UpdateWindow(hWnd);		   
	//SetPriorityClass(hInstance,31);
	/*
	if	(bWinNTDetected)
	{
		if	(OpenProcessToken(GetCurrentProcess(),TOKEN_ADJUST_PRIVILEGES|TOKEN_QUERY,&hToken)==TRUE)
		{
			if	(SetPrivilege(hToken,"SeSystemProfilePrivilege",TRUE)==TRUE)
			{
				MessageBox(hWnd,"We are in system level,enjoy!","Info:",MB_OK);
			}
			CloseHandle(hToken);
		}
	}
   	*/
	while(GetMessage(&msg,NULL,(WPARAM)NULL,(LPARAM)NULL))
	{
		if(!IsDialogMessage(hWnd,&msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	return(msg.wParam);
}

/* Shortcomings of Regshot

1) It was written in C,not in OOL as MFC or something,So it
   is rather difficult to read and understand;and it is not easy
   to Export.[But it is so small,30K~50K,the early versions have
   some ASM codes,only 20K]

2) The memory allocate API[GlobalAlloc()] I used was so slow under Windows NT4,
   I should use VisulAlloc() or something that would allocate a big block of memory 
   rather than thousands of small pieces of memory.And if I do that,the ReAlign()
   would be useless,and the whole program will be super fast!

3) Currently,files are not save in hive file.

 *********************************************************
 *      Code by  TiANWEi[Huainan,Anhui,China]            *
 *********************************************************/
