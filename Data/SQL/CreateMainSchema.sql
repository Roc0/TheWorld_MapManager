drop table Params;
drop table WorldDefiner;
drop table MapVertex;
drop table MapVertex_WD;
drop table MapVertex_Mod;

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
	PRIMARY KEY(PosZ, PosX, Level, Type)
);
create index WorldDefiner_PolarCoord on WorldDefiner(radius, azimuth, level, Type);
	
create table MapVertex(
	PosX REAL NOT NULL,
	PosZ REAL NOT NULL,
	Level INTEGER NOT NULL,
	Radius REAL NOT NULL,
	Azimuth REAL NOT NULL,
	InitialAltitude REAL NOT NULL,
	PosY REAL NOT NULL,
	PRIMARY KEY(PosZ, PosX, Level)
);
create index MapVertex_PolarCoord on MapVertex(radius, azimuth, level);

create table MapVertex_WD(
	VertexRowId INTEGER NOT NULL,
	WDRowId INTEGER NOT NULL,
	PRIMARY KEY(VertexRowId, WDRowId)
);
create index MapVertex_WD_Vertex on MapVertex_WD(VertexRowId);
create index MapVertex_WD_WDRowId on MapVertex_WD(WDRowId);

create table MapVertex_Mod(
	VertexRowId INTEGER NOT NULL,
	PRIMARY KEY(VertexRowId)
);

insert into Params (ParamName, ParamValue) values ("GrowingBlockVertexNumberShift", "8");
insert into Params (ParamName, ParamValue) values ("GridStepInWU", "5.0");
