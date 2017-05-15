@echo off

REM Build the project file
IarBuild.exe EDVSReceiver.ewp Debug

REM Erase and reset the chip to start the program
"C:\Program Files (x86)\STMicroelectronics\STM32 ST-LINK Utility\ST-LINK Utility\ST-LINK_CLI.exe" -P Debug/Exe/EDVSReceiver.bin 0x08000000 -V "after_programming"
"C:\Program Files (x86)\STMicroelectronics\STM32 ST-LINK Utility\ST-LINK Utility\ST-LINK_CLI.exe" -Rst
