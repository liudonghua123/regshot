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
#include "version.h"
#include <LMCons.h>  // for UNLEN define

LPTSTR lpszResultFileBaseName = REGSHOT_RESULT_FILE;

LPTSTR lpszFilter =
#ifdef _UNICODE
    TEXT("Regshot Unicode hive files (*.hivu)\0*.hivu\0All files\0*.*\0\0");
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
    TEXT("hivu");
#else
    TEXT("hiv");
#endif

LPTSTR lpszValueDataIsNULL = TEXT(": (NULL!)");

LPTSTR lpszEmpty = TEXT("");

REGSHOT Shot1;
REGSHOT Shot2;

FILEHEADER fileheader;
FILEEXTRADATA fileextradata;
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

// Compare result
COMPRESULTS CompareResult;

DWORD NBW;               // that is: NumberOfBytesWritten

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


// VS 6 fix, may depend on PSDK/WSDK too
#ifndef _tcsnlen
size_t _tcsnlen(const TCHAR *str, size_t numberOfElements)
{
    size_t i;

    for (i = 0; i < numberOfElements; i++, str++) {
        if ((TCHAR)'\0' == *str) {
            break;
        }
    }

    return i;
}
#endif

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
// Transform value content data from binary into string according
// to given conversion type (may differ from original type)
// Called by GetWholeValueData()
// ----------------------------------------------------------------------
LPTSTR TransData(LPVALUECONTENT lpVC, DWORD nConversionType)
{
    LPTSTR lpszValueData;
    LPDWORD lpDword;
    LPQWORD lpQword;
    DWORD nDwordCpu;
#if 1 == __LITTLE_ENDIAN__
    // no REG_QWORD_BIG_ENDIAN yet
#elif
    QWORD nQwordCpu;
#endif

    lpszValueData = NULL;
    lpDword = NULL;
    lpQword = NULL;

    if (NULL == lpVC->lpValueData) {
        lpszValueData = MYALLOC((_tcslen(lpszValueDataIsNULL) + 1) * sizeof(TCHAR));
        _tcscpy(lpszValueData, lpszValueDataIsNULL);
    } else {
        DWORD cbData;
        DWORD ibCurrent;
        size_t cchToGo;
        size_t cchString;
        size_t cchActual;
        LPTSTR lpszSrc;
        LPTSTR lpszDst;

        cbData = lpVC->cbData;

        switch (nConversionType) {
            case REG_SZ:
            case REG_EXPAND_SZ:
                // string value that can be displayed as a string
                // format  ": \"<string>\"\0"
                lpszValueData = MYALLOC0(((3 + 2) * sizeof(TCHAR)) + cbData);
                _tcscpy(lpszValueData, TEXT(": \""));
                if (NULL != lpVC->lpValueData) {
                    memcpy(&lpszValueData[3], lpVC->lpValueData, cbData);
                }
                _tcscat(lpszValueData, TEXT("\""));
                break;

            case REG_MULTI_SZ:
                // multi string value that can be displayed as strings
                // format  ": \"<string>\", \"<string>\", \"<string>\", ...\0"
                // see http://msdn.microsoft.com/en-us/library/windows/desktop/ms724884.aspx
                nStringBufferSize = AdjustBuffer(&lpStringBuffer, nStringBufferSize, 10 + (2 * cbData), REGSHOT_BUFFER_BLOCK_BYTES);
                ZeroMemory(lpStringBuffer, nStringBufferSize);
                lpszDst = lpStringBuffer;
                _tcscpy(lpszDst, TEXT(": \""));
                lpszDst += 3;
                cchActual = 0;
                if (NULL != lpVC->lpValueData) {
                    lpszSrc = (LPTSTR)lpVC->lpValueData;
                    cchToGo = cbData / sizeof(TCHAR);  // convert byte count to char count
                    while ((cchToGo > 0) && (*lpszSrc)) {
                        if (0 != cchActual) {
                            _tcscpy(lpszDst, TEXT("\", \""));
                            lpszDst += 4;
                            cchActual += 4;
                        }
                        cchString = _tcsnlen(lpszSrc, cchToGo);
                        _tcsncpy(lpszDst, lpszSrc, cchString);
                        lpszDst += cchString;
                        cchActual += cchString;

                        cchToGo -= cchString;
                        if (cchToGo == 0) {
                            break;
                        }

                        // account for null char
                        lpszSrc += cchString + 1;
                        cchToGo -= 1;
                    }
                }
                _tcscpy(lpszDst, TEXT("\""));
                cchActual += 3 + 1 + 1;  // account for null char
                lpszValueData = MYALLOC(cchActual * sizeof(TCHAR));
                _tcscpy(lpszValueData, lpStringBuffer);
                break;

#if 1 == __LITTLE_ENDIAN__
            case REG_DWORD_BIG_ENDIAN:
#elif
            case REG_DWORD_LITTLE_ENDIAN:
#endif
                // convert DWORD with different endianness
                lpDword = &nDwordCpu;
                for (ibCurrent = 0; ibCurrent < sizeof(DWORD); ibCurrent++) {
                    ((LPBYTE)&nDwordCpu)[ibCurrent] = lpVC->lpValueData[sizeof(DWORD) - 1 - ibCurrent];
                }

#if 1 == __LITTLE_ENDIAN__
            case REG_DWORD_LITTLE_ENDIAN:
#elif
            case REG_DWORD_BIG_ENDIAN:
#endif
                // native DWORD that can be displayed as DWORD
                if (NULL == lpDword) {
                    lpDword = (LPDWORD)lpVC->lpValueData;
                }
                // format  ": 0xXXXXXXXX\0"
                lpszValueData = MYALLOC0((1 + 3 + 8 + 1) * sizeof(TCHAR));
                _tcscpy(lpszValueData, TEXT(":"));
                if (NULL != lpVC->lpValueData) {
                    _sntprintf(lpszValueData + 1, (3 + 8 + 1), TEXT(" 0x%08X\0"), *lpDword);
                }
                break;

#if 1 == __LITTLE_ENDIAN__
                //case REG_QWORD_BIG_ENDIAN:
#elif
            case REG_QWORD_LITTLE_ENDIAN:
#endif
#if 1 == __LITTLE_ENDIAN__
                // no REG_QWORD_BIG_ENDIAN yet
#elif
                // convert QWORD with different endianness
                lpQword = &nQwordCpu;
                for (ibCurrent = 0; ibCurrent < sizeof(QWORD); i++) {
                    ((LPBYTE)&nQwordCpu)[ibCurrent] = lpVC->lpValueData[sizeof(QWORD) - ibCurrent - ibCurrent];
                }
#endif

#if 1 == __LITTLE_ENDIAN__
            case REG_QWORD_LITTLE_ENDIAN:
#elif
                //case REG_QWORD_BIG_ENDIAN:
#endif
                // native QWORD that can be displayed as QWORD
                if (NULL == lpQword) {
                    lpQword = (LPQWORD)lpVC->lpValueData;
                }
                // format  ": 0xXXXXXXXXXXXXXXXX\0"
                lpszValueData = MYALLOC0((1 + 3 + 16 + 1) * sizeof(TCHAR));
                _tcscpy(lpszValueData, TEXT(":"));
                if (NULL != lpVC->lpValueData) {
                    _sntprintf(lpszValueData + 1, (3 + 16 + 1), TEXT(" 0x%016I64X\0"), *lpQword);
                }
                break;

            default:
                // display value as hex bytes
                // format ":[ xx][ xx]...[ xx]\0"
                lpszValueData = MYALLOC0((1 + (cbData * 3) + 1) * sizeof(TCHAR));
                _tcscpy(lpszValueData, TEXT(":"));
                for (ibCurrent = 0; ibCurrent < cbData; ibCurrent++) {
                    _sntprintf(lpszValueData + (1 + (ibCurrent * 3)), 4, TEXT(" %02X\0"), *(lpVC->lpValueData + ibCurrent));
                }
        }
    }

    return lpszValueData;
}


// ----------------------------------------------------------------------
// Get value data from value content as string
// Check for special cases to call TransData() properly
// ----------------------------------------------------------------------
LPTSTR GetWholeValueData(LPVALUECONTENT lpVC)
{
    LPTSTR lpszValueData;

    lpszValueData = NULL;

    if (NULL == lpVC->lpValueData) {
        lpszValueData = TransData(lpVC, REG_BINARY);
    } else {
        DWORD cbData;
        size_t cchMax;
        size_t cchActual;

        cbData = lpVC->cbData;

        switch (lpVC->nTypeCode) {
            case REG_SZ:
            case REG_EXPAND_SZ:
            case REG_MULTI_SZ:
                // string values
                // check for hidden bytes after string[s]
                cchMax = cbData / sizeof(TCHAR);  // convert byte count to char count
                if (REG_MULTI_SZ == lpVC->nTypeCode) {
                    // search double null chars
                    for (cchActual = 0; cchActual < cchMax; cchActual++) {
                        if (0 != ((LPTSTR)lpVC->lpValueData)[cchActual]) {
                            continue;
                        }

                        cchActual++;
                        if (cchActual >= cchMax) {  // special case: incorrectly terminated
                            break;
                        }

                        if (0 != ((LPTSTR)lpVC->lpValueData)[cchActual]) {
                            continue;
                        }

                        // found
                        cchActual++;
                        break;
                    }
                } else {
                    cchActual = _tcsnlen((LPTSTR)lpVC->lpValueData, cchMax);
                    if (cchActual < cchMax) {
                        cchActual++;  // account for null char
                    }
                }
                if ((cchActual * sizeof(TCHAR)) == cbData) {
                    lpszValueData = TransData(lpVC, lpVC->nTypeCode);
                } else {
                    lpszValueData = TransData(lpVC, REG_BINARY);
                }
                break;

            case REG_DWORD_LITTLE_ENDIAN:
            case REG_DWORD_BIG_ENDIAN:
                // DWORD values
                if (sizeof(DWORD) == cbData) {
                    lpszValueData = TransData(lpVC, lpVC->nTypeCode);
                } else {
                    lpszValueData = TransData(lpVC, REG_BINARY);
                }
                break;

            case REG_QWORD_LITTLE_ENDIAN:
                //case REG_QWORD_BIG_ENDIAN:
                // QWORD values
                if (sizeof(QWORD) == cbData) {
                    lpszValueData = TransData(lpVC, lpVC->nTypeCode);
                } else {
                    lpszValueData = TransData(lpVC, REG_BINARY);
                }
                break;

            default:
                lpszValueData = TransData(lpVC, REG_BINARY);
        }
    }

    return lpszValueData;
}


