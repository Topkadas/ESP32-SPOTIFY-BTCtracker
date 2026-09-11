@echo off
REM Dvojklikem spusti ziskani Spotify refresh tokenu.
setlocal
cd /d "%~dp0.."

where py >nul 2>&1
if %errorlevel%==0 (
    py -3 tools\get_token.py %*
    goto :done
)

where python >nul 2>&1
if %errorlevel%==0 (
    python tools\get_token.py %*
    goto :done
)

echo.
echo   Python nebyl nalezen. Nainstaluj ho z https://python.org
echo   ^(pri instalaci zaskrtni "Add Python to PATH"^).
echo.

:done
endlocal
pause
