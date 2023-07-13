insert into Params (ParamName, ParamValue) values ("MapName", "TestMap01");

alter table GridVertex ADD NormX REAL;
alter table GridVertex ADD NormY REAL;
alter table GridVertex ADD NormZ REAL;
alter table GridVertex ADD ColorR INTEGER;						-- 0/255
alter table GridVertex ADD ColorG INTEGER;						-- 0/255
alter table GridVertex ADD ColorB INTEGER;						-- 0/255
alter table GridVertex ADD ColorA INTEGER;						-- 0/255
alter table GridVertex ADD LowElevationTexAmount INTEGER;		-- 0/255
alter table GridVertex ADD HighElevationTexAmount INTEGER;		-- 0/255
alter table GridVertex ADD DirtTexAmount INTEGER;				-- 0/255
alter table GridVertex ADD RocksTexAmount INTEGER;				-- 0/255
alter table GridVertex ADD GlobalMapR INTEGER;					-- 0/255
alter table GridVertex ADD GlobalMapG INTEGER;					-- 0/255
alter table GridVertex ADD GlobalMapB INTEGER;					-- 0/255

drop table Quadrant;
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