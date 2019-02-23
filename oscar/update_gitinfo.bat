@echo off
setlocal
set DIR=%~dp0

echo Updating %DIR%git_info.h

for /f %%i in ('git -C %DIR% rev-parse --abbrev-ref HEAD') do set GIT_BRANCH=%%i
for /f %%i in ('git -C %DIR% rev-parse --short HEAD') do set GIT_REVISION=%%i

if "%GIT_BRANCH"=="" set GIT_BRANCH="Unknown";
if "%GIT_REVIISION"=="" set GIT_REVISION="Unknown";

echo // This is an auto generated file > %DIR%git_info.h
echo const QString GIT_BRANCH="%GIT_BRANCH%"; >> %DIR%git_info.h
echo const QString GIT_REVISION="%GIT_REVISION%"; >> %DIR%git_info.h
