/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/console.h>
#include <engine/storage.h>
#include <engine/external/pnglite/pnglite.h>
#include <game/gamecore.h> // StrToInts, IntsToStr

#include "mapitems_race.h"

#include "map.h"

static void IntVariableCommand(IConsole::IResult *pResult, void *pUserData);

struct CIntVariableData
{
	IConsole *m_pConsole;
	const char *m_pName;
	int *m_pVariable;
	int m_Def;
	int m_Min;
	int m_Max;

	void Register()
	{
		*m_pVariable = m_Def;
		m_pConsole->Register(m_pName, "i", CFGFLAG_MAPSETTINGS, IntVariableCommand, this, "");
	}
};

void IntVariableCommand(IConsole::IResult *pResult, void *pUserData)
{
	CIntVariableData *pData = (CIntVariableData *)pUserData;

	if(pResult->NumArguments())
	{
		int Val = pResult->GetInteger(0);

		// do clamping
		if(pData->m_Min != pData->m_Max)
		{
			if (Val < pData->m_Min)
				Val = pData->m_Min;
			if (pData->m_Max != 0 && Val > pData->m_Max)
				Val = pData->m_Max;
		}

		*(pData->m_pVariable) = Val;
	}
}

int LoadPNG(CImageInfo *pImg, class IStorage *pStorage, const char *pFilename, int StorageType)
{
	char aCompleteFilename[512];
	unsigned char *pBuffer;
	png_t Png; // ignore_convention

	// open file for reading
	png_init(0,0); // ignore_convention

	IOHANDLE File = pStorage->OpenFile(pFilename, IOFLAG_READ, StorageType, aCompleteFilename, sizeof(aCompleteFilename));
	if(File)
		io_close(File);
	else
	{
		dbg_msg("game/png", "failed to open file. filename='%s'", pFilename);
		return 0;
	}

	int Error = png_open_file(&Png, aCompleteFilename); // ignore_convention
	if(Error != PNG_NO_ERROR)
	{
		dbg_msg("game/png", "failed to open file. filename='%s'", aCompleteFilename);
		if(Error != PNG_FILE_ERROR)
			png_close_file(&Png); // ignore_convention
		return 0;
	}

	if(Png.depth != 8 || (Png.color_type != PNG_TRUECOLOR && Png.color_type != PNG_TRUECOLOR_ALPHA) || Png.width > (2<<12) || Png.height > (2<<12)) // ignore_convention
	{
		dbg_msg("game/png", "invalid format. filename='%s'", aCompleteFilename);
		png_close_file(&Png); // ignore_convention
		return 0;
	}

	pBuffer = (unsigned char *)mem_alloc(Png.width * Png.height * Png.bpp, 1); // ignore_convention
	png_get_data(&Png, pBuffer); // ignore_convention
	png_close_file(&Png); // ignore_convention

	pImg->m_Width = Png.width; // ignore_convention
	pImg->m_Height = Png.height; // ignore_convention
	if(Png.color_type == PNG_TRUECOLOR) // ignore_convention
		pImg->m_Format = CImageInfo::FORMAT_RGB;
	else if(Png.color_type == PNG_TRUECOLOR_ALPHA) // ignore_convention
		pImg->m_Format = CImageInfo::FORMAT_RGBA;
	pImg->m_pData = pBuffer;
	return 1;
}

int LoadExternalImage(IConsole *pConsole, IStorage *pStorage, CEditorImage *pImg, const char *pName)
{
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "mapres/%s.png", pName);

	// load external
	CEditorImage ImgInfo(pConsole);
	int Ret = LoadPNG(&ImgInfo, pStorage, aBuf, IStorage::TYPE_ALL);
	if(Ret)
	{
		*pImg = ImgInfo;
		//pImg->m_Texture = m_pEditor->Graphics()->LoadTextureRaw(ImgInfo.m_Width, ImgInfo.m_Height, ImgInfo.m_Format, ImgInfo.m_pData, CImageInfo::FORMAT_AUTO, IGraphics::TEXLOAD_MULTI_DIMENSION);
		ImgInfo.m_pData = 0;
		pImg->m_External = 1;
	}
	return Ret;
}

CLayerTiles *CreateCustomLayer(CEditorMap *pMap, int Width, int Height, const char *pName, const char *pImageName)
{
	CLayerTiles *pLayer = new CLayerTiles(Width, Height);
	str_copy(pLayer->m_aName, pName, sizeof(pLayer->m_aName));
	pLayer->m_pMap = pMap;
	pLayer->m_Color.a = 0;

	CEditorImage *pImg = new CEditorImage(pMap->m_pConsole);
    pImg->m_External = 1;
    str_copy(pImg->m_aName, pImageName, sizeof(pImg->m_aName));
	pLayer->m_Image = pMap->m_lImages.add(pImg);

	return pLayer;
}

template<typename T>
static int MakeVersion(int i, const T &v)
{ return (i<<16)+sizeof(T); }

CEditorImage::~CEditorImage()
{
	if(m_pData)
	{
		mem_free(m_pData);
		m_pData = 0;
	}
}