//-------------------------------------------------------------
// Routine to create new comparison result, distribute to different pointers
//-------------------------------------------------------------
VOID CreateNewResult(DWORD nActionType, LPVOID lpContentOld, LPVOID lpContentNew)
{
    LPCOMPRESULT lpCR;

    lpCR = (LPCOMPRESULT)MYALLOC0(sizeof(COMPRESULT));
    lpCR->lpContentOld = lpContentOld;
    lpCR->lpContentNew = lpContentNew;

    switch (nActionType) {
        case KEYDEL:
            (NULL == CompareResult.stCRHeads.lpCRKeyDeleted) ? (CompareResult.stCRHeads.lpCRKeyDeleted = lpCR) : (CompareResult.stCRCurrent.lpCRKeyDeleted->lpNextCR = lpCR);
            CompareResult.stCRCurrent.lpCRKeyDeleted = lpCR;
            break;
        case KEYADD:
            (NULL == CompareResult.stCRHeads.lpCRKeyAdded) ? (CompareResult.stCRHeads.lpCRKeyAdded = lpCR) : (CompareResult.stCRCurrent.lpCRKeyAdded->lpNextCR = lpCR);
            CompareResult.stCRCurrent.lpCRKeyAdded = lpCR;
            break;
        case VALDEL:
            (NULL == CompareResult.stCRHeads.lpCRValDeleted) ? (CompareResult.stCRHeads.lpCRValDeleted = lpCR) : (CompareResult.stCRCurrent.lpCRValDeleted->lpNextCR = lpCR);
            CompareResult.stCRCurrent.lpCRValDeleted = lpCR;
            break;
        case VALADD:
            (NULL == CompareResult.stCRHeads.lpCRValAdded) ? (CompareResult.stCRHeads.lpCRValAdded = lpCR) : (CompareResult.stCRCurrent.lpCRValAdded->lpNextCR = lpCR);
            CompareResult.stCRCurrent.lpCRValAdded = lpCR;
            break;
        case VALMODI:
            (NULL == CompareResult.stCRHeads.lpCRValModified) ? (CompareResult.stCRHeads.lpCRValModified = lpCR) : (CompareResult.stCRCurrent.lpCRValModified->lpNextCR = lpCR);
            CompareResult.stCRCurrent.lpCRValModified = lpCR;
            break;
        case DIRDEL:
            (NULL == CompareResult.stCRHeads.lpCRDirDeleted) ? (CompareResult.stCRHeads.lpCRDirDeleted = lpCR) : (CompareResult.stCRCurrent.lpCRDirDeleted->lpNextCR = lpCR);
            CompareResult.stCRCurrent.lpCRDirDeleted = lpCR;
            break;
        case DIRADD:
            (NULL == CompareResult.stCRHeads.lpCRDirAdded) ? (CompareResult.stCRHeads.lpCRDirAdded = lpCR) : (CompareResult.stCRCurrent.lpCRDirAdded->lpNextCR = lpCR);
            CompareResult.stCRCurrent.lpCRDirAdded = lpCR;
            break;
        case DIRMODI:
            (NULL == CompareResult.stCRHeads.lpCRDirModified) ? (CompareResult.stCRHeads.lpCRDirModified = lpCR) : (CompareResult.stCRCurrent.lpCRDirModified->lpNextCR = lpCR);
            CompareResult.stCRCurrent.lpCRDirModified = lpCR;
            break;
        case FILEDEL:
            (NULL == CompareResult.stCRHeads.lpCRFileDeleted) ? (CompareResult.stCRHeads.lpCRFileDeleted = lpCR) : (CompareResult.stCRCurrent.lpCRFileDeleted->lpNextCR = lpCR);
            CompareResult.stCRCurrent.lpCRFileDeleted = lpCR;
            break;
        case FILEADD:
            (NULL == CompareResult.stCRHeads.lpCRFileAdded) ? (CompareResult.stCRHeads.lpCRFileAdded = lpCR) : (CompareResult.stCRCurrent.lpCRFileAdded->lpNextCR = lpCR);
            CompareResult.stCRCurrent.lpCRFileAdded = lpCR;
            break;
        case FILEMODI:
            (NULL == CompareResult.stCRHeads.lpCRFileModified) ? (CompareResult.stCRHeads.lpCRFileModified = lpCR) : (CompareResult.stCRCurrent.lpCRFileModified->lpNextCR = lpCR);
            CompareResult.stCRCurrent.lpCRFileModified = lpCR;
            break;
    }
}


//-------------------------------------------------------------
// Convert content to result strings
//-------------------------------------------------------------
size_t ResultToString(LPTSTR rgszResultStrings[], size_t iResultStringsMac, DWORD nActionType, LPVOID lpContent, BOOL fNewContent)
{
    LPTSTR lpszName;
    LPTSTR lpszData;
    LPTSTR lpszOldData;
    size_t cchData;
    size_t iResultStringsNew;
    size_t iResultStringsTemp1;

    iResultStringsNew = iResultStringsMac;

    if ((KEYDEL == nActionType) || (KEYADD == nActionType)) {
        // create result
        // name only
        if (iResultStringsNew < MAX_RESULT_STRINGS) {
            rgszResultStrings[iResultStringsNew] = GetWholeKeyName(lpContent, fUseLongRegHead);
            iResultStringsNew++;
        }
    } else if ((VALDEL == nActionType) || (VALADD == nActionType) || (VALMODI == nActionType)) {
        // name
        lpszName = GetWholeValueName(lpContent, fUseLongRegHead);
        // data
        lpszData = GetWholeValueData(lpContent);
        cchData = 0;
        if (NULL != lpszData) {
            cchData = _tcslen(lpszData);
        }
        // create result
        if (iResultStringsNew < MAX_RESULT_STRINGS) {
            rgszResultStrings[iResultStringsNew] = MYALLOC((_tcslen(lpszName) + cchData + 1) * sizeof(TCHAR));
            _tcscpy(rgszResultStrings[iResultStringsNew], lpszName);
            if (NULL != lpszData) {
                _tcscat(rgszResultStrings[iResultStringsNew], lpszData);
            }
            iResultStringsNew++;
        }
        MYFREE(lpszName);
        if (NULL != lpszData) {
            MYFREE(lpszData);
        }
    } else if ((DIRDEL == nActionType) || (DIRADD == nActionType) || (DIRMODI == nActionType)) {
        // create result (1st line)
        // name only
        if ((!fNewContent) || (0 >= iResultStringsMac)) {
            if (iResultStringsNew < MAX_RESULT_STRINGS) {
                rgszResultStrings[iResultStringsNew] = GetWholeFileName(lpContent, 0);
                iResultStringsNew++;
            }
        }
        // attributes
        lpszData = MYALLOC0(SIZEOF_RESULT_DATA * sizeof(TCHAR));
        cchData = _sntprintf(lpszData, SIZEOF_RESULT_DATA, TEXT("0x%08X\0"), ((LPFILECONTENT)lpContent)->nFileAttributes);
        // create result (2nd line)
        // add to previous line if old and new data present
        iResultStringsTemp1 = iResultStringsNew;
        lpszOldData = NULL;
        if ((fNewContent) && (0 < iResultStringsMac)) {
            iResultStringsTemp1 = iResultStringsMac - 1;
            lpszOldData = rgszResultStrings[iResultStringsTemp1];
            cchData += _tcslen(lpszOldData) + 5;  // length in chars of separator between old and new content
        }
        if (iResultStringsTemp1 < MAX_RESULT_STRINGS) {
            rgszResultStrings[iResultStringsTemp1] = MYALLOC((cchData + 1) * sizeof(TCHAR));
            if ((fNewContent) && (0 < iResultStringsMac)) {
                _tcscpy(rgszResultStrings[iResultStringsTemp1], lpszOldData);
                _tcscat(rgszResultStrings[iResultStringsTemp1], TEXT(" --> "));
            } else {
                rgszResultStrings[iResultStringsNew][0] = (TCHAR)'\0';
            }
            _tcscat(rgszResultStrings[iResultStringsTemp1], lpszData);

            if (iResultStringsTemp1 >= iResultStringsMac) {
                iResultStringsNew++;
            }
        }
        MYFREE(lpszData);
        if (NULL != lpszOldData) {
            MYFREE(lpszOldData);
        }
    } else if ((FILEDEL == nActionType) || (FILEADD == nActionType) || (FILEMODI == nActionType)) {
        SYSTEMTIME stFile;
        FILETIME ftFile;
        LARGE_INTEGER cbFile;
        // create result (1st line)
        // name only
        if ((!fNewContent) || (0 >= iResultStringsMac)) {
            if (iResultStringsNew < MAX_RESULT_STRINGS) {
                rgszResultStrings[iResultStringsNew] = GetWholeFileName(lpContent, 0);
                iResultStringsNew++;
            }
        }
        // last write time, attributes, size
        ZeroMemory(&ftFile, sizeof(ftFile));
        ftFile.dwLowDateTime = ((LPFILECONTENT)lpContent)->nWriteDateTimeLow;
        ftFile.dwHighDateTime = ((LPFILECONTENT)lpContent)->nWriteDateTimeHigh;
        ZeroMemory(&stFile, sizeof(stFile));
        FileTimeToSystemTime(&ftFile, &stFile);
        ZeroMemory(&cbFile, sizeof(cbFile));
        cbFile.LowPart = ((LPFILECONTENT)lpContent)->nFileSizeLow;
        cbFile.HighPart = ((LPFILECONTENT)lpContent)->nFileSizeHigh;
        lpszData = MYALLOC0(SIZEOF_RESULT_DATA * sizeof(TCHAR));
        cchData = _sntprintf(lpszData, SIZEOF_RESULT_DATA, TEXT("%04d-%02d-%02d %02d:%02d:%02d, 0x%08X, %ld\0"),
                             stFile.wYear, stFile.wMonth, stFile.wDay,
                             stFile.wHour, stFile.wMinute, stFile.wSecond,
                             ((LPFILECONTENT)lpContent)->nFileAttributes, cbFile);
        // create result (2nd/3rd line)
        if (iResultStringsNew < MAX_RESULT_STRINGS) {
            rgszResultStrings[iResultStringsNew] = lpszData;
            iResultStringsNew++;
        }
    } else {
        // TODO: error message and handling
    }

    return iResultStringsNew;
}


//-------------------------------------------------------------
// Routine to free all comparison results (release memory)
//-------------------------------------------------------------
VOID FreeAllCompResults(LPCOMPRESULT lpCR)
{
    LPCOMPRESULT lpNextCR;

    for (; NULL != lpCR; lpCR = lpNextCR) {
        // Save pointer in local variable
        lpNextCR = lpCR->lpNextCR;

        // Increase count
        cCurrent++;

        // Update progress bar display
        if (0 != cEnd) {
            nCurrentTime = GetTickCount();
            if (REFRESHINTERVAL < (nCurrentTime - nLastTime)) {
                UI_UpdateProgressBar();
            }
        }

        // Free compare result itself
        MYFREE(lpCR);
    }
}


