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
#include "version.h"
#include "LMCons.h"  // for UNLEN define

LPTSTR lpszDefResPre = REGSHOT_RESULT_FILE;

LPTSTR lpszFilter =
#ifdef _UNICODE
    TEXT("Regshot Unicode hive files (*.hiv2)\0*.hiv2\0All files\0*.*\0\0");
#else
    TEXT("Regshot ANSI hive files (*.hiv)\0*.hiv\0All files\0*.*\0\0");
#endif

// SBCS/MBCS signature (even in Unicode builds for backwards compatibility)
char szRegshotFileSignatureSBCS[] = "REGSHOTHIVE";
char szRegshotFileSignatureUTF16[] = "REGSHOTHIV2";  // use a number to define a file format not compatible with older releases (e.g. "3" could be UTF-32 or Big Endian)
#ifdef _UNICODE
#define szRegshotFileSignature szRegshotFileSignatureUTF16
#else
#define szRegshotFileSignature szRegshotFileSignatureSBCS
#endif

LPTSTR lpszRegshotFileDefExt =
#ifdef _UNICODE
    TEXT("hiv2");
#else
    TEXT("hiv");
#endif

LPTSTR lpszValueDataIsNULL = TEXT(": (NULL!)");

#ifndef _UNICODE
LPTSTR lpszEmpty = TEXT("");
#endif

FILEHEADER fileheader;
SAVEKEYCONTENT sKC;
SAVEVALUECONTENT sVC;

LPBYTE lpFileBuffer;
LPTSTR lpStringBuffer;
size_t nStringBufferSize;
LPBYTE lpDataBuffer;
size_t nDataBufferSize;
size_t nSourceSize;

#define MAX_SIGNATURE_LENGTH 12
#define REGSHOT_READ_BLOCK_SIZE 8192

// Pointers to compare result
LPCOMRESULT lpKEYADD;
LPCOMRESULT lpKEYDEL;
LPCOMRESULT lpVALADD;
LPCOMRESULT lpVALDEL;
LPCOMRESULT lpVALMODI;
LPCOMRESULT lpFILEADD;
LPCOMRESULT lpFILEDEL;
LPCOMRESULT lpFILEMODI;
LPCOMRESULT lpDIRADD;
LPCOMRESULT lpDIRDEL;
LPCOMRESULT lpDIRMODI;

LPCOMRESULT lpKEYADDHEAD;
LPCOMRESULT lpKEYDELHEAD;
LPCOMRESULT lpVALADDHEAD;
LPCOMRESULT lpVALDELHEAD;
LPCOMRESULT lpVALMODIHEAD;
LPCOMRESULT lpFILEADDHEAD;
LPCOMRESULT lpFILEDELHEAD;
LPCOMRESULT lpFILEMODIHEAD;
LPCOMRESULT lpDIRADDHEAD;
LPCOMRESULT lpDIRDELHEAD;
LPCOMRESULT lpDIRMODIHEAD;

// Number of Modifications detected
DWORD nKEYADD;
DWORD nKEYDEL;
DWORD nVALADD;
DWORD nVALDEL;
DWORD nVALMODI;
DWORD nFILEADD;
DWORD nFILEDEL;
DWORD nFILEMODI;
DWORD nDIRADD;
DWORD nDIRDEL;
DWORD nDIRMODI;

// Some DWORDs used to show the progress bar and etc
DWORD nGettingValue;
DWORD nGettingKey;
DWORD nComparing;
DWORD nRegStep;
DWORD nFileStep;
DWORD nSavingKey;
DWORD nGettingTime;
DWORD nBASETIME;
DWORD nBASETIME1;
//DWORD nMask = 0xf7fd;     // not used now, but should be added; TODO: what for?
DWORD NBW;                // that is: NumberOfBytesWritten;

HANDLE   hFileWholeReg;  // Handle of file regshot use
FILETIME ftLastWrite;    // Filetime struct

// in regshot.ini, "UseLongRegHead" to control this
// 1.6 using long name, so in 1.8.1 add an option
LPTSTR lpszHKLMShort = TEXT("HKLM");
LPTSTR lpszHKLMLong  = TEXT("HKEY_LOCAL_MACHINE");
#define CCH_HKLM_LONG  18

LPTSTR lpszHKUShort  = TEXT("HKU");
LPTSTR lpszHKULong   = TEXT("HKEY_USERS");
#define CCH_HKU_LONG  10

LONG nErrNo;


// ----------------------------------------------------------------------
// Get whole registry key name from KEYCONTENT
// ----------------------------------------------------------------------
LPTSTR GetWholeKeyName(LPKEYCONTENT lpStartKC, BOOL fUseLongNames)
{
    LPKEYCONTENT lpKC;
    LPTSTR lpszName;
    LPTSTR lpszTail;
    size_t cchName;
    LPTSTR lpszKeyName;

    cchName = 0;
    for (lpKC = lpStartKC; NULL != lpKC; lpKC = lpKC->lpFatherKC) {
        if (NULL != lpKC->lpszKeyName) {
            if ((fUseLongNames) && (lpszHKLMShort == lpKC->lpszKeyName)) {
                cchName += CCH_HKLM_LONG;
            } else if ((fUseLongNames) && (lpszHKUShort == lpKC->lpszKeyName)) {
                cchName += CCH_HKU_LONG;
            } else {
                cchName += lpKC->cchKeyName;
            }
            cchName++;  // +1 char for backslash or NULL char
        }
    }
    if (0 == cchName) {  // at least create an empty string with NULL char
        cchName++;
    }

    lpszName = MYALLOC(cchName * sizeof(TCHAR));

    lpszTail = &lpszName[cchName - 1];
    lpszTail[0] = (TCHAR)'\0';

    for (lpKC = lpStartKC; NULL != lpKC; lpKC = lpKC->lpFatherKC) {
        if (NULL != lpKC->lpszKeyName) {
            if ((fUseLongNames) && (lpszHKLMShort == lpKC->lpszKeyName)) {
                cchName = CCH_HKLM_LONG;
                lpszKeyName = lpszHKLMLong;
            } else if ((fUseLongNames) && (lpszHKUShort == lpKC->lpszKeyName)) {
                cchName = CCH_HKU_LONG;
                lpszKeyName = lpszHKULong;
            } else {
                cchName = lpKC->cchKeyName;
                lpszKeyName = lpKC->lpszKeyName;
            }
            _tcsncpy(lpszTail -= cchName, lpszKeyName, cchName);
            if (lpszTail > lpszName) {
                *--lpszTail = (TCHAR)'\\';
            }
        }
    }

    return lpszName;
}


// ----------------------------------------------------------------------
// Get whole value name from VALUECONTENT
// ----------------------------------------------------------------------
LPTSTR GetWholeValueName(LPVALUECONTENT lpVC, BOOL fUseLongNames)
{
    LPKEYCONTENT lpKC;
    LPTSTR lpszName;
    LPTSTR lpszTail;
    size_t cchName;
    size_t cchValueName;
    LPTSTR lpszKeyName;

    cchValueName = 0;
    if (NULL != lpVC->lpszValueName) {
        cchValueName = lpVC->cchValueName;
    }
    cchName = cchValueName + 1;  // +1 char for NULL char

    for (lpKC = lpVC->lpFatherKC; NULL != lpKC; lpKC = lpKC->lpFatherKC) {
        if (NULL != lpKC->lpszKeyName) {
            if ((fUseLongNames) && (lpszHKLMShort == lpKC->lpszKeyName)) {
                cchName += CCH_HKLM_LONG;
            } else if ((fUseLongNames) && (lpszHKUShort == lpKC->lpszKeyName)) {
                cchName += CCH_HKU_LONG;
            } else {
                cchName += lpKC->cchKeyName;
            }
            cchName++;  // +1 char for backslash
        }
    }

    lpszName = MYALLOC(cchName * sizeof(TCHAR));

    lpszTail = &lpszName[cchName - 1];
    lpszTail[0] = (TCHAR)'\0';

    if (NULL != lpVC->lpszValueName) {
        _tcsncpy(lpszTail -= cchValueName, lpVC->lpszValueName, cchValueName);
    }
    if (lpszTail > lpszName) {
        *--lpszTail = (TCHAR)'\\';
    }

    for (lpKC = lpVC->lpFatherKC; NULL != lpKC; lpKC = lpKC->lpFatherKC) {
        if (NULL != lpKC->lpszKeyName) {
            if ((fUseLongNames) && (lpszHKLMShort == lpKC->lpszKeyName)) {
                cchName = CCH_HKLM_LONG;
                lpszKeyName = lpszHKLMLong;
            } else if ((fUseLongNames) && (lpszHKUShort == lpKC->lpszKeyName)) {
                cchName = CCH_HKU_LONG;
                lpszKeyName = lpszHKULong;
            } else {
                cchName = lpKC->cchKeyName;
                lpszKeyName = lpKC->lpszKeyName;
            }
            _tcsncpy(lpszTail -= cchName, lpszKeyName, cchName);
            if (lpszTail > lpszName) {
                *--lpszTail = (TCHAR)'\\';
            }
        }
    }

    return lpszName;
}


// ----------------------------------------------------------------------
// Transform VALUECONTENT.data from binary into string
// Called by GetWholeValueData()
// ----------------------------------------------------------------------
LPTSTR TransData(LPVALUECONTENT lpVC, DWORD type)
{
    LPTSTR lpszValueData;
    DWORD nCount;
    DWORD nSize;

    lpszValueData = NULL;
    nSize = lpVC->cbData;

    if (NULL == lpVC->lpValueData) {
        lpszValueData = MYALLOC0(sizeof(lpszValueDataIsNULL));
        _tcscpy(lpszValueData, lpszValueDataIsNULL);
    } else {
        switch (type) {
            case REG_SZ:
                // case REG_EXPAND_SZ: Not used any more, is included in [default], because some non-regular value would corrupt this.
                lpszValueData = MYALLOC0(((3 + 2) * sizeof(TCHAR)) + nSize);  // format  ": \"<string>\"\0"
                _tcscpy(lpszValueData, TEXT(": \""));
                if (NULL != lpVC->lpValueData) {
                    _tcscat(lpszValueData, (LPTSTR)lpVC->lpValueData);
                }
                _tcscat(lpszValueData, TEXT("\""));
                // wsprintf has a bug that cannot print string too long one time!);
                //wsprintf(lpszValueData,"%s%s%s",": \"",lpVC->lpValueData,"\"");
                break;
            case REG_MULTI_SZ:
                // Be sure to add below line outside of following "if",
                // for that GlobalFree(lp) must had lp already located!
                lpszValueData = MYALLOC0(((3 + 2) * sizeof(TCHAR)) + nSize);  // format  ": \"<string>\"\0"
                nSize /= sizeof(TCHAR);  // convert bytes to chars
                nSize--;  // account for last NULL char
                // TODO: this logic destroys the original data, better copy data to target and replace there
                for (nCount = 0; nCount < nSize; nCount++) {
                    if ((TCHAR)'\0' == ((LPTSTR)lpVC->lpValueData)[nCount]) {        // look for a NULL char before the end of the data
                        ((LPTSTR)lpVC->lpValueData)[nCount] = (TCHAR)' ';  // then overwrite with space  // TODO: check if works with Unicode
                    }
                }
                _tcscpy(lpszValueData, TEXT(": \""));
                if (NULL != lpVC->lpValueData) {
                    _tcscat(lpszValueData, (LPTSTR)lpVC->lpValueData);
                }
                _tcscat(lpszValueData, TEXT("\""));
                //wsprintf(lpszValueData,"%s%s%s",": \"",lpVC->lpValueData,"\"");
                break;
            case REG_DWORD:
                // case REG_DWORD_BIG_ENDIAN: Not used any more, they all included in [default]
                lpszValueData = MYALLOC0((2 + 2 + 8 + 1) * sizeof(TCHAR));  // format  ": 0xXXXXXXXX\0"
                _tcscpy(lpszValueData, TEXT(": "));
                if (NULL != lpVC->lpValueData) {
                    _sntprintf(lpszValueData + 2, (2 + 8 + 1), TEXT("%s%08X\0"), TEXT("0x"), *(LPDWORD)(lpVC->lpValueData));
                }
                break;
            default:
                lpszValueData = MYALLOC0((2 + (nSize * 3) + 1) * sizeof(TCHAR));  // format ": [ xx][ xx]...[ xx]\0"
                _tcscpy(lpszValueData, TEXT(": "));
                // for the resttype lengthofvaluedata doesn't contains the 0!
                for (nCount = 0; nCount < nSize; nCount++) {
                    _sntprintf(lpszValueData + (2 + (nCount * 3)), 4, TEXT(" %02X\0"), *(lpVC->lpValueData + nCount));
                }
        }
    }

    return lpszValueData;
}


