@echo on
set curpath=%~dp0
cd %curpath%
cd ..\..\..\ConsoleMapManager\x64
ConsoleMapManager.exe GenerateGroundImage D:\TheWorld\Client\TheWorld_MapManager\Data\Ground\sand sand 512 D:\TheWorld\Client\TheWorld_MapManager\Data\Ground\sand\sand_albedo.png D:\TheWorld\Client\TheWorld_MapManager\Data\Ground\sand\sand_depth.png D:\TheWorld\Client\TheWorld_MapManager\Data\Ground\sand\sand_normal.png D:\TheWorld\Client\TheWorld_MapManager\Data\Ground\sand\sand_roughness.png
pause