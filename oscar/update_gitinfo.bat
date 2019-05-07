@echo off
setlocal
set DIR=%~dp0

for /f %%i in ('git --work-tree %DIR% rev-parse --abbrev-ref HEAD') do set GIT_BRANCH=%%i
for /f %%i in ('git --work-tree %DIR% rev-parse --short HEAD') do set GIT_REVISION=%%i
git --work-tree $DIR diff-index --quiet HEAD
if %ERRORLEVEL% neq 0 (
    do set GIT_REVISION="%GIT_REVISION%+"  # uncommitted changes
)

if "%GIT_BRANCH"=="" set GIT_BRANCH="Unknown";
if "%GIT_REVISION"=="" set GIT_REVISION="Unknown";

echo // This is an auto generated file > %DIR%git_info.new
echo const QString GIT_BRANCH="%GIT_BRANCH%"; >> %DIR%git_info.new
echo const QString GIT_REVISION="%GIT_REVISION%"; >> %DIR%git_info.new

fc %DIR%git_info.h %DIR%git_info.new > nul
if errorlevel 0 (
    del /y %DIR%git_info.new
) else (
    echo Updating %DIR%git_info.h
    move /y %DIR%git_info.new %DIR%git_info.h
)
