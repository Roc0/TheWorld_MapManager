@echo on
set curpath=%~dp0
cd %curpath%
del /Q TheWorldMap.db
del /Q *.db-journal
del /Q *.journal
call ..\scripts\SQliteMainSchema.cmd nopause