-- .schema <table_name>	or pragma table_info('table_name');==> describes the table

drop table Params;
drop table WorldDefiner;
drop table GridVertex;
drop table GridVertex_WD;
drop table GridVertex_Mod;
drop table Quadrant;
drop table QuadrantLoading;
drop table TerrainQuadrant;
drop table NoiseValuesQuadrant;

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
	NormX REAL,
	NormY REAL,
	NormZ REAL,
	ColorR INTEGER,					-- 0/255
	ColorG INTEGER,					-- 0/255
	ColorB INTEGER,					-- 0/255
	ColorA INTEGER,					-- 0/255
	LowElevationTexAmount INTEGER,	-- 0/255
	HighElevationTexAmount INTEGER,	-- 0/255
	DirtTexAmount INTEGER,			-- 0/255
	RocksTexAmount INTEGER,			-- 0/255
	GlobalMapR INTEGER,				-- 0/255
	GlobalMapG INTEGER,				-- 0/255
	GlobalMapB INTEGER,				-- 0/255
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

insert into Params (ParamName, ParamValue) values ("MapName", "TestMap01");
insert into Params (ParamName, ParamValue) values ("GrowingBlockVertexNumberShift", "8");	-- 8 ==> 256 vertices per growing block
insert into Params (ParamName, ParamValue) values ("GridStepInWU", "2.0");

create table Quadrant(
	GridStepInWU REAL NOT NULL,
	VertexPerSize INTEGER NOT NULL,
	Level INTEGER NOT NULL,
	PosXStart REAL NOT NULL,
	PosZStart REAL NOT NULL,
	PosXEnd REAL NOT NULL,
	PosZEnd REAL NOT NULL,
	Hash BLOB NOT NULL,
	Status TEXT CHECK( Status IN ('C','L','E') ) NOT NULL DEFAULT 'E',
	PRIMARY KEY(GridStepInWU, VertexPerSize, Level, PosXStart, PosZStart)
);
--create unique index Quadrant_Hash_Idx on Quadrant(Hash);

create table QuadrantLoading(
	Hash BLOB NOT NULL,
	LastXIdxLoaded INTEGER NOT NULL,	-- 0 to VertexPerSize - 1
	LastZIdxLoaded INTEGER NOT NULL,	-- 0 to VertexPerSize - 1
	PRIMARY KEY(Hash)
);

create table TerrainQuadrant(
	Hash BLOB NOT NULL,
	TerrainType INTEGER NOT NULL,
	MinHeigth FLOAT NOT NULL,
	MaxHeigth FLOAT NOT NULL,
	LowElevationTexName TEXT NOT NULL,
	HighElevationTexName TEXT NOT NULL,
	DirtElevationTexName TEXT NOT NULL,
	RocksElevationTexName TEXT NOT NULL,
	EastSideXPlusNeedBlend INTEGER NOT NULL,
	EastSideXPlusMinHeight FLOAT NOT NULL,
	EastSideXPlusMaxHeight FLOAT NOT NULL,
	WestSideXMinusNeedBlend INTEGER NOT NULL,
	WestSideXMinusMinHeight FLOAT NOT NULL,
	WestSideXMinusMaxHeight FLOAT NOT NULL,
	SouthSideZPlusNeedBlend INTEGER NOT NULL,
	SouthSideZPlusMinHeight FLOAT NOT NULL,
	SouthSideZPlusMaxHeight FLOAT NOT NULL,
	NorthSideZMinusNeedBlend INTEGER NOT NULL,
	NorthSideZMinusMinHeight FLOAT NOT NULL,
	NorthSideZMinusMaxHeight FLOAT NOT NULL,
	PRIMARY KEY(Hash)
);

create table NoiseValuesQuadrant(
	Hash TEXT NOT NULL,
	NoiseType INTEGER NOT NULL,
	RotationType INTEGER NOT NULL,
	NoiseSeed INTEGER NOT NULL,
	Frequency REAL NOT NULL,
	FractalType INTEGER NOT NULL,
	FractalOctaves INTEGER NOT NULL,
	FractalLacunarity REAL NOT NULL,
	FractalGain REAL NOT NULL,
	FractalWeightedStrength REAL NOT NULL,
	FractalPingPongStrength REAL NOT NULL,
	CellularDistanceFunction INTEGER NOT NULL,
	CellularReturnType INTEGER NOT NULL,
	CellularJitter REAL NOT NULL,
	WarpNoiseDomainWarpType INTEGER NOT NULL,
	WarpNoiseRotationType3D INTEGER NOT NULL,
	WarpNoiseSeed INTEGER NOT NULL,
	WarpNoiseDomainWarpAmp REAL NOT NULL,
	WarpNoiseFrequency REAL NOT NULL,
	WarpNoieseFractalType INTEGER NOT NULL,
	WarpNoiseFractalOctaves INTEGER NOT NULL,
	WarpNoiseFractalLacunarity REAL NOT NULL,
	WarpNoiseFractalGain REAL NOT NULL,
	Amplitude INTEGER NOT NULL,
	ScaleFactor REAL NOT NULL,
	DesideredMinHeight REAL NOT NULL,
	DesideredMinHeigthMandatory INTEGER NOT NULL,
	PRIMARY KEY(Hash)
);