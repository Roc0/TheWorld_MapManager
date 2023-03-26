@echo on
set curpath=%~dp0
cd %curpath%
cd ..\..\..\ConsoleMapManager\x64
ConsoleMapManager.exe GenerateGroundImage D:\TheWorld\Client\TheWorld_MapManager\Data\Ground\Wood089_1K Wood089_1K 512 D:\TheWorld\Client\TheWorld_MapManager\Data\Ground\Wood089_1K\Wood089_1K_Color.png D:\TheWorld\Client\TheWorld_MapManager\Data\Ground\Wood089_1K\Wood089_1K_Displacement.png D:\TheWorld\Client\TheWorld_MapManager\Data\Ground\Wood089_1K\Wood089_1K_NormalGL.png D:\TheWorld\Client\TheWorld_MapManager\Data\Ground\Wood089_1K\Wood089_1K_Roughness.png
pause