@echo off
REM Double-click to fetch a Spotify refresh token.
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
echo   Python was not found. Install it from https://python.org
echo   ^(tick "Add Python to PATH" during the install^).
echo.

:done
endlocal
pause
