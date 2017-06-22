@ECHO OFF

REM SET BUILDTYPE=release
SET BUILDTYPE=debug

call "c:\Program Files (x86)\Microsoft Visual Studio\2017\Professional\VC\Auxiliary\Build\vcvarsall.bat" amd64
SET VSDIR=vs2017

REM call "c:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" amd64
REM SET VSDIR=vs2015

REM one of the perl scripts ignores the cmake option and reads GPERF from an env var
REM (or expects it in the path)
set GPERF=c:/msys64/usr/bin/gperf.exe

REM cmake searches for gcc.exe first in the path; tell it otherwise.
set CC=cl.exe
set CXX=cl.exe

perl Tools/Scripts/build-jsc --%BUILDTYPE% --64-bit --jsc-only --cmakeargs="-DENABLE_STATIC_JSC=ON -DUSE_THIN_ARCHIVES=OFF -DENABLE_WEBASSEMBLY=OFF -DRUBY_EXECUTABLE=c:/msys64/mingw64/bin/ruby.exe -DGPERF_EXECUTABLE=c:/msys64/usr/bin/gperf.exe" --makeargs="-v"

:docopy

mkdir jscore-%VSDIR%\includes\JavaScriptCore
mkdir jscore-%VSDIR%\x64

xcopy /y /q Source\JavaScriptCore\API\*.h jscore-%VSDIR%\includes\JavaScriptCore
copy WebKitBuild\%BUILDTYPE%\lib\JavaScriptCore.lib jscore-%VSDIR%\x64\JavaScriptCore-%BUILDTYPE%.lib
copy WebKitBuild\%BUILDTYPE%\lib\WTF.lib jscore-%VSDIR%\x64\WTF-%BUILDTYPE%.lib
copy WebKitLibraries\win\lib64\libicu*.lib jscore-%VSDIR%\x64
copy WebKitLibraries\win\bin64\icu*.dll jscore-%VSDIR%\x64

:done