// ----------------------------------------------------------------------
// Free all compare results
// ----------------------------------------------------------------------
VOID FreeCompareResult(void)
{
    FreeAllCompResults(CompareResult.stCRHeads.lpCRKeyDeleted);
    FreeAllCompResults(CompareResult.stCRHeads.lpCRKeyAdded);
    FreeAllCompResults(CompareResult.stCRHeads.lpCRValDeleted);
    FreeAllCompResults(CompareResult.stCRHeads.lpCRValAdded);
    FreeAllCompResults(CompareResult.stCRHeads.lpCRValModified);
    FreeAllCompResults(CompareResult.stCRHeads.lpCRDirDeleted);
    FreeAllCompResults(CompareResult.stCRHeads.lpCRDirAdded);
    FreeAllCompResults(CompareResult.stCRHeads.lpCRDirModified);
    FreeAllCompResults(CompareResult.stCRHeads.lpCRFileDeleted);
    FreeAllCompResults(CompareResult.stCRHeads.lpCRFileAdded);
    FreeAllCompResults(CompareResult.stCRHeads.lpCRFileModified);

    ZeroMemory(&CompareResult, sizeof(CompareResult));
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
        CompareResult.stcCompared.cKeys++;
        // Find a matching key for KC1
        for (lpKC2 = lpStartKC2; NULL != lpKC2; lpKC2 = lpKC2->lpBrotherKC) {
            // skip KC2 if already matched
            if (NOMATCH != lpKC2->fKeyMatch) {
                continue;
            }
            // skip KC2 if names do *not* match (ATTENTION: test for match, THEN negate)
            if (!(
                        (lpKC1->lpszKeyName == lpKC2->lpszKeyName)
                        || ((NULL != lpKC1->lpszKeyName) && (NULL != lpKC2->lpszKeyName) && (0 == _tcscmp(lpKC1->lpszKeyName, lpKC2->lpszKeyName)))  // TODO: case-insensitive compare?
                    )) {
                continue;
            }

            // Same key name of KC1 found in KC2! Mark KC2 as matched to skip it for the next KC1, then compare their values and sub keys!
            lpKC2->fKeyMatch = ISMATCH;

            // Extra local block to reduce stack usage due to recursive calls
            {
                LPVALUECONTENT lpVC1;
                LPVALUECONTENT lpVC2;

                // Compare values
                for (lpVC1 = lpKC1->lpFirstVC; NULL != lpVC1; lpVC1 = lpVC1->lpBrotherVC) {
                    CompareResult.stcCompared.cValues++;
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
                                    || ((NULL != lpVC1->lpszValueName) && (NULL != lpVC2->lpszValueName) && (0 == _tcscmp(lpVC1->lpszValueName, lpVC2->lpszValueName)))  // TODO: case-insensitive compare?
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
                            CompareResult.stcChanged.cValues++;
                            CompareResult.stcModified.cValues++;
                            CreateNewResult(VALMODI, lpVC1, lpVC2);
                        }
                        break;
                    }
                    if (NULL == lpVC2) {
                        // VC1 has no match in KC2, so VC1 is a deleted value
                        CompareResult.stcChanged.cValues++;
                        CompareResult.stcDeleted.cValues++;
                        CreateNewResult(VALDEL, lpVC1, NULL);
                    }
                }
                // After looping all values of KC1, do an extra loop over all KC2 values and check previously set match flags to determine added values
                for (lpVC2 = lpKC2->lpFirstVC; NULL != lpVC2; lpVC2 = lpVC2->lpBrotherVC) {
                    // skip VC2 if already matched
                    if (NOMATCH != lpVC2->fValueMatch) {
                        continue;
                    }

                    CompareResult.stcCompared.cValues++;

                    // VC2 has no match in KC1, so VC2 is an added value
                    CompareResult.stcChanged.cValues++;
                    CompareResult.stcAdded.cValues++;
                    CreateNewResult(VALADD, NULL, lpVC2);
                }
            }  // End of extra local block

            // Compare sub keys if any
            if ((NULL != lpKC1->lpFirstSubKC) || (NULL != lpKC2->lpFirstSubKC)) {
                CompareRegKeys(lpKC1->lpFirstSubKC, lpKC2->lpFirstSubKC);
            }
            break;
        }
        if (NULL == lpKC2) {
            // KC1 has no matching KC2, so KC1 is a deleted key
            CompareResult.stcChanged.cKeys++;
            CompareResult.stcDeleted.cKeys++;
            CreateNewResult(KEYDEL, lpKC1, NULL);
            // Extra local block to reduce stack usage due to recursive calls
            {
                LPVALUECONTENT lpVC1;

                for (lpVC1 = lpKC1->lpFirstVC; NULL != lpVC1; lpVC1 = lpVC1->lpBrotherVC) {
                    CompareResult.stcCompared.cValues++;
                    CompareResult.stcChanged.cValues++;
                    CompareResult.stcDeleted.cValues++;
                    CreateNewResult(VALDEL, lpVC1, NULL);
                }
            }  // End of extra local block

            // "Compare"/Log sub keys if any
            if (NULL != lpKC1->lpFirstSubKC) {
                CompareRegKeys(lpKC1->lpFirstSubKC, NULL);
            }
        }
    }
    // After looping all KC1 keys, do an extra loop over all KC2 keys and check previously set match flags to determine added keys
    for (lpKC2 = lpStartKC2; NULL != lpKC2; lpKC2 = lpKC2->lpBrotherKC) {
        // skip KC2 if already matched
        if (NOMATCH != lpKC2->fKeyMatch) {
            continue;
        }

        // KC2 has no matching KC1, so KC2 is an added key
        CompareResult.stcCompared.cKeys++;
        CompareResult.stcChanged.cKeys++;
        CompareResult.stcAdded.cKeys++;
        CreateNewResult(KEYADD, NULL, lpKC2);
        // Extra local block to reduce stack usage due to recursive calls
        {
            LPVALUECONTENT lpVC2;

            for (lpVC2 = lpKC2->lpFirstVC; NULL != lpVC2; lpVC2 = lpVC2->lpBrotherVC) {
                CompareResult.stcCompared.cValues++;
                CompareResult.stcChanged.cValues++;
                CompareResult.stcAdded.cValues++;
                CreateNewResult(VALADD, NULL, lpVC2);
            }
        }  // End of extra local block

        // "Compare"/Log sub keys if any
        if (NULL != lpKC2->lpFirstSubKC) {
            CompareRegKeys(NULL, lpKC2->lpFirstSubKC);
        }
    }

    // Update counters display
    nCurrentTime = GetTickCount();
    if (REFRESHINTERVAL < (nCurrentTime - nLastTime)) {
        UI_UpdateCounters(asLangTexts[iszTextKey].lpszText, asLangTexts[iszTextValue].lpszText, CompareResult.stcCompared.cKeys, CompareResult.stcCompared.cValues);
    }
}


//------------------------------------------------------------
// Routine to call registry/file comparison engine
//------------------------------------------------------------
VOID CompareShots(VOID)
{
    if (!DirChainMatch(Shot1.lpHF, Shot2.lpHF)) {
        MessageBox(hWnd, TEXT("Found two shots with different DIR chain! (or with different order)\r\nYou can continue, but file comparison result would be abnormal!"), asLangTexts[iszTextWarning].lpszText, MB_ICONWARNING);  //TODO: I18N, create text index and translate
    }

    // Initialize result and markers
    cEnd = 0;
    FreeCompareResult();
    CompareResult.lpShot1 = &Shot1;
    CompareResult.lpShot2 = &Shot2;
    ClearRegKeyMatchFlags(Shot1.lpHKLM);
    ClearRegKeyMatchFlags(Shot2.lpHKLM);
    ClearRegKeyMatchFlags(Shot1.lpHKU);
    ClearRegKeyMatchFlags(Shot2.lpHKU);
    ClearHeadFileMatchFlags(Shot1.lpHF);
    ClearHeadFileMatchFlags(Shot2.lpHF);

    // Setup GUI for comparing...
    UI_InitCounters();

    // Compare HKLM
    if ((NULL != CompareResult.lpShot1->lpHKLM) || (NULL != CompareResult.lpShot2->lpHKLM)) {
        CompareRegKeys(CompareResult.lpShot1->lpHKLM, CompareResult.lpShot2->lpHKLM);

        // Update counters display (keys/values final)
        nCurrentTime = GetTickCount();
        UI_UpdateCounters(asLangTexts[iszTextKey].lpszText, asLangTexts[iszTextValue].lpszText, CompareResult.stcCompared.cKeys, CompareResult.stcCompared.cValues);
    }

    // Compare HKU
    if ((NULL != CompareResult.lpShot1->lpHKU) || (NULL != CompareResult.lpShot2->lpHKU)) {
        CompareRegKeys(CompareResult.lpShot1->lpHKU, CompareResult.lpShot2->lpHKU);

        // Update counters display (keys/values final)
        nCurrentTime = GetTickCount();
        UI_UpdateCounters(asLangTexts[iszTextKey].lpszText, asLangTexts[iszTextValue].lpszText, CompareResult.stcCompared.cKeys, CompareResult.stcCompared.cValues);
    }

    // Compare HEADFILEs v1.8.1
    if ((NULL != CompareResult.lpShot1->lpHF) || (NULL != CompareResult.lpShot2->lpHF)) {
        CompareHeadFiles(CompareResult.lpShot1->lpHF, CompareResult.lpShot2->lpHF);

        // Update counters display (dirs/files final)
        nCurrentTime = GetTickCount();
        UI_UpdateCounters(asLangTexts[iszTextDir].lpszText, asLangTexts[iszTextFile].lpszText, CompareResult.stcCompared.cDirs, CompareResult.stcCompared.cFiles);
    }

    // Get total count of all items
    CompareResult.stcCompared.cAll = CompareResult.stcCompared.cKeys
                                     + CompareResult.stcCompared.cValues
                                     + CompareResult.stcCompared.cDirs
                                     + CompareResult.stcCompared.cFiles;
    CompareResult.stcChanged.cAll = CompareResult.stcChanged.cKeys
                                    + CompareResult.stcChanged.cValues
                                    + CompareResult.stcChanged.cDirs
                                    + CompareResult.stcChanged.cFiles;
    CompareResult.stcDeleted.cAll = CompareResult.stcDeleted.cKeys
                                    + CompareResult.stcDeleted.cValues
                                    + CompareResult.stcDeleted.cDirs
                                    + CompareResult.stcDeleted.cFiles;
    CompareResult.stcAdded.cAll = CompareResult.stcAdded.cKeys
                                  + CompareResult.stcAdded.cValues
                                  + CompareResult.stcAdded.cDirs
                                  + CompareResult.stcAdded.cFiles;
    CompareResult.stcModified.cAll = CompareResult.stcModified.cKeys
                                     + CompareResult.stcModified.cValues
                                     + CompareResult.stcModified.cDirs
                                     + CompareResult.stcModified.cFiles;
    CompareResult.fFilled = TRUE;

    UI_ShowHideCounters(SW_HIDE);
}


