@echo off
setlocal
set ErrorCode=0

if not exist build mkdir build
pushd build

set CompilerFlags=-FC -Zi -Od -nologo

cl %CompilerFlags% ..\src\asio_example.c -link -subsystem:console
set /A ErrorCode+=%ERRORLEVEL%
cl %CompilerFlags% ..\src\wasapi_example.c -link -subsystem:console
set /A ErrorCode+=%ERRORLEVEL%

popd
exit /B %ErrorCode%
