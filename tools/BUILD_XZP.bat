@REM Original script written by Byrom90, edited by frankischilling for Snes360-enhanced
@REM Edited for FCE360-enhanced - builds from ui\xui folder with media\xui path structure

@echo off

@REM Change to parent directory to get correct relative paths
@pushd "%~dp0.."

@if exist ui\FCE360.xzp del ui\FCE360.xzp

@set XUIPKG="%XEDK%\bin\win32\xuipkg.exe"

@REM Get absolute path to XZP file before changing directories
@set XUI_XZP_ABS=%CD%\ui\ui.xzp

@echo Building FCE360.xzp with media\xui path structure
@echo Source: ui\xui
@echo Output: %XUI_XZP_ABS%

@REM Create temporary media\xui structure
@set TEMP_MEDIA=%~dp0temp_media_xui
@if exist %TEMP_MEDIA% rmdir /s /q %TEMP_MEDIA%
@mkdir %TEMP_MEDIA%\media\xui 2>nul
@xcopy /E /I /Y "ui\xui\*" "%TEMP_MEDIA%\media\xui\" >nul

@REM Change to temp directory so paths are stored as media\xui\...
@pushd %TEMP_MEDIA%

@REM Add all files from media\xui directory (will be stored as media\xui\...)
@REM Add root files first, then Graphics folder recursively to avoid duplicates
@echo Adding root files from media\xui...
%XUIPKG% /nologo /O "%XUI_XZP_ABS%" "media\xui\*.xui" "media\xui\*.xur"

@echo Adding Graphics files recursively...
%XUIPKG% /nologo /A "%XUI_XZP_ABS%" /R "media\xui\Graphics\*.*"

@popd

@REM Clean up temporary directory
@if exist %TEMP_MEDIA% rmdir /s /q %TEMP_MEDIA%

@popd

@echo.
@echo Build complete: %XUI_XZP_ABS%
@cmd /k
