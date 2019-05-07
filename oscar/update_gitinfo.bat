@echo off
setlocal
set DIR=%~dp0
cd %DIR%

for /f %%i in ('git rev-parse --abbrev-ref HEAD') do set GIT_BRANCH=%%i
for /f %%i in ('git rev-parse --short HEAD') do set GIT_REVISION=%%i
git diff-index --quiet HEAD -- || set GIT_REVISION=%GIT_REVISION%+

if "%GIT_BRANCH"=="" set GIT_BRANCH="Unknown"
if "%GIT_REVISION"=="" set GIT_REVISION="Unknown"

echo // This is an auto generated file > %DIR%git_info.new
echo const QString GIT_BRANCH="%GIT_BRANCH%"; >> %DIR%git_info.new
echo const QString GIT_REVISION="%GIT_REVISION%"; >> %DIR%git_info.new

fc %DIR%git_info.h %DIR%git_info.new > nul
if errorlevel 0 (
    del /q %DIR%git_info.new
) else (
    echo Updating %DIR%git_info.h
    move /y %DIR%git_info.new %DIR%git_info.h
)