// ----------------------------------------------------------------------
// Get value data from VALUECONTENT as string
// ----------------------------------------------------------------------
LPTSTR GetWholeValueData(LPVALUECONTENT lpVC)
{
    LPTSTR lpszValueData;
    DWORD nCount;
    DWORD nSize;

    lpszValueData = NULL;
    nSize = lpVC->cbData;

    if (NULL == lpVC->lpValueData) {
        lpszValueData = MYALLOC0(sizeof(lpszValueDataIsNULL));
        _tcscpy(lpszValueData, lpszValueDataIsNULL);
    } else {
        switch (lpVC->nTypeCode) {
            case REG_SZ:
            case REG_EXPAND_SZ:
                if ((DWORD)((_tcslen((LPTSTR)lpVC->lpValueData) + 1) * sizeof(TCHAR)) == nSize) {
                    lpszValueData = TransData(lpVC, REG_SZ);
                } else {
                    lpszValueData = TransData(lpVC, REG_BINARY);
                }
                break;
            case REG_MULTI_SZ:
                if (0 != ((LPTSTR)lpVC->lpValueData)[0]) {
                    for (nCount = 0; ; nCount++) {
                        if (0 == ((LPTSTR)lpVC->lpValueData)[nCount]) {
                            break;
                        }
                    }
                    if (((nCount + 1) * sizeof(TCHAR)) == nSize) {
                        lpszValueData = TransData(lpVC, REG_MULTI_SZ);
                    } else {
                        lpszValueData = TransData(lpVC, REG_BINARY);
                    }
                } else {
                    lpszValueData = TransData(lpVC, REG_BINARY);
                }
                break;
            case REG_DWORD:
            case REG_DWORD_BIG_ENDIAN:
                if (sizeof(DWORD) == nSize) {
                    lpszValueData = TransData(lpVC, REG_DWORD);
                } else {
                    lpszValueData = TransData(lpVC, REG_BINARY);
                }
                break;
            default :
                lpszValueData = TransData(lpVC, REG_BINARY);
        }
    }

    return lpszValueData;
}


//-------------------------------------------------------------
// Routine to create new comparison result, distribute to different lp???MODI
//-------------------------------------------------------------
VOID CreateNewResult(DWORD actiontype, LPDWORD lpcount, LPTSTR lpresult)
{
    LPCOMRESULT lpnew;
    lpnew = (LPCOMRESULT)MYALLOC0(sizeof(COMRESULT));
    lpnew->lpszResult = lpresult;

    switch (actiontype) {
        case KEYADD:
            *lpcount == 0 ? (lpKEYADDHEAD = lpnew) : (lpKEYADD->lpnextresult = lpnew);
            lpKEYADD = lpnew;
            break;
        case KEYDEL:
            *lpcount == 0 ? (lpKEYDELHEAD = lpnew) : (lpKEYDEL->lpnextresult = lpnew);
            lpKEYDEL = lpnew;
            break;
        case VALADD:
            *lpcount == 0 ? (lpVALADDHEAD = lpnew) : (lpVALADD->lpnextresult = lpnew);
            lpVALADD = lpnew;
            break;
        case VALDEL:
            *lpcount == 0 ? (lpVALDELHEAD = lpnew) : (lpVALDEL->lpnextresult = lpnew);
            lpVALDEL = lpnew;
            break;
        case VALMODI:
            *lpcount == 0 ? (lpVALMODIHEAD = lpnew) : (lpVALMODI->lpnextresult = lpnew);
            lpVALMODI = lpnew;
            break;
        case FILEADD:
            *lpcount == 0 ? (lpFILEADDHEAD = lpnew) : (lpFILEADD->lpnextresult = lpnew);
            lpFILEADD = lpnew;
            break;
        case FILEDEL:
            *lpcount == 0 ? (lpFILEDELHEAD = lpnew) : (lpFILEDEL->lpnextresult = lpnew);
            lpFILEDEL = lpnew;
            break;
        case FILEMODI:
            *lpcount == 0 ? (lpFILEMODIHEAD = lpnew) : (lpFILEMODI->lpnextresult = lpnew);
            lpFILEMODI = lpnew;
            break;
        case DIRADD:
            *lpcount == 0 ? (lpDIRADDHEAD = lpnew) : (lpDIRADD->lpnextresult = lpnew);
            lpDIRADD = lpnew;
            break;
        case DIRDEL:
            *lpcount == 0 ? (lpDIRDELHEAD = lpnew) : (lpDIRDEL->lpnextresult = lpnew);
            lpDIRDEL = lpnew;
            break;
        case DIRMODI:
            *lpcount == 0 ? (lpDIRMODIHEAD = lpnew) : (lpDIRMODI->lpnextresult = lpnew);
            lpDIRMODI = lpnew;
            break;

    }
    (*lpcount)++;
}


//-------------------------------------------------------------
// Write comparison results into memory and call CreateNewResult()
//-------------------------------------------------------------
VOID LogToMem(DWORD actiontype, LPDWORD lpcount, LPVOID lp)
{
    LPTSTR   lpname;
    LPTSTR   lpdata;
    LPTSTR   lpall;

    if (actiontype == KEYADD || actiontype == KEYDEL) {
        lpname = GetWholeKeyName(lp, bUseLongRegHead);
        CreateNewResult(actiontype, lpcount, lpname);
    } else {
        if (actiontype == VALADD || actiontype == VALDEL || actiontype == VALMODI) {
            lpname = GetWholeValueName(lp, bUseLongRegHead);
            lpdata = GetWholeValueData(lp);
            lpall = MYALLOC((_tcslen(lpname) + _tcslen(lpdata) + 1) * sizeof(TCHAR));
            // do not use:wsprintf(lpall,"%s%s",lpname,lpdata); !!! strlen limit!
            _tcscpy(lpall, lpname);
            _tcscat(lpall, lpdata);
            MYFREE(lpname);
            MYFREE(lpdata);
            CreateNewResult(actiontype, lpcount, lpall);
        } else {
            lpname = GetWholeFileName(lp, 0);
            CreateNewResult(actiontype, lpcount, lpname);
        }
    }
}


//-------------------------------------------------------------
// Log all values of registry key
//-------------------------------------------------------------
VOID LogAllRegValues(DWORD typevalue, LPDWORD lpcountvalue, LPKEYCONTENT lpKC)
{
    LPVALUECONTENT lpVC;

    for (lpVC = lpKC->lpFirstVC; NULL != lpVC; lpVC = lpVC->lpBrotherVC) {
        LogToMem(typevalue, lpcountvalue, lpVC);
    }
}


//-------------------------------------------------------------
// Log registry key plus sub keys with all values
// Optionally log all brother keys too
//-------------------------------------------------------------
VOID LogRegKeys(
    BOOL    fIncludeBrothers,
    DWORD   typekey,
    DWORD   typevalue,
    LPDWORD lpcountkey,
    LPDWORD lpcountvalue,
    LPKEYCONTENT lpStartKC
)
{
    LPKEYCONTENT lpKC;

    for (lpKC = lpStartKC; NULL != lpKC; lpKC = lpKC->lpBrotherKC) {
        LogToMem(typekey, lpcountkey, lpKC);
        LogAllRegValues(typevalue, lpcountvalue, lpKC);

        if (NULL != lpKC->lpFirstSubKC) {
            LogRegKeys(TRUE, typekey, typevalue, lpcountkey, lpcountvalue, lpKC->lpFirstSubKC);
        }

        if (!fIncludeBrothers) {  // do not include brother keys
            break;  // exit after processing start key
        }
    }
}


//-------------------------------------------------------------
// Routine to free all comparison results (release memory)
//-------------------------------------------------------------
VOID FreeAllCom(LPCOMRESULT lpComResult)
{
    LPCOMRESULT lp;
    LPCOMRESULT lpold;

    for (lp = lpComResult; NULL != lp;) {
        if (NULL != lp->lpszResult) {
            MYFREE(lp->lpszResult);
        }
        lpold = lp;
        lp = lp->lpnextresult;
        MYFREE(lpold);
    }

}

// ----------------------------------------------------------------------
// Free all compare results
// ----------------------------------------------------------------------
VOID FreeAllCompareResults(void)
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


    nKEYADD = 0;
    nKEYDEL = 0;
    nVALADD = 0;
    nVALDEL = 0;
    nVALMODI = 0;
    nFILEADD = 0;
    nFILEDEL = 0;
    nFILEMODI = 0;
    nDIRADD = 0;
    nDIRDEL = 0;
    nDIRMODI = 0;
    lpKEYADDHEAD = NULL;
    lpKEYDELHEAD = NULL;
    lpVALADDHEAD = NULL;
    lpVALDELHEAD = NULL;
    lpVALMODIHEAD = NULL;
    lpFILEADDHEAD = NULL;
    lpFILEDELHEAD = NULL;
    lpFILEMODIHEAD = NULL;
    lpDIRADDHEAD = NULL;
    lpDIRDELHEAD = NULL;
    lpDIRMODIHEAD = NULL;
}


