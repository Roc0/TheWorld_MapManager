@echo on
set curpath=%~dp0
cd %curpath%
cd ..\..\..\ConsoleMapManager\x64
ConsoleMapManager.exe GenerateGroundImage D:\TheWorld\Client\TheWorld_MapManager\Data\Ground\Rock028_1K Rock028_1K 512 Y D:\TheWorld\Client\TheWorld_MapManager\Data\Ground\Rock028_1K\Rock028_1K_Color.jpg D:\TheWorld\Client\TheWorld_MapManager\Data\Ground\Rock028_1K\Rock028_1K_Displacement.jpg D:\TheWorld\Client\TheWorld_MapManager\Data\Ground\Rock028_1K\Rock028_1K_NormalGL.jpg D:\TheWorld\Client\TheWorld_MapManager\Data\Ground\Rock028_1K\Rock028_1K_Roughness.jpg
pause