//------------------------------------------------------------
// Routine to output comparison result
//------------------------------------------------------------
BOOL OutputComparisonResult(VOID)
{
    BOOL   fAsHTML;
    LPTSTR lpszBuffer;
    LPTSTR lpszExtension;
    LPTSTR lpszDestFileName;
    DWORD  nBufferSize = 2048;
    size_t cchString;

    // Setup GUI for saving...
    cEnd = CompareResult.stcChanged.cAll;
    UI_InitProgressBar();

    if (1 == SendMessage(GetDlgItem(hWnd, IDC_RADIO1), BM_GETCHECK, (WPARAM)0, (LPARAM)0)) {
        fAsHTML = FALSE;
        lpszExtension = TEXT(".txt");
    } else {
        fAsHTML = TRUE;
        lpszExtension = TEXT(".htm");
    }

    lpszDestFileName = MYALLOC0(EXTDIRLEN * sizeof(TCHAR));
    lpszBuffer = MYALLOC0(nBufferSize * sizeof(TCHAR)); // nBufferSize must > commentlength + 10 .txt 0000
    GetDlgItemText(hWnd, IDC_EDITCOMMENT, lpszBuffer, COMMENTLENGTH);  // length incl. NULL character
    GetDlgItemText(hWnd, IDC_EDITPATH, lpszOutputPath, MAX_PATH);  // length incl. NULL character

    cchString = _tcslen(lpszOutputPath);

    if ((0 < cchString) && ((TCHAR)'\\' != *(lpszOutputPath + cchString - 1))) {
        *(lpszOutputPath + cchString) = (TCHAR)'\\';
        *(lpszOutputPath + cchString + 1) = (TCHAR)'\0';  // bug found by "itschy" <itschy@lycos.de> 1.61d->1.61e
        cchString++;
    }
    _tcscpy(lpszDestFileName, lpszOutputPath);

    if (ReplaceInvalidFileNameChars(lpszBuffer)) {
        _tcscat(lpszDestFileName, lpszBuffer);
    } else {
        _tcscat(lpszDestFileName, lpszResultFileBaseName);
    }

    cchString = _tcslen(lpszDestFileName);
    _tcscat(lpszDestFileName, lpszExtension);
    hFile = CreateFile(lpszDestFileName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (INVALID_HANDLE_VALUE == hFile) {
        DWORD filetail = 0;

        for (filetail = 0; MAXAMOUNTOFFILE > filetail; filetail++) {
            _sntprintf(lpszDestFileName + cchString, 6, TEXT("_%04u\0"), filetail);
            //*(lpszDestFileName+cchString + 5) = 0x00;
            _tcscpy(lpszDestFileName + cchString + 5, lpszExtension);

            hFile = CreateFile(lpszDestFileName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
            if (INVALID_HANDLE_VALUE == hFile) {
                if (ERROR_FILE_EXISTS == GetLastError()) {  // My God! I use stupid ERROR_ALREADY_EXISTS first!!
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

    if (fAsHTML) {
        WriteHTMLBegin();
    } else {
        WriteFile(hFile, lpszProgramName, (DWORD)(_tcslen(lpszProgramName) * sizeof(TCHAR)), &NBW, NULL);
        WriteFile(hFile, lpszCRLF, (DWORD)(_tcslen(lpszCRLF) * sizeof(TCHAR)), &NBW, NULL);
    }

    //_asm int 3;
    GetDlgItemText(hWnd, IDC_EDITCOMMENT, lpszBuffer, COMMENTLENGTH);  // length incl. NULL character
    WriteTitle(asLangTexts[iszTextComments].lpszText, lpszBuffer, fAsHTML);

    _sntprintf(lpszBuffer, nBufferSize, TEXT("%04d-%02d-%02d %02d:%02d:%02d, %04d-%02d-%02d %02d:%02d:%02d\0"),
               CompareResult.lpShot1->systemtime.wYear, CompareResult.lpShot1->systemtime.wMonth, CompareResult.lpShot1->systemtime.wDay,
               CompareResult.lpShot1->systemtime.wHour, CompareResult.lpShot1->systemtime.wMinute, CompareResult.lpShot1->systemtime.wSecond,
               CompareResult.lpShot2->systemtime.wYear, CompareResult.lpShot2->systemtime.wMonth, CompareResult.lpShot2->systemtime.wDay,
               CompareResult.lpShot2->systemtime.wHour, CompareResult.lpShot2->systemtime.wMinute, CompareResult.lpShot2->systemtime.wSecond);
    lpszBuffer[nBufferSize - 1] = (TCHAR)'\0'; // saftey NULL char

    WriteTitle(asLangTexts[iszTextDateTime].lpszText, lpszBuffer, fAsHTML);

    lpszBuffer[0] = (TCHAR)'\0';
    if (NULL != CompareResult.lpShot1->lpszComputerName) {
        _tcscpy(lpszBuffer, CompareResult.lpShot1->lpszComputerName);
    }
    _tcscat(lpszBuffer, TEXT(", "));
    if (NULL != CompareResult.lpShot2->lpszComputerName) {
        _tcscat(lpszBuffer, CompareResult.lpShot2->lpszComputerName);
    }
    WriteTitle(asLangTexts[iszTextComputer].lpszText, lpszBuffer, fAsHTML);

    lpszBuffer[0] = (TCHAR)'\0';
    if (NULL != CompareResult.lpShot1->lpszUserName) {
        _tcscpy(lpszBuffer, CompareResult.lpShot1->lpszUserName);
    }
    _tcscat(lpszBuffer, TEXT(", "));
    if (NULL != CompareResult.lpShot2->lpszUserName) {
        _tcscat(lpszBuffer, CompareResult.lpShot2->lpszUserName);
    }
    WriteTitle(asLangTexts[iszTextUsername].lpszText, lpszBuffer, fAsHTML);

    MYFREE(lpszBuffer);

    // Write keydel part
    if (0 != CompareResult.stcDeleted.cKeys) {
        WriteTableHead(asLangTexts[iszTextKeyDel].lpszText, CompareResult.stcDeleted.cKeys, fAsHTML);
        WritePart(KEYDEL, CompareResult.stCRHeads.lpCRKeyDeleted, fAsHTML, FALSE);
    }
    // Write keyadd part
    if (0 != CompareResult.stcAdded.cKeys) {
        WriteTableHead(asLangTexts[iszTextKeyAdd].lpszText, CompareResult.stcAdded.cKeys, fAsHTML);
        WritePart(KEYADD, CompareResult.stCRHeads.lpCRKeyAdded, fAsHTML, FALSE);
    }
    // Write valdel part
    if (0 != CompareResult.stcDeleted.cValues) {
        WriteTableHead(asLangTexts[iszTextValDel].lpszText, CompareResult.stcDeleted.cValues, fAsHTML);
        WritePart(VALDEL, CompareResult.stCRHeads.lpCRValDeleted, fAsHTML, FALSE);
    }
    // Write valadd part
    if (0 != CompareResult.stcAdded.cValues) {
        WriteTableHead(asLangTexts[iszTextValAdd].lpszText, CompareResult.stcAdded.cValues, fAsHTML);
        WritePart(VALADD, CompareResult.stCRHeads.lpCRValAdded, fAsHTML, FALSE);
    }
    // Write valmodi part
    if (0 != CompareResult.stcModified.cValues) {
        WriteTableHead(asLangTexts[iszTextValModi].lpszText, CompareResult.stcModified.cValues, fAsHTML);
        WritePart(VALMODI, CompareResult.stCRHeads.lpCRValModified, fAsHTML, TRUE);
    }
    // Write directory del part
    if (0 != CompareResult.stcDeleted.cDirs) {
        WriteTableHead(asLangTexts[iszTextDirDel].lpszText, CompareResult.stcDeleted.cDirs, fAsHTML);
        WritePart(DIRDEL, CompareResult.stCRHeads.lpCRDirDeleted, fAsHTML, FALSE);
    }
    // Write directory add part
    if (0 != CompareResult.stcAdded.cDirs) {
        WriteTableHead(asLangTexts[iszTextDirAdd].lpszText, CompareResult.stcAdded.cDirs, fAsHTML);
        WritePart(DIRADD, CompareResult.stCRHeads.lpCRDirAdded, fAsHTML, FALSE);
    }
    // Write directory modi part
    if (0 != CompareResult.stcModified.cDirs) {
        WriteTableHead(asLangTexts[iszTextDirModi].lpszText, CompareResult.stcModified.cDirs, fAsHTML);
        WritePart(DIRMODI, CompareResult.stCRHeads.lpCRDirModified, fAsHTML, FALSE);
    }
    // Write file del part
    if (0 != CompareResult.stcDeleted.cFiles) {
        WriteTableHead(asLangTexts[iszTextFileDel].lpszText, CompareResult.stcDeleted.cFiles, fAsHTML);
        WritePart(FILEDEL, CompareResult.stCRHeads.lpCRFileDeleted, fAsHTML, FALSE);
    }
    // Write file add part
    if (0 != CompareResult.stcAdded.cFiles) {
        WriteTableHead(asLangTexts[iszTextFileAdd].lpszText, CompareResult.stcAdded.cFiles, fAsHTML);
        WritePart(FILEADD, CompareResult.stCRHeads.lpCRFileAdded, fAsHTML, FALSE);
    }
    // Write file modi part
    if (0 != CompareResult.stcModified.cFiles) {
        WriteTableHead(asLangTexts[iszTextFileModi].lpszText, CompareResult.stcModified.cFiles, fAsHTML);
        WritePart(FILEMODI, CompareResult.stCRHeads.lpCRFileModified, fAsHTML, FALSE);
    }

    if (fAsHTML) {
        WriteHTML_BR();
    }
    WriteTableHead(asLangTexts[iszTextTotal].lpszText, CompareResult.stcChanged.cAll, fAsHTML);
    if (fAsHTML) {
        WriteHTMLEnd();
    }

    // Close file
    CloseHandle(hFile);

    if (32 >= (size_t)ShellExecute(hWnd, TEXT("open"), lpszDestFileName, NULL, NULL, SW_SHOW)) {
        ErrMsg(asLangTexts[iszTextErrorExecViewer].lpszText);
    }
    MYFREE(lpszDestFileName);

    UI_ShowHideProgressBar(SW_HIDE);

    return TRUE;
}


// ----------------------------------------------------------------------
// Clear comparison match flags of registry keys and values
// ----------------------------------------------------------------------
VOID ClearRegKeyMatchFlags(LPKEYCONTENT lpKC)
{
    LPVALUECONTENT lpVC;

    for (; NULL != lpKC; lpKC = lpKC->lpBrotherKC) {
        lpKC->fKeyMatch = NOMATCH;
        for (lpVC = lpKC->lpFirstVC; NULL != lpVC; lpVC = lpVC->lpBrotherVC) {
            lpVC->fValueMatch = NOMATCH;
        }
        ClearRegKeyMatchFlags(lpKC->lpFirstSubKC);
    }
}


// ----------------------------------------------------------------------
// Free all registry values
// ----------------------------------------------------------------------
VOID FreeAllValueContents(LPVALUECONTENT lpVC)
{
    LPVALUECONTENT lpBrotherVC;

    for (; NULL != lpVC; lpVC = lpBrotherVC) {
        // Save pointer in local variable
        lpBrotherVC = lpVC->lpBrotherVC;

        // Increase count
        cCurrent++;

        // Free value name
        if (NULL != lpVC->lpszValueName) {
            MYFREE(lpVC->lpszValueName);
        }

        // Free value data
        if (NULL != lpVC->lpValueData) {
            MYFREE(lpVC->lpValueData);
        }

        // Free entry itself
        MYFREE(lpVC);
    }
}


// ----------------------------------------------------------------------
// Free all registry keys and values
// ----------------------------------------------------------------------
VOID FreeAllKeyContents(LPKEYCONTENT lpKC)
{
    LPKEYCONTENT lpBrotherKC;

    for (; NULL != lpKC; lpKC = lpBrotherKC) {
        // Save pointer in local variable
        lpBrotherKC = lpKC->lpBrotherKC;

        // Increase count
        cCurrent++;

        // Free key name
        if (NULL != lpKC->lpszKeyName) {
            if ((NULL != lpKC->lpFatherKC)  // only the top KC can have HKLM/HKU, so ignore the sub KCs
                    || ((lpKC->lpszKeyName != lpszHKLMShort)
                        && (lpKC->lpszKeyName != lpszHKLMLong)
                        && (lpKC->lpszKeyName != lpszHKUShort)
                        && (lpKC->lpszKeyName != lpszHKULong))) {
                MYFREE(lpKC->lpszKeyName);
            }
        }

        // If the entry has values, then do a call for the first value
        if (NULL != lpKC->lpFirstVC) {
            FreeAllValueContents(lpKC->lpFirstVC);
        }

        // Update progress bar display
        if (0 != cEnd) {
            nCurrentTime = GetTickCount();
            if (REFRESHINTERVAL < (nCurrentTime - nLastTime)) {
                UI_UpdateProgressBar();
            }
        }

        // If the entry has childs, then do a recursive call for the first child
        if (NULL != lpKC->lpFirstSubKC) {
            FreeAllKeyContents(lpKC->lpFirstSubKC);
        }

        // Free entry itself
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

    if (NULL != lpShot->lpHKLM) {
        FreeAllKeyContents(lpShot->lpHKLM);
    }

    if (NULL != lpShot->lpHKU) {
        FreeAllKeyContents(lpShot->lpHKU);
    }

    if (NULL != lpShot->lpHF) {
        FreeAllHeadFiles(lpShot->lpHF);
    }

    ZeroMemory(lpShot, sizeof(REGSHOT));
}


// ----------------------------------------------------------------------
// Get registry snap shot
// ----------------------------------------------------------------------
LPKEYCONTENT GetRegistrySnap(LPREGSHOT lpShot, HKEY hRegKey, LPTSTR lpszRegKeyName, LPKEYCONTENT lpFatherKC, LPKEYCONTENT *lplpCaller)
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

        // Set father of current key
        lpKC->lpFatherKC = lpFatherKC;

        // Set key name
        lpKC->lpszKeyName = lpszRegKeyName;
        lpKC->cchKeyName = _tcslen(lpKC->lpszKeyName);

        // Check if key is to be excluded
        if (NULL != lprgszRegSkipStrings[0]) {  // only if there is something to exclude
            if ((NULL != lpKC->lpszKeyName) && (IsInSkipList(lpKC->lpszKeyName, lprgszRegSkipStrings))) {
                FreeAllKeyContents(lpKC);
                return NULL;
            }

            lpszFullName = GetWholeKeyName(lpKC, FALSE);
            if (IsInSkipList(lpszFullName, lprgszRegSkipStrings)) {
                MYFREE(lpszFullName);
                FreeAllKeyContents(lpKC);
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
            // TODO: process/protocol issue in some way, do not silently ignore it (at least in Debug builds)
            FreeAllKeyContents(lpKC);
            return NULL;
        }

        // Copy pointer to current key into caller's pointer
        if (NULL != lplpCaller) {
            *lplpCaller = lpKC;
        }

        // Increase key count
        lpShot->stCounts.cKeys++;

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
                    // TODO: process/protocol issue in some way, do not silently ignore it (at least in Debug builds)
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
                    if ((NULL != lpVC->lpszValueName) && (IsInSkipList(lpVC->lpszValueName, lprgszRegSkipStrings))) {
                        FreeAllValueContents(lpVC);
                        continue;  // ignore this entry and continue with next brother value
                    }

                    lpszFullName = GetWholeValueName(lpVC, FALSE);
                    if (IsInSkipList(lpszFullName, lprgszRegSkipStrings)) {
                        MYFREE(lpszFullName);
                        FreeAllValueContents(lpVC);
                        continue;  // ignore this entry and continue with next brother value
                    }
                    MYFREE(lpszFullName);
                }

                // Copy pointer to current value into previous value's next value pointer
                if (NULL != lplpVCPrev) {
                    *lplpVCPrev = lpVC;
                }

                // Increase value count
                lpShot->stCounts.cValues++;

                // Copy value meta data
                lpVC->nTypeCode = nValueType;
                lpVC->cbData = cbValueData;

                // Copy value data
                if (0 < cbValueData) {  // otherwise leave it NULL
                    lpVC->lpValueData = MYALLOC(cbValueData);
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

                // Set "lpBrotherVC" pointer for storing the next brother's pointer
                lplpVCPrev = &lpVC->lpBrotherVC;
            }
        }
    }  // End of extra local block

    // Update counters display
    nCurrentTime = GetTickCount();
    if (REFRESHINTERVAL < (nCurrentTime - nLastTime)) {
        UI_UpdateCounters(asLangTexts[iszTextKey].lpszText, asLangTexts[iszTextValue].lpszText, lpShot->stCounts.cKeys, lpShot->stCounts.cValues);
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
                    // TODO: process/protocol issue in some way, do not silently ignore it (at least in Debug builds)
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
                // TODO: process/protocol issue in some way, do not silently ignore it (at least in Debug builds)
                //       e.g. when ERROR_ACCESS_DENIED then at least add key itself to the list
                MYFREE(lpszRegSubKeyName);
                continue;
            }

            lpKCSub = GetRegistrySnap(lpShot, hRegSubKey, lpszRegSubKeyName, lpKC, lplpKCPrev);
            RegCloseKey(hRegSubKey);

            // Set "lpBrotherKC" pointer for storing the next brother's pointer
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

    // Clear shot
    cEnd = 0;
    FreeShot(lpShot);

    // Setup GUI for shot...
    UI_InitCounters();

    // New temporary buffers
    lpStringBuffer = NULL;
    lpDataBuffer = NULL;

    // Set computer name
    lpShot->lpszComputerName = MYALLOC0((MAX_COMPUTERNAME_LENGTH + 1) * sizeof(TCHAR));
    cchString = MAX_COMPUTERNAME_LENGTH + 1;
    GetComputerName(lpShot->lpszComputerName, &cchString);   // in TCHARs; in *including* and out *excluding* NULL char

    // Set user name
    lpShot->lpszUserName = MYALLOC0((UNLEN + 1) * sizeof(TCHAR));
    cchString = UNLEN + 1;
    GetUserName(lpShot->lpszUserName, &cchString);   // in TCHARs; in and out including NULL char

    // Set current system time
    GetSystemTime(&lpShot->systemtime);

    // Take HKLM registry shot
    GetRegistrySnap(lpShot, HKEY_LOCAL_MACHINE, lpszHKLMShort, NULL, &lpShot->lpHKLM);

    // Update counters display (reg keys/values final)
    nCurrentTime = GetTickCount();
    UI_UpdateCounters(asLangTexts[iszTextKey].lpszText, asLangTexts[iszTextValue].lpszText, lpShot->stCounts.cKeys, lpShot->stCounts.cValues);

    // Take HKU registry shot
    GetRegistrySnap(lpShot, HKEY_USERS, lpszHKUShort, NULL, &lpShot->lpHKU);

    // Update counters display (reg keys/values final)
    nCurrentTime = GetTickCount();
    UI_UpdateCounters(asLangTexts[iszTextKey].lpszText, asLangTexts[iszTextValue].lpszText, lpShot->stCounts.cKeys, lpShot->stCounts.cValues);

    // Take file shot
    if (1 == SendMessage(GetDlgItem(hWnd, IDC_CHECKDIR), BM_GETCHECK, (WPARAM)0, (LPARAM)0)) {
        FileShot(lpShot);

        // Update counters display (dirs/files final)
        nCurrentTime = GetTickCount();
        UI_UpdateCounters(asLangTexts[iszTextDir].lpszText, asLangTexts[iszTextFile].lpszText, lpShot->stCounts.cDirs, lpShot->stCounts.cFiles);
    }

    // Get total count of all items
    lpShot->stCounts.cAll = lpShot->stCounts.cKeys
                            + lpShot->stCounts.cValues
                            + lpShot->stCounts.cDirs
                            + lpShot->stCounts.cFiles;

    // Set flags
    lpShot->fFilled = TRUE;
    lpShot->fLoaded = FALSE;

    UI_ShowHideCounters(SW_HIDE);

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
VOID SaveRegKeys(LPREGSHOT lpShot, LPKEYCONTENT lpKC, DWORD nFPFatherKey, DWORD nFPCaller)
{
    DWORD nFPKey;

    for (; NULL != lpKC; lpKC = lpKC->lpBrotherKC) {
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

                if ((fUseLongRegHead) && (0 == nFPFatherKey)) {
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

            // Increase count
            cCurrent++;

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

        // Save the values of the current key
        if (NULL != lpKC->lpFirstVC) {
            LPVALUECONTENT lpVC;
            DWORD nFPValue;

            // Write all values of key
            nFPCaller = nFPKey + offsetof(SAVEKEYCONTENT, ofsFirstValue);  // Write position of first value into key
            for (lpVC = lpKC->lpFirstVC; NULL != lpVC; lpVC = lpVC->lpBrotherVC) {
                // Get current file position
                // put in a separate var for later use
                nFPValue = SetFilePointer(hFileWholeReg, 0, NULL, FILE_CURRENT);

                // Write position of current reg value in caller's field
                if (0 < nFPCaller) {
                    SetFilePointer(hFileWholeReg, nFPCaller, NULL, FILE_BEGIN);
                    WriteFile(hFileWholeReg, &nFPValue, sizeof(nFPValue), &NBW, NULL);

                    SetFilePointer(hFileWholeReg, nFPValue, NULL, FILE_BEGIN);
                }

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

                // Increase count
                cCurrent++;

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

                // Set "ofsBrotherValue" position for storing the following brother's position
                nFPCaller = nFPValue + offsetof(SAVEVALUECONTENT, ofsBrotherValue);
            }
        }

        // Update progress bar display
        if (0 != cEnd) {
            nCurrentTime = GetTickCount();
            if (REFRESHINTERVAL < (nCurrentTime - nLastTime)) {
                UI_UpdateProgressBar();
            }
        }

        // ATTENTION!!! sKC will be INVALID from this point on, due to recursive calls
        // If the entry has childs, then do a recursive call for the first child
        // Pass this entry as father and "ofsFirstSubKey" position for storing the first child's position
        if (NULL != lpKC->lpFirstSubKC) {
            SaveRegKeys(lpShot, lpKC->lpFirstSubKC, nFPKey, nFPKey + offsetof(SAVEKEYCONTENT, ofsFirstSubKey));
        }

        // Set "ofsBrotherKey" position for storing the following brother's position
        nFPCaller = nFPKey + offsetof(SAVEKEYCONTENT, ofsBrotherKey);
    }
}


// ----------------------------------------------------------------------
// Save registry and files to HIVE file
// ----------------------------------------------------------------------
VOID SaveShot(LPREGSHOT lpShot)
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
    cEnd = lpShot->stCounts.cAll;
    UI_InitProgressBar();

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
        SaveRegKeys(lpShot, lpShot->lpHKLM, 0, offsetof(FILEHEADER, ofsHKLM));

        // Update progress bar display (keys/values final)
        if (0 != cEnd) {
            nCurrentTime = GetTickCount();
            UI_UpdateProgressBar();
        }
    }

    // Save HKU
    if (NULL != lpShot->lpHKU) {
        SaveRegKeys(lpShot, lpShot->lpHKU, 0, offsetof(FILEHEADER, ofsHKU));

        // Update progress bar display (keys/values final)
        if (0 != cEnd) {
            nCurrentTime = GetTickCount();
            UI_UpdateProgressBar();
        }
    }

    // Save HEADFILEs
    if (NULL != lpShot->lpHF) {
        SaveHeadFiles(lpShot, lpShot->lpHF, offsetof(FILEHEADER, ofsHF));

        // Update progress bar display (dirs/files final)
        if (0 != cEnd) {
            nCurrentTime = GetTickCount();
            UI_UpdateProgressBar();
        }
    }

    // Close file
    CloseHandle(hFileWholeReg);

    // Update progress bar display (final)
    if (0 != cEnd) {
        nCurrentTime = GetTickCount();
        UI_UpdateProgressBar();
    }

    UI_ShowHideProgressBar(SW_HIDE);

    // overwrite first letter of file name with NULL character to get path only, then create backup for initialization on next call
    *(opfn.lpstrFile + opfn.nFileOffset) = (TCHAR)'\0';  // TODO: check
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
                nCurrentSize += nAlign;
            }
        }

        *lpBuffer = MYALLOC(nCurrentSize);
    }

    return nCurrentSize;
}


// ----------------------------------------------------------------------
// Load registry key with values from HIVE file
// ----------------------------------------------------------------------
VOID LoadRegKeys(LPREGSHOT lpShot, DWORD ofsKey, LPKEYCONTENT lpFatherKC, LPKEYCONTENT *lplpCaller)
{
    LPKEYCONTENT lpKC;
    DWORD ofsBrotherKey;

    // Read all reg keys and their value and sub key contents
    for (; 0 != ofsKey; ofsKey = ofsBrotherKey) {
        // Copy SAVEKEYCONTENT to aligned memory block
        CopyMemory(&sKC, (lpFileBuffer + ofsKey), fileheader.nKCSize);

        // Save offsets in local variables due to recursive calls
        ofsBrotherKey = sKC.ofsBrotherKey;

        // Create new key content
        // put in a separate var for later use
        lpKC = MYALLOC0(sizeof(KEYCONTENT));

        // Set father of current key
        lpKC->lpFatherKC = lpFatherKC;

        // Copy key name
        if (fileextradata.bOldKCVersion) {  // old SBCS/MBCS version
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

            lpKC->lpszKeyName = MYALLOC(sKC.nKeyNameLen * sizeof(TCHAR));
            if (fileextradata.bSameCharSize) {
                _tcsncpy(lpKC->lpszKeyName, lpStringBuffer, sKC.nKeyNameLen);
            } else {
#ifdef _UNICODE
                MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, (LPCSTR)lpStringBuffer, -1, lpKC->lpszKeyName, sKC.nKeyNameLen);
#else
                WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK | WC_DEFAULTCHAR, (LPCWSTR)lpStringBuffer, -1, lpKC->lpszKeyName, sKC.nKeyNameLen, NULL, NULL);
#endif
            }
            lpKC->lpszKeyName[sKC.nKeyNameLen - 1] = (TCHAR)'\0';  // safety NULL char

            // Set key name length in chars
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

        // Check if key is to be excluded
        if (NULL != lprgszRegSkipStrings[0]) {  // only if there is something to exclude
            LPTSTR lpszFullName;

            if ((NULL != lpKC->lpszKeyName) && (IsInSkipList(lpKC->lpszKeyName, lprgszRegSkipStrings))) {
                FreeAllKeyContents(lpKC);
                continue;  // ignore this entry and continue with next brother key
            }

            lpszFullName = GetWholeKeyName(lpKC, FALSE);
            if (IsInSkipList(lpszFullName, lprgszRegSkipStrings)) {
                MYFREE(lpszFullName);
                FreeAllKeyContents(lpKC);
                continue;  // ignore this entry and continue with next brother key
            }
            MYFREE(lpszFullName);
        }

        // Copy pointer to current key into caller's pointer
        if (NULL != lplpCaller) {
            *lplpCaller = lpKC;
        }

        // Increase key count
        lpShot->stCounts.cKeys++;

        // Copy the value contents of the current key
        if (0 != sKC.ofsFirstValue) {
            LPVALUECONTENT lpVC;
            LPVALUECONTENT *lplpCallerVC;
            DWORD ofsValue;
            LPTSTR lpszFullName;

            // Read all values of key
            lplpCallerVC = &lpKC->lpFirstVC;
            for (ofsValue = sKC.ofsFirstValue; 0 != ofsValue; ofsValue = sVC.ofsBrotherValue) {
                // Copy SAVEVALUECONTENT to aligned memory block
                CopyMemory(&sVC, (lpFileBuffer + ofsValue), fileheader.nVCSize);

                // Create new value content
                lpVC = MYALLOC0(sizeof(VALUECONTENT));

                // Set father key of current value to current key
                lpVC->lpFatherKC = lpKC;

                // Copy value name
                if (fileextradata.bOldVCVersion) {  // old SBCS/MBCS version
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

                    lpVC->lpszValueName = MYALLOC(sVC.nValueNameLen * sizeof(TCHAR));
                    if (fileextradata.bSameCharSize) {
                        _tcsncpy(lpVC->lpszValueName, lpStringBuffer, sVC.nValueNameLen);
                    } else {
#ifdef _UNICODE
                        MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, (LPCSTR)lpStringBuffer, -1, lpVC->lpszValueName, sVC.nValueNameLen);
#else
                        WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK | WC_DEFAULTCHAR, (LPCWSTR)lpStringBuffer, -1, lpVC->lpszValueName, sVC.nValueNameLen, NULL, NULL);
#endif
                    }
                    lpVC->lpszValueName[sVC.nValueNameLen - 1] = (TCHAR)'\0';  // safety NULL char

                    // Set value name length in chars
                    lpVC->cchValueName = _tcslen(lpVC->lpszValueName);
                }

                // Check if value is to be excluded
                if (NULL != lprgszRegSkipStrings[0]) {  // only if there is something to exclude
                    if ((NULL != lpVC->lpszValueName) && (IsInSkipList(lpVC->lpszValueName, lprgszRegSkipStrings))) {
                        FreeAllValueContents(lpVC);
                        continue;  // ignore this entry and continue with next brother value
                    }

                    lpszFullName = GetWholeValueName(lpVC, FALSE);
                    if (IsInSkipList(lpszFullName, lprgszRegSkipStrings)) {
                        MYFREE(lpszFullName);
                        FreeAllValueContents(lpVC);
                        continue;  // ignore this entry and continue with next brother value
                    }
                    MYFREE(lpszFullName);
                }

                // Copy pointer to current value into previous value's next value pointer
                if (NULL != lplpCallerVC) {
                    *lplpCallerVC = lpVC;
                }

                // Increase value count
                lpShot->stCounts.cValues++;

                // Copy value meta data
                lpVC->nTypeCode = sVC.nTypeCode;
                lpVC->cbData = sVC.cbData;

                // Copy value data
                if (0 < sVC.cbData) {  // otherwise leave it NULL
                    lpVC->lpValueData = MYALLOC(sVC.cbData);
                    CopyMemory(lpVC->lpValueData, (lpFileBuffer + sVC.ofsValueData), sVC.cbData);
                }

                // Set "lpBrotherVC" pointer for storing the next brother's pointer
                lplpCallerVC = &lpVC->lpBrotherVC;
            }
        }

        // Update counters display
        nCurrentTime = GetTickCount();
        if (REFRESHINTERVAL < (nCurrentTime - nLastTime)) {
            UI_UpdateCounters(asLangTexts[iszTextKey].lpszText, asLangTexts[iszTextValue].lpszText, lpShot->stCounts.cKeys, lpShot->stCounts.cValues);
        }

        // ATTENTION!!! sKC will be INVALID from this point on, due to recursive calls
        // If the entry has childs, then do a recursive call for the first child
        // Pass this entry as father and "lpFirstSubKC" pointer for storing the first child's pointer
        if (0 != sKC.ofsFirstSubKey) {
            LoadRegKeys(lpShot, sKC.ofsFirstSubKey, lpKC, &lpKC->lpFirstSubKC);
        }

        // Set "lpBrotherKC" pointer for storing the next brother's pointer
        lplpCaller = &lpKC->lpBrotherKC;
    }
}


