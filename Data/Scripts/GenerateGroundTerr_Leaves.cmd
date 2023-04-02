@echo on
set curpath=%~dp0
cd %curpath%
cd ..\..\..\ConsoleMapManager\x64
ConsoleMapManager.exe GenerateGroundImage D:\TheWorld\Client\TheWorld_MapManager\Data\Ground\leaves leaves 1024 Y D:\TheWorld\Client\TheWorld_MapManager\Data\Ground\leaves\leaves_albedo.png D:\TheWorld\Client\TheWorld_MapManager\Data\Ground\leaves\leaves_depth.png D:\TheWorld\Client\TheWorld_MapManager\Data\Ground\leaves\leaves_normal.png D:\TheWorld\Client\TheWorld_MapManager\Data\Ground\leaves\leaves_roughness.png
pause