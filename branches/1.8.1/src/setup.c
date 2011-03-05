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


//setup based on regshot.ini by tulipfan
LPSTR	INI_SETUP			="Setup";
LPSTR	INI_FLAG			="Flag";
LPSTR	INI_EXTDIR			="ExtDir";
LPSTR	INI_OUTDIR			="OutDir";
LPSTR	INI_SKIPREGKEY		="SkipRegKey";
LPSTR	INI_SKIPDIR			="SkipDir";
LPSTR	INI_USELONGREGHEAD	="UseLongRegHead"; //1.8.1 tianwei for compatible to undoreg 1.46 again

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
			if((lpSnapReturn=AtPos(lpSnapRegsStr,lpSnapKey,SIZEOF_REGSHOT))!=NULL)
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
			if((lpSnapReturn=AtPos(lpSnapFilesStr,lpSnapKey,SIZEOF_REGSHOT))!=NULL)
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
	//if(nFlag!=0)
	{
		SendMessage(GetDlgItem(hDlg,IDC_RADIO1),BM_SETCHECK,(WPARAM)(nFlag&0x01),(LPARAM)0);
		SendMessage(GetDlgItem(hDlg,IDC_RADIO2),BM_SETCHECK,(WPARAM)((nFlag&0x01)^0x01),(LPARAM)0);
		//SendMessage(GetDlgItem(hDlg,IDC_CHECKDIR),BM_SETCHECK,(WPARAM)((nFlag&0x04)>>1),(LPARAM)0); //1.7
		SendMessage(GetDlgItem(hDlg,IDC_CHECKDIR),BM_SETCHECK,(WPARAM)((nFlag&0x02)>>1),(LPARAM)0);
	}
	/*else  delete in 1.8.1
	{
		SendMessage(GetDlgItem(hDlg,IDC_RADIO1),BM_SETCHECK,(WPARAM)0x01,(LPARAM)0);
		SendMessage(GetDlgItem(hDlg,IDC_RADIO2),BM_SETCHECK,(WPARAM)0x00,(LPARAM)0);
		SendMessage(GetDlgItem(hDlg,IDC_CHECKDIR),BM_SETCHECK,(WPARAM)0x00,(LPARAM)0);
	}
	*/
	//added in 1.8.1 for compatible with undoreg1.46
	bUseLongRegHead=GetPrivateProfileInt(INI_SETUP,INI_USELONGREGHEAD,0,lpRegshotIni)!=0 ? TRUE:FALSE;

	if(GetPrivateProfileString(INI_SETUP,INI_EXTDIR,NULL,lpExtDir,MAX_PATH,lpRegshotIni)!=0)
		SetDlgItemText(hDlg,IDC_EDITDIR,lpExtDir);
	else
		SetDlgItemText(hDlg,IDC_EDITDIR,lpWindowsDirName);

	if(GetPrivateProfileString(INI_SETUP,INI_OUTDIR,NULL,lpOutputpath,MAX_PATH,lpRegshotIni)!=0)
		SetDlgItemText(hDlg,IDC_EDITPATH,lpOutputpath);
	else
		SetDlgItemText(hDlg,IDC_EDITPATH,lpTempPath);

	SendMessage(hDlg,WM_COMMAND,(WPARAM)IDC_CHECKDIR,(LPARAM)0);

	return TRUE;
}


BOOL SetSnapRegs(HWND hDlg) //tfx 保存信息到配置文件
{
	BYTE nFlag;
	LPSTR lpString;

	//nFlag=(BYTE)(SendMessage(GetDlgItem(hDlg,IDC_RADIO1),BM_GETCHECK,(WPARAM)0,(LPARAM)0) //1.7
	//	|SendMessage(GetDlgItem(hDlg,IDC_RADIO2),BM_GETCHECK,(WPARAM)0,(LPARAM)0)<<1
	//	|SendMessage(GetDlgItem(hDlg,IDC_CHECKDIR),BM_GETCHECK,(WPARAM)0,(LPARAM)0)<<2);
	nFlag=(BYTE)(SendMessage(GetDlgItem(hDlg,IDC_RADIO1),BM_GETCHECK,(WPARAM)0,(LPARAM)0)|
			SendMessage(GetDlgItem(hDlg,IDC_CHECKDIR),BM_GETCHECK,(WPARAM)0,(LPARAM)0)<<1);

	lpString=GlobalAlloc(LMEM_ZEROINIT,EXTDIRLEN+2);
	//sprintf(lpString,"%s=%d",INI_FLAG,nFlag); //1.7 solokey
	//WritePrivateProfileSection(INI_SETUP,lpString,lpRegshotIni);  //1.7 solokey ,can only have one key.
	
	//1.8.1
	sprintf(lpString,"%d",nFlag);
	WritePrivateProfileString(INI_SETUP,INI_FLAG,lpString,lpRegshotIni);
	sprintf(lpString,"%d",bUseLongRegHead);
	WritePrivateProfileString(INI_SETUP,INI_USELONGREGHEAD,lpString,lpRegshotIni);


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

