@echo off

@if NOT "%MOZILLA_SOURCE_DIR%" == "" goto dirSetOK
@echo [ghzil] Step 4: ERROR: RUN step1-setEnvironment.bat first!
@goto end
:dirSetOK

if "%1" == "FILE" goto file

set CMODE=NONE
set CREVERSE=NO
if "%1" == "CHECK" set CMODE=CHECK
if "%1" == "check" set CMODE=CHECK
if "%1" == "DIFF"  set CMODE=CHECKSHOWDIFF
if "%1" == "diff"  set CMODE=CHECKSHOWDIFF
if "%1" == "UPDATE" set CMODE=UPDATE
if "%1" == "update" set CMODE=UPDATE
if %CMODE%==NONE goto usage
shift
if "%1" == "REVERSE" set CREVERSE=YES
if "%1" == "reverse" set CREVERSE=YES
if "%1" == "REVERSE" shift
if "%1" == "reverse" shift
if NOT "%1"  == ""   goto argsOK
:usage
echo USAGE: %0 {CHECK or DIFF or UPDATE} [REVERSE] contribDir [contribSubdir]
set CMODE=
goto end

:argsOK
set CONTRIBDIR=%1
if NOT "%2" == "" set CONTRIBDIR=%CONTRIBDIR%\%2

echo CONTRIBDIR: %CONTRIBDIR%
echo GHDIR: %GHDIR%

echo.
set  GHTARGET=%GHDIR%
echo.
for /f "eol=; tokens=1,2" %%f in (%CONTRIBDIR%\contrib.lst) do @call contrib FILE %%f %GHTARGET%\%%g
set CONTRIBDIR=
set CREVERSE=
set CMODE=
goto end

:file
set srcfile=%CONTRIBDIR%\%2
set dstfile=%3
if %CREVERSE%==NO goto noreverse
echo REVERSE
set x=%srcfile%
set srcfile=%dstfile%
set dstfile=%x%
set x=
:noreverse
set diffOptions=-b -q
if %CMODE%==CHECKSHOWDIFF set diffOptions=-b
echo DIFF %diffOptions% %srcfile% %dstfile%
diff %diffOptions% %srcfile% %dstfile%
if ERRORLEVEL 1 goto differ
goto fileend
:differ
if %CMODE%==CHECK goto fileend
if %CMODE%==CHECKSHOWDIFF goto fileend
echo COPY %srcfile% to %dstfile%
copy %dstfile% %dstfile%.BAK > NUL
copy %srcfile% %dstfile%
touch %dstfile%
:fileend
set srcfile=
set dstfile=
echo.
goto end



:end

:EOF
