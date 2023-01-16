drop table Quadrant;

create table Quadrant(
	GridStep INTEGER NOT NULL,
	VertexPerSize INTEGER NOT NULL,
	Level INTEGER NOT NULL,
	PosXStart REAL NOT NULL,
	PosZStart REAL NOT NULL,
	PosXEnd REAL NOT NULL,
	PosZEnd REAL NOT NULL,
	Hash TEXT NOT NULL,
	PRIMARY KEY(GridStep, VertexPerSize, Level, PosXStart, PosZStart)
);
