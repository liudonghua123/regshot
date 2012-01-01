@ECHO OFF
SETLOCAL

PUSHD %~dp0

AStyle.exe --version 1>&2 2>NUL

IF %ERRORLEVEL% NEQ 0 (
  ECHO.
  ECHO ERROR: Astyle wasn't found!
  ECHO Visit http://astyle.sourceforge.net/ for download and details.
  PAUSE
  EXIT /B
)

AStyle.exe -s4 --style=kr --indent-switches --indent-namespaces --add-brackets^
 --indent-col1-comments --pad-header --align-pointer=name --align-reference=name^
 --preserve-date --pad-oper --unpad-paren --recursive ..\src\*.c ..\src\*.h

IF %ERRORLEVEL% NEQ 0 (
  ECHO.
  ECHO ERROR: Something went wrong!
  PAUSE
)

POPD
PAUSE
ENDLOCAL
EXIT /B
