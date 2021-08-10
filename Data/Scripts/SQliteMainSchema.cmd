@echo on
set curpath=%~dp0
..\..\..\..\SQLite\3.36.0\sqlite3.exe ..\DB\TheWorldMap.db ".read ..\\SQL\\CreateMainSchema.sql"
if /I a%1==anopause goto fine
pause
:fine