//-------------------------------------------------------------
// Registry comparison engine
//-------------------------------------------------------------
VOID CompareRegKeys(LPKEYCONTENT lpStartKC1, LPKEYCONTENT lpStartKC2)
{
    LPKEYCONTENT lpKC1;
    LPKEYCONTENT lpKC2;

    // Compare keys
    for (lpKC1 = lpStartKC1; NULL != lpKC1; lpKC1 = lpKC1->lpBrotherKC) {
        // Find a matching key for KC1
        for (lpKC2 = lpStartKC2; NULL != lpKC2; lpKC2 = lpKC2->lpBrotherKC) {
            // skip KC2 if already matched
            if (NOMATCH != lpKC2->fKeyMatch) {
                continue;
            }
            // skip KC2 if names do *not* match (ATTENTION: test for match, THEN negate)
            if (!(
                        (lpKC1->lpszKeyName == lpKC2->lpszKeyName)
                        || ((NULL != lpKC1->lpszKeyName) && (NULL != lpKC2->lpszKeyName) && (0 == _tcscmp(lpKC1->lpszKeyName, lpKC2->lpszKeyName)))
                    )) {
                continue;
            }

            // Same key name of KC1 found in KC2! Mark KC2 as matched to skip it for the next KC1, then compare their values and sub keys!
            lpKC2->fKeyMatch = ISMATCH;

            // Compare values
            if ((NULL == lpKC1->lpFirstVC) && (NULL != lpKC2->lpFirstVC)) {
                // KC1 has *no* values but KC2, so KC2 values are added! Log all values that belong to KC2!
                LogAllRegValues(VALADD, &nVALADD, lpKC2);
            } else if ((NULL != lpKC1->lpFirstVC) && (NULL == lpKC2->lpFirstVC)) {
                // KC1 *has* values but KC2 none, so KC1 values are deleted! Log all values that belong to KC1!
                LogAllRegValues(VALDEL, &nVALDEL, lpKC1);
            } else {
                LPVALUECONTENT lpVC1;
                LPVALUECONTENT lpVC2;

                // Both keys have values, so compare these
                for (lpVC1 = lpKC1->lpFirstVC; NULL != lpVC1; lpVC1 = lpVC1->lpBrotherVC) {
                    // Find a matching value for VC1
                    for (lpVC2 = lpKC2->lpFirstVC; NULL != lpVC2; lpVC2 = lpVC2->lpBrotherVC) {
                        // skip VC2 if already matched
                        if (NOMATCH != lpVC2->fValueMatch) {
                            continue;
                        }
                        // skip VC2 if types do *not* match (even if same name then interpret as deleted+added)
                        if (lpVC1->nTypeCode != lpVC2->nTypeCode) {
                            continue;
                        }
                        // skip VC2 if names do *not* match (ATTENTION: test for match, THEN negate)
                        if (!(
                                    (lpVC1->lpszValueName == lpVC2->lpszValueName)
                                    || ((NULL != lpVC1->lpszValueName) && (NULL != lpVC2->lpszValueName) && (0 == _tcscmp(lpVC1->lpszValueName, lpVC2->lpszValueName)))
                                )) {
                            continue;
                        }
                        // Same value type and name of VC1 found in VC2, so compare their size and data
                        if ((lpVC1->cbData == lpVC2->cbData)
                                && ((lpVC1->lpValueData == lpVC2->lpValueData)
                                    || ((NULL != lpVC1->lpValueData) && (NULL != lpVC2->lpValueData) && (0 == memcmp(lpVC1->lpValueData, lpVC2->lpValueData, lpVC1->cbData)))
                                   )) {
                            // Same value data of VC1 found in VC2
                            lpVC2->fValueMatch = ISMATCH;
                        } else {
                            // Value data differ, so value is modified
                            lpVC2->fValueMatch = ISMODI;
                            LogToMem(VALMODI, &nVALMODI, lpVC1);
                            LogToMem(VALMODI, &nVALMODI, lpVC2);
                            nVALMODI--;  // TODO: better solution to avoid duplicate count
                        }
                        break;
                    }
                    if (NULL == lpVC2) {
                        // VC1 has no match in KC2, so VC1 is a deleted value
                        LogToMem(VALDEL, &nVALDEL, lpVC1);
                    }
                }
                // After looping all values of KC1, do an extra loop over all KC2 values and check previously set match flags to determine added values
                for (lpVC2 = lpKC2->lpFirstVC; NULL != lpVC2; lpVC2 = lpVC2->lpBrotherVC) {
                    if (NOMATCH == lpVC2->fValueMatch) {
                        // VC2 has no match in KC1, so VC2 is an added value
                        LogToMem(VALADD, &nVALADD, lpVC2);
                    }
                }
            }

            // Compare sub keys
            if ((NULL == lpKC1->lpFirstSubKC) && (NULL != lpKC2->lpFirstSubKC)) {
                // KC1 has *no* sub keys but KC2, so KC2 sub keys are added! Log all sub keys that belong to KC2!
                LogRegKeys(TRUE, KEYADD, VALADD, &nKEYADD, &nVALADD, lpKC2->lpFirstSubKC);
            } else if ((NULL != lpKC1->lpFirstSubKC) && (NULL == lpKC2->lpFirstSubKC)) {
                // KC1 *has* sub keys but KC2 none, so KC1 sub keys are deleted! Log all sub keys that belong to KC1!
                LogRegKeys(TRUE, KEYDEL, VALDEL, &nKEYDEL, &nVALDEL, lpKC1->lpFirstSubKC);
            } else {
                // Both keys have sub keys, so compare these
                CompareRegKeys(lpKC1->lpFirstSubKC, lpKC2->lpFirstSubKC);
            }
            break;
        }
        if (NULL == lpKC2) {
            // KC1 has no matching KC2, so KC1 is a deleted key
            LogRegKeys(FALSE, KEYDEL, VALDEL, &nKEYDEL, &nVALDEL, lpKC1);
        }
    }
    // After looping all KC1 keys, do an extra loop over all KC2 keys and check previously set match flags to determine added keys
    for (lpKC2 = lpStartKC2; NULL != lpKC2; lpKC2 = lpKC2->lpBrotherKC) {
        nComparing++;
        if (NOMATCH == lpKC2->fKeyMatch) {
            // KC2 has no matching KC1, so KC2 is an added key
            LogRegKeys(FALSE, KEYADD, VALADD, &nKEYADD, &nVALADD, lpKC2);
        }
    }

    // Progress bar update (TODO)
    if (0 != nGettingKey) {
        if ((nComparing % nGettingKey) > nRegStep) {
            nComparing = 0;
            SendDlgItemMessage(hWnd, IDC_PROGBAR, PBM_STEPIT, (WPARAM)0, (LPARAM)0);
        }
    }
}


//------------------------------------------------------------
// Routine to call registry/file comparison engine
//------------------------------------------------------------
BOOL CompareShots(LPREGSHOT lpShot1, LPREGSHOT lpShot2)
{
    BOOL    fAsHTML;
    BOOL    bshot2isnewer;
    //BOOL    bSaveWithCommentName;
    LPTSTR   lpstrcomp;
    LPTSTR   lpExt;
    LPTSTR   lpDestFileName;
    DWORD   buffersize = 2048;
    DWORD   nTotal;
    size_t  nLengthofStr;
    FILETIME ftime1;
    FILETIME ftime2;

    if (!DirChainMatch(lpShot1->lpHF, lpShot2->lpHF)) {
        MessageBox(hWnd, TEXT("Found two shots with different DIR chain! (or with different order)\r\nYou can continue, but file comparison result would be abnormal!"), TEXT("Warning"), MB_ICONWARNING);
    }

    InitProgressBar();

    SystemTimeToFileTime(&lpShot1->systemtime, &ftime1);
    SystemTimeToFileTime(&lpShot2->systemtime, &ftime2);

    bshot2isnewer = (CompareFileTime(&ftime1, &ftime2) <= 0) ? TRUE : FALSE;
    if (bshot2isnewer) {
        CompareRegKeys(lpShot1->lpHKLM, lpShot2->lpHKLM);
        CompareRegKeys(lpShot1->lpHKU, lpShot2->lpHKU);
    } else {
        CompareRegKeys(lpShot2->lpHKLM, lpShot1->lpHKLM);
        CompareRegKeys(lpShot2->lpHKU, lpShot1->lpHKU);
    }

    SendDlgItemMessage(hWnd, IDC_PROGBAR, PBM_SETPOS, (WPARAM)0, (LPARAM)0);

    // Dir comparison v1.8.1
    // determine newer
    if (bshot2isnewer) {
        CompareHeadFiles(lpShot1->lpHF, lpShot2->lpHF);
    } else {
        CompareHeadFiles(lpShot2->lpHF, lpShot1->lpHF);
    }

    SendDlgItemMessage(hWnd, IDC_PROGBAR, PBM_SETPOS, (WPARAM)MAXPBPOSITION, (LPARAM)0);

    if (SendMessage(GetDlgItem(hWnd, IDC_RADIO1), BM_GETCHECK, (WPARAM)0, (LPARAM)0) == 1) {
        fAsHTML = FALSE;
        lpExt = TEXT(".txt");
    } else {
        fAsHTML = TRUE;
        lpExt = TEXT(".htm");
    }

    lpDestFileName = MYALLOC0(EXTDIRLEN * sizeof(TCHAR));
    lpstrcomp = MYALLOC0(buffersize * sizeof(TCHAR)); // buffersize must > commentlength + 10 .txt 0000
    GetDlgItemText(hWnd, IDC_EDITCOMMENT, lpstrcomp, COMMENTLENGTH);  // length incl. NULL character
    GetDlgItemText(hWnd, IDC_EDITPATH, lpszOutputPath, MAX_PATH);  // length incl. NULL character

    nLengthofStr = _tcslen(lpszOutputPath);

    if (nLengthofStr > 0 && *(lpszOutputPath + nLengthofStr - 1) != '\\') {
        *(lpszOutputPath + nLengthofStr) = '\\';
        *(lpszOutputPath + nLengthofStr + 1) = 0x00;    // bug found by "itschy" <itschy@lycos.de> 1.61d->1.61e
        nLengthofStr++;
    }
    _tcscpy(lpDestFileName, lpszOutputPath);

    //bSaveWithCommentName = TRUE;
    if (ReplaceInvalidFileNameChars(lpstrcomp)) {
        _tcscat(lpDestFileName, lpstrcomp);
    } else {
        _tcscat(lpDestFileName, lpszDefResPre);
    }

    nLengthofStr = _tcslen(lpDestFileName);
    _tcscat(lpDestFileName, lpExt);
    hFile = CreateFile(lpDestFileName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        DWORD filetail = 0;

        for (filetail = 0; filetail < MAXAMOUNTOFFILE; filetail++) {
            _sntprintf(lpDestFileName + nLengthofStr, 6, TEXT("_%04u\0"), filetail);
            //*(lpDestFileName+nLengthofStr + 5) = 0x00;
            _tcscpy(lpDestFileName + nLengthofStr + 5, lpExt);

            hFile = CreateFile(lpDestFileName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hFile == INVALID_HANDLE_VALUE) {
                if (GetLastError() == ERROR_FILE_EXISTS) {    // My God! I use stupid ERROR_ALREADY_EXISTS first!!
                    continue;
                } else {
                    ErrMsg(asLangTexts[iszTextErrorCreateFile].lpszText);
                    return FALSE;
                }
            } else {
                break;
            }
        }
        if (filetail >= MAXAMOUNTOFFILE) {
            ErrMsg(asLangTexts[iszTextErrorCreateFile].lpszText);
            return FALSE;
        }

    }

    if (fAsHTML == TRUE) {
        WriteHTMLBegin();
    } else {
        WriteFile(hFile, lpszProgramName, (DWORD)(_tcslen(lpszProgramName) * sizeof(TCHAR)), &NBW, NULL);
        WriteFile(hFile, lpszCRLF, (DWORD)(_tcslen(lpszCRLF) * sizeof(TCHAR)), &NBW, NULL);
    }

    //_asm int 3;
    GetDlgItemText(hWnd, IDC_EDITCOMMENT, lpstrcomp, COMMENTLENGTH);  // length incl. NULL character
    WriteTitle(asLangTexts[iszTextComments].lpszText, lpstrcomp, fAsHTML);

    _sntprintf(lpstrcomp, buffersize, TEXT("%d%s%d%s%d %02d%s%02d%s%02d %s %d%s%d%s%d %02d%s%02d%s%02d\0"),
               lpShot1->systemtime.wYear, TEXT("/"),
               lpShot1->systemtime.wMonth, TEXT("/"),
               lpShot1->systemtime.wDay,
               lpShot1->systemtime.wHour, TEXT(":"),
               lpShot1->systemtime.wMinute, TEXT(":"),
               lpShot1->systemtime.wSecond, TEXT(" , "),
               lpShot2->systemtime.wYear, TEXT("/"),
               lpShot2->systemtime.wMonth, TEXT("/"),
               lpShot2->systemtime.wDay,
               lpShot2->systemtime.wHour, TEXT(":"),
               lpShot2->systemtime.wMinute, TEXT(":"),
               lpShot2->systemtime.wSecond
              );
    lpstrcomp[buffersize - 1] = (TCHAR)'\0'; // saftey NULL char

    WriteTitle(asLangTexts[iszTextDateTime].lpszText, lpstrcomp, fAsHTML);

    lpstrcomp[0] = (TCHAR)'\0';    //ZeroMemory(lpstrcomp,buffersize);
    if (NULL != lpShot1->lpszComputerName) {
        _tcscpy(lpstrcomp, lpShot1->lpszComputerName);
    }
    _tcscat(lpstrcomp, TEXT(" , "));
    if (NULL != lpShot2->lpszComputerName) {
        _tcscat(lpstrcomp, lpShot2->lpszComputerName);
    }
    WriteTitle(asLangTexts[iszTextComputer].lpszText, lpstrcomp, fAsHTML);

    lpstrcomp[0] = (TCHAR)'\0';    //ZeroMemory(lpstrcomp,buffersize);
    if (NULL != lpShot1->lpszUserName) {
        _tcscpy(lpstrcomp, lpShot1->lpszUserName);
    }
    _tcscat(lpstrcomp, TEXT(" , "));
    if (NULL != lpShot2->lpszUserName) {
        _tcscat(lpstrcomp, lpShot2->lpszUserName);
    }
    WriteTitle(asLangTexts[iszTextUsername].lpszText, lpstrcomp, fAsHTML);

    MYFREE(lpstrcomp);

    // Write keydel part
    if (nKEYDEL != 0) {
        WriteTableHead(asLangTexts[iszTextKeyDel].lpszText, nKEYDEL, fAsHTML);
        WritePart(lpKEYDELHEAD, fAsHTML, FALSE);
    }
    // Write keyadd part
    if (nKEYADD != 0) {
        WriteTableHead(asLangTexts[iszTextKeyAdd].lpszText, nKEYADD, fAsHTML);
        WritePart(lpKEYADDHEAD, fAsHTML, FALSE);
    }
    // Write valdel part
    if (nVALDEL != 0) {
        WriteTableHead(asLangTexts[iszTextValDel].lpszText, nVALDEL, fAsHTML);
        WritePart(lpVALDELHEAD, fAsHTML, FALSE);
    }
    // Write valadd part
    if (nVALADD != 0) {
        WriteTableHead(asLangTexts[iszTextValAdd].lpszText, nVALADD, fAsHTML);
        WritePart(lpVALADDHEAD, fAsHTML, FALSE);
    }
    // Write valmodi part
    if (nVALMODI != 0) {
        WriteTableHead(asLangTexts[iszTextValModi].lpszText, nVALMODI, fAsHTML);
        WritePart(lpVALMODIHEAD, fAsHTML, TRUE);
    }
    // Write file add part
    if (nFILEADD != 0) {
        WriteTableHead(asLangTexts[iszTextFileAdd].lpszText, nFILEADD, fAsHTML);
        WritePart(lpFILEADDHEAD, fAsHTML, FALSE);
    }
    // Write file del part
    if (nFILEDEL != 0) {
        WriteTableHead(asLangTexts[iszTextFileDel].lpszText, nFILEDEL, fAsHTML);
        WritePart(lpFILEDELHEAD, fAsHTML, FALSE);
    }
    // Write file modi part
    if (nFILEMODI != 0) {
        WriteTableHead(asLangTexts[iszTextFileModi].lpszText, nFILEMODI, fAsHTML);
        WritePart(lpFILEMODIHEAD, fAsHTML, FALSE);
    }
    // Write directory add part
    if (nDIRADD != 0) {
        WriteTableHead(asLangTexts[iszTextDirAdd].lpszText, nDIRADD, fAsHTML);
        WritePart(lpDIRADDHEAD, fAsHTML, FALSE);
    }
    // Write directory del part
    if (nDIRDEL != 0) {
        WriteTableHead(asLangTexts[iszTextDirDel].lpszText, nDIRDEL, fAsHTML);
        WritePart(lpDIRDELHEAD, fAsHTML, FALSE);
    }
    // Write directory modi part
    if (nDIRMODI != 0) {
        WriteTableHead(asLangTexts[iszTextDirModi].lpszText, nDIRMODI, fAsHTML);
        WritePart(lpDIRMODIHEAD, fAsHTML, FALSE);
    }

    nTotal = nKEYADD + nKEYDEL + nVALADD + nVALDEL + nVALMODI + nFILEADD + nFILEDEL  + nFILEMODI + nDIRADD + nDIRDEL + nDIRMODI;
    if (fAsHTML == TRUE) {
        WriteHTML_BR();
    }
    WriteTableHead(asLangTexts[iszTextTotal].lpszText, nTotal, fAsHTML);
    if (fAsHTML == TRUE) {
        WriteHTMLEnd();
    }

    CloseHandle(hFile);

    if ((size_t)ShellExecute(hWnd, TEXT("open"), lpDestFileName, NULL, NULL, SW_SHOW) <= 32) {
        ErrMsg(asLangTexts[iszTextErrorExecViewer].lpszText);
    }
    MYFREE(lpDestFileName);

    return TRUE;
}


