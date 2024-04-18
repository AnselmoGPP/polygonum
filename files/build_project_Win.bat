@ECHO OFF
ECHO Current directory: 
CD

RMDIR /S /Q ..\_BUILD
MKDIR ..\_BUILD
MKDIR ..\_BUILD\extern
MKDIR ..\_BUILD\projects

ECHO Building project dependencies:
CD ..\_BUILD\extern
"C:\Program Files\CMake\bin\cmake" -G "Visual Studio 17 2022" -A "x64" ..\..\extern

ECHO Building renderer and example projects:
CD ..\projects
"C:\Program Files\CMake\bin\cmake" -G "Visual Studio 17 2022" -A "x64" ..\..\projects

PAUSE
REM -D CMAKE_BUILD_TYPE=Debug   --config Debug