void CEditorImage::AnalyseTileFlags()
{
	mem_zero(m_aTileFlagsExtended, sizeof(m_aTileFlagsExtended));

	if(m_Format == CImageInfo::FORMAT_RGB)
	{
		for(int i = 0; i < 256; ++i)
			m_aTileFlags[i] = TILEFLAG_OPAQUE;
	}
	else
	{
		mem_zero(m_aTileFlags, sizeof(m_aTileFlags));
	}

	if(!m_pData)
		return;

	int tw = m_Width/16; // tilesizes
	int th = m_Height/16;
	if(tw == th)
	{
		unsigned char *pPixelData = (unsigned char *)m_pData;

		int TileID = 0;
		for(int ty = 0; ty < 16; ty++)
			for(int tx = 0; tx < 16; tx++, TileID++)
			{
				bool Opaque = true;
				bool Invisible = true;
				bool OneColor = true;
				unsigned char StartColor[4];
				mem_copy(StartColor, &pPixelData[((ty*tw)*m_Width + tx*tw)*4], sizeof(StartColor));
				for(int x = 0; x < tw; x++)
					for(int y = 0; y < th; y++)
					{
						int p = (ty*tw+y)*m_Width + tx*tw+x;
						if(pPixelData[p*4+3] < 250)
							Opaque = false;
						if(pPixelData[p*4+3] != 0)
							Invisible = false;
						if(mem_comp(StartColor, &pPixelData[p*4], sizeof(StartColor)) != 0)
							OneColor = false;
					}

				if(Opaque)
					m_aTileFlags[TileID] |= TILEFLAG_OPAQUE;
				if(Invisible)
					m_aTileFlagsExtended[TileID] |= TILEFLAG_EX_INVISIBLE;
				if(OneColor)
					m_aTileFlagsExtended[TileID] |= TILEFLAG_EX_ONE_COLOR;
			}
	}
}

CLayerGroup::CLayerGroup()
{
	m_aName[0] = 0;
	m_SaveToMap = true;
	m_GameGroup = false;
	m_OffsetX = 0;
	m_OffsetY = 0;
	m_ParallaxX = 100;
	m_ParallaxY = 100;

	m_UseClipping = 0;
	m_ClipX = 0;
	m_ClipY = 0;
	m_ClipW = 0;
	m_ClipH = 0;
}

CLayerGroup::~CLayerGroup()
{
	Clear();
}

void CLayerGroup::AddLayer(CLayer *l)
{
	m_lLayers.add(l);
}

void CLayerGroup::DeleteLayer(int Index)
{
	if(Index < 0 || Index >= m_lLayers.size()) return;
	delete m_lLayers[Index];
	m_lLayers.remove_index(Index);
}

void CLayerGroup::DeleteLayer(CLayer *pLayer)
{
	delete pLayer;
	m_lLayers.remove(pLayer);
}

void CLayerGroup::GetSize(float *w, float *h) const
{
	*w = 0; *h = 0;
	for(int i = 0; i < m_lLayers.size(); i++)
	{
		float lw, lh;
		m_lLayers[i]->GetSize(&lw, &lh);
		*w = max(*w, lw);
		*h = max(*h, lh);
	}
}

int CLayerGroup::SwapLayers(int Index0, int Index1)
{
	if(Index0 < 0 || Index0 >= m_lLayers.size()) return Index0;
	if(Index1 < 0 || Index1 >= m_lLayers.size()) return Index0;
	if(Index0 == Index1) return Index0;
	tl_swap(m_lLayers[Index0], m_lLayers[Index1]);
	return Index1;
}

void CEditorMap::Clean()
{
	m_lGroups.delete_all();
	m_lEnvelopes.delete_all();
	m_lImages.delete_all();

	m_MapInfo.Reset();

	m_pGameLayer = 0x0;
	m_pGameGroup = 0x0;
}

void CEditorMap::DeleteEnvelope(int Index)
{
	if(Index < 0 || Index >= m_lEnvelopes.size())
		return;

	// fix links between envelopes and quads
	for(int i = 0; i < m_lGroups.size(); ++i)
		for(int j = 0; j < m_lGroups[i]->m_lLayers.size(); ++j)
			if(m_lGroups[i]->m_lLayers[j]->m_Type == LAYERTYPE_QUADS)
			{
				CLayerQuads *pLayer = static_cast<CLayerQuads *>(m_lGroups[i]->m_lLayers[j]);
				for(int k = 0; k < pLayer->m_lQuads.size(); ++k)
				{
					if(pLayer->m_lQuads[k].m_PosEnv == Index)
						pLayer->m_lQuads[k].m_PosEnv = -1;
					else if(pLayer->m_lQuads[k].m_PosEnv > Index)
						pLayer->m_lQuads[k].m_PosEnv--;
					if(pLayer->m_lQuads[k].m_ColorEnv == Index)
						pLayer->m_lQuads[k].m_ColorEnv = -1;
					else if(pLayer->m_lQuads[k].m_ColorEnv > Index)
						pLayer->m_lQuads[k].m_ColorEnv--;
				}
			}
			else if(m_lGroups[i]->m_lLayers[j]->m_Type == LAYERTYPE_TILES)
			{
				CLayerTiles *pLayer = static_cast<CLayerTiles *>(m_lGroups[i]->m_lLayers[j]);
				if(pLayer->m_ColorEnv == Index)
					pLayer->m_ColorEnv = -1;
				if(pLayer->m_ColorEnv > Index)
					pLayer->m_ColorEnv--;
			}

	m_lEnvelopes.remove_index(Index);
}

void CEditorMap::MakeGameLayer(CLayer *pLayer)
{
	m_pGameLayer = (CLayerGame *)pLayer;
	m_pGameLayer->m_pMap = this;
	//m_pGameLayer->m_Texture = m_pEditor->m_EntitiesTexture;
}

