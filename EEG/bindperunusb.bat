@echo off
setlocal

REM input arguments
set "ACTION=attach"
if /i "%~1"=="-A"      set "ACTION=attach"
if /i "%~1"=="-Attach" set "ACTION=attach"
if /i "%~1"=="-D"      set "ACTION=detach"
if /i "%~1"=="-Detach" set "ACTION=detach"

REM list usb
usbipd list

set /p "BUSID=Enter Perun bus-id:"

if not defined BUSID (
   echo no bus-id given.
   goto :finish
)

call :%ACTION%

REM check
usbipd list

:finish
pause
endlocal
exit /b

:attach
REM share usb device
usbipd bind --busid %BUSID%

REM attach shared usb to wsl2
usbipd attach --wsl --busid %BUSID%

REM if you now see device on wsl but can't open check permission of this bus
goto :eof

:detach
REM detach and unbind
usbipd detach --busid %BUSID%
timeout 1
usbipd unbind --busid %BUSID%
goto :eof