// ----------------------------------------------------------------------
// Clear comparison match flags of registry keys and values
// ----------------------------------------------------------------------
VOID ClearRegKeyMatchFlags(LPKEYCONTENT lpStartKC)
{
    LPKEYCONTENT lpKC;
    LPVALUECONTENT lpVC;

    for (lpKC = lpStartKC; NULL != lpKC; lpKC = lpKC->lpBrotherKC) {
        lpKC->fKeyMatch = NOMATCH;
        for (lpVC = lpKC->lpFirstVC; NULL != lpVC; lpVC = lpVC->lpBrotherVC) {
            lpVC->fValueMatch = NOMATCH;
        }
        ClearRegKeyMatchFlags(lpKC->lpFirstSubKC);
    }
}


// ----------------------------------------------------------------------
// Free all registry keys and values
// ----------------------------------------------------------------------
VOID FreeAllValueContent(LPVALUECONTENT lpVC)
{
    if (NULL != lpVC) {
        if (NULL != lpVC->lpszValueName) {
            MYFREE(lpVC->lpszValueName);
        }
        if (NULL != lpVC->lpValueData) {
            MYFREE(lpVC->lpValueData);
        }
        FreeAllValueContent(lpVC->lpBrotherVC);
        MYFREE(lpVC);
    }
}

// ----------------------------------------------------------------------
// Free all registry keys and values
// ----------------------------------------------------------------------
VOID FreeAllKeyContent(LPKEYCONTENT lpKC)
{
    if (NULL != lpKC) {
        if (NULL != lpKC->lpszKeyName) {
            if ((lpKC->lpszKeyName != lpszHKLMShort)
                    && (lpKC->lpszKeyName != lpszHKLMLong)
                    && (lpKC->lpszKeyName != lpszHKUShort)
                    && (lpKC->lpszKeyName != lpszHKULong)) {
                MYFREE(lpKC->lpszKeyName);
            }
        }
        FreeAllValueContent(lpKC->lpFirstVC);
        FreeAllKeyContent(lpKC->lpFirstSubKC);
        FreeAllKeyContent(lpKC->lpBrotherKC);
        MYFREE(lpKC);
    }
}

// ----------------------------------------------------------------------
// Free shot completely and initialize
// ----------------------------------------------------------------------
VOID FreeShot(LPREGSHOT lpShot)
{
    if (NULL != lpShot->lpszComputerName) {
        MYFREE(lpShot->lpszComputerName);
    }

    if (NULL != lpShot->lpszUserName) {
        MYFREE(lpShot->lpszUserName);
    }

    FreeAllKeyContent(lpShot->lpHKLM);
    FreeAllKeyContent(lpShot->lpHKU);
    FreeAllFileHead(lpShot->lpHF);

    ZeroMemory(lpShot, sizeof(REGSHOT));
}


// ----------------------------------------------------------------------
// Get registry snap shot
// ----------------------------------------------------------------------
LPKEYCONTENT GetRegistrySnap(HKEY hRegKey, LPTSTR lpszRegKeyName, LPKEYCONTENT lpFatherKC, LPKEYCONTENT *lplpCaller)
{
    LPKEYCONTENT lpKC;
    DWORD cSubKeys;
    DWORD cchMaxSubKeyName;

    // Process registry key itself, then key values with data, then sub keys (see msdn.microsoft.com/en-us/library/windows/desktop/ms724256.aspx)

    // Extra local block to reduce stack usage due to recursive calls
    {
        LPTSTR lpszFullName;
        DWORD cValues;
        DWORD cchMaxValueName;
        DWORD cbMaxValueData;

        // Create new key content
        // put in a separate var for later use
        lpKC = MYALLOC0(sizeof(KEYCONTENT));
        ZeroMemory(lpKC, sizeof(KEYCONTENT));

        // Set father of current key
        lpKC->lpFatherKC = lpFatherKC;

        // Set key name
        lpKC->lpszKeyName = lpszRegKeyName;
        lpKC->cchKeyName = _tcslen(lpKC->lpszKeyName);

        // Check if key is to be excluded
        if (NULL != lprgszRegSkipStrings[0]) {  // only if there is something to exclude
            lpszFullName = GetWholeKeyName(lpKC, FALSE);
            if (IsInSkipList(lpszFullName, lprgszRegSkipStrings)) {
                MYFREE(lpszFullName);
                FreeAllKeyContent(lpKC);
                return NULL;
            }
            MYFREE(lpszFullName);
        }

        // Examine key for values and sub keys, get counts and also maximum lengths of names plus value data
        nErrNo = RegQueryInfoKey(
                     hRegKey,
                     NULL,
                     NULL,
                     NULL,
                     &cSubKeys,
                     &cchMaxSubKeyName,  // in TCHARs *not* incl. NULL char
                     NULL,
                     &cValues,
                     &cchMaxValueName,   // in TCHARs *not* incl. NULL char
                     &cbMaxValueData,
                     NULL,
                     NULL
                 );
        if (ERROR_SUCCESS != nErrNo) {
            // TODO: process/protocol issue in some way, do not silently ignore it
            FreeAllKeyContent(lpKC);
            return NULL;
        }

        // Increase key count
        nGettingKey++;

        // Write pointer to current key into caller's pointer
        if (NULL != lplpCaller) {
            *lplpCaller = lpKC;
        }

        // Copy the registry values of the current key
        if (0 < cValues) {
            LPVALUECONTENT lpVC;
            LPVALUECONTENT *lplpVCPrev;
            DWORD i;
            DWORD cchValueName;
            DWORD nValueType;
            DWORD cbValueData;
#ifdef DEBUGLOG
            LPTSTR lpszDebugMsg;
#endif

            // Account for NULL char
            if (0 < cchMaxValueName) {
                cchMaxValueName++;
            }

            // Get buffer for maximum value name length
            nSourceSize = cchMaxValueName * sizeof(TCHAR);
            nStringBufferSize = AdjustBuffer(&lpStringBuffer, nStringBufferSize, nSourceSize, REGSHOT_BUFFER_BLOCK_BYTES);

            // Get buffer for maximum value data length
            nSourceSize = cbMaxValueData;
            nDataBufferSize = AdjustBuffer(&lpDataBuffer, nDataBufferSize, nSourceSize, REGSHOT_BUFFER_BLOCK_BYTES);

            // Get registry key values
            lplpVCPrev = &lpKC->lpFirstVC;
            for (i = 0; ; i++) {
                // Enumerate value
                cchValueName = (DWORD)nStringBufferSize;
                cbValueData = (DWORD)nDataBufferSize;
                nErrNo = RegEnumValue(hRegKey, i,
                                      lpStringBuffer,
                                      &cchValueName,   // in TCHARs; in *including* and out *excluding* NULL char
                                      NULL,
                                      &nValueType,
                                      lpDataBuffer,
                                      &cbValueData);
                if (ERROR_NO_MORE_ITEMS == nErrNo) {
                    break;
                }
                if (ERROR_SUCCESS != nErrNo) {
                    // TODO: process/protocol issue in some way, do not silently ignore it
                    continue;
                }
                lpStringBuffer[cchValueName] = (TCHAR)'\0';  // safety NULL char

#ifdef DEBUGLOG
                DebugLog(lpszDebugTryToGetValueLog, TEXT("trying: "), FALSE);
                DebugLog(lpszDebugTryToGetValueLog, lpStringBuffer, TRUE);
#endif

                // Create new value content
                // put in a separate var for later use
                lpVC = MYALLOC0(sizeof(VALUECONTENT));
                ZeroMemory(lpVC, sizeof(VALUECONTENT));

                // Set father key of current value to current key
                lpVC->lpFatherKC = lpKC;

                // Copy value name
                if (0 < cchValueName) {
                    lpVC->lpszValueName = MYALLOC((cchValueName + 1) * sizeof(TCHAR));
                    _tcscpy(lpVC->lpszValueName, lpStringBuffer);
                    lpVC->cchValueName = _tcslen(lpVC->lpszValueName);
                }

                // Check if value is to be excluded
                if (NULL != lprgszRegSkipStrings[0]) {  // only if there is something to exclude
                    lpszFullName = GetWholeValueName(lpVC, FALSE);
                    if (IsInSkipList(lpszFullName, lprgszRegSkipStrings)) {
                        MYFREE(lpszFullName);
                        FreeAllValueContent(lpVC);
                        continue;
                    }
                    MYFREE(lpszFullName);
                }

                // Increase value count
                nGettingValue++;

                // Write pointer to current value into previous value's next value pointer
                if (NULL != lplpVCPrev) {
                    *lplpVCPrev = lpVC;
                }
                lplpVCPrev = &lpVC->lpBrotherVC;

                // Copy value meta data
                lpVC->nTypeCode = nValueType;
                lpVC->cbData = cbValueData;

                // Copy value data
                if (0 < cbValueData) {  // otherwise leave it NULL
                    lpVC->lpValueData = MYALLOC0(cbValueData);
                    CopyMemory(lpVC->lpValueData, lpDataBuffer, cbValueData);
                }

#ifdef DEBUGLOG
                lpszDebugMsg = MYALLOC0(REGSHOT_DEBUG_MESSAGE_LENGTH * sizeof(TCHAR));
                _sntprintf(lpszDebugMsg, REGSHOT_DEBUG_MESSAGE_LENGTH, TEXT("LGVN:%08d LGVD:%08d VN:%08d VD:%08d\0"), cchMaxValueName, cbMaxValueData, cchValueName, cbValueData);
                lpszDebugMsg[REGSHOT_DEBUG_MESSAGE_LENGTH - 1] = (TCHAR)'\0';  // safety NULL char
                DebugLog(lpszDebugValueNameDataLog, lpszDebugMsg, TRUE);
                MYFREE(lpszDebugMsg);

                lpszDebugMsg = GetWholeValueName(lpVC, FALSE);
                DebugLog(lpszDebugValueNameDataLog, lpszDebugMsg, FALSE);
                MYFREE(lpszDebugMsg);

                lpszDebugMsg = GetWholeValueData(lpVC);
                DebugLog(lpszDebugValueNameDataLog, lpszDebugMsg, TRUE);
                MYFREE(lpszDebugMsg);
#endif
            }
        }
    }  // End of extra local block

    // Update counters display
    nGettingTime = GetTickCount();
    if (REFRESHINTERVAL < (nGettingTime - nBASETIME1)) {
        UpdateCounters(asLangTexts[iszTextKey].lpszText, asLangTexts[iszTextValue].lpszText, nGettingKey, nGettingValue);
    }

    // Process sub keys
    if (0 < cSubKeys) {
        LPKEYCONTENT lpKCSub;
        LPKEYCONTENT *lplpKCPrev;
        DWORD i;
        LPTSTR lpszRegSubKeyName;
        HKEY hRegSubKey;

        // Account for NULL char
        if (0 < cchMaxSubKeyName) {
            cchMaxSubKeyName++;
        }

        // Get buffer for maximum sub key name length
        nSourceSize = cchMaxSubKeyName * sizeof(TCHAR);
        nStringBufferSize = AdjustBuffer(&lpStringBuffer, nStringBufferSize, nSourceSize, REGSHOT_BUFFER_BLOCK_BYTES);

        // Get registry sub keys
        lplpKCPrev = &lpKC->lpFirstSubKC;
        for (i = 0; ; i++) {
            // Extra local block to reduce stack usage due to recursive calls
            {
                DWORD cchSubKeyName;
#ifdef DEBUGLOG
                LPTSTR lpszDebugMsg;
#endif

                // Enumerate sub key
                cchSubKeyName = (DWORD)nStringBufferSize;
                nErrNo = RegEnumKeyEx(hRegKey, i,
                                      lpStringBuffer,
                                      &cchSubKeyName,  // in TCHARs; in *including* and out *excluding* NULL char
                                      NULL,
                                      NULL,
                                      NULL,
                                      NULL);
                if (ERROR_NO_MORE_ITEMS == nErrNo) {
                    break;
                }
                if (ERROR_SUCCESS != nErrNo) {
                    // TODO: process/protocol issue in some way, do not silently ignore it
                    continue;
                }
                lpStringBuffer[cchSubKeyName] = (TCHAR)'\0';  // safety NULL char

                // Copy sub key name
                lpszRegSubKeyName = NULL;
                if (0 < cchSubKeyName) {
                    lpszRegSubKeyName = MYALLOC((cchSubKeyName + 1) * sizeof(TCHAR));
                    _tcscpy(lpszRegSubKeyName, lpStringBuffer);
                }

#ifdef DEBUGLOG
                lpszDebugMsg = MYALLOC0(REGSHOT_DEBUG_MESSAGE_LENGTH * sizeof(TCHAR));
                _sntprintf(lpszDebugMsg, REGSHOT_DEBUG_MESSAGE_LENGTH, TEXT("LGKN:%08d KN:%08d\0"), cchMaxSubKeyName, cchSubKeyName);
                lpszDebugMsg[REGSHOT_DEBUG_MESSAGE_LENGTH - 1] = (TCHAR)'\0';  // safety NULL char
                DebugLog(lpszDebugKeyLog, lpszDebugMsg, TRUE);
                MYFREE(lpszDebugMsg);

                lpszDebugMsg = GetWholeKeyName(lpKC, FALSE);
                DebugLog(lpszDebugKeyLog, lpszDebugMsg, FALSE);
                MYFREE(lpszDebugMsg);

                DebugLog(lpszDebugKeyLog, TEXT("\\"), FALSE);
                DebugLog(lpszDebugKeyLog, lpszRegSubKeyName, TRUE);
#endif
            }  // End of extra local block

            nErrNo = RegOpenKeyEx(hRegKey, lpszRegSubKeyName, 0, KEY_QUERY_VALUE | KEY_ENUMERATE_SUB_KEYS, &hRegSubKey);
            if (ERROR_SUCCESS != nErrNo) {
                // TODO: process/protocol issue in some way, do not silently ignore it
                //       e.g. when ERROR_ACCESS_DENIED then at least add key itself to the list
                MYFREE(lpszRegSubKeyName);
                continue;
            }

            lpKCSub = GetRegistrySnap(hRegSubKey, lpszRegSubKeyName, lpKC, lplpKCPrev);
            RegCloseKey(hRegSubKey);

            if (NULL != lpKCSub) {
                lplpKCPrev = &lpKCSub->lpBrotherKC;
            }
        }
    }

    return lpKC;
}


