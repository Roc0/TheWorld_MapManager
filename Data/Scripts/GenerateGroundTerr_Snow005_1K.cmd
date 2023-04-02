@echo on
set curpath=%~dp0
cd %curpath%
cd ..\..\..\ConsoleMapManager\x64
ConsoleMapManager.exe GenerateGroundImage D:\TheWorld\Client\TheWorld_MapManager\Data\Ground\Snow005_1K Snow005_1K 512 Y D:\TheWorld\Client\TheWorld_MapManager\Data\Ground\Snow005_1K\Snow005_1K_Color.png D:\TheWorld\Client\TheWorld_MapManager\Data\Ground\Snow005_1K\Snow005_1K_Displacement.png D:\TheWorld\Client\TheWorld_MapManager\Data\Ground\Snow005_1K\Snow005_1K_NormalGL.png D:\TheWorld\Client\TheWorld_MapManager\Data\Ground\Snow005_1K\Snow005_1K_Roughness.png
pause