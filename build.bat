@echo off
setlocal

if not exist build mkdir build
pushd build

set CompilerFlags=-FC -Zi -Od

cl %CompilerFlags% ..\src\asio_example.c -link -subsystem:console

popd
exit /B %ErrorCode%
