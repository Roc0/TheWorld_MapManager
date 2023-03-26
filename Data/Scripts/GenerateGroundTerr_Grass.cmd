@echo on
set curpath=%~dp0
cd %curpath%
cd ..\..\..\ConsoleMapManager\x64
ConsoleMapManager.exe GenerateGroundImage D:\TheWorld\Client\TheWorld_MapManager\Data\Ground\grass grass 512 D:\TheWorld\Client\TheWorld_MapManager\Data\Ground\grass\grass_albedo.png D:\TheWorld\Client\TheWorld_MapManager\Data\Ground\grass\grass_depth.png D:\TheWorld\Client\TheWorld_MapManager\Data\Ground\grass\grass_normal.png D:\TheWorld\Client\TheWorld_MapManager\Data\Ground\grass\grass_roughness.png
pause