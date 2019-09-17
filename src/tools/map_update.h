#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))

struct CTilesetMoveData
{
	const char *m_pImageName;
	const int m_NumMovedTiles;
	const int *m_pOldData;
	const int *m_pNewData;
};

struct CTilesetUpdateData
{
	const char *m_pImageName;
	const int m_NumReplacedTiles;
	const int m_NumRemovedTiles;
	const int m_NumMoves;
	const int *m_pOldData;
	const int *m_pNewData;
	const int *m_pRemoveData;
	const CTilesetMoveData *m_pMoveData;
};

static const char *s_aImageNames[] = {
	"bg_cloud1",
	"bg_cloud2",
	"bg_cloud3",
	"desert_doodads",
	"desert_main",
	"desert_mountains",
	"desert_mountains2",
	"desert_sun",
	"generic_deathtiles",
	"generic_lamps",
	"generic_shadows",
	"generic_unhookable",
	"grass_doodads",
	"grass_main",
	"jungle_background",
	"jungle_deathtiles",
	"jungle_doodads",
	"jungle_main",
	"jungle_midground",
	"jungle_unhookables",
	"light",
	"moon",
	"mountains",
	"snow",
	"stars",
	"sun",
	"winter_doodads",
	"winter_main",
	"winter_mountains",
	"winter_mountains2",
	"winter_mountains3"
};

// generic_shadows
static const int s_aShadowIndicesMoved[] = {
	 17,  18,  33,  34,  19,  49,  50,  65,  66,  51,  81,  82,  97,  98,  83
};

// grass_doodads
static const int s_aGrassDoodadsIndicesOld[] = {
	  1,   2,   3,  17,  18,  19,  33,  34,  35,   4,   5,   6,   7,  20,  21,  22,
	 23,  10,  11,  26,  27,  24,  25,  30,  36,  37,  39,  40,  41,  42,  43,  44,
	 45,  48,  49,  50,  51,  52,  53,  54,  55,  64,  65,  66,  67,  68,  69,  70,
	 71,  80,  81,  82,  83,  84,  85,  86,  87,  56,  57,  58,  72,  73,  74,  59,
	 60,  61,  75,  76,  77,  90,  91,  92,  93,  94,  95, 106, 107, 108, 109, 110,
	111, 122, 123, 124, 125, 126, 127, 138, 139, 140, 141, 142, 143, 154, 155, 156,
	157, 158, 159, 170, 171, 172, 173, 174, 175, 101, 102, 103, 104, 117, 118, 119,
	120, 133, 134, 135, 136, 149, 150, 151, 152, 165, 166, 167, 168
};
static const int s_aGrassDoodadsIndicesNew[] = {
	217, 218, 219, 233, 234, 235, 249, 250, 251,   3,   4,   5,   6,  19,  20,  21,
	 22,   9,  10,  25,  26,   7,   8,  13,  23,  24,  49,  41,  45,  46,  47,  38,
	 39,  51,  52,  53,  54,  55,  56,  57,  58,  67,  68,  69,  70,  71,  72,  73,
	 74,  83,  84,  85,  86,  87,  88,  89,  90,  64,  65,  66,  80,  81,  82,  32,
	 33,  34,  48,  49,  50, 106, 107, 108, 109, 110, 111, 122, 123, 124, 125, 126,
	127, 138, 139, 140, 141, 142, 143, 154, 155, 156, 157, 158, 159, 170, 171, 172,
	173, 174, 175, 186, 187, 188, 189, 190, 191, 102, 103, 104, 105, 118, 119, 120,
	121, 134, 135, 136, 137, 150, 151, 152, 153, 166, 167, 168, 169
};
static const int s_aGrassDoodadsIndicesDel[] = {
	  8,   9,  12,  13,  15,  16,  28,  29,  31,  32,  38,  46,  47,  62,  63,  78,
	 79,  88,  89, 105, 121, 137, 153, 169, 181, 182, 183, 184, 185, 186, 187, 188,
	189, 190, 191, 197, 198, 199, 200, 201, 202, 203, 213, 214, 215, 216, 217, 218,
	219, 229, 230, 231, 232, 233, 234, 235, 245, 246, 247, 248, 249, 250, 251
};

