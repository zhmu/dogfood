@echo off
:: Copyright (C) 1996-2022 Free Software Foundation, Inc.
:: This file is part of GNU Make.
::
:: GNU Make is free software; you can redistribute it and/or modify it under
:: the terms of the GNU General Public License as published by the Free
:: Software Foundation; either version 3 of the License, or (at your option)
:: any later version.
::
:: GNU Make is distributed in the hope that it will be useful, but WITHOUT
:: ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
:: FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for.
:: more details.
::
:: You should have received a copy of the GNU General Public License along
:: with this program.  If not, see <https://www.gnu.org/licenses/>.

setlocal
if not "%RECURSEME%"=="%~0" (
    set "RECURSEME=%~0"
    %ComSpec% /s /c ""%~0" %*"
    goto :EOF
)

call :Reset

if "%1" == "-h" goto Usage
if "%1" == "--help" goto Usage

echo.
echo Creating GNU Make for Windows 9X/NT/2K/XP/Vista/7/8/10
echo.

set MAKE=gnumake
set GUILE=Y
set COMPILER=cl.exe
set O=obj
set ARCH=x64
set DEBUG=N
set DIRENT=Y
set VERBOSE=N

if exist maintMakefile (
    set MAINT=Y
) else (
    set MAINT=N
)

:ParseSW
if "%1" == "--verbose" goto SetVerbose
if "%1" == "--debug" goto SetDebug
if "%1" == "--without-guile" goto NoGuile
if "%1" == "--x86" goto Set32Bit
if "%1" == "gcc" goto SetCC
if "%1" == "tcc" goto SetTCC
if "%1" == "" goto DoneSW
goto Usage

:SetVerbose
set VERBOSE=Y
shift
goto ParseSW

:SetDebug
set DEBUG=Y
echo - Building without compiler optimizations
shift
goto ParseSW

:NoGuile
set GUILE=N
echo - Building without Guile
shift
goto ParseSW

:Set32Bit
set ARCH=x86
echo - Building 32bit GNU Make
shift
goto ParseSW

:SetCC
set COMPILER=gcc
set O=o
echo - Building with GCC
shift
goto ParseSW

:SetTCC
set COMPILER=tcc
set O=o
echo - Building with TinyC
shift
goto ParseSW

:DoneSW
if "%MAINT%" == "Y" echo - Enabling maintainer mode

if "%COMPILER%" == "gcc" goto FindGcc
if "%COMPILER%" == "tcc" goto FindTcc

:: Find a compiler.  Visual Studio requires a lot of effort to locate :-/.
call %COMPILER% >nul 2>&1
if not ERRORLEVEL 1 goto FoundMSVC

:: Visual Studio 17 and above provides the "vswhere" tool
call :FindVswhere
if ERRORLEVEL 1 goto LegacyVS

for /f "tokens=* usebackq" %%i in (`"%VSWHERE%" -latest -property installationPath`) do (
    set InstallPath=%%i
)
set "VSVARS=%InstallPath%\VC\Auxiliary\Build\vcvarsall.bat"
call :CheckMSVC
if not ERRORLEVEL 1 goto FoundMSVC

:: No "vswhere" or it can't find a compiler.  Go old-school.
:LegacyVS
set "VSVARS=%VS150COMNTOOLS%\..\..\VC\vcvarsall.bat"
call :CheckMSVC
if not ERRORLEVEL 1 goto FoundMSVC

set "VSVARS=%VS140COMNTOOLS%\..\..\VC\vcvarsall.bat"
call :CheckMSVC
if not ERRORLEVEL 1 goto FoundMSVC

set "VSVARS=%VS120COMNTOOLS%\..\..\VC\vcvarsall.bat"
call :CheckMSVC
if not ERRORLEVEL 1 goto FoundMSVC

set "VSVARS=%VS110COMNTOOLS%\..\..\VC\vcvarsall.bat"
call :CheckMSVC
if not ERRORLEVEL 1 goto FoundMSVC

set "VSVARS=%VS100COMNTOOLS%\..\..\VC\vcvarsall.bat"
call :CheckMSVC
if not ERRORLEVEL 1 goto FoundMSVC

set "VSVARS=%VS90COMNTOOLS%\..\..\VC\vcvarsall.bat"
call :CheckMSVC
if not ERRORLEVEL 1 goto FoundMSVC