// ----------------------------------------------------------------------
// Shot Engine
// ----------------------------------------------------------------------
VOID Shot(LPREGSHOT lpShot)
{
    DWORD cchString;

    FreeShot(lpShot);

    lpStringBuffer = NULL;
    lpDataBuffer = NULL;

    // Set computer name
    lpShot->lpszComputerName = MYALLOC0((MAX_COMPUTERNAME_LENGTH + 1) * sizeof(TCHAR));
    ZeroMemory(lpShot->lpszComputerName, (MAX_COMPUTERNAME_LENGTH + 1) * sizeof(TCHAR));
    cchString = MAX_COMPUTERNAME_LENGTH + 1;
    GetComputerName(lpShot->lpszComputerName, &cchString);   // in TCHARs; in *including* and out *excluding* NULL char

    // Set user name
    lpShot->lpszUserName = MYALLOC0((UNLEN + 1) * sizeof(TCHAR));
    ZeroMemory(lpShot->lpszUserName, (UNLEN + 1) * sizeof(TCHAR));
    cchString = UNLEN + 1;
    GetUserName(lpShot->lpszUserName, &cchString);   // in TCHARs; in and out including NULL char

    // Set current system time
    GetSystemTime(&lpShot->systemtime);

    // Initialize counters
    nGettingKey   = 0;
    nGettingValue = 0;
    nGettingTime  = 0;
    nGettingFile  = 0;
    nGettingDir   = 0;
    nBASETIME  = GetTickCount();
    nBASETIME1 = nBASETIME;
    if (&Shot1 == lpShot) {
        UI_BeforeShot(IDC_1STSHOT);
    } else {
        UI_BeforeShot(IDC_2NDSHOT);
    }

    GetRegistrySnap(HKEY_LOCAL_MACHINE, lpszHKLMShort, NULL, &lpShot->lpHKLM);
    GetRegistrySnap(HKEY_USERS, lpszHKUShort, NULL, &lpShot->lpHKU);

    // Update counters display (reg keys/values final)
    nGettingTime = GetTickCount();
    UpdateCounters(asLangTexts[iszTextKey].lpszText, asLangTexts[iszTextValue].lpszText, nGettingKey, nGettingValue);

    if (1 == SendMessage(GetDlgItem(hWnd, IDC_CHECKDIR), BM_GETCHECK, (WPARAM)0, (LPARAM)0)) {
        FileShot(lpShot);
    }

    // Set flag
    lpShot->fFilled = TRUE;

    UI_AfterShot();

    if (NULL != lpStringBuffer) {
        MYFREE(lpStringBuffer);
        lpStringBuffer = NULL;
    }
    if (NULL != lpDataBuffer) {
        MYFREE(lpDataBuffer);
        lpDataBuffer = NULL;
    }
}


// ----------------------------------------------------------------------
// Save registry key with values to HIVE file
//
// This routine is called recursively to store the keys of the Registry tree
// Therefore temporary vars are put in a local block to reduce stack usage
// ----------------------------------------------------------------------
VOID SaveRegKeys(LPKEYCONTENT lpStartKC, DWORD nFPFatherKey, DWORD nFPCaller)
{
    LPKEYCONTENT lpKC;
    DWORD nFPKey;

    for (lpKC = lpStartKC; NULL != lpKC; lpKC = lpKC->lpBrotherKC) {
        // Get current file position
        // put in a separate var for later use
        nFPKey = SetFilePointer(hFileWholeReg, 0, NULL, FILE_CURRENT);

        // Write position of current reg key in caller's field
        if (0 < nFPCaller) {
            SetFilePointer(hFileWholeReg, nFPCaller, NULL, FILE_BEGIN);
            WriteFile(hFileWholeReg, &nFPKey, sizeof(nFPKey), &NBW, NULL);

            SetFilePointer(hFileWholeReg, nFPKey, NULL, FILE_BEGIN);
        }

        // Initialize key content

        // Set file positions of the relatives inside the tree
#ifdef _UNICODE
        sKC.ofsKeyName = 0;      // not known yet, may be defined in this call
#endif
#ifndef _UNICODE
        // Key name will always be stored behind the structure, so its position is already known
        sKC.ofsKeyName = nFPKey + sizeof(sKC);
#endif
        sKC.ofsFirstValue = 0;   // not known yet, may be re-written in this iteration
        sKC.ofsFirstSubKey = 0;  // not known yet, may be re-written by another recursive call
        sKC.ofsBrotherKey = 0;   // not known yet, may be re-written in another iteration
        sKC.ofsFatherKey = nFPFatherKey;

        // New since key content version 2
        sKC.nKeyNameLen = 0;

        // Extra local block to reduce stack usage due to recursive calls
        {
            LPTSTR lpszKeyName;

            lpszKeyName = lpKC->lpszKeyName;

            // Determine correct key name and length plus file offset
            if (NULL != lpszKeyName) {
                sKC.nKeyNameLen = (DWORD)lpKC->cchKeyName;

                if ((0 == nFPFatherKey) && (bUseLongRegHead)) {
                    // Adopt to long HKLM/HKU
                    if (lpszHKLMShort == lpszKeyName) {
                        lpszKeyName = lpszHKLMLong;
                        sKC.nKeyNameLen = (DWORD)_tcslen(lpszKeyName);;
                    } else if (lpszHKUShort == lpszKeyName) {
                        lpszKeyName = lpszHKULong;
                        sKC.nKeyNameLen = (DWORD)_tcslen(lpszKeyName);;
                    }
                }

                // Determine key name length and file offset
                if (0 < sKC.nKeyNameLen) {  // otherwise leave it all 0
                    sKC.nKeyNameLen++;  // account for NULL char
#ifdef _UNICODE
                    // Key name will always be stored behind the structure, so its position is already known
                    sKC.ofsKeyName = nFPKey + sizeof(sKC);
#endif
                }
            }

            // Write key content to file
            // Make sure that ALL fields have been initialized/set
            WriteFile(hFileWholeReg, &sKC, sizeof(sKC), &NBW, NULL);

            // Write key name to file
            if (0 < sKC.nKeyNameLen) {
                WriteFile(hFileWholeReg, lpszKeyName, sKC.nKeyNameLen * sizeof(TCHAR), &NBW, NULL);
#ifndef _UNICODE
            } else {
                // Write empty string for backward compatibility
                WriteFile(hFileWholeReg, lpszEmpty, 1 * sizeof(TCHAR), &NBW, NULL);
#endif
            }
        }  // End of extra local block

        // Save the values of current key
        if (NULL != lpKC->lpFirstVC) {
            LPVALUECONTENT lpVC;
            DWORD nFPValueCaller;
            DWORD nFPValue;

            // Write all values of key
            nFPValueCaller = nFPKey + offsetof(SAVEKEYCONTENT, ofsFirstValue);  // Write position of first value into key
            for (lpVC = lpKC->lpFirstVC; NULL != lpVC; lpVC = lpVC->lpBrotherVC) {
                nFPValue = SetFilePointer(hFileWholeReg, 0, NULL, FILE_CURRENT);

                // Write position of previous value content in value content field ofsBrotherValue
                if (0 < nFPValueCaller) {
                    SetFilePointer(hFileWholeReg, nFPValueCaller, NULL, FILE_BEGIN);
                    WriteFile(hFileWholeReg, &nFPValue, sizeof(nFPValue), &NBW, NULL);

                    SetFilePointer(hFileWholeReg, nFPValue, NULL, FILE_BEGIN);
                }
                nFPValueCaller = nFPValue + offsetof(SAVEVALUECONTENT, ofsBrotherValue);

                // Initialize value content

                // Copy values
                sVC.nTypeCode = lpVC->nTypeCode;
                sVC.cbData = lpVC->cbData;

                // Set file positions of the relatives inside the tree
#ifdef _UNICODE
                sVC.ofsValueName = 0;       // not known yet, may be defined in this iteration
#endif
#ifndef _UNICODE
                // Value name will always be stored behind the structure, so its position is already known
                sVC.ofsValueName = nFPValue + sizeof(sVC);
#endif
                sVC.ofsValueData = 0;       // not known yet, may be re-written in this iteration
                sVC.ofsBrotherValue = 0;    // not known yet, may be re-written in next iteration
                sVC.ofsFatherKey = nFPKey;

                // New since value content version 2
                sVC.nValueNameLen = 0;

                // Determine value name length
                if (NULL != lpVC->lpszValueName) {
                    sVC.nValueNameLen = (DWORD)lpVC->cchValueName;
                    if (0 < sVC.nValueNameLen) {  // otherwise leave it all 0
                        sVC.nValueNameLen++;  // account for NULL char
#ifdef _UNICODE
                        // Value name will always be stored behind the structure, so its position is already known
                        sVC.ofsValueName = nFPValue + sizeof(sVC);
#endif
                    }
                }

                // Write value content to file
                // Make sure that ALL fields have been initialized/set
                WriteFile(hFileWholeReg, &sVC, sizeof(sVC), &NBW, NULL);

                // Write value name to file
                if (0 < sVC.nValueNameLen) {
                    WriteFile(hFileWholeReg, lpVC->lpszValueName, sVC.nValueNameLen * sizeof(TCHAR), &NBW, NULL);
#ifndef _UNICODE
                } else {
                    // Write empty string for backward compatibility
                    WriteFile(hFileWholeReg, lpszEmpty, 1 * sizeof(TCHAR), &NBW, NULL);
#endif
                }

                // Write value data to file
                if (0 < sVC.cbData) {
                    DWORD nFPValueData;

                    // Write position of value data in value content field ofsValueData
                    nFPValueData = SetFilePointer(hFileWholeReg, 0, NULL, FILE_CURRENT);

                    SetFilePointer(hFileWholeReg, nFPValue + offsetof(SAVEVALUECONTENT, ofsValueData), NULL, FILE_BEGIN);
                    WriteFile(hFileWholeReg, &nFPValueData, sizeof(nFPValueData), &NBW, NULL);

                    SetFilePointer(hFileWholeReg, nFPValueData, NULL, FILE_BEGIN);

                    // Write value data
                    WriteFile(hFileWholeReg, lpVC->lpValueData, sVC.cbData, &NBW, NULL);
                }
            }
        }

        // ATTENTION!!! sKC is INVALID from this point on, due to recursive calls

        // If the entry has childs, then do a recursive call for the first child
        // Pass this entry as father and "ofsFirstSubKey" position for storing the first child's position
        if (NULL != lpKC->lpFirstSubKC) {
            SaveRegKeys(lpKC->lpFirstSubKC, nFPKey, nFPKey + offsetof(SAVEKEYCONTENT, ofsFirstSubKey));
        }

        // Set "ofsBrotherKey" position for storing the following brother's position
        nFPCaller = nFPKey + offsetof(SAVEKEYCONTENT, ofsBrotherKey);

        // TODO: Need to adjust progress bar para!!
        nSavingKey++;
        if (0 != nGettingKey) {
            if (nSavingKey % nGettingKey > nRegStep) {
                nSavingKey = 0;
                SendDlgItemMessage(hWnd, IDC_PROGBAR, PBM_STEPIT, (WPARAM)0, (LPARAM)0);
                UpdateWindow(hWnd);
                PeekMessage(&msg, hWnd, WM_ACTIVATE, WM_ACTIVATE, PM_REMOVE);
            }
        }
    }
}

