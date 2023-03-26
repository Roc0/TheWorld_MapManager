@echo on
set curpath=%~dp0
cd %curpath%
cd ..\..\..\ConsoleMapManager\x64
ConsoleMapManager.exe GenerateGroundImage D:\TheWorld\Client\TheWorld_MapManager\Data\Ground\rocks07 rocks07 512 D:\TheWorld\Client\TheWorld_MapManager\Data\Ground\rocks07\rocks07_col.png D:\TheWorld\Client\TheWorld_MapManager\Data\Ground\rocks07\rocks07_disp.jpg D:\TheWorld\Client\TheWorld_MapManager\Data\Ground\rocks07\rocks07_nrm.jpg D:\TheWorld\Client\TheWorld_MapManager\Data\Ground\rocks07\rocks07_rgh.jpg
pause