set "VSVARS=%VS80COMNTOOLS%\..\..\VC\vcvarsall.bat"
call :CheckMSVC
if not ERRORLEVEL 1 goto FoundMSVC

set "VSVARS=%VS71COMNTOOLS%\..\..\VC\vcvarsall.bat"
call :CheckMSVC
if not ERRORLEVEL 1 goto FoundMSVC

set "VSVARS=%VS70COMNTOOLS%\..\..\VC\vcvarsall.bat"
call :CheckMSVC
if not ERRORLEVEL 1 goto FoundMSVC

set "VSVARS=%V6TOOLS%\VC98\Bin\vcvars32.bat"
call :CheckMSVC
if not ERRORLEVEL 1 goto FoundMSVC

set "VSVARS=%V6TOOLS%\VC97\Bin\vcvars32.bat"
call :CheckMSVC
if not ERRORLEVEL 1 goto FoundMSVC

set "VSVARS=%V5TOOLS%\VC\Bin\vcvars32.bat"
call :CheckMSVC
if not ERRORLEVEL 1 goto FoundMSVC

:: We did not find anything--fail
echo No MSVC compiler available.
echo Please run vcvarsall.bat and/or configure your Path.
exit 1

:FoundMSVC
set OUTDIR=.\WinRel
set LNKOUT=./WinRel
set "OPTS=/O2 /D NDEBUG"
set LINKOPTS=
if "%DEBUG%" == "Y" set OUTDIR=.\WinDebug
if "%DEBUG%" == "Y" set LNKOUT=./WinDebug
if "%DEBUG%" == "Y" set "OPTS=/Zi /Od /D _DEBUG"
if "%DEBUG%" == "Y" set LINKOPTS=/DEBUG
if "%MAINT%" == "Y" set "OPTS=%OPTS% /D MAKE_MAINTAINER_MODE"
:: Show the compiler version that we found
:: Unfortunately this also shows a "usage" note; I can't find anything better.
echo.
call %COMPILER%
goto Build

:FindGcc
set OUTDIR=.\GccRel
set LNKOUT=./GccRel
set OPTS=-O2
set DIRENT=N
if "%DEBUG%" == "Y" set OPTS=-O0
if "%DEBUG%" == "Y" set OUTDIR=.\GccDebug
if "%DEBUG%" == "Y" set LNKOUT=./GccDebug
if "%MAINT%" == "Y" set "OPTS=%OPTS% -DMAKE_MAINTAINER_MODE"
:: Show the compiler version that we found
echo.
call %COMPILER% --version
if not ERRORLEVEL 1 goto Build
echo No %COMPILER% found.
exit 1

:FindTcc
set OUTDIR=.\TccRel
set LNKOUT=./TccRel
set OPTS=-O2
if "%DEBUG%" == "Y" set OPTS=-O0
if "%DEBUG%" == "Y" set OUTDIR=.\TccDebug
if "%DEBUG%" == "Y" set LNKOUT=./TccDebug
if "%MAINT%" == "Y" set "OPTS=%OPTS% -DMAKE_MAINTAINER_MODE"
:: Show the compiler version that we found
echo.
call %COMPILER% -v
if not ERRORLEVEL 1 goto Build
echo No %COMPILER% found.
exit 1

:Build
:: Clean the directory if it exists
if exist %OUTDIR%\nul rmdir /S /Q %OUTDIR%

:: Recreate it
mkdir %OUTDIR%
mkdir %OUTDIR%\src
mkdir %OUTDIR%\src\w32
mkdir %OUTDIR%\src\w32\compat
mkdir %OUTDIR%\src\w32\subproc
mkdir %OUTDIR%\lib

if "%GUILE%" == "Y" call :ChkGuile

if not exist src\config.h.W32 goto NotConfig

echo.
echo Compiling %OUTDIR% version

copy src\config.h.W32 %OUTDIR%\src\config.h

copy lib\glob.in.h %OUTDIR%\lib\glob.h
copy lib\fnmatch.in.h %OUTDIR%\lib\fnmatch.h

if exist %OUTDIR%\link.sc del %OUTDIR%\link.sc