void CEditorMap::MakeGameGroup(CLayerGroup *pGroup)
{
	m_pGameGroup = pGroup;
	m_pGameGroup->m_GameGroup = true;
	str_copy(m_pGameGroup->m_aName, "Game", sizeof(m_pGameGroup->m_aName));
}

int CEditorMap::Save(class IStorage *pStorage, const char *pFileName)
{
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "saving to '%s'...", pFileName);
	m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "editor", aBuf);
	CDataFileWriter df;
	if(!df.Open(pStorage, pFileName))
	{
		str_format(aBuf, sizeof(aBuf), "failed to open file '%s'...", pFileName);
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "editor", aBuf);
		return 0;
	}

	// save version
	{
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "editor", "saving version");

		CMapItemVersion Item;
		Item.m_Version = CMapItemVersion::CURRENT_VERSION;
		df.AddItem(MAPITEMTYPE_VERSION, 0, sizeof(Item), &Item);
	}

	// save map info
	{
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "editor", "saving info");

		CMapItemInfo Item;
		Item.m_Version = CMapItemInfo::CURRENT_VERSION;

		if(m_MapInfo.m_aAuthor[0])
			Item.m_Author = df.AddData(str_length(m_MapInfo.m_aAuthor)+1, m_MapInfo.m_aAuthor);
		else
			Item.m_Author = -1;
		if(m_MapInfo.m_aVersion[0])
			Item.m_MapVersion = df.AddData(str_length(m_MapInfo.m_aVersion)+1, m_MapInfo.m_aVersion);
		else
			Item.m_MapVersion = -1;
		if(m_MapInfo.m_aCredits[0])
			Item.m_Credits = df.AddData(str_length(m_MapInfo.m_aCredits)+1, m_MapInfo.m_aCredits);
		else
			Item.m_Credits = -1;
		if(m_MapInfo.m_aLicense[0])
			Item.m_License = df.AddData(str_length(m_MapInfo.m_aLicense)+1, m_MapInfo.m_aLicense);
		else
			Item.m_License = -1;

		df.AddItem(MAPITEMTYPE_INFO, 0, sizeof(Item), &Item);
	}

	// save images
	for(int i = 0; i < m_lImages.size(); i++)
	{
		CEditorImage *pImg = m_lImages[i];

		m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "editor", "saving image");

		// analyse the image for when saving (should be done when we load the image)
		// TODO!
		pImg->AnalyseTileFlags();

		CMapItemImage Item;
		Item.m_Version = CMapItemImage::CURRENT_VERSION;

		Item.m_Width = pImg->m_Width;
		Item.m_Height = pImg->m_Height;
		Item.m_External = pImg->m_External;
		Item.m_ImageName = df.AddData(str_length(pImg->m_aName)+1, pImg->m_aName);
		if(pImg->m_External)
			Item.m_ImageData = -1;
		else
		{
			int PixelSize = pImg->m_Format == CImageInfo::FORMAT_RGB ? 3 : 4;
			Item.m_ImageData = df.AddData(Item.m_Width*Item.m_Height*PixelSize, pImg->m_pData);
		}
		Item.m_Format = pImg->m_Format;
		df.AddItem(MAPITEMTYPE_IMAGE, i, sizeof(Item), &Item);
	}

	// save layers
	int LayerCount = 0, GroupCount = 0;
	for(int g = 0; g < m_lGroups.size(); g++)
	{
		CLayerGroup *pGroup = m_lGroups[g];
		if(!pGroup->m_SaveToMap)
			continue;

		m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "editor", "saving layer group");

		CMapItemGroup GItem;
		GItem.m_Version = CMapItemGroup::CURRENT_VERSION;

		GItem.m_ParallaxX = pGroup->m_ParallaxX;
		GItem.m_ParallaxY = pGroup->m_ParallaxY;
		GItem.m_OffsetX = pGroup->m_OffsetX;
		GItem.m_OffsetY = pGroup->m_OffsetY;
		GItem.m_UseClipping = pGroup->m_UseClipping;
		GItem.m_ClipX = pGroup->m_ClipX;
		GItem.m_ClipY = pGroup->m_ClipY;
		GItem.m_ClipW = pGroup->m_ClipW;
		GItem.m_ClipH = pGroup->m_ClipH;
		GItem.m_StartLayer = LayerCount;
		GItem.m_NumLayers = 0;

		// save group name
		StrToInts(GItem.m_aName, sizeof(GItem.m_aName)/sizeof(int), pGroup->m_aName);

		for(int l = 0; l < pGroup->m_lLayers.size(); l++)
		{
			if(!pGroup->m_lLayers[l]->m_SaveToMap)
				continue;

			if(pGroup->m_lLayers[l]->m_Type == LAYERTYPE_TILES)
			{
				m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "editor", "saving tiles layer");
				CLayerTiles *pLayer = (CLayerTiles *)pGroup->m_lLayers[l];
				pLayer->PrepareForSave();

				CMapItemLayerTilemap Item;
				Item.m_Version = CMapItemLayerTilemap::CURRENT_VERSION;

				Item.m_Layer.m_Version = 0;
				Item.m_Layer.m_Flags = pLayer->m_Flags;
				Item.m_Layer.m_Type = pLayer->m_Type;

				Item.m_Color = pLayer->m_Color;
				Item.m_ColorEnv = pLayer->m_ColorEnv;
				Item.m_ColorEnvOffset = pLayer->m_ColorEnvOffset;

				Item.m_Width = pLayer->m_Width;
				Item.m_Height = pLayer->m_Height;
				Item.m_Flags = pLayer->m_Game ? TILESLAYERFLAG_GAME : 0;
				Item.m_Image = pLayer->m_Image;
				Item.m_Data = df.AddData(pLayer->m_SaveTilesSize, pLayer->m_pSaveTiles);

				// save layer name
				StrToInts(Item.m_aName, sizeof(Item.m_aName)/sizeof(int), pLayer->m_aName);

				df.AddItem(MAPITEMTYPE_LAYER, LayerCount, sizeof(Item), &Item);

				GItem.m_NumLayers++;
				LayerCount++;
			}
			else if(pGroup->m_lLayers[l]->m_Type == LAYERTYPE_QUADS)
			{
				m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "editor", "saving quads layer");
				CLayerQuads *pLayer = (CLayerQuads *)pGroup->m_lLayers[l];
				if(pLayer->m_lQuads.size())
				{
					CMapItemLayerQuads Item;
					Item.m_Version = CMapItemLayerQuads::CURRENT_VERSION;
					Item.m_Layer.m_Version = 0;
					Item.m_Layer.m_Flags = pLayer->m_Flags;
					Item.m_Layer.m_Type = pLayer->m_Type;
					Item.m_Image = pLayer->m_Image;

					// add the data
					Item.m_NumQuads = pLayer->m_lQuads.size();
					Item.m_Data = df.AddDataSwapped(pLayer->m_lQuads.size()*sizeof(CQuad), pLayer->m_lQuads.base_ptr());

					// save layer name
					StrToInts(Item.m_aName, sizeof(Item.m_aName)/sizeof(int), pLayer->m_aName);

					df.AddItem(MAPITEMTYPE_LAYER, LayerCount, sizeof(Item), &Item);

					GItem.m_NumLayers++;
					LayerCount++;
				}
			}
		}

		df.AddItem(MAPITEMTYPE_GROUP, GroupCount++, sizeof(GItem), &GItem);
	}

	// check for bezier curve envelopes, otherwise use older, smaller envelope points
	int Version = CMapItemEnvelope_v2::CURRENT_VERSION;
	int Size = sizeof(CEnvPoint_v1);	
	for(int e = 0; e < m_lEnvelopes.size(); e++)
	{
		for(int p = 0; p < m_lEnvelopes[e]->m_lPoints.size(); p++)
		{
			if(m_lEnvelopes[e]->m_lPoints[p].m_Curvetype == CURVETYPE_BEZIER)
			{
				Version = CMapItemEnvelope::CURRENT_VERSION;
				Size = sizeof(CEnvPoint);
				break;
			}
		}
	}

	// save envelopes
	int PointCount = 0;
	for(int e = 0; e < m_lEnvelopes.size(); e++)
	{
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "editor", "saving envelope");

		CMapItemEnvelope Item;
		Item.m_Version = Version;
		Item.m_Channels = m_lEnvelopes[e]->m_Channels;
		Item.m_StartPoint = PointCount;
		Item.m_NumPoints = m_lEnvelopes[e]->m_lPoints.size();
		Item.m_Synchronized = m_lEnvelopes[e]->m_Synchronized;
		StrToInts(Item.m_aName, sizeof(Item.m_aName)/sizeof(int), m_lEnvelopes[e]->m_aName);

		df.AddItem(MAPITEMTYPE_ENVELOPE, e, sizeof(Item), &Item);
		PointCount += Item.m_NumPoints;
	}

	m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "editor", "saving env points");

	// save points
	int TotalSize = Size * PointCount;
	unsigned char *pPoints = (unsigned char *)mem_alloc(TotalSize, 1);
	int Offset = 0;
	for(int e = 0; e < m_lEnvelopes.size(); e++)
	{
		for(int p = 0; p < m_lEnvelopes[e]->m_lPoints.size(); p++)
		{
			mem_copy(pPoints + Offset, &(m_lEnvelopes[e]->m_lPoints[p]), Size);
			Offset += Size;
		}
	}

	df.AddItem(MAPITEMTYPE_ENVPOINTS, 0, TotalSize, pPoints);
	mem_free(pPoints);

	// finish the data file
	df.Finish();
	m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "editor", "saving done");

	return 1;
}

