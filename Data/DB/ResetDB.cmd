@echo on
set curpath=%~dp0
cd %curpath%
del /Q TheWorldMap.db
del /Q *.db-jpurnal
call ..\scripts\SQliteMainSchema.cmd nopause