call :Compile src/ar
call :Compile src/arscan
call :Compile src/commands
call :Compile src/default
call :Compile src/dir
call :Compile src/expand
call :Compile src/file
call :Compile src/function
call :Compile src/getopt
call :Compile src/getopt1
call :Compile src/guile GUILE
call :Compile src/hash
call :Compile src/implicit
call :Compile src/job
call :Compile src/load
call :Compile src/loadapi
call :Compile src/main GUILE
call :Compile src/misc
call :Compile src/output
call :Compile src/read
call :Compile src/remake
call :Compile src/remote-stub
call :Compile src/rule
call :Compile src/shuffle
call :Compile src/signame
call :Compile src/strcache
call :Compile src/variable
call :Compile src/version
call :Compile src/vpath
call :Compile src/w32/pathstuff
call :Compile src/w32/w32os
call :Compile src/w32/compat/posixfcn
call :Compile src/w32/subproc/misc
call :Compile src/w32/subproc/sub_proc
call :Compile src/w32/subproc/w32err
call :Compile lib/fnmatch
call :Compile lib/glob
call :Compile lib/getloadavg

:: Compile dirent unless it is supported by compiler library (like with gcc).
if "%DIRENT%" == "Y" call :Compile src\w32\compat\dirent

call :Link

echo.
if exist %OUTDIR%\%MAKE%.exe goto Success
echo %OUTDIR% build FAILED!
exit 1

:Success
echo %OUTDIR% build succeeded.
if exist Basic.mk copy /Y Basic.mk Makefile
if not exist tests\config-flags.pm copy /Y tests\config-flags.pm.W32 tests\config-flags.pm
call :Reset
goto :EOF

::
:: Subroutines
::

:Compile
if "%VERBOSE%" == "N" echo - Compiling %1.c
echo %LNKOUT%/%1.%O% >>%OUTDIR%\link.sc
set EXTRAS=
if "%2" == "GUILE" set "EXTRAS=%GUILECFLAGS%"
if exist "%OUTDIR%\%1.%O%" del "%OUTDIR%\%1.%O%"
if "%COMPILER%" == "gcc" goto GccCompile
if "%COMPILER%" == "tcc" goto TccCompile

:: MSVC Compile
if "%VERBOSE%" == "Y" echo on
call %COMPILER% /nologo /MT /W4 /EHsc %OPTS% /I %OUTDIR%/src /I src /I %OUTDIR%/lib /I lib /I src/w32/include /D _CONSOLE /D HAVE_CONFIG_H /FR%OUTDIR% /Fp%OUTDIR%\%MAKE%.pch /Fo%OUTDIR%\%1.%O% /Fd%OUTDIR%\%MAKE%.pdb %EXTRAS% /c %1.c
@echo off
goto CompileDone

:GccCompile
:: GCC Compile
if "%VERBOSE%" == "Y" echo on
call %COMPILER% -mthreads -Wall -std=gnu99 -gdwarf-2 -g3 %OPTS% -I%OUTDIR%/src -I./src -I%OUTDIR%/lib -I./lib -I./src/w32/include -DHAVE_CONFIG_H %EXTRAS% -o %OUTDIR%/%1.%O% -c %1.c
@echo off
goto CompileDone

:TccCompile
:: TCC Compile
if "%VERBOSE%" == "Y" echo on
call %COMPILER% -mthreads -Wall -std=c11 %OPTS% -I%OUTDIR%/src -I./src -I%OUTDIR%/lib -I./lib -I./src/w32/include -D_cdecl= -D_MSC_VER -DHAVE_CONFIG_H %EXTRAS% -o %OUTDIR%/%1.%O% -c %1.c
@echo off
goto CompileDone

:CompileDone
if not exist "%OUTDIR%\%1.%O%" exit 1
goto :EOF

:Link
echo.
echo - Linking %LNKOUT%/%MAKE%.exe
if "%COMPILER%" == "gcc" goto GccLink
if "%COMPILER%" == "tcc" goto TccLink

:: MSVC Link
echo %GUILELIBS% kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib >>%OUTDIR%\link.sc
if "%VERBOSE%" == "Y" echo on
call link.exe /NOLOGO /SUBSYSTEM:console /PDB:%LNKOUT%\%MAKE%.pdb %LINKOPTS% /OUT:%LNKOUT%\%MAKE%.exe @%LNKOUT%\link.sc
@echo off
goto :EOF

