/*
drop table Quadrant;

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