// ----------------------------------------------------------------------
// Load registry and files from HIVE file
// ----------------------------------------------------------------------
BOOL LoadShot(LPREGSHOT lpShot)
{
    OPENFILENAME opfn;
    TCHAR filepath[MAX_PATH];  // length incl. NULL character

    DWORD cbFileSize;
    DWORD cbFileRemain;
    DWORD cbReadSize;
    DWORD cbFileRead;

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

    cbFileSize = GetFileSize(hFileWholeReg, NULL);
    if (sizeof(fileheader) > cbFileSize) {
        CloseHandle(hFileWholeReg);
        ErrMsg(TEXT("wrong filesize"));  //TODO: I18N, create text index and translate
        return FALSE;
    }

    // Initialize file header
    ZeroMemory(&fileheader, sizeof(fileheader));
    ZeroMemory(&fileextradata, sizeof(fileextradata));

    // Read first part of file header from file (signature, nHeaderSize)
    ReadFile(hFileWholeReg, &fileheader, offsetof(FILEHEADER, ofsHKLM), &NBW, NULL);

    // Check for valid file signatures (SBCS/MBCS and UTF-16 signature)
    if ((0 != strncmp(szRegshotFileSignatureSBCS, fileheader.signature, MAX_SIGNATURE_LENGTH)) && (0 != strncmp(szRegshotFileSignatureUTF16, fileheader.signature, MAX_SIGNATURE_LENGTH))) {
        CloseHandle(hFileWholeReg);
        ErrMsg(TEXT("It is not a Regshot hive file!"));  //TODO: I18N, create text index and translate
        return FALSE;
    }

    // Check file signature for correct type (SBCS/MBCS or UTF-16)
    if (0 != strncmp(szRegshotFileSignature, fileheader.signature, MAX_SIGNATURE_LENGTH)) {
        CloseHandle(hFileWholeReg);
#ifdef _UNICODE
        ErrMsg(TEXT("It is not a Unicode Regshot hive file! Try the ANSI version with it."));  //TODO: I18N, create text index and translate
#else
        ErrMsg(TEXT("It is not an ANSI Regshot hive file! Try the Unicode version with it."));  //TODO: I18N, create text index and translate
#endif
        return FALSE;
    }

    // Clear shot
    cEnd = 0;
    FreeShot(lpShot);

    // Allocate memory to hold the complete file
    lpFileBuffer = MYALLOC(cbFileSize);

    // Setup GUI for loading...
    cEnd = cbFileSize;
    UI_InitProgressBar();

    // Read file blockwise for progress bar
    SetFilePointer(hFileWholeReg, 0, NULL, FILE_BEGIN);
    cbFileRemain = cbFileSize;  // 100% to go
    cbReadSize = REGSHOT_READ_BLOCK_SIZE;  // next block length to read
    for (cbFileRead = 0; 0 < cbFileRemain; cbFileRead += cbReadSize) {
        // If the rest is smaller than a block, then use the rest length
        if (REGSHOT_READ_BLOCK_SIZE > cbFileRemain) {
            cbReadSize = cbFileRemain;
        }

        // Read the next block
        ReadFile(hFileWholeReg, lpFileBuffer + cbFileRead, cbReadSize, &NBW, NULL);
        if (NBW != cbReadSize) {
            CloseHandle(hFileWholeReg);
            ErrMsg(TEXT("Reading ERROR!"));  //TODO: I18N, create text index and translate
            return FALSE;
        }

        // Determine how much to go, if zero leave the for-loop
        cbFileRemain -= cbReadSize;
        if (0 == cbFileRemain) {
            break;
        }

        // Update progress bar display
        if (0 != cEnd) {
            nCurrentTime = GetTickCount();
            if (REFRESHINTERVAL < (nCurrentTime - nLastTime)) {
                cCurrent = cbFileRead;
                UI_UpdateProgressBar();
            }
        }
    }

    // Close file
    CloseHandle(hFileWholeReg);

    // Update progress bar display (load final)
    if (0 != cEnd) {
        nCurrentTime = GetTickCount();
        UI_UpdateProgressBar();
    }

    cEnd = 0;
    UI_ShowHideProgressBar(SW_HIDE);

    // Setup GUI for parsing loaded file...
    UI_InitCounters();

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
    if (sizeof(TCHAR) == fileheader.nCharSize) {
        fileextradata.bSameCharSize = TRUE;
    } else {
        fileextradata.bSameCharSize = FALSE;
#ifdef _UNICODE
        if (1 == fileheader.nCharSize) {
            ErrMsg(TEXT("It is not a Unicode Regshot hive file! Try the ANSI version with it."));  //TODO: I18N, create text index and translate
        } else if (2 != fileheader.nCharSize) {
            ErrMsg(TEXT("Unknown character size! Maybe created by a newer Regshot version."));  //TODO: I18N, create text index and translate
        } else {
            ErrMsg(TEXT("Unsupported character size!"));  //TODO: I18N, create text index and translate
        }
#else
        ErrMsg(TEXT("It is not an ANSI Regshot hive file! Try the Unicode version with it."));  //TODO: I18N, create text index and translate
#endif
        if (NULL != lpFileBuffer) {
            MYFREE(lpFileBuffer);
            lpFileBuffer = NULL;
        }
        return FALSE;
    }

    // Check for incompatible endianness (known: Intel = Little Endian)
    if (FILEHEADER_ENDIANNESS_VALUE == fileheader.nEndianness) {
        fileextradata.bSameEndianness = TRUE;
    } else {
        fileextradata.bSameEndianness = FALSE;
#ifdef __LITTLE_ENDIAN__
        ErrMsg(TEXT("It is not a Little Endian Regshot hive file!"));  //TODO: I18N, create text index and translate
#else
        ErrMsg(TEXT("It is not a Big Endian Regshot hive file!"));  //TODO: I18N, create text index and translate
#endif
        if (NULL != lpFileBuffer) {
            MYFREE(lpFileBuffer);
            lpFileBuffer = NULL;
        }
        return FALSE;
    }

    if (KEYCONTENT_VERSION_2 > fileheader.nKCVersion) {  // old SBCS/MBCS version
        fileextradata.bOldKCVersion = TRUE;
    } else {
        fileextradata.bOldKCVersion = FALSE;
    }

    if (VALUECONTENT_VERSION_2 > fileheader.nVCVersion) {  // old SBCS/MBCS version
        fileextradata.bOldVCVersion = TRUE;
    } else {
        fileextradata.bOldVCVersion = FALSE;
    }

    if (FILECONTENT_VERSION_2 > fileheader.nFCVersion) {  // old SBCS/MBCS version
        fileextradata.bOldFCVersion = TRUE;
    } else {
        fileextradata.bOldFCVersion = FALSE;
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

            lpShot->lpszComputerName = MYALLOC(fileheader.nComputerNameLen * sizeof(TCHAR));
            if (fileextradata.bSameCharSize) {
                _tcsncpy(lpShot->lpszComputerName, lpStringBuffer, fileheader.nComputerNameLen);
            } else {
#ifdef _UNICODE
                MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, (LPCSTR)lpStringBuffer, -1, lpShot->lpszComputerName, fileheader.nComputerNameLen);
#else
                WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK | WC_DEFAULTCHAR, (LPCWSTR)lpStringBuffer, -1, lpShot->lpszComputerName, fileheader.nComputerNameLen, NULL, NULL);
#endif
            }
            lpShot->lpszComputerName[fileheader.nComputerNameLen - 1] = (TCHAR)'\0';  // safety NULL char
        }
    } else {  // old SBCS/MBCS version
        // Copy string to an aligned memory block
        nSourceSize = strnlen((const char *)&fileheader.computername, OLD_COMPUTERNAMELEN);
        if (0 < nSourceSize) {  // otherwise leave it NULL
            nStringBufferSize = AdjustBuffer(&lpStringBuffer, nStringBufferSize, (nSourceSize + 1), REGSHOT_BUFFER_BLOCK_BYTES);
            ZeroMemory(lpStringBuffer, nStringBufferSize);
            CopyMemory(lpStringBuffer, &fileheader.computername, nSourceSize);

            lpShot->lpszComputerName = MYALLOC((nSourceSize + 1) * sizeof(TCHAR));
#ifdef _UNICODE
            MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, (LPCSTR)lpStringBuffer, (int)(nSourceSize + 1), lpShot->lpszComputerName, (int)(nSourceSize + 1));
#else
            strncpy(lpShot->lpszComputerName, lpStringBuffer, nSourceSize);
#endif
            lpShot->lpszComputerName[nSourceSize] = (TCHAR)'\0';  // safety NULL char
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

            lpShot->lpszUserName = MYALLOC(fileheader.nUserNameLen * sizeof(TCHAR));
            if (fileextradata.bSameCharSize) {
                _tcsncpy(lpShot->lpszUserName, lpStringBuffer, fileheader.nUserNameLen);
            } else {
#ifdef _UNICODE
                MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, (LPCSTR)lpStringBuffer, -1, lpShot->lpszUserName, fileheader.nUserNameLen);
#else
                WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK | WC_DEFAULTCHAR, (LPCWSTR)lpStringBuffer, -1, lpShot->lpszUserName, fileheader.nUserNameLen, NULL, NULL);