:GccLink
:: GCC Link
if "%VERBOSE%" == "Y" echo on
echo %GUILELIBS% -lkernel32 -luser32 -lgdi32 -lwinspool -lcomdlg32 -ladvapi32 -lshell32 -lole32 -loleaut32 -luuid -lodbc32 -lodbccp32 >>%OUTDIR%\link.sc
call %COMPILER% -mthreads -gdwarf-2 -g3 %OPTS% -o %LNKOUT%/%MAKE%.exe @%LNKOUT%/link.sc -Wl,--out-implib=%LNKOUT%/libgnumake-1.dll.a
@echo off
goto :EOF

:TccLink
:: TCC Link
if "%VERBOSE%" == "Y" echo on
echo %GUILELIBS% -lkernel32 -luser32 -lgdi32 -lcomdlg32 -ladvapi32 -lshell32 -lole32 -loleaut32 -lodbc32 -lodbccp32 >>%OUTDIR%\link.sc
call %COMPILER% -mthreads %OPTS% -o %LNKOUT%/%MAKE%.exe @%LNKOUT%/link.sc 
@echo off
goto :EOF

:ChkGuile
:: Build with Guile is supported only on NT and later versions
if not "%OS%" == "Windows_NT" goto NoGuile
call pkg-config --help > %OUTDIR%\guile.tmp 2> NUL
if ERRORLEVEL 1 goto NoPkgCfg

set PKGMSC=
if not "%COMPILER%" == "gcc" set PKGMSC=--msvc-syntax

echo Checking for Guile 2.0
call pkg-config --cflags --short-errors "guile-2.0" > %OUTDIR%\gl-c2.tmp 2> NUL
if not ERRORLEVEL 1 set /P GUILECFLAGS= < %OUTDIR%\gl-c2.tmp

call pkg-config --libs --static --short-errors %PKGMSC% "guile-2.0" > %OUTDIR%\gl-l2.tmp 2> NUL
if not ERRORLEVEL 1 set /P GUILELIBS= < %OUTDIR%\gl-l2.tmp

if not "%GUILECFLAGS%" == "" goto GuileDone

echo Checking for Guile 1.8
call pkg-config --cflags --short-errors "guile-1.8" > %OUTDIR%\gl-c18.tmp 2> NUL
if not ERRORLEVEL 1 set /P GUILECFLAGS= < %OUTDIR%\gl-c18.tmp

call pkg-config --libs --static --short-errors %PKGMSC% "guile-1.8" > %OUTDIR%\gl-l18.tmp 2> NUL
if not ERRORLEVEL 1 set /P GUILELIBS= < %OUTDIR%\gl-l18.tmp

if not "%GUILECFLAGS%" == "" goto GuileDone

echo - No Guile found, building without Guile
goto GuileDone

:NoPkgCfg
echo - pkg-config not found, building without Guile

:GuileDone
if "%GUILECFLAGS%" == "" goto :EOF

echo - Guile found: building with Guile
set "GUILECFLAGS=%GUILECFLAGS% -DHAVE_GUILE"
goto :EOF

:FindVswhere
set VSWHERE=vswhere
call "%VSWHERE%" -help >nul 2>&1
if not ERRORLEVEL 1 exit /b 0
set "VSWHERE=C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere"
call "%VSWHERE%" -help >nul 2>&1
if ERRORLEVEL 1 exit /b 1
goto :EOF

:CheckMSVC
if not exist "%VSVARS%" exit /b 1
call "%VSVARS%" %ARCH%
if ERRORLEVEL 1 exit /b 1
call %COMPILER% >nul 2>&1
if ERRORLEVEL 1 exit /b 1
goto :EOF

:NotConfig
echo.
echo *** This workspace is not configured.
echo Either retrieve the configured source in the release tarball
echo or, if building from Git, run the .\bootstrap.bat script first.
exit /b 1

:Usage
echo Usage: %0 [options] [gcc] OR [tcc]
echo Options:
echo.  --without-guile   Do not compile Guile support even if found
echo.  --debug           Make a Debug build--default is Release
echo.  --x86             Make a 32bit binary--default is 64bit
echo.  --help            Display these instructions and exit
echo.
echo. "gcc" means compile with GCC, "tcc" means compile with Tiny C's TCC
goto :EOF

:Reset
set ARCH=
set COMPILER=
set DEBUG=
set GUILE=
set GUILECFLAGS=
set GUILELIBS=
set LINKOPTS=
set MAKE=
set NOGUILE=
set O=
set OPTS=
set OUTDIR=
set LNKOUT=
set PKGMSC=
set VSVARS=
goto :EOF