// winter_main
static const int s_aWinterMainIndicesOld[] = {
	167, 168, 169, 170, 171, 182, 183, 184, 185, 186, 187, 198, 199, 200, 201, 202,
	203, 174, 177, 178, 179, 180, 193, 194, 195, 196, 197, 209, 210, 211, 212, 215,
	216, 231, 232, 248, 249, 250, 251, 252, 253, 254, 255, 229, 230, 224, 225, 226,
	227, 228
};
static const int s_aWinterMainIndicesNew[] = {
	219, 220, 221, 222, 223, 234, 235, 236, 237, 238, 239, 250, 251, 252, 253, 254,
	255,  95, 160, 161, 162, 163, 192, 193, 194, 195, 196, 176, 177, 178, 179, 232,
	233, 248, 249, 240, 241, 242, 243, 244, 245, 246, 247, 224, 225, 226, 227, 228,
	229, 230
};
static const int s_aWinterMainIndicesDel[] = {
	 95, 160, 161, 162, 163, 165, 172, 166, 173, 175, 176, 181, 188, 189, 190, 191,
	192, 204, 205, 206, 207, 208, 213, 214, 217, 218, 219, 220, 221, 222, 223, 233,
	234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247
};

// desert_main
static const int s_aDesertMainIndicesDel[] = {
	  8,   9,  10,  11,  12,  13,  14,  15,  24,  25,  26,  27,  28,  29,  30,  31,
	 40,  41,  42,  43,  44,  45,  46,  47,  56,  57,  58,  59,  60,  61,  62,  63,
	 72,  73,  74,  75,  76,  77,  78,  79
};
static const int s_aDesertMainShadowIndicesOld[] = {
	 92,  93, 108, 109,  94, 124, 125, 140, 141, 126, 156, 157, 172, 173, 158
};
static const CTilesetMoveData s_aDesertMainMoveData[] = {
	{
		"generic_shadows",
		ARRAY_SIZE(s_aDesertMainShadowIndicesOld),
		s_aDesertMainShadowIndicesOld,
		s_aShadowIndicesMoved
	}
};

// generic_unhookable
static const int s_aGenericUnhookableIndicesDel[] = {
	 38,  54,  70,  86,  45,  61,  77,  93, 166, 182, 198, 214
};

// grass_main
static const int s_aGrassMainIndicesDel[] = {
	 14,  15,  30,  31,  46,  47,  61,  62,  63,  96,  97,  98,  99, 100, 101, 102,
	112, 113, 114, 115, 116, 117, 118, 128, 129, 130, 131, 132, 133, 134, 144, 145,
	146, 147, 148, 149, 150, 160, 161, 162, 163, 164, 165, 166
};
static const int s_aGrassMainShadowIndicesOld[] = {
	 76,  77,  92,  93,  78, 108, 109, 124, 125, 110, 140, 141, 156, 157, 142
};
static const CTilesetMoveData s_aGrassMainMoveData[] = {
	{
		"generic_shadows",
		ARRAY_SIZE(s_aGrassMainShadowIndicesOld),
		s_aGrassMainShadowIndicesOld,
		s_aShadowIndicesMoved
	}
};

// jungle_main
static const int s_aJungleMainShadowIndicesOld[] = {
	 76,  77,  92,  93,  78, 108, 109, 124, 125, 110, 140, 141, 156, 157, 142
};
static const CTilesetMoveData s_aJungleMainMoveData[] = {
	{
		"generic_shadows",
		ARRAY_SIZE(s_aJungleMainShadowIndicesOld),
		s_aJungleMainShadowIndicesOld,
		s_aShadowIndicesMoved
	}
};

// uodate data
static const CTilesetUpdateData s_TilesetUpdateData[] = {
	{
		"grass_doodads",
		ARRAY_SIZE(s_aGrassDoodadsIndicesOld),
		ARRAY_SIZE(s_aGrassDoodadsIndicesDel),
		0,
		s_aGrassDoodadsIndicesOld,
		s_aGrassDoodadsIndicesNew,
		s_aGrassDoodadsIndicesDel,
		0
	},
	{
		"winter_main",
		ARRAY_SIZE(s_aWinterMainIndicesOld),
		ARRAY_SIZE(s_aWinterMainIndicesDel),
		0,
		s_aWinterMainIndicesOld,
		s_aWinterMainIndicesNew,
		s_aWinterMainIndicesDel,
		0
	},
	{
		"desert_main",
		0,
		ARRAY_SIZE(s_aDesertMainIndicesDel),
		ARRAY_SIZE(s_aDesertMainMoveData),
		0,
		0,
		s_aDesertMainIndicesDel,
		s_aDesertMainMoveData
	},
	{
		"generic_unhookable",
		0,
		ARRAY_SIZE(s_aGenericUnhookableIndicesDel),
		0,
		0,
		0,
		s_aGenericUnhookableIndicesDel,
		0
	},
	{
		"grass_main",
		0,
		ARRAY_SIZE(s_aGrassMainIndicesDel),
		ARRAY_SIZE(s_aGrassMainMoveData),
		0,
		0,
		s_aGrassMainIndicesDel,
		s_aGrassMainMoveData
	},
	{
		"jungle_main",
		0,
		0,
		ARRAY_SIZE(s_aJungleMainMoveData),
		0,
		0,
		0,
		s_aJungleMainMoveData
	}
};
