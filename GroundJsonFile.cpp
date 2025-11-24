//#include "pch.h"

#include "MapManager.h"

namespace TheWorld_MapManager
{
	std::string MapManager::defaultGroudJsonContent =
"{\n \
		\"continents\" :\n \
	[\n \
	{\n \
		\"id\" : 0,\n \
			\"name\" : \"FirstLand\",\n \
			\"layout\" : \"default\",\n \
			\"margin_01\" : \"NW:-35,-25;SE:35,64\"\n \
	},\n \
		{\n \
			\"id\" : 1,\n \
			\"name\" : \"GreenLand\",\n \
			\"layout\" : \"green\",\n \
			\"margin_01\" : \"\"\n \
		}\n \
	],\n \
		\"_comment_01\" : \"Layouts which are not the default one can contain override values one or more textures\",\n \
		\"_comment_02\" : \"***base-albedo*** contains a string of rgb values seaprated by commas each in the range 0/255 or 0.0/1.0\",\n \
		\"_comment_03\" : \"***base-scale_????*** contain float values in the range 0.0/16.0\",\n \
		\"ground-texture-layouts\" :\n \
		[\n \
	{\n \
		\"layout-name\" : \"default\",\n \
			\"layout-textures\" :\n \
			[\n \
		{\n \
			\"type\": \"grass\",\n \
				\"sub-types\" :\n \
				[\n \
			{\n \
				\"qualifier\": \"1\",\n \
					\"base-albedo\" : \"255, 255, 255\",\n \
					\"base-scale-ao\" : 1.0,\n \
					\"base-scale-normal\" : 6.0,\n \
					\"base-scale-roughness\" : 1.0,\n \
					\"base-scale-uv\" : 2.0,\n \
					\"folder\" : \"aerial_grass_rock_1k\"\n \
			},\n \
						{\n \
							\"qualifier\": \"2\",\n \
							\"base-albedo\" : \"1.0, 1.0, 1.0\",\n \
							\"base-scale-ao\" : 1.0,\n \
							\"base-scale-normal\" : 6.0,\n \
							\"base-scale-roughness\" : 1.0,\n \
							\"base-scale-uv\" : 2.0,\n \
							\"folder\" : \"rocky_terrain_02_1k\"\n \
						},\n \
						{\n \
							\"qualifier\": \"3\",\n \
							\"base-albedo\" : \"1.0, 1.0, 1.0\",\n \
							\"base-scale-ao\" : 1.0,\n \
							\"base-scale-normal\" : 1.0,\n \
							\"base-scale-roughness\" : 1.0,\n \
							\"base-scale-uv\" : 2.0,\n \
							\"folder\" : \"Ground037_1K\"\n \
						}\n \
				]\n \
		},\n \
				{\n \
					\"type\": \"dirt\",\n \
					\"sub-types\" :\n \
					[\n \
						{\n \
							\"qualifier\": \"1\",\n \
							\"base-albedo\" : \"1.0, 1.0, 1.0\",\n \
							\"base-scale-ao\" : 1.0,\n \
							\"base-scale-normal\" : 1.0,\n \
							\"base-scale-roughness\" : 1.0,\n \
							\"base-scale-uv\" : 1.0,\n \
							\"folder\" : \"leaves\"\n \
						},\n \
						{\n \
							\"qualifier\": \"2\",\n \
							\"base-albedo\" : \"1.0, 1.0, 1.0\",\n \
							\"base-scale-ao\" : 1.0,\n \
							\"base-scale-normal\" : 1.0,\n \
							\"base-scale-roughness\" : 1.0,\n \
							\"base-scale-uv\" : 1.0,\n \
							\"folder\" : \"dirt_aerial_02_1k\"\n \
						},\n \
						{\n \
							\"qualifier\": \"3\",\n \
							\"base-albedo\" : \"1.0, 1.0, 1.0\",\n \
							\"base-scale-ao\" : 1.0,\n \
							\"base-scale-normal\" : 1.0,\n \
							\"base-scale-roughness\" : 1.0,\n \
							\"base-scale-uv\" : 1.0,\n \
							\"folder\" : \"dirt_aerial_03_1k\"\n \
						}\n \
					]\n \
				},\n \
				{\n \
					\"type\": \"beach\",\n \
					\"sub-types\" :\n \
					[\n \
						{\n \
							\"qualifier\": \"1\",\n \
							\"base-albedo\" : \"1.0, 1.0, 1.0\",\n \
							\"base-scale-ao\" : 1.0,\n \
							\"base-scale-normal\" : 1.0,\n \
							\"base-scale-roughness\" : 1.0,\n \
							\"base-scale-uv\" : 1.0,\n \
							\"folder\" : \"sand\"\n \
						},\n \
						{\n \
							\"qualifier\": \"2\",\n \
							\"base-albedo\" : \"1.0, 1.0, 1.0\",\n \
							\"base-scale-ao\" : 1.0,\n \
							\"base-scale-normal\" : 6.0,\n \
							\"base-scale-roughness\" : 1.0,\n \
							\"base-scale-uv\" : 4.0,\n \
							\"folder\" : \"Ground049C_1K\"\n \
						},\n \
						{\n \
							\"qualifier\": \"3\",\n \
							\"base-albedo\" : \"1.0, 1.0, 1.0\",\n \
							\"base-scale-ao\" : 1.0,\n \
							\"base-scale-normal\" : 1.0,\n \
							\"base-scale-roughness\" : 1.0,\n \
							\"base-scale-uv\" : 1.0,\n \
							\"folder\" : \"aerial_beach_01_1k\"\n \
						},\n \
						{\n \
							\"qualifier\": \"4\",\n \
							\"base-albedo\" : \"1.0, 1.0, 1.0\",\n \
							\"base-scale-ao\" : 1.0,\n \
							\"base-scale-normal\" : 1.0,\n \
							\"base-scale-roughness\" : 1.0,\n \
							\"base-scale-uv\" : 1.0,\n \
							\"folder\" : \"aerial_beach_02_1k\"\n \
						}\n \
					]\n \
				},\n \
				{\n \
					\"type\": \"desert\",\n \
					\"sub-types\" :\n \
					[\n \
						{\n \
							\"qualifier\": \"1\",\n \
							\"base-albedo\" : \"1.0, 1.0, 1.0\",\n \
							\"base-scale-ao\" : 1.0,\n \
							\"base-scale-normal\" : 1.0,\n \
							\"base-scale-roughness\" : 1.0,\n \
							\"base-scale-uv\" : 1.0,\n \
							\"folder\" : \"Ground080_1K\"\n \
						},\n \
						{\n \
							\"qualifier\": \"2\",\n \
							\"base-albedo\" : \"1.0, 1.0, 1.0\",\n \
							\"base-scale-ao\" : 1.0,\n \
							\"base-scale-normal\" : 1.0,\n \
							\"base-scale-roughness\" : 1.0,\n \
							\"base-scale-uv\" : 1.0,\n \
							\"folder\" : \"Rock029_1K\"\n \
						}\n \
					]\n \
				},\n \
				{\n \
					\"type\": \"rocks\",\n \
					\"sub-types\" :\n \
					[\n \
						{\n \
							\"qualifier\": \"1\",\n \
							\"base-albedo\" : \"1.0, 1.0, 1.0\",\n \
							\"base-scale-ao\" : 1.0,\n \
							\"base-scale-normal\" : 6.0,\n \
							\"base-scale-roughness\" : 1.0,\n \
							\"base-scale-uv\" : 1.0,\n \
							\"folder\" : \"Rock028_1K\"\n \
						},\n \
						{\n \
							\"qualifier\": \"2\",\n \
							\"base-albedo\" : \"1.0, 1.0, 1.0\",\n \
							\"base-scale-ao\" : 1.0,\n \
							\"base-scale-normal\" : 1.0,\n \
							\"base-scale-roughness\" : 1.0,\n \
							\"base-scale-uv\" : 1.0,\n \
							\"folder\" : \"aerial_rocks_02_1k\"\n \
						}\n \
					]\n \
				},\n \
				{\n \
					\"type\": \"snow\",\n \
					\"sub-types\" :\n \
					[\n \
						{\n \
							\"qualifier\": \"1\",\n \
							\"base-albedo\" : \"1.0, 1.0, 1.0\",\n \
							\"base-scale-ao\" : 1.0,\n \
							\"base-scale-normal\" : 1.0,\n \
							\"base-scale-roughness\" : 0.5,\n \
							\"base-scale-uv\" : 1.0,\n \
							\"folder\" : \"snow_02_1k\"\n \
						},\n \
						{\n \
							\"qualifier\": \"2\",\n \
							\"base-albedo\" : \"1.0, 1.0, 1.0\",\n \
							\"base-scale-ao\" : 1.0,\n \
							\"base-scale-normal\" : 1.0,\n \
							\"base-scale-roughness\" : 1.0,\n \
							\"base-scale-uv\" : 1.0,\n \
							\"folder\" : \"snow_field_aerial_1k\"\n \
						}\n \
					]\n \
				}\n \
			]\n \
	},\n \
		{\n \
			\"layout-name\" : \"green\",\n \
			\"layout-textures\" :\n \
			[\n \
				{\n \
					\"type\": \"grass\",\n \
					\"sub-types\" :\n \
					[\n \
						{\n \
							\"qualifier\": \"1\",\n \
							\"base-albedo\" : \"255, 255, 255\",\n \
							\"base-scale-ao\" : 1.0,\n \
							\"base-scale-normal\" : 6.0,\n \
							\"base-scale-roughness\" : 1.0,\n \
							\"base-scale-uv\" : 2.0,\n \
							\"folder\" : \"rocky_terrain_02_1k\"\n \
						},\n \
						{\n \
							\"qualifier\": \"2\",\n \
							\"base-albedo\" : \"1.0, 1.0, 1.0\",\n \
							\"base-scale-ao\" : 1.0,\n \
							\"base-scale-normal\" : 6.0,\n \
							\"base-scale-roughness\" : 1.0,\n \
							\"base-scale-uv\" : 2.0,\n \
							\"folder\" : \"rocky_terrain_02_1k\"\n \
						},\n \
						{\n \
							\"qualifier\": \"3\",\n \
							\"base-albedo\" : \"1.0, 1.0, 1.0\",\n \
							\"base-scale-ao\" : 1.0,\n \
							\"base-scale-normal\" : 6.0,\n \
							\"base-scale-roughness\" : 1.0,\n \
							\"base-scale-uv\" : 2.0,\n \
							\"folder\" : \"rocky_terrain_02_1k\"\n \
						}\n \
					]\n \
				}\n \
			]\n \
		}\n \
		]\n \
}";
}