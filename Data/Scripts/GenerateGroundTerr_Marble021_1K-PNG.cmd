@echo on
set curpath=%~dp0
cd %curpath%
cd ..\..\..\ConsoleMapManager\x64
ConsoleMapManager_d.exe GenerateGroundImage D:\TheWorld\Client\TheWorld_MapManager\Data\Ground\Marble021_1K-PNG Marble021_1K 512 Y D:\TheWorld\Client\TheWorld_MapManager\Data\Ground\Marble021_1K-PNG\Marble021_1K-PNG_Color.png D:\TheWorld\Client\TheWorld_MapManager\Data\Ground\Marble021_1K-PNG\Marble021_1K-PNG_Displacement.png D:\TheWorld\Client\TheWorld_MapManager\Data\Ground\Marble021_1K-PNG\Marble021_1K-PNG_NormalGL.png D:\TheWorld\Client\TheWorld_MapManager\Data\Ground\Marble021_1K-PNG\Marble021_1K-PNG_Roughness.png
pause