#endif
            }
            lpShot->lpszUserName[fileheader.nUserNameLen - 1] = (TCHAR)'\0';  // safety NULL char
        }
    } else {  // old SBCS/MBCS version
        // Copy string to an aligned memory block
        nSourceSize = strnlen((const char *)&fileheader.username, OLD_COMPUTERNAMELEN);
        if (0 < nSourceSize) {  // otherwise leave it NULL
            nStringBufferSize = AdjustBuffer(&lpStringBuffer, nStringBufferSize, (nSourceSize + 1), REGSHOT_BUFFER_BLOCK_BYTES);
            ZeroMemory(lpStringBuffer, nStringBufferSize);
            CopyMemory(lpStringBuffer, &fileheader.username, nSourceSize);

            lpShot->lpszUserName = MYALLOC((nSourceSize + 1) * sizeof(TCHAR));
#ifdef _UNICODE
            MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, (LPCSTR)lpStringBuffer, (int)(nSourceSize + 1), lpShot->lpszUserName, (int)(nSourceSize + 1));
#else
            strncpy(lpShot->lpszUserName, lpStringBuffer, nSourceSize);
#endif
            lpShot->lpszUserName[nSourceSize] = (TCHAR)'\0';  // safety NULL char
        }
    }

    CopyMemory(&lpShot->systemtime, &fileheader.systemtime, sizeof(SYSTEMTIME));

    // Initialize save structures
    ZeroMemory(&sKC, sizeof(sKC));
    ZeroMemory(&sVC, sizeof(sVC));

    if (0 != fileheader.ofsHKLM) {
        LoadRegKeys(lpShot, fileheader.ofsHKLM, NULL, &lpShot->lpHKLM);

        // Update counters display (keys/values final)
        nCurrentTime = GetTickCount();
        UI_UpdateCounters(asLangTexts[iszTextKey].lpszText, asLangTexts[iszTextValue].lpszText, lpShot->stCounts.cKeys, lpShot->stCounts.cValues);
    }

    if (0 != fileheader.ofsHKU) {
        LoadRegKeys(lpShot, fileheader.ofsHKU, NULL, &lpShot->lpHKU);

        // Update counters display (keys/values final)
        nCurrentTime = GetTickCount();
        UI_UpdateCounters(asLangTexts[iszTextKey].lpszText, asLangTexts[iszTextValue].lpszText, lpShot->stCounts.cKeys, lpShot->stCounts.cValues);
    }

    if (0 != fileheader.ofsHF) {
        LoadHeadFiles(lpShot, fileheader.ofsHF, &lpShot->lpHF);

        // Update counters display (dirs/files final)
        nCurrentTime = GetTickCount();
        UI_UpdateCounters(asLangTexts[iszTextDir].lpszText, asLangTexts[iszTextFile].lpszText, lpShot->stCounts.cDirs, lpShot->stCounts.cFiles);
    }

    // Get total count of all items
    lpShot->stCounts.cAll = lpShot->stCounts.cKeys
                            + lpShot->stCounts.cValues
                            + lpShot->stCounts.cDirs
                            + lpShot->stCounts.cFiles;

    // Set GUI fields after loading...
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

    // Set flags
    lpShot->fFilled = TRUE;
    lpShot->fLoaded = TRUE;

    UI_ShowHideCounters(SW_HIDE);

    // overwrite first letter of file name with NULL character to get path only, then create backup for initialization on next call
    *(opfn.lpstrFile + opfn.nFileOffset) = 0x00;
    _tcscpy(lpszLastOpenDir, opfn.lpstrFile);

    return TRUE;
}


