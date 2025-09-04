@echo off
echo Testing Intel Graphics Compatibility Mode
echo =========================================
echo.

echo Setting environment variables for Intel graphics simulation...
set CEF_TEST_INTEL_GRAPHICS=1
set CEF_ALLOW_INTEL_OVERRIDE=0

echo.
echo Environment variables set:
echo CEF_TEST_INTEL_GRAPHICS=%CEF_TEST_INTEL_GRAPHICS%
echo CEF_ALLOW_INTEL_OVERRIDE=%CEF_ALLOW_INTEL_OVERRIDE%
echo.

echo Starting OTClient in Intel graphics test mode...
echo You should see these messages in the logs:
echo - "TEST MODE: Simulating Intel graphics behavior"
echo - "Intel graphics detected - applying compatibility workarounds"
echo - "Intel compatibility flags applied"
echo.

pause
otclient.exe

pause