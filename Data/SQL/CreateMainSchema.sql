drop table Params;
drop table WorldDefiner;
drop table GridVertex;
drop table GridVertex_WD;
drop table GridVertex_Mod;
--drop table Quadrant;

create table Params(
	ParamName REAL NOT NULL,
	ParamValue TEXT NOT NULL,
	PRIMARY KEY(ParamName)
);

create table WorldDefiner(
	PosX REAL NOT NULL,
	PosZ REAL NOT NULL,
	Level INTEGER NOT NULL,
	Type INTEGER NOT NULL,
	Radius REAL NOT NULL,
	Azimuth REAL NOT NULL,
	AzimuthDegree REAL NOT NULL,
	Strength REAL NOT NULL,
	AOE REAL NOT NULL,
	FunctionType INTEGER NOT NULL,
	WDRowId INTEGER PRIMARY KEY
);
create unique index WorldDefiner_CartesianCoord on WorldDefiner(PosZ, PosX, Level, Type);
create index WorldDefiner_PolarCoord on WorldDefiner(radius, azimuth, level, Type);
	
create table GridVertex(
	PosX REAL NOT NULL,
	PosZ REAL NOT NULL,
	Level INTEGER NOT NULL,
	Radius REAL NOT NULL,
	Azimuth REAL NOT NULL,
	InitialAltitude REAL NOT NULL,
	PosY REAL NOT NULL,
	VertexRowId INTEGER PRIMARY KEY
);
create unique index GridVertex_CartesianCoord on GridVertex(PosZ, PosX, Level);
create index GridVertex_PolarCoord on GridVertex(radius, azimuth, level);

create table GridVertex_WD(
	VertexRowId INTEGER NOT NULL,
	WDRowId INTEGER NOT NULL,
	PRIMARY KEY(VertexRowId, WDRowId)
);
create index GridVertex_WD_Vertex on GridVertex_WD(VertexRowId);
create index GridVertex_WD_WDRowId on GridVertex_WD(WDRowId);

create table GridVertex_Mod(
	VertexRowId INTEGER NOT NULL,
	PRIMARY KEY(VertexRowId)
);

insert into Params (ParamName, ParamValue) values ("GrowingBlockVertexNumberShift", "8");	-- 8 ==> 256 vertices per growing block
insert into Params (ParamName, ParamValue) values ("GridStepInWU", "2.0");

/*
create table Quadrant(
	VertexPerSize INTEGER NOT NULL,
	Level INTEGER NOT NULL,
	PosX REAL NOT NULL,
	PosZ REAL NOT NULL,
	ToLoad INTEGER NOT NULL,
	Hash BLOB NOT NULL,
	PRIMARY KEY(VertexPerSize, Level, PosZ, PosX)
);
*/