// ----------------------------------------------------------------------
// Display details of shot in message box
// ----------------------------------------------------------------------
VOID DisplayShotInfo(HWND hDlg, LPREGSHOT lpShot)
{
    LPTSTR lpszInfoBox;
    LPTSTR lpszComputerName;
    LPTSTR lpszUserName;
    LPTSTR lpszLoaded;
    LPTSTR lpszTitle;

    lpszComputerName = lpShot->lpszComputerName;
    if (NULL == lpszComputerName) {
        lpszComputerName = lpszEmpty;
    }
    lpszUserName = lpShot->lpszUserName;
    if (NULL == lpszUserName) {
        lpszUserName = lpszEmpty;
    }
    lpszLoaded = lpszEmpty;
    if (lpShot->fLoaded) {
        lpszLoaded = asLangTexts[iszTextLoadedFromFile].lpszText;
    }

    lpszInfoBox = MYALLOC0(SIZEOF_INFOBOX * sizeof(TCHAR));
    _sntprintf(lpszInfoBox, SIZEOF_INFOBOX, TEXT("%s %04d-%02d-%02d %02d:%02d:%02d\n%s %s\n%s %s\n%s %u\n%s %u\n%s %u\n%s %u\n%s\0"),
               asLangTexts[iszTextDateTime].lpszText,
               lpShot->systemtime.wYear, lpShot->systemtime.wMonth, lpShot->systemtime.wDay,
               lpShot->systemtime.wHour, lpShot->systemtime.wMinute, lpShot->systemtime.wSecond,
               asLangTexts[iszTextComputer].lpszText, lpszComputerName,
               asLangTexts[iszTextUsername].lpszText, lpszUserName,
               asLangTexts[iszTextKey].lpszText, lpShot->stCounts.cKeys,
               asLangTexts[iszTextValue].lpszText, lpShot->stCounts.cValues,
               asLangTexts[iszTextDir].lpszText, lpShot->stCounts.cDirs,
               asLangTexts[iszTextFile].lpszText, lpShot->stCounts.cFiles,
               lpszLoaded
              );
    lpszInfoBox[SIZEOF_INFOBOX - 1] = (TCHAR)'\0';  // safety NULL char

    lpszTitle = asLangTexts[iszTextMenuInfo].lpszText;
    if (&Shot1 == lpShot) {
        lpszTitle = asLangTexts[iszTextButtonShot1].lpszText;
    } else if (&Shot2 == lpShot) {
        lpszTitle = asLangTexts[iszTextButtonShot2].lpszText;
    }

    MessageBox(hDlg, lpszInfoBox, lpszTitle, MB_OK);
    MYFREE(lpszInfoBox);
}


