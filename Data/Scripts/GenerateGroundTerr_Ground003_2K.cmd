@echo on
set curpath=%~dp0
cd %curpath%
cd ..\..\..\ConsoleMapManager\x64
ConsoleMapManager.exe GenerateGroundImage D:\TheWorld\Client\TheWorld_MapManager\Data\Ground\Ground003_2K Ground003_2K 1024 Y D:\TheWorld\Client\TheWorld_MapManager\Data\Ground\Ground003_2K\Ground003_2K_Color.jpg D:\TheWorld\Client\TheWorld_MapManager\Data\Ground\Ground003_2K\Ground003_2K_Displacement.jpg D:\TheWorld\Client\TheWorld_MapManager\Data\Ground\Ground003_2K\Ground003_2K_NormalGL.jpg D:\TheWorld\Client\TheWorld_MapManager\Data\Ground\Ground003_2K\Ground003_2K_Roughness.jpg
pause