int CEditorMap::Load(class IStorage *pStorage, const char *pFileName, int StorageType, int Mod)
{
	CDataFileReader DataFile;
	if(!DataFile.Open(pStorage, pFileName, StorageType))
		return 0;

	Clean();

	// check version
	CMapItemVersion *pItem = (CMapItemVersion *)DataFile.FindItem(MAPITEMTYPE_VERSION, 0);
	if(!pItem)
	{
		return 0;
	}
	else if(pItem->m_Version == CMapItemVersion::CURRENT_VERSION)
	{
		// load map info
		{
			CMapItemInfo *pItem = (CMapItemInfo *)DataFile.FindItem(MAPITEMTYPE_INFO, 0);
			if(pItem && pItem->m_Version == 1)
			{
				if(pItem->m_Author > -1)
					str_copy(m_MapInfo.m_aAuthor, (char *)DataFile.GetData(pItem->m_Author), sizeof(m_MapInfo.m_aAuthor));
				if(pItem->m_MapVersion > -1)
					str_copy(m_MapInfo.m_aVersion, (char *)DataFile.GetData(pItem->m_MapVersion), sizeof(m_MapInfo.m_aVersion));
				if(pItem->m_Credits > -1)
					str_copy(m_MapInfo.m_aCredits, (char *)DataFile.GetData(pItem->m_Credits), sizeof(m_MapInfo.m_aCredits));
				if(pItem->m_License > -1)
					str_copy(m_MapInfo.m_aLicense, (char *)DataFile.GetData(pItem->m_License), sizeof(m_MapInfo.m_aLicense));
			}
		}

		// load images
		{
			int Start, Num;
			DataFile.GetType( MAPITEMTYPE_IMAGE, &Start, &Num);
			for(int i = 0; i < Num; i++)
			{
				CMapItemImage *pItem = (CMapItemImage *)DataFile.GetItem(Start+i, 0, 0);
				char *pName = (char *)DataFile.GetData(pItem->m_ImageName);

				// copy base info
				CEditorImage *pImg = new CEditorImage(m_pConsole);
				pImg->m_External = pItem->m_External;

				if(pItem->m_External || (pItem->m_Version > 1 && pItem->m_Format != CImageInfo::FORMAT_RGB && pItem->m_Format != CImageInfo::FORMAT_RGBA))
				{
					LoadExternalImage(m_pConsole, pStorage, pImg, pName);
				}
				else
				{
					pImg->m_Width = pItem->m_Width;
					pImg->m_Height = pItem->m_Height;
					pImg->m_Format = pItem->m_Version == 1 ? CImageInfo::FORMAT_RGBA : pItem->m_Format;
					int PixelSize = pImg->m_Format == CImageInfo::FORMAT_RGB ? 3 : 4;

					// copy image data
					void *pData = DataFile.GetData(pItem->m_ImageData);
					pImg->m_pData = mem_alloc(pImg->m_Width*pImg->m_Height*PixelSize, 1);
					mem_copy(pImg->m_pData, pData, pImg->m_Width*pImg->m_Height*PixelSize);
					//pImg->m_Texture = m_pEditor->Graphics()->LoadTextureRaw(pImg->m_Width, pImg->m_Height, pImg->m_Format, pImg->m_pData, CImageInfo::FORMAT_AUTO, IGraphics::TEXLOAD_MULTI_DIMENSION);
				}

				// copy image name
				if(pName)
					str_copy(pImg->m_aName, pName, 128);

				m_lImages.add(pImg);

				// unload image
				DataFile.UnloadData(pItem->m_ImageData);
				DataFile.UnloadData(pItem->m_ImageName);
			}
		}

		CLayerTiles *pTele = 0;
		CLayerTiles *pSpeedup = 0;
		int Tele = -1;
		int Speedup = -1;

		// load groups
		{
			int LayersStart, LayersNum;
			DataFile.GetType(MAPITEMTYPE_LAYER, &LayersStart, &LayersNum);

			int Start, Num;
			DataFile.GetType(MAPITEMTYPE_GROUP, &Start, &Num);
			for(int g = 0; g < Num; g++)
			{
				CMapItemGroup *pGItem = (CMapItemGroup *)DataFile.GetItem(Start+g, 0, 0);

				if(pGItem->m_Version < 1 || pGItem->m_Version > CMapItemGroup::CURRENT_VERSION)
					continue;

				CLayerGroup *pGroup = NewGroup();
				pGroup->m_ParallaxX = pGItem->m_ParallaxX;
				pGroup->m_ParallaxY = pGItem->m_ParallaxY;
				pGroup->m_OffsetX = pGItem->m_OffsetX;
				pGroup->m_OffsetY = pGItem->m_OffsetY;

				if(pGItem->m_Version >= 2)
				{
					pGroup->m_UseClipping = pGItem->m_UseClipping;
					pGroup->m_ClipX = pGItem->m_ClipX;
					pGroup->m_ClipY = pGItem->m_ClipY;
					pGroup->m_ClipW = pGItem->m_ClipW;
					pGroup->m_ClipH = pGItem->m_ClipH;
				}

				// load group name
				if(pGItem->m_Version >= 3)
					IntsToStr(pGItem->m_aName, sizeof(pGroup->m_aName)/sizeof(int), pGroup->m_aName);

				for(int l = 0; l < pGItem->m_NumLayers; l++)
				{
					CLayer *pLayer = 0;
					CMapItemLayer *pLayerItem = (CMapItemLayer *)DataFile.GetItem(LayersStart+pGItem->m_StartLayer+l, 0, 0);
					if(!pLayerItem)
						continue;

					if(pLayerItem->m_Type == LAYERTYPE_TILES)
					{
						CMapItemLayerTilemap *pTilemapItem = (CMapItemLayerTilemap *)pLayerItem;
						CLayerTiles *pTiles = 0;

						bool IsRaceLayer = pTilemapItem->m_Flags&(TILESLAYERFLAG_TELE|TILESLAYERFLAG_SPEEDUP) && Mod == MOD_RACE;

						if(pTilemapItem->m_Flags&TILESLAYERFLAG_GAME)
						{
							pTiles = new CLayerGame(pTilemapItem->m_Width, pTilemapItem->m_Height);
							MakeGameLayer(pTiles);
							MakeGameGroup(pGroup);
						}
						else if(!IsRaceLayer)
						{
							pTiles = new CLayerTiles(pTilemapItem->m_Width, pTilemapItem->m_Height);
							pTiles->m_pMap = this;
							pTiles->m_Color = pTilemapItem->m_Color;
							pTiles->m_ColorEnv = pTilemapItem->m_ColorEnv;
							pTiles->m_ColorEnvOffset = pTilemapItem->m_ColorEnvOffset;
						}

						if(IsRaceLayer)
						{
							CMapItemLayerTilemapRace *pRaceTilemapItem = (CMapItemLayerTilemapRace *)pLayerItem;
							if(pRaceTilemapItem->m_Version < 3)
							{
								pRaceTilemapItem->m_Tele = *((int*)(pTilemapItem) + 15);
								pRaceTilemapItem->m_Speedup = *((int*)(pTilemapItem) + 16);
							}

							if(pTilemapItem->m_Flags&TILESLAYERFLAG_TELE)
							{
								pTele = CreateCustomLayer(this, pTilemapItem->m_Width, pTilemapItem->m_Height, "#tele", "_numbers");
								Tele = pRaceTilemapItem->m_Tele;
							}
							else if(pTilemapItem->m_Flags&TILESLAYERFLAG_SPEEDUP)
							{
								pSpeedup = CreateCustomLayer(this, pTilemapItem->m_Width, pTilemapItem->m_Height, "#spu-force", "_numbers");
								Speedup = pRaceTilemapItem->m_Speedup;
							}
						}
						else
						{
							pLayer = pTiles;

							pGroup->AddLayer(pTiles);
							void *pData = DataFile.GetData(pTilemapItem->m_Data);
							pTiles->m_Image = pTilemapItem->m_Image;
							pTiles->m_Game = pTilemapItem->m_Flags&TILESLAYERFLAG_GAME;

							// load layer name
							if(pTilemapItem->m_Version >= 3)
								IntsToStr(pTilemapItem->m_aName, sizeof(pTiles->m_aName)/sizeof(int), pTiles->m_aName);

							// get tile data
							if(pTilemapItem->m_Version > 3)
								pTiles->ExtractTiles((CTile *)pData);
							else
								mem_copy(pTiles->m_pTiles, pData, pTiles->m_Width*pTiles->m_Height*sizeof(CTile));

							if(pTiles->m_Game && pTilemapItem->m_Version == MakeVersion(1, *pTilemapItem))
							{
								for(int i = 0; i < pTiles->m_Width*pTiles->m_Height; i++)
								{
									if(pTiles->m_pTiles[i].m_Index)
										pTiles->m_pTiles[i].m_Index += ENTITY_OFFSET;
								}
							}
						}

						DataFile.UnloadData(pTilemapItem->m_Data);
					}
					else if(pLayerItem->m_Type == LAYERTYPE_QUADS)
					{
						CMapItemLayerQuads *pQuadsItem = (CMapItemLayerQuads *)pLayerItem;
						CLayerQuads *pQuads = new CLayerQuads;
						pQuads->m_pMap = this;
						pLayer = pQuads;
						pQuads->m_Image = pQuadsItem->m_Image;
						if(pQuads->m_Image < -1 || pQuads->m_Image >= m_lImages.size())
							pQuads->m_Image = -1;

						// load layer name
						if(pQuadsItem->m_Version >= 2)
							IntsToStr(pQuadsItem->m_aName, sizeof(pQuads->m_aName)/sizeof(int), pQuads->m_aName);

						void *pData = DataFile.GetDataSwapped(pQuadsItem->m_Data);
						pGroup->AddLayer(pQuads);
						pQuads->m_lQuads.set_size(pQuadsItem->m_NumQuads);
						mem_copy(pQuads->m_lQuads.base_ptr(), pData, sizeof(CQuad)*pQuadsItem->m_NumQuads);
						DataFile.UnloadData(pQuadsItem->m_Data);
					}

					if(pLayer)
						pLayer->m_Flags = pLayerItem->m_Flags;
				}
			}
		}

		// load envelopes
		{
			CEnvPoint *pEnvPoints = 0;
			{
				int Start, Num;
				DataFile.GetType(MAPITEMTYPE_ENVPOINTS, &Start, &Num);
				if(Num)
					pEnvPoints = (CEnvPoint *)DataFile.GetItem(Start, 0, 0);
			}

			int Start, Num;
			DataFile.GetType(MAPITEMTYPE_ENVELOPE, &Start, &Num);
			for(int e = 0; e < Num; e++)
			{
				CMapItemEnvelope *pItem = (CMapItemEnvelope *)DataFile.GetItem(Start+e, 0, 0);
				CEnvelope *pEnv = new CEnvelope(pItem->m_Channels);
				pEnv->m_lPoints.set_size(pItem->m_NumPoints);
				for(int n = 0; n < pItem->m_NumPoints; n++)
				{
					if(pItem->m_Version >= 3)
					{
						pEnv->m_lPoints[n] = pEnvPoints[pItem->m_StartPoint + n];
					}
					else
					{
						// backwards compatibility
						CEnvPoint_v1 *pEnvPoint_v1 = &((CEnvPoint_v1 *)pEnvPoints)[pItem->m_StartPoint + n];

						pEnv->m_lPoints[n].m_Time = pEnvPoint_v1->m_Time;
						pEnv->m_lPoints[n].m_Curvetype = pEnvPoint_v1->m_Curvetype;
						for(int c = 0; c < 4; c++)
						{
							pEnv->m_lPoints[n].m_aValues[c] = (c < pItem->m_Channels) ? pEnvPoint_v1->m_aValues[c] : 0;
							pEnv->m_lPoints[n].m_aInTangentdx[c] = 0;
							pEnv->m_lPoints[n].m_aInTangentdy[c] = 0;
							pEnv->m_lPoints[n].m_aOutTangentdx[c] = 0;
							pEnv->m_lPoints[n].m_aOutTangentdy[c] = 0;
						}
					}
				}

				if(pItem->m_aName[0] != -1)	// compatibility with old maps
					IntsToStr(pItem->m_aName, sizeof(pItem->m_aName)/sizeof(int), pEnv->m_aName);
				m_lEnvelopes.add(pEnv);
				if(pItem->m_Version >= 2)
					pEnv->m_Synchronized = pItem->m_Synchronized;
			}
		}

		static int s_TeleportVelReset = 0;

		// load race
		{
			CLayerTiles *pGameExtended = 0;
			CLayerTiles *pSpeedupAngle = 0;
			CLayerTiles *pSettings = 0;

			if(Mod == MOD_RACE)
			{
				int TileCount[1<<8] = {0};
				for(int i = 0; i < m_pGameLayer->m_Width*m_pGameLayer->m_Height; i++)
					TileCount[m_pGameLayer->m_pTiles[i].m_Index]++;
				
				bool Race = TileCount[TILE_BEGIN] > 0 && TileCount[TILE_END] > 0;
				bool FastCap = TileCount[ENTITY_FLAGSTAND_RED + ENTITY_OFFSET] > 0 && TileCount[ENTITY_FLAGSTAND_BLUE + ENTITY_OFFSET] > 0;

				bool AllowRestart = Race;

				if(Race && FastCap)
					dbg_msg("race", "warning: cannot determine race type");
				else if(!Race && !FastCap)
					dbg_msg("race", "warning: not a valid race map");
				else if(Race && !FastCap)
					dbg_msg("race", "race type: race");
				else if(!Race && FastCap)
					dbg_msg("race", "race type: fastcap");

				// map settings
				bool AddMapSettings = AllowRestart;

				int NumVars = 8;
				static int s_aValues[9];
				static CIntVariableData s_aVars[] = {
					{ m_pConsole, "sv_regen", &s_aValues[0], 0, 0, 50 },
					{ m_pConsole, "sv_strip", &s_aValues[1], 0, 0, 1 },
					{ m_pConsole, "sv_infinite_ammo", &s_aValues[2], 0, 0, 1 },
					{ m_pConsole, "sv_no_items", &s_aValues[3], 0, 0, 1 },
					{ m_pConsole, "sv_teleport_grenade", &s_aValues[4], 0, 0, 1 },
					{ m_pConsole, "sv_delete_grenades_after_death", &s_aValues[5], 1, 0, 1 },
					{ m_pConsole, "sv_rocket_jump_damage", &s_aValues[6], 1, 0, 1 },
					{ m_pConsole, "sv_pickup_respawn", &s_aValues[7], -1, -1, 120 },
					{ m_pConsole, "sv_allow_restart_old", &s_aValues[8], 0, 0, 1 }
				};
				static CIntVariableData s_TeleportVelResetVar = { m_pConsole, "sv_teleport_vel_reset", &s_TeleportVelReset, 0, 0, 1 };

				for(int i = 0; i < NumVars; i++)
					s_aVars[i].Register();
				s_TeleportVelResetVar.Register();

				int Start, Num;
				DataFile.GetType(MAPITEMTYPE_INFO, &Start, &Num);
				for(int e = 0; e < Num; e++)
				{
					int ItemID;
					CMapItemInfo *pItem = (CMapItemInfo *)DataFile.GetItem(Start + e, 0, &ItemID);
					int ItemSize = DataFile.GetItemSize(Start + e) - sizeof(int) * 2;
					if(!pItem || ItemID != 0)
						continue;

					CMapItemInfoRace *pItemRace = (CMapItemInfoRace *)pItem;
					if(pItemRace->m_Version == 1 && ItemSize >= (int)sizeof(CMapItemInfoRace) && pItemRace->m_Settings > -1)
					{
						int Size = DataFile.GetUncompressedDataSize(pItemRace->m_Settings);
						const char *pTmp = (char*)DataFile.GetData(pItemRace->m_Settings);
						const char *pEnd = pTmp + Size;
						while(pTmp < pEnd && str_length(pTmp) > 0)
						{
							m_pConsole->ExecuteLineFlag(pTmp, CFGFLAG_MAPSETTINGS);
							pTmp += str_length(pTmp) + 1;
							AddMapSettings = true;
						}
						DataFile.UnloadData(pItemRace->m_Settings);
					}
					break;
				}

				if(AddMapSettings)
				{
					if(AllowRestart) // TODO: only add this for teerace maps?
					{
						NumVars++;
						s_aVars[8].Register();
						s_aValues[8] = 1;
					}

					char aBuf[64];
					int MaxLen = 0;
					for(int i = 0; i < NumVars; i++)
					{
						str_format(aBuf, sizeof(aBuf), "%s %d", s_aVars[i].m_pName, s_aValues[i]);
						MaxLen = max(MaxLen, str_length(aBuf));
					}
					
					pSettings = CreateCustomLayer(this, MaxLen, NumVars, "#settings", "_letters");
					for(int i = 0; i < NumVars; i++)
					{
						str_format(aBuf, sizeof(aBuf), "%s %d", s_aVars[i].m_pName, s_aValues[i]);
						for(int j = 0; j < str_length(aBuf); j++)
							pSettings->m_pTiles[i*MaxLen+j].m_Index = aBuf[j];
					}
				}
			}

			if(m_pGameLayer)
			{
				if(pTele && pTele->m_Width == m_pGameLayer->m_Width && pTele->m_Height == m_pGameLayer->m_Height)
				{
					CTeleTile *pTeleTiles = (CTeleTile *)DataFile.GetData(Tele);
					int NumTiles = 0;
					bool aTeleporterIn[1<<8] = {0};
					bool aTeleporterOut[1<<8] = {0};

					for(int i = 0; i < m_pGameLayer->m_Width*m_pGameLayer->m_Height; i++)
					{
						if(pTeleTiles[i].m_Type == TILE_TELEIN || pTeleTiles[i].m_Type == TILE_TELEOUT)
						{
							CLayerTiles *pGameLayer = m_pGameLayer;
							int GameIndex = m_pGameLayer->m_pTiles[i].m_Index;
							if((GameIndex >= TILE_SOLID && GameIndex <= TILE_NOHOOK) || (GameIndex >= TILE_STOPL && GameIndex <= 59))
							{
								if(!pGameExtended)
									pGameExtended = CreateCustomLayer(this, m_pGameLayer->m_Width, m_pGameLayer->m_Height, "#game", "_entities_race");
								pGameLayer = pGameExtended;
							}
							pGameLayer->m_pTiles[i].m_Index = pTeleTiles[i].m_Type;
							if(pGameLayer->m_pTiles[i].m_Index == TILE_TELEIN && s_TeleportVelReset)
								pGameLayer->m_pTiles[i].m_Index = TILE_TELEIN_STOP;
							pTele->m_pTiles[i].m_Index = pTeleTiles[i].m_Number;
							NumTiles++;
							if(pTeleTiles[i].m_Type == TILE_TELEIN)
								aTeleporterIn[pTeleTiles[i].m_Number] = true;
							if(pTeleTiles[i].m_Type == TILE_TELEOUT)
								aTeleporterOut[pTeleTiles[i].m_Number] = true;
						}
					}

					if(!NumTiles)
					{
						delete pTele;
						pTele = 0;
					}

					for(int i = 1; i < 1<<8; i++)
						if(aTeleporterIn[i] != aTeleporterOut[i])
							dbg_msg("race", "warning: missing %s for tele #%d", aTeleporterIn[i] ? "out" : "in", i);

					DataFile.UnloadData(Tele);
				}

				if(pSpeedup && pSpeedup->m_Width == m_pGameLayer->m_Width && pSpeedup->m_Height == m_pGameLayer->m_Height)
				{
					CSpeedupTile *pSpeedupTiles = (CSpeedupTile *)DataFile.GetData(Speedup);
					int NumTiles = 0;

					pSpeedupAngle = CreateCustomLayer(this, m_pGameLayer->m_Width, m_pGameLayer->m_Height, "#spu-angle", "_angle");

					for(int i = 0; i < m_pGameLayer->m_Width*m_pGameLayer->m_Height; i++)
					{
						if(pSpeedupTiles[i].m_Force > 0)
						{
							CLayerTiles *pGameLayer = m_pGameLayer;
							int GameIndex = m_pGameLayer->m_pTiles[i].m_Index;
							if((GameIndex >= TILE_SOLID && GameIndex <= TILE_NOHOOK) || (GameIndex >= TILE_TELEIN_STOP && GameIndex <= 59 && GameIndex != TILE_BOOST))
							{
								if(!pGameExtended)
									pGameExtended = CreateCustomLayer(this, m_pGameLayer->m_Width, m_pGameLayer->m_Height, "#game", "_entities_race");
								pGameLayer = pGameExtended;
							}
							pGameLayer->m_pTiles[i].m_Index = TILE_BOOST;
							pSpeedup->m_pTiles[i].m_Index = pSpeedupTiles[i].m_Force;
							int Angle = pSpeedupTiles[i].m_Angle % 360;
							pSpeedupAngle->m_pTiles[i].m_Index = (Angle % 90)+1;
							static const int s_Rotation[] = { ROTATION_0, ROTATION_90, ROTATION_180, ROTATION_270 };
							pSpeedupAngle->m_pTiles[i].m_Flags = s_Rotation[Angle / 90];
							NumTiles++;
						}
					}

					if(!pSpeedup)
					{
						delete pSpeedupAngle;
						delete pTele;
						pTele = 0;
					}

					DataFile.UnloadData(Speedup);
				}
			}

			if(pGameExtended || pTele || pSpeedup || pSettings)
			{
				CLayerGroup *pRaceGroup = NewGroup();
				str_copy(pRaceGroup->m_aName, "#race", sizeof(pRaceGroup->m_aName));

				if(pSettings)
					pRaceGroup->AddLayer(pSettings);
				if(pSpeedup)
					pRaceGroup->AddLayer(pSpeedupAngle);
				if(pGameExtended)
					pRaceGroup->AddLayer(pGameExtended);
				if(pTele)
					pRaceGroup->AddLayer(pTele);
				if(pSpeedup)
					pRaceGroup->AddLayer(pSpeedup);
			}
		}
	}
	else
		return 0;

	return 1;
}