// ----------------------------------------------------------------------
// Save registry and files to HIVE file
// ----------------------------------------------------------------------
VOID SaveHive(LPREGSHOT lpShot)
{
    OPENFILENAME opfn;
    TCHAR filepath[MAX_PATH];
    DWORD nFPCurrent;

    // Check if there's anything to save
    if ((NULL == lpShot->lpHKLM) && (NULL == lpShot->lpHKU) && (NULL == lpShot->lpHF)) {
        return;  // leave silently
    }

    // Clear Save File Name result buffer
    ZeroMemory(filepath, sizeof(filepath));

    // Prepare Save File Name dialog
    ZeroMemory(&opfn, sizeof(opfn));
    opfn.lStructSize = sizeof(opfn);
    opfn.hwndOwner = hWnd;
    opfn.lpstrFilter = lpszFilter;
    opfn.lpstrFile = filepath;
    opfn.nMaxFile = MAX_PATH;  // incl. NULL character
    opfn.lpstrInitialDir = lpszLastSaveDir;
    opfn.Flags = OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY;
    opfn.lpstrDefExt = lpszRegshotFileDefExt;

    // Display Save File Name dialog
    if (!GetSaveFileName(&opfn)) {
        return;  // leave silently
    }

    // Open file for writing
    hFileWholeReg = CreateFile(opfn.lpstrFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (INVALID_HANDLE_VALUE == hFileWholeReg) {
        ErrMsg(asLangTexts[iszTextErrorCreateFile].lpszText);
        return;
    }

    // Setup GUI for saving...
    UI_BeforeClear();
    InitProgressBar();

    // Initialize file header
    ZeroMemory(&fileheader, sizeof(fileheader));

    // Copy SBCS/MBCS signature to header (even in Unicode builds for backwards compatibility)
    strncpy(fileheader.signature, szRegshotFileSignature, MAX_SIGNATURE_LENGTH);

    // Set file positions of hives inside the file
    fileheader.ofsHKLM = 0;   // not known yet, may be empty
    fileheader.ofsHKU = 0;    // not known yet, may be empty
    fileheader.ofsHF = 0;  // not known yet, may be empty

    // Copy SBCS/MBCS strings to header (even in Unicode builds for backwards compatibility)
    if (NULL != lpShot->lpszComputerName) {
#ifdef _UNICODE
        WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK | WC_DEFAULTCHAR, lpShot->lpszComputerName, -1, fileheader.computername, OLD_COMPUTERNAMELEN, NULL, NULL);
#else
        strncpy(fileheader.computername, lpShot->lpszComputerName, OLD_COMPUTERNAMELEN);
#endif
    }
    if (NULL != lpShot->lpszUserName) {
#ifdef _UNICODE
        WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK | WC_DEFAULTCHAR, lpShot->lpszUserName, -1, fileheader.username, OLD_COMPUTERNAMELEN, NULL, NULL);
#else
        strncpy(fileheader.username, lpShot->lpszUserName, OLD_COMPUTERNAMELEN);
#endif
    }

    // Copy system time to header
    CopyMemory(&fileheader.systemtime, &lpShot->systemtime, sizeof(SYSTEMTIME));

    // new since header version 2
    fileheader.nFHSize = sizeof(fileheader);

    fileheader.nFHVersion = FILEHEADER_VERSION_CURRENT;
    fileheader.nCharSize = sizeof(TCHAR);

    fileheader.ofsComputerName = 0;  // not known yet, may be empty
    fileheader.nComputerNameLen = 0;
    if (NULL != lpShot->lpszComputerName) {
        fileheader.nComputerNameLen = (DWORD)_tcslen(lpShot->lpszComputerName);
        if (0 < fileheader.nComputerNameLen) {
            fileheader.nComputerNameLen++;  // account for NULL char
        }
    }

    fileheader.ofsUserName = 0;      // not known yet, may be empty
    fileheader.nUserNameLen = 0;
    if (NULL != lpShot->lpszUserName) {
        fileheader.nUserNameLen = (DWORD)_tcslen(lpShot->lpszUserName);
        if (0 < fileheader.nUserNameLen) {
            fileheader.nUserNameLen++;  // account for NULL char
        }
    }

    fileheader.nKCVersion = KEYCONTENT_VERSION_CURRENT;
    fileheader.nKCSize = sizeof(SAVEKEYCONTENT);

    fileheader.nVCVersion = VALUECONTENT_VERSION_CURRENT;
    fileheader.nVCSize = sizeof(SAVEVALUECONTENT);

    fileheader.nHFVersion = HEADFILE_VERSION_CURRENT;
    fileheader.nHFSize = sizeof(SAVEHEADFILE);

    fileheader.nFCVersion = FILECONTENT_VERSION_CURRENT;
    fileheader.nFCSize = sizeof(SAVEFILECONTENT);

    fileheader.nEndianness = FILEHEADER_ENDIANNESS_VALUE;

    // Write header to file
    WriteFile(hFileWholeReg, &fileheader, sizeof(fileheader), &NBW, NULL);

    // new since header version 2
    // (v2) full computername
    if (0 < fileheader.nComputerNameLen) {
        // Write position in file header
        nFPCurrent = SetFilePointer(hFileWholeReg, 0, NULL, FILE_CURRENT);

        SetFilePointer(hFileWholeReg, offsetof(FILEHEADER, ofsComputerName), NULL, FILE_BEGIN);
        WriteFile(hFileWholeReg, &nFPCurrent, sizeof(nFPCurrent), &NBW, NULL);
        fileheader.ofsComputerName = nFPCurrent;  // keep track in memory too

        SetFilePointer(hFileWholeReg, nFPCurrent, NULL, FILE_BEGIN);

        // Write computername
        WriteFile(hFileWholeReg, lpShot->lpszComputerName, fileheader.nComputerNameLen * sizeof(TCHAR), &NBW, NULL);
    }

    // (v2) full username
    if (0 < fileheader.nUserNameLen) {
        // Write position in file header
        nFPCurrent = SetFilePointer(hFileWholeReg, 0, NULL, FILE_CURRENT);

        SetFilePointer(hFileWholeReg, offsetof(FILEHEADER, ofsUserName), NULL, FILE_BEGIN);
        WriteFile(hFileWholeReg, &nFPCurrent, sizeof(nFPCurrent), &NBW, NULL);
        fileheader.ofsUserName = nFPCurrent;  // keep track in memory too

        SetFilePointer(hFileWholeReg, nFPCurrent, NULL, FILE_BEGIN);

        // Write username
        WriteFile(hFileWholeReg, lpShot->lpszUserName, fileheader.nUserNameLen * sizeof(TCHAR), &NBW, NULL);
    }

    // Save HKLM
    if (NULL != lpShot->lpHKLM) {
        SaveRegKeys(lpShot->lpHKLM, 0, offsetof(FILEHEADER, ofsHKLM));
    }

    // Save HKU
    if (NULL != lpShot->lpHKU) {
        SaveRegKeys(lpShot->lpHKU, 0, offsetof(FILEHEADER, ofsHKU));
    }

    // Save HEADFILEs
    if (NULL != lpShot->lpHF) {
        SaveHeadFiles(lpShot->lpHF, offsetof(FILEHEADER, ofsHF));
    }

    // Close file
    CloseHandle(hFileWholeReg);

    ShowWindow(GetDlgItem(hWnd, IDC_PROGBAR), SW_HIDE);
    SetCursor(hSaveCursor);
    MessageBeep(0xffffffff);

    // overwrite first letter of file name with NULL character to get path only, then create backup for initialization on next call
    *(opfn.lpstrFile + opfn.nFileOffset) = 0x00;  // TODO: check
    _tcscpy(lpszLastSaveDir, opfn.lpstrFile);
}


// ----------------------------------------------------------------------
//
// ----------------------------------------------------------------------
size_t AdjustBuffer(LPVOID *lpBuffer, size_t nCurrentSize, size_t nWantedSize, size_t nAlign)
{
    if (NULL == *lpBuffer) {
        nCurrentSize = 0;
    }

    if (nWantedSize > nCurrentSize) {
        if (NULL != *lpBuffer) {
            MYFREE(*lpBuffer);
            *lpBuffer = NULL;
        }

        if (1 >= nAlign) {
            nCurrentSize = nWantedSize;
        } else {
            nCurrentSize = nWantedSize / nAlign;
            nCurrentSize *= nAlign;
            if (nWantedSize > nCurrentSize) {
                nCurrentSize +=  nAlign;
            }
        }

        *lpBuffer = MYALLOC0(nCurrentSize);
    }

    return nCurrentSize;
}

