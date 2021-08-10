drop table WorldDefiner;
drop table MapVertex;
drop table MapVertexWD;
drop table MapVertex_Mod;

create table WorldDefiner(
	PosX REAL NOT NULL,
	PosZ REAL NOT NULL,
	Level INTEGER NOT NULL,
	Type INTEGER NOT NULL,
	radius REAL NOT NULL,
	azimuth REAL NOT NULL,
	azimuthDegree REAL NOT NULL,
	Strength REAL NOT NULL,
	AOE REAL NOT NULL,
	PRIMARY KEY(PosX, PosZ, level, Type)
);
create index WorldDefiner_PolarCoord on WorldDefiner(radius, azimuth, level, Type);
	
create table MapVertex(
	PosX REAL NOT NULL,
	PosZ REAL NOT NULL,
	Level INTEGER NOT NULL,
	radius REAL NOT NULL,
	azimuth REAL NOT NULL,
	PosY REAL NOT NULL,
	PRIMARY KEY(PosX, PosZ, level)
);
create index MapVertex_PolarCoord on MapVertex(radius, azimuth, level);

create table MapVertexWD(
	VertexRowId INTEGER NOT NULL,
	WDRowId INTEGER NOT NULL
);
create index MapVertexWD_Vertex on MapVertexWD(VertexRowId);
create index MapVertexWD_WDRowId on MapVertexWD(WDRowId);

create table MapVertex_Mod(
	PosX REAL NOT NULL,
	PosZ REAL NOT NULL,
	Level INTEGER NOT NULL,
	PRIMARY KEY(PosX, PosZ, level)
);