// ----------------------------------------------------------------------
// Display details of comparison result in message box
// ----------------------------------------------------------------------
VOID DisplayResultInfo(HWND hDlg)
{
    LPTSTR lpszInfoBox;
    LPTSTR lpszTitle;

    lpszInfoBox = MYALLOC0(SIZEOF_INFOBOX * sizeof(TCHAR));
    _sntprintf(lpszInfoBox, SIZEOF_INFOBOX, TEXT("%s %u\n%s %u\n%s %u\n%s %u\n%s %u\n%s %u\n%s %u\n%s %u\n%s %u\n%s %u\n%s %u\n%s %u\n\0"),
               asLangTexts[iszTextKeyDel].lpszText, CompareResult.stcDeleted.cKeys,
               asLangTexts[iszTextKeyAdd].lpszText, CompareResult.stcAdded.cKeys,
               asLangTexts[iszTextValDel].lpszText, CompareResult.stcDeleted.cValues,
               asLangTexts[iszTextValAdd].lpszText, CompareResult.stcAdded.cValues,
               asLangTexts[iszTextValModi].lpszText, CompareResult.stcModified.cValues,
               asLangTexts[iszTextDirDel].lpszText, CompareResult.stcDeleted.cDirs,
               asLangTexts[iszTextDirAdd].lpszText, CompareResult.stcAdded.cDirs,
               asLangTexts[iszTextDirModi].lpszText, CompareResult.stcModified.cDirs,
               asLangTexts[iszTextFileDel].lpszText, CompareResult.stcDeleted.cFiles,
               asLangTexts[iszTextFileAdd].lpszText, CompareResult.stcAdded.cFiles,
               asLangTexts[iszTextFileModi].lpszText, CompareResult.stcModified.cFiles,
               asLangTexts[iszTextTotal].lpszText, CompareResult.stcChanged.cAll
              );
    lpszInfoBox[SIZEOF_INFOBOX - 1] = (TCHAR)'\0';  // safety NULL char

    lpszTitle = asLangTexts[iszTextButtonCompare].lpszText;

    MessageBox(hDlg, lpszInfoBox, lpszTitle, MB_OK);
    MYFREE(lpszInfoBox);
}


// ----------------------------------------------------------------------
// Swap shot 1 and shot 2
// ----------------------------------------------------------------------
VOID SwapShots(VOID)
{
    REGSHOT ShotTemp;

    memcpy(&ShotTemp, &Shot1, sizeof(Shot1));     // backup Shot1 in ShotTemp
    memcpy(&Shot1, &Shot2, sizeof(Shot2));        // copy Shot2 to Shot1
    memcpy(&Shot2, &ShotTemp, sizeof(ShotTemp));  // copy ShotTemp (Shot1) to Shot2
}


// ----------------------------------------------------------------------
// Check if shot 1 and shot 2 are in chronological order, otherwise prompt to swap
//   YES = swap, then proceed (returns TRUE)
//   NO = no swap, then proceed (returns TRUE)
//   CANCEL = no swap, then cancel functions (returns FALSE)
// ----------------------------------------------------------------------
BOOL CheckShotsChronology(HWND hDlg)
{
    FILETIME ftime1;
    FILETIME ftime2;
    LPTSTR lpszChronoBox;
    LPTSTR lpszTitle;
    int nDialogAnswer;

    // CANCEL if not both shots filled
    if ((!Shot1.fFilled) || (!Shot2.fFilled)) {
        return FALSE;
    }

    // Compare time stamps of shots
    SystemTimeToFileTime(&(Shot1.systemtime), &ftime1);
    SystemTimeToFileTime(&(Shot2.systemtime), &ftime2);
    if (0 >= CompareFileTime(&ftime1, &ftime2)) {
        return TRUE;
    }

    // Define texts of message box  //TODO: I18N, create text indexes and translate
    lpszChronoBox = MYALLOC0(SIZEOF_CHRONOBOX * sizeof(TCHAR));
    _sntprintf(lpszChronoBox, SIZEOF_CHRONOBOX, TEXT("%s\n%s %s %04d-%02d-%02d %02d:%02d:%02d\n%s %s %04d-%02d-%02d %02d:%02d:%02d\n%s\n\0"),
               asLangTexts[iszTextShotsNotChronological].lpszText,
               asLangTexts[iszTextButtonShot1].lpszText, asLangTexts[iszTextDateTime].lpszText,
               Shot1.systemtime.wYear, Shot1.systemtime.wMonth, Shot1.systemtime.wDay,
               Shot1.systemtime.wHour, Shot1.systemtime.wMinute, Shot1.systemtime.wSecond,
               asLangTexts[iszTextButtonShot2].lpszText, asLangTexts[iszTextDateTime].lpszText,
               Shot2.systemtime.wYear, Shot2.systemtime.wMonth, Shot2.systemtime.wDay,
               Shot2.systemtime.wHour, Shot2.systemtime.wMinute, Shot2.systemtime.wSecond,
               asLangTexts[iszTextQuestionSwapShots].lpszText
              );
    lpszChronoBox[SIZEOF_CHRONOBOX - 1] = (TCHAR)'\0';  // safety NULL char

    lpszTitle = MYALLOC0(SIZEOF_CHRONOBOX * sizeof(TCHAR));
    _sntprintf(lpszTitle, SIZEOF_CHRONOBOX, TEXT("%s %s\0"),
               asLangTexts[iszTextChronology].lpszText,
               asLangTexts[iszTextWarning].lpszText
              );
    lpszTitle[SIZEOF_CHRONOBOX - 1] = (TCHAR)'\0';  // safety NULL char

    nDialogAnswer = MessageBox(hDlg, lpszChronoBox, lpszTitle, MB_ICONWARNING | MB_YESNOCANCEL | MB_DEFBUTTON2);
    if (IDCANCEL == nDialogAnswer) {
        return FALSE;
    }

    if (IDYES == nDialogAnswer) {
        SwapShots();
        cEnd = CompareResult.stcChanged.cAll;
        UI_InitProgressBar();
        FreeCompareResult();
        if (0 != cEnd) {
            nCurrentTime = GetTickCount();
            UI_UpdateProgressBar();
        }
        ClearRegKeyMatchFlags(Shot1.lpHKLM);
        ClearRegKeyMatchFlags(Shot2.lpHKLM);
        ClearRegKeyMatchFlags(Shot1.lpHKU);
        ClearRegKeyMatchFlags(Shot2.lpHKU);
        ClearHeadFileMatchFlags(Shot1.lpHF);
        ClearHeadFileMatchFlags(Shot2.lpHF);
        UI_ShowHideProgressBar(SW_HIDE);
    }

    return TRUE;
}