// ----------------------------------------------------------------------
// Load registry key with values from HIVE file
// ----------------------------------------------------------------------
VOID LoadRegKey(DWORD ofsKeyContent, LPKEYCONTENT lpFatherKC, LPKEYCONTENT *lplpCaller)
{
    LPKEYCONTENT lpKC;
    DWORD ofsFirstSubKey;
    DWORD ofsBrotherKey;

    // Copy SAVEKEYCONTENT to aligned memory block
    ZeroMemory(&sKC, sizeof(sKC));
    CopyMemory(&sKC, (lpFileBuffer + ofsKeyContent), fileheader.nKCSize);

    // Create new key content
    // put in a separate var for later use
    lpKC = MYALLOC0(sizeof(KEYCONTENT));
    ZeroMemory(lpKC, sizeof(KEYCONTENT));

    // Set father of current key
    lpKC->lpFatherKC = lpFatherKC;

    // Copy key name
    if (KEYCONTENT_VERSION_2 > fileheader.nKCVersion) {  // old SBCS/MBCS version
        sKC.nKeyNameLen = (DWORD)strlen((const char *)(lpFileBuffer + sKC.ofsKeyName));
        if (0 < sKC.nKeyNameLen) {
            sKC.nKeyNameLen++;  // account for NULL char
        }
    }
    if (0 < sKC.nKeyNameLen) {  // otherwise leave it NULL
        // Copy string to an aligned memory block
        nSourceSize = sKC.nKeyNameLen * fileheader.nCharSize;
        nStringBufferSize = AdjustBuffer(&lpStringBuffer, nStringBufferSize, nSourceSize, REGSHOT_BUFFER_BLOCK_BYTES);
        ZeroMemory(lpStringBuffer, nStringBufferSize);
        CopyMemory(lpStringBuffer, (lpFileBuffer + sKC.ofsKeyName), nSourceSize);

        lpKC->lpszKeyName = MYALLOC0(sKC.nKeyNameLen * sizeof(TCHAR));
        if (sizeof(TCHAR) == fileheader.nCharSize) {
            _tcsncpy(lpKC->lpszKeyName, lpStringBuffer, sKC.nKeyNameLen);
        } else {
#ifdef _UNICODE
            MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, (LPCSTR)lpStringBuffer, -1, lpKC->lpszKeyName, sKC.nKeyNameLen);
#else
            WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK | WC_DEFAULTCHAR, (LPCWSTR)lpStringBuffer, -1, lpKC->lpszKeyName, sKC.nKeyNameLen, NULL, NULL);
#endif
        }

        // Set key name length in chars
        lpKC->lpszKeyName[sKC.nKeyNameLen - 1] = (TCHAR)'\0';  // safety NULL char
        lpKC->cchKeyName = _tcslen(lpKC->lpszKeyName);
    }

    // Adopt to short HKLM/HKU
    if (NULL == lpKC->lpFatherKC) {
        if ((0 == _tcscmp(lpKC->lpszKeyName, lpszHKLMShort))
                || (0 == _tcscmp(lpKC->lpszKeyName, lpszHKLMLong))) {
            MYFREE(lpKC->lpszKeyName);
            lpKC->lpszKeyName = lpszHKLMShort;
            lpKC->cchKeyName = _tcslen(lpKC->lpszKeyName);
        } else if ((0 == _tcscmp(lpKC->lpszKeyName, lpszHKUShort))
                   || (0 == _tcscmp(lpKC->lpszKeyName, lpszHKULong))) {
            MYFREE(lpKC->lpszKeyName);
            lpKC->lpszKeyName = lpszHKUShort;
            lpKC->cchKeyName = _tcslen(lpKC->lpszKeyName);
        }
    }

    // Increase key count
    nGettingKey++;

    // Write pointer to current key into caller's pointer
    if (NULL != lplpCaller) {
        *lplpCaller = lpKC;
    }

    // Copy the value contents of the current key
    if (0 != sKC.ofsFirstValue) {
        LPVALUECONTENT lpVC;
        LPVALUECONTENT *lplpVCPrev;
        DWORD ofsValueContent;

        lplpVCPrev = &lpKC->lpFirstVC;
        for (ofsValueContent = sKC.ofsFirstValue; 0 != ofsValueContent; ofsValueContent = sVC.ofsBrotherValue) {
            // Copy SAVEVALUECONTENT to aligned memory block
            ZeroMemory(&sVC, sizeof(sVC));
            CopyMemory(&sVC, (lpFileBuffer + ofsValueContent), fileheader.nVCSize);

            // Create new value content
            lpVC = MYALLOC0(sizeof(VALUECONTENT));
            ZeroMemory(lpVC, sizeof(VALUECONTENT));

            // Set father key of current value to current key
            lpVC->lpFatherKC = lpKC;

            // Copy value name
            if (VALUECONTENT_VERSION_2 > fileheader.nVCVersion) {  // old SBCS/MBCS version
                sVC.nValueNameLen = (DWORD)strlen((const char *)(lpFileBuffer + sVC.ofsValueName));
                if (0 < sVC.nValueNameLen) {
                    sVC.nValueNameLen++;  // account for NULL char
                }
            }
            if (0 < sVC.nValueNameLen) {  // otherwise leave it NULL
                // Copy string to an aligned memory block
                nSourceSize = sVC.nValueNameLen * fileheader.nCharSize;
                nStringBufferSize = AdjustBuffer(&lpStringBuffer, nStringBufferSize, nSourceSize, REGSHOT_BUFFER_BLOCK_BYTES);
                ZeroMemory(lpStringBuffer, nStringBufferSize);
                CopyMemory(lpStringBuffer, (lpFileBuffer + sVC.ofsValueName), nSourceSize);

                lpVC->lpszValueName = MYALLOC0(sVC.nValueNameLen * sizeof(TCHAR));
                if (sizeof(TCHAR) == fileheader.nCharSize) {
                    _tcsncpy(lpVC->lpszValueName, lpStringBuffer, sVC.nValueNameLen);
                } else {
#ifdef _UNICODE
                    MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, (LPCSTR)lpStringBuffer, -1, lpVC->lpszValueName, sVC.nValueNameLen);
#else
                    WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK | WC_DEFAULTCHAR, (LPCWSTR)lpStringBuffer, -1, lpVC->lpszValueName, sVC.nValueNameLen, NULL, NULL);
#endif
                }

                // Set value name length in chars
                lpVC->lpszValueName[sVC.nValueNameLen - 1] = (TCHAR)'\0';  // safety NULL char
                lpVC->cchValueName = _tcslen(lpVC->lpszValueName);
            }

            // Increase value count
            nGettingValue++;

            // Write pointer to current value into previous value's next value pointer
            if (NULL != lplpVCPrev) {
                *lplpVCPrev = lpVC;
            }
            lplpVCPrev = &lpVC->lpBrotherVC;

            // Copy value meta data
            lpVC->nTypeCode = sVC.nTypeCode;
            lpVC->cbData = sVC.cbData;

            // Copy value data
            if (0 < sVC.cbData) {  // otherwise leave it NULL
                lpVC->lpValueData = MYALLOC0(sVC.cbData);
                CopyMemory(lpVC->lpValueData, (lpFileBuffer + sVC.ofsValueData), sVC.cbData);
            }
        }
    }

    // Update counters display
    nGettingTime = GetTickCount();
    if (REFRESHINTERVAL < (nGettingTime - nBASETIME1)) {
        UpdateCounters(asLangTexts[iszTextKey].lpszText, asLangTexts[iszTextValue].lpszText, nGettingKey, nGettingValue);
    }

    // Save offsets in local variables
    ofsFirstSubKey = sKC.ofsFirstSubKey;
    ofsBrotherKey = sKC.ofsBrotherKey;

    // ATTENTION!!! sKC is INVALID from this point on, due to recursive calls

    // If the entry has childs, then do a recursive call for the first child
    // Pass this entry as father and "lpFirstSubKC" pointer for storing the first child's pointer
    if (0 != ofsFirstSubKey) {
        LoadRegKey(ofsFirstSubKey, lpKC, &lpKC->lpFirstSubKC);
    }

    // If the entry has a following brother, then do a recursive call for the following brother
    // Pass father as father and "lpBrotherKC" pointer for storing the next brother's pointer
    if (0 != ofsBrotherKey) {
        LoadRegKey(ofsBrotherKey, lpFatherKC, &lpKC->lpBrotherKC);
    }
}

