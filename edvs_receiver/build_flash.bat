@echo off

REM Build the project file
IarBuild.exe EDVSReceiver.ewp Debug

REM Attempt to erase the chip
rem "C:\Program Files (x86)\STMicroelectronics\STM32 ST-LINK Utility\ST-LINK Utility\ST-LINK_CLI.exe" -ME
"C:\Program Files (x86)\STMicroelectronics\STM32 ST-LINK Utility\ST-LINK Utility\ST-LINK_CLI.exe" -P Debug/Exe/EDVSReceiver.bin 0x08000000 -V "after_programming"