// ----------------------------------------------------------------------
// Load registry and files from HIVE file
// ----------------------------------------------------------------------
BOOL LoadHive(LPREGSHOT lpShot)
{
    OPENFILENAME opfn;
    TCHAR filepath[MAX_PATH];  // length incl. NULL character

    DWORD nFileSize;
    DWORD i, j;
    DWORD nRemain;
    DWORD nReadSize;

    // Clear Get File Name result buffer
    ZeroMemory(filepath, sizeof(filepath));

    // Prepare Open File Name dialog
    ZeroMemory(&opfn, sizeof(opfn));
    opfn.lStructSize = sizeof(opfn);
    opfn.hwndOwner = hWnd;
    opfn.lpstrFilter = lpszFilter;
    opfn.lpstrFile = filepath;
    opfn.nMaxFile = MAX_PATH;  // incl. NULL character
    opfn.lpstrInitialDir = lpszLastOpenDir;
    opfn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    opfn.lpstrDefExt = lpszRegshotFileDefExt;

    // Display Open File Name dialog
    if (!GetOpenFileName(&opfn)) {
        return FALSE;
    }

    // Open file for reading
    hFileWholeReg = CreateFile(opfn.lpstrFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (INVALID_HANDLE_VALUE == hFileWholeReg) {
        ErrMsg(asLangTexts[iszTextErrorOpenFile].lpszText);
        return FALSE;
    }

    nFileSize = GetFileSize(hFileWholeReg, NULL);
    if (sizeof(fileheader) > nFileSize) {
        CloseHandle(hFileWholeReg);
        ErrMsg(TEXT("wrong filesize"));
        return FALSE;
    }

    // Initialize file header
    ZeroMemory(&fileheader, sizeof(fileheader));

    // Read first part of file header from file (signature, nHeaderSize)
    ReadFile(hFileWholeReg, &fileheader, offsetof(FILEHEADER, ofsHKLM), &NBW, NULL);

    // Check for valid file signatures (SBCS/MBCS and UTF-16 signature)
    if ((0 != strncmp(szRegshotFileSignatureSBCS, fileheader.signature, MAX_SIGNATURE_LENGTH)) && (0 != strncmp(szRegshotFileSignatureUTF16, fileheader.signature, MAX_SIGNATURE_LENGTH))) {
        CloseHandle(hFileWholeReg);
        ErrMsg(TEXT("It is not a Regshot hive file!"));
        return FALSE;
    }

    // Check file signature for correct type (SBCS/MBCS or UTF-16)
    if (0 != strncmp(szRegshotFileSignature, fileheader.signature, MAX_SIGNATURE_LENGTH)) {
        CloseHandle(hFileWholeReg);
#ifdef _UNICODE
        ErrMsg(TEXT("It is not a Unicode Regshot hive file! Try the ANSI version with it."));
#else
        ErrMsg(TEXT("It is not an ANSI Regshot hive file! Try the Unicode version with it."));
#endif
        return FALSE;
    }


    // Clear shot
    FreeShot(lpShot);

    // Setup GUI for loading...
    nGettingKey   = 0;
    nGettingValue = 0;
    nGettingTime  = 0;
    nGettingFile  = 0;
    nGettingDir   = 0;
    nBASETIME  = GetTickCount();
    nBASETIME1 = nBASETIME;
    if (&Shot1 == lpShot) {
        UI_BeforeShot(IDC_1STSHOT);
    } else {
        UI_BeforeShot(IDC_2NDSHOT);
    }

    // Allocate memory to hold the complete file
    lpFileBuffer = MYALLOC(nFileSize);

    // Read file blockwise for progress bar
    InitProgressBar();
    nFileStep = nFileSize / REGSHOT_READ_BLOCK_SIZE / MAXPBPOSITION;  // TODO: does look wrong!?! PBM_SETSTEP message was in InitProgressBar()

    SetFilePointer(hFileWholeReg, 0, NULL, FILE_BEGIN);
    nRemain = nFileSize;  // 100% to go
    nReadSize = REGSHOT_READ_BLOCK_SIZE;  // next block length to read
    for (i = 0, j = 0; nRemain > 0; i += nReadSize, j++) {
        // If the rest is smaller than a block, then use the rest length
        if (REGSHOT_READ_BLOCK_SIZE > nRemain) {
            nReadSize = nRemain;
        }

        // Read the next block
        ReadFile(hFileWholeReg, lpFileBuffer + i, nReadSize, &NBW, NULL);
        if (NBW != nReadSize) {
            CloseHandle(hFileWholeReg);
            ErrMsg(TEXT("Reading ERROR!"));
            return FALSE;
        }

        // Determine how much to go, if zero leave the for loop
        nRemain -= nReadSize;
        if (0 == nRemain) {
            break;
        }

        // Handle progress bar
        if (j % (nFileSize / REGSHOT_READ_BLOCK_SIZE) > nFileStep) {  // TODO: does look wrong!?!
            j = 0;
            SendDlgItemMessage(hWnd, IDC_PROGBAR, PBM_STEPIT, (WPARAM)0, (LPARAM)0);
            UpdateWindow(hWnd);
            PeekMessage(&msg, hWnd, WM_ACTIVATE, WM_ACTIVATE, PM_REMOVE);
        }
    }

    CloseHandle(hFileWholeReg);

    ShowWindow(GetDlgItem(hWnd, IDC_PROGBAR), SW_HIDE);  // Hide progress bar
    ShowHideCounters(SW_SHOW);

    // Check size for copying file header
    nSourceSize = fileheader.nFHSize;
    if (0 == nSourceSize) {
        nSourceSize = offsetof(FILEHEADER, nFHVersion);
    } else if (sizeof(fileheader) < nSourceSize) {
        nSourceSize = sizeof(fileheader);
    }

    // Copy file header to structure
    CopyMemory(&fileheader, lpFileBuffer, nSourceSize);

    // Enhance data of old headers to be used with newer code
    if (FILEHEADER_VERSION_EMPTY == fileheader.nFHVersion) {
        if ((0 != fileheader.ofsHKLM) && (fileheader.ofsHKU == fileheader.ofsHKLM)) {
            fileheader.ofsHKLM = 0;
        }
        if ((0 != fileheader.ofsHKU) && (fileheader.ofsHF == fileheader.ofsHKU)) {
            fileheader.ofsHKU = 0;
        }

        fileheader.nFHVersion = FILEHEADER_VERSION_1;
        fileheader.nCharSize = 1;
        fileheader.nKCVersion = KEYCONTENT_VERSION_1;
        fileheader.nKCSize = offsetof(SAVEKEYCONTENT, nKeyNameLen);
        fileheader.nVCVersion = VALUECONTENT_VERSION_1;
        fileheader.nVCSize = offsetof(SAVEVALUECONTENT, nValueNameLen);
        fileheader.nHFVersion = HEADFILE_VERSION_1;
        fileheader.nHFSize = sizeof(SAVEHEADFILE);  // not changed yet, if it is then adopt to offsetof(SAVEHEADFILE, <first new field>)
        fileheader.nFCVersion = FILECONTENT_VERSION_1;
        fileheader.nFCSize = offsetof(SAVEFILECONTENT, nFileNameLen);
    }
    if (FILEHEADER_VERSION_2 >= fileheader.nFHVersion) {
        if (0 == fileheader.nEndianness) {
            *((unsigned char *)&fileheader.nEndianness)     = 0x78;  // old existing versions were all little endian
            *((unsigned char *)&fileheader.nEndianness + 1) = 0x56;
            *((unsigned char *)&fileheader.nEndianness + 2) = 0x34;
            *((unsigned char *)&fileheader.nEndianness + 3) = 0x12;
        }
    }

    // Check structure boundaries
    if (sizeof(SAVEKEYCONTENT) < fileheader.nKCSize) {
        fileheader.nKCSize = sizeof(SAVEKEYCONTENT);
    }
    if (sizeof(SAVEVALUECONTENT) < fileheader.nVCSize) {
        fileheader.nVCSize = sizeof(SAVEVALUECONTENT);
    }
    if (sizeof(SAVEHEADFILE) < fileheader.nHFSize) {
        fileheader.nHFSize = sizeof(SAVEHEADFILE);
    }
    if (sizeof(SAVEFILECONTENT) < fileheader.nFCSize) {
        fileheader.nFCSize = sizeof(SAVEFILECONTENT);
    }

    // Check for incompatible char size (known: 1 = SBCS/MBCS, 2 = UTF-16)
    if (sizeof(TCHAR) != fileheader.nCharSize) {
#ifdef _UNICODE
        if (1 == fileheader.nCharSize) {
            ErrMsg(TEXT("It is not a Unicode Regshot hive file! Try the ANSI version with it."));
        } else if (2 != fileheader.nCharSize) {
            ErrMsg(TEXT("Unknown character size! Maybe created by a newer Regshot version."));
        } else {
            ErrMsg(TEXT("Unsupported character size!"));
        }
#else
        ErrMsg(TEXT("It is not an ANSI Regshot hive file! Try the Unicode version with it."));
#endif
        if (NULL != lpFileBuffer) {
            MYFREE(lpFileBuffer);
            lpFileBuffer = NULL;
        }
        UI_AfterShot();
        return FALSE;
    }

    // Check for incompatible endianness (known: Intel = Little Endian)
    if (fileheader.nEndianness != FILEHEADER_ENDIANNESS_VALUE) {
#ifdef __LITTLE_ENDIAN__
        ErrMsg(TEXT("It is not a Little Endian Regshot hive file!"));
#else
        ErrMsg(TEXT("It is not a Big Endian Regshot hive file!"));
#endif
        if (NULL != lpFileBuffer) {
            MYFREE(lpFileBuffer);
            lpFileBuffer = NULL;
        }
        UI_AfterShot();
        return FALSE;
    }

    // ^^^ here the file header can be checked for additional extended content
    // * remember that files from older versions do not provide these additional data

    // New temporary string buffer
    lpStringBuffer = NULL;

    // Copy computer name from file header to shot data
    if (FILEHEADER_VERSION_1 < fileheader.nFHVersion) {  // newer Unicode/SBCS/MBCS version
        if (0 < fileheader.nComputerNameLen) {  // otherwise leave it NULL
            // Copy string to an aligned memory block
            nSourceSize = fileheader.nComputerNameLen * fileheader.nCharSize;
            nStringBufferSize = AdjustBuffer(&lpStringBuffer, nStringBufferSize, nSourceSize, REGSHOT_BUFFER_BLOCK_BYTES);
            ZeroMemory(lpStringBuffer, nStringBufferSize);
            CopyMemory(lpStringBuffer, (lpFileBuffer + fileheader.ofsComputerName), nSourceSize);

            lpShot->lpszComputerName = MYALLOC0(fileheader.nComputerNameLen * sizeof(TCHAR));
            if (sizeof(TCHAR) == fileheader.nCharSize) {
                _tcscpy(lpShot->lpszComputerName, lpStringBuffer);
            } else {
#ifdef _UNICODE
                MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, (LPCSTR)lpStringBuffer, -1, lpShot->lpszComputerName, fileheader.nComputerNameLen);
#else
                WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK | WC_DEFAULTCHAR, (LPCWSTR)lpStringBuffer, -1, lpShot->lpszComputerName, fileheader.nComputerNameLen, NULL, NULL);
#endif
            }
        }
    } else {  // old SBCS/MBCS version
        // Copy string to an aligned memory block
        nSourceSize = strnlen((const char *)&fileheader.computername, OLD_COMPUTERNAMELEN);
        if (0 < nSourceSize) {  // otherwise leave it NULL
            nStringBufferSize = AdjustBuffer(&lpStringBuffer, nStringBufferSize, (nSourceSize + 1), REGSHOT_BUFFER_BLOCK_BYTES);
            ZeroMemory(lpStringBuffer, nStringBufferSize);
            CopyMemory(lpStringBuffer, &fileheader.computername, nSourceSize);

            lpShot->lpszComputerName = MYALLOC0((nSourceSize + 1) * sizeof(TCHAR));
#ifdef _UNICODE
            MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, (LPCSTR)lpStringBuffer, (int)(nSourceSize + 1), lpShot->lpszComputerName, (int)(nSourceSize + 1));
#else
            strncpy(lpShot->lpszComputerName, lpStringBuffer, nSourceSize);
#endif
            lpShot->lpszComputerName[nSourceSize] = 0;
        }
    }

    // Copy user name from file header to shot data
    if (FILEHEADER_VERSION_1 < fileheader.nFHVersion) {  // newer Unicode/SBCS/MBCS version
        if (0 < fileheader.nUserNameLen) {  // otherwise leave it NULL
            // Copy string to an aligned memory block
            nSourceSize = fileheader.nUserNameLen * fileheader.nCharSize;
            nStringBufferSize = AdjustBuffer(&lpStringBuffer, nStringBufferSize, nSourceSize, REGSHOT_BUFFER_BLOCK_BYTES);
            ZeroMemory(lpStringBuffer, nStringBufferSize);
            CopyMemory(lpStringBuffer, (lpFileBuffer + fileheader.ofsUserName), nSourceSize);

            lpShot->lpszUserName = MYALLOC0(fileheader.nUserNameLen * sizeof(TCHAR));
            if (sizeof(TCHAR) == fileheader.nCharSize) {
                _tcscpy(lpShot->lpszUserName, lpStringBuffer);
            } else {
#ifdef _UNICODE
                MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, (LPCSTR)lpStringBuffer, -1, lpShot->lpszUserName, fileheader.nUserNameLen);
#else
                WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK | WC_DEFAULTCHAR, (LPCWSTR)lpStringBuffer, -1, lpShot->lpszUserName, fileheader.nUserNameLen, NULL, NULL);
#endif
            }
        }
    } else {  // old SBCS/MBCS version
        // Copy string to an aligned memory block
        nSourceSize = strnlen((const char *)&fileheader.username, OLD_COMPUTERNAMELEN);
        if (0 < nSourceSize) {  // otherwise leave it NULL
            nStringBufferSize = AdjustBuffer(&lpStringBuffer, nStringBufferSize, (nSourceSize + 1), REGSHOT_BUFFER_BLOCK_BYTES);
            ZeroMemory(lpStringBuffer, nStringBufferSize);
            CopyMemory(lpStringBuffer, &fileheader.username, nSourceSize);

            lpShot->lpszUserName = MYALLOC0((nSourceSize + 1) * sizeof(TCHAR));
#ifdef _UNICODE
            MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, (LPCSTR)lpStringBuffer, (int)(nSourceSize + 1), lpShot->lpszUserName, (int)(nSourceSize + 1));
#else
            strncpy(lpShot->lpszUserName, lpStringBuffer, nSourceSize);
#endif
            lpShot->lpszUserName[nSourceSize] = 0;
        }
    }

    CopyMemory(&lpShot->systemtime, &fileheader.systemtime, sizeof(SYSTEMTIME));

    if (0 != fileheader.ofsHKLM) {
        LoadRegKey(fileheader.ofsHKLM, NULL, &lpShot->lpHKLM);
    }

    if (0 != fileheader.ofsHKU) {
        LoadRegKey(fileheader.ofsHKU, NULL, &lpShot->lpHKU);
    }

    if (0 != fileheader.ofsHF) {
        LoadHeadFile(fileheader.ofsHF, &lpShot->lpHF);
    }

    // Setup GUI for loading...
    if (NULL != lpShot->lpHF) {
        SendMessage(GetDlgItem(hWnd, IDC_CHECKDIR), BM_SETCHECK, (WPARAM)BST_CHECKED, (LPARAM)0);
        SendMessage(hWnd, WM_COMMAND, (WPARAM)IDC_CHECKDIR, (LPARAM)0);

        FindDirChain(lpShot->lpHF, lpszExtDir, EXTDIRLEN);  // Get new chains, must do this after loading! Length in TCHARs incl. NULL char
        SetDlgItemText(hWnd, IDC_EDITDIR, lpszExtDir);
    } else {
        SetDlgItemText(hWnd, IDC_EDITDIR, TEXT(""));
    }

    if (NULL != lpStringBuffer) {
        MYFREE(lpStringBuffer);
        lpStringBuffer = NULL;
    }

    if (NULL != lpFileBuffer) {
        MYFREE(lpFileBuffer);
        lpFileBuffer = NULL;
    }

    // Set flag
    lpShot->fFilled = TRUE;
    lpShot->fLoaded = TRUE;

    UI_AfterShot();

    // overwrite first letter of file name with NULL character to get path only, then create backup for initialization on next call
    *(opfn.lpstrFile + opfn.nFileOffset) = 0x00;
    _tcscpy(lpszLastOpenDir, opfn.lpstrFile);

    return TRUE;
}
