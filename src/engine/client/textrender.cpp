/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/system.h>
#include <base/math.h>
#include <engine/external/json-parser/json.h>
#include <engine/graphics.h>
#include <engine/textrender.h>

#include "textrender.h"

#define MSDF_SIZE 42

int CAtlas::TrySection(int Index, int Width, int Height)
{
	ivec3 Section = m_Sections[Index];
	int CurX = Section.x;
	int CurY = Section.y;

	int FitWidth = Width;

	if(CurX + Width > m_Width - 1) return -1;

	for(int i = Index; i < m_Sections.size(); ++i)
	{
		if(FitWidth <= 0) break;

		Section = m_Sections[i];
		if(Section.y > CurY) CurY = Section.y;
		if(CurY + Height > m_Height - 1) return -1;
		FitWidth -= Section.l;
	}

	return CurY;
}

void CAtlas::Init(int Index, int X, int Y, int Width, int Height)
{
	m_Offset.x = X;
	m_Offset.y = Y;
	m_Width = Width;
	m_Height = Height;

	m_ID = Index;
	m_Sections.clear();

	ivec3 Section;
	Section.x = 1;
	Section.y = 1;
	Section.l = m_Width - 2;
	m_Sections.add(Section);
}

ivec2 CAtlas::Add(int Width, int Height)
{
	int BestHeight = m_Height;
	int BestWidth = m_Width;
	int BestSectionIndex = -1;

	ivec2 Position;

	for(int i = 0; i < m_Sections.size(); ++i)
	{
		int y = TrySection(i, Width, Height);
		if(y >= 0)
		{
			ivec3 Section = m_Sections[i];
			int NewHeight = y + Height;
			if((NewHeight < BestHeight) || ((NewHeight == BestHeight) && (Section.l > 0 && Section.l < BestWidth)))
			{
				BestHeight = NewHeight;
				BestWidth = Section.l;
				BestSectionIndex = i;
				Position.x = Section.x;
				Position.y = y;
			}
		}
	}

	if(BestSectionIndex < 0)
	{
		Position.x = -1;
		Position.y = -1;
		return Position;
	}

	ivec3 NewSection;
	NewSection.x = Position.x;
	NewSection.y = Position.y + Height;
	NewSection.l = Width;
	m_Sections.insert(NewSection, m_Sections.all().slice(BestSectionIndex, BestSectionIndex + 1));

	for(int i = BestSectionIndex + 1; i < m_Sections.size(); ++i)
	{
		ivec3 *Section = &m_Sections[i];
		ivec3 *Previous = &m_Sections[i-1];

		if(Section->x >= Previous->x + Previous->l) break;

		int Shrink = Previous->x + Previous->l - Section->x;
		Section->x += Shrink;
		Section->l -= Shrink;
		if(Section->l > 0) break;

		m_Sections.remove_index(i);
		i -= 1;
	}

	for(int i = 0; i < m_Sections.size()-1; ++i)
	{
		ivec3 *Section = &m_Sections[i];
		ivec3 *Next = &m_Sections[i+1];
		if(Section->y == Next->y)
		{
			Section->l += Next->l;
			m_Sections.remove_index(i+1);
			i -= 1;
		}
	}

	m_Access++;
	return Position + m_Offset;
}

/*
int CGlyphMap::AdjustOutlineThicknessToFontSize(int OutlineThickness, int FontSize)
{
	if(FontSize > 36)
		OutlineThickness *= 4;
	else if(FontSize >= 18)
		OutlineThickness *= 2;
	return OutlineThickness;
}
*/

unsigned char CGlyphMap::ms_aGlyphData[] = {};

void CGlyphMap::InitTexture(int Width, int Height)
{
	for(int y = 0; y < PAGE_COUNT; ++y)
	{
		for(int x = 0; x < PAGE_COUNT; ++x)
		{
			m_aAtlasPages[y*PAGE_COUNT+x].Init(m_NumTotalPages++, x * PAGE_SIZE, y * PAGE_SIZE, PAGE_SIZE, PAGE_SIZE);
		}
	}
	m_ActiveAtlasIndex = 0;
	m_Glyphs.clear();

	int TextureSize = Width*Height*3;

	void *pMem = mem_alloc(TextureSize, 1);
	mem_zero(pMem, TextureSize);

	if(m_Texture.IsValid())
		m_pGraphics->UnloadTexture(&m_Texture);

	m_Texture = m_pGraphics->LoadTextureRaw(Width, Height, CImageInfo::FORMAT_RGB, pMem, CImageInfo::FORMAT_RGB, IGraphics::TEXLOAD_NOMIPMAPS);
	dbg_msg("textrender", "memory usage: %d", TextureSize);
	mem_free(pMem);
}

int CGlyphMap::FitGlyph(int Width, int Height, ivec2 *Position)
{
	*Position = m_aAtlasPages[m_ActiveAtlasIndex].Add(Width, Height);
	if(Position->x >= 0 && Position->y >= 0)
		return m_ActiveAtlasIndex;

	// out of space, drop a page
	int LeastAccess = INT_MAX;
	int Atlas = 0;
	for(int i = 0; i < PAGE_COUNT*PAGE_COUNT; ++i)
	{
		// Do not drop the active page
		if(m_ActiveAtlasIndex == i)
			continue;

		int PageAccess = m_aAtlasPages[i].GetAccess();
		if(PageAccess < LeastAccess)
		{
			LeastAccess = PageAccess;
			Atlas = i;
		}
	}

	int X = m_aAtlasPages[Atlas].GetOffsetX();
	int Y = m_aAtlasPages[Atlas].GetOffsetY();
	int W = m_aAtlasPages[Atlas].GetWidth();
	int H = m_aAtlasPages[Atlas].GetHeight();

	m_aAtlasPages[Atlas].Init(m_NumTotalPages++, X, Y, W, H);
	*Position = m_aAtlasPages[Atlas].Add(Width, Height);
	m_ActiveAtlasIndex = Atlas;

	dbg_msg("textrender", "atlas is full, dropping atlas %d, total pages: %u", Atlas, m_NumTotalPages);
	return Atlas;
}

void CGlyphMap::UploadGlyph(int PosX, int PosY, int Width, int Height, const unsigned char *pData)
{
	m_pGraphics->LoadTextureRawSub(m_Texture, PosX, PosY,
		Width, Height, CImageInfo::FORMAT_RGB, pData);
}

bool CGlyphMap::SetFaceByName(msdfgen::FontHandle **ppFace, const char *pFamilyName)
{
	msdfgen::FontHandle *pFace = NULL;
	char aFamilyStyleName[128];

	if(pFamilyName != NULL)
	{
		for(int i = 0; i < m_NumFtFaces; ++i)
		{
			str_format(aFamilyStyleName, 128, "%s %s", msdfgen::getFamilyName(m_apFtFaces[i]), msdfgen::getStyleName(m_apFtFaces[i]));
			if(str_comp(pFamilyName, aFamilyStyleName) == 0)
			{
				pFace = m_apFtFaces[i];
				break;
			}

			if(!pFace && str_comp(pFamilyName, msdfgen::getFamilyName(m_apFtFaces[i])) == 0)
			{
				pFace = m_apFtFaces[i];
			}
		}
	}

	if(pFace)
	{
		*ppFace = pFace;
		return true;
	}
	return false;
}

CGlyphMap::CGlyphMap(IGraphics *pGraphics)
{
	m_pGraphics = pGraphics;

	m_pDefaultFace = NULL;
	m_pVariantFace = NULL;

	mem_zero(m_apFallbackFaces, sizeof(m_apFallbackFaces));
	mem_zero(m_apFtFaces, sizeof(m_apFtFaces));

	m_NumFtFaces = 0;
	m_NumFallbackFaces = 0;
	m_NumTotalPages = 0;

	m_MSDF = msdfgen::Bitmap<float, 3>(64, 64);

	InitTexture(TEXTURE_SIZE, TEXTURE_SIZE);
}

CGlyphMap::~CGlyphMap()
{
	for(int i = 0; i < m_Glyphs.size(); ++i)
		delete m_Glyphs[i].m_pGlyph;
}

int CGlyphMap::GetCharGlyph(int Chr, msdfgen::FontHandle **ppFace)
{
	int GlyphIndex = msdfgen::getCharIndex(m_pDefaultFace, Chr);
	*ppFace = m_pDefaultFace;

	if(!m_pDefaultFace || GlyphIndex)
		return GlyphIndex;

	if(m_pVariantFace)
	{
		GlyphIndex = msdfgen::getCharIndex(m_pVariantFace, Chr);
		if(GlyphIndex)
		{
			*ppFace = m_pVariantFace;
			return GlyphIndex;
		}
	}

	for(int i = 0; i < m_NumFallbackFaces; ++i)
	{
		if(m_apFallbackFaces[i])
		{
			GlyphIndex = msdfgen::getCharIndex(m_apFallbackFaces[i], Chr);
			if(GlyphIndex)
			{
				*ppFace = m_apFallbackFaces[i];
				return GlyphIndex;
			}
		}
	}

	return 0;
}

int CGlyphMap::AddFace(msdfgen::FontHandle *pFace)
{
	if(m_NumFtFaces == MAX_FACES) 
		return -1;

	m_apFtFaces[m_NumFtFaces++] = pFace;
	if(!m_pDefaultFace) m_pDefaultFace = pFace;

	return 0; 
}

void CGlyphMap::SetDefaultFaceByName(const char *pFamilyName)
{
	SetFaceByName(&m_pDefaultFace, pFamilyName);
}

void CGlyphMap::AddFallbackFaceByName(const char *pFamilyName)
{
	if(m_NumFallbackFaces == MAX_FACES)
		return;

	msdfgen::FontHandle *pFace = NULL;
	if(SetFaceByName(&pFace, pFamilyName))
	{
		m_apFallbackFaces[m_NumFallbackFaces++] = pFace;
	}
}

void CGlyphMap::SetVariantFaceByName(const char *pFamilyName)
{
	msdfgen::FontHandle *pFace = NULL;
	SetFaceByName(&pFace, pFamilyName);
	if(m_pVariantFace != pFace)
	{
		m_pVariantFace = pFace;
		InitTexture(TEXTURE_SIZE, TEXTURE_SIZE);
	}
}

bool CGlyphMap::RenderGlyph(CGlyph *pGlyph, bool Render)
{
	if(Render && pGlyph->m_Rendered && m_aAtlasPages[pGlyph->m_AtlasIndex].GetPageID() == pGlyph->m_PageID)
	{
		m_aAtlasPages[pGlyph->m_AtlasIndex].Touch();
		return true;
	}

	msdfgen::FontHandle *pGlyphFace = NULL;
	int GlyphIndex = GetCharGlyph(pGlyph->m_ID, &pGlyphFace);

	double GlyphAdvance = 0;
	msdfgen::Shape Shape;
	if(!msdfgen::loadGlyph(Shape, pGlyphFace, GlyphIndex, &GlyphAdvance))
	{
		dbg_msg("textrender", "error loading glyph %d", pGlyph->m_ID);
		return false;
	}

	if(!Shape.validate())
	{
		dbg_msg("textrender", "error: geometry of the loaded shape is invalid, glyph %d", GlyphIndex);
		return false;
	}
	Shape.normalize();
	msdfgen::Shape::Bounds Bounds = Shape.getBounds();

	// msdf
	msdfgen::FontMetrics FontMetrics = { };
	msdfgen::getFontMetrics(FontMetrics, pGlyphFace);
	double emSize = FontMetrics.emSize > 0.0 ? FontMetrics.emSize : 32.0;
	double MSDFScale = MSDF_SIZE / emSize;

	double w = MSDFScale * (Bounds.r - Bounds.l);
	double h = MSDFScale * (Bounds.t - Bounds.b);
	double l = MSDFScale * Bounds.l;
	double t = MSDFScale * Bounds.t;

	// adjust spacing
	int Spacing = 1;

	int BitmapWidth = ceil(w);
	int BitmapHeight = ceil(h);

	int Width = BitmapWidth + Spacing * 2;
	int Height = BitmapHeight + Spacing * 2;

	int AtlasIndex = -1;
	int Page = -1;

	if(Render)
	{
		// find space in atlas
		ivec2 Position;
		AtlasIndex = FitGlyph(Width, Height, &Position);
		Page = m_aAtlasPages[AtlasIndex].GetPageID();

		const bool ScanlinePass = true;
		const double EdgeThreshold = MSDFGEN_DEFAULT_ERROR_CORRECTION_THRESHOLD;
		const double AngleThreshold = 3;
		const bool OverlapSupport = true;
		const double pxRange = 2;

		double Range = pxRange/MSDFScale;
		msdfgen::Vector2 Translate(-Bounds.l, -Bounds.b);
		msdfgen::Vector2 ScaleVec = MSDFScale;

		msdfgen::edgeColoringSimple(Shape, AngleThreshold);
		msdfgen::generateMSDF(m_MSDF, Shape, 2, ScaleVec, Translate, ScanlinePass ? 0 : EdgeThreshold, OverlapSupport);

		if(ScanlinePass)
		{
			msdfgen::distanceSignCorrection(m_MSDF, Shape, ScaleVec, Translate);
			if(EdgeThreshold > 0)
				msdfgen::msdfErrorCorrection(m_MSDF, EdgeThreshold/(ScaleVec*Range));
		}

		mem_zero(ms_aGlyphData, Width*Height*3);

		for(int py = 0; py < BitmapHeight; py++)
			for(int px = 0; px < BitmapWidth; px++)
				for(int c = 0; c < 3; c++)
					ms_aGlyphData[((py+Spacing)*Width+px+Spacing)*3+c] = msdfgen::pixelFloatToByte(m_MSDF(px, BitmapHeight-py)[c]);

		UploadGlyph(Position.x, Position.y, Width, Height, ms_aGlyphData);

		m_aAtlasPages[AtlasIndex].Touch();

		float UVscale = 1.0f / TEXTURE_SIZE;
		pGlyph->m_aUvCoords[0] = (Position.x + Spacing) * UVscale;
		pGlyph->m_aUvCoords[1] = (Position.y + Spacing) * UVscale;
		pGlyph->m_aUvCoords[2] = pGlyph->m_aUvCoords[0] + BitmapWidth * UVscale;
		pGlyph->m_aUvCoords[3] = pGlyph->m_aUvCoords[1] + BitmapHeight * UVscale;
	}

	float Scale = 1.0f / MSDF_SIZE;

	pGlyph->m_AtlasIndex = AtlasIndex;
	pGlyph->m_PageID = Page;
	pGlyph->m_pFace = pGlyphFace;
	pGlyph->m_Height = BitmapHeight * Scale;
	pGlyph->m_Width = BitmapWidth * Scale;
	pGlyph->m_BearingX = l * Scale; // ignore_convention
	pGlyph->m_BearingY = (MSDF_SIZE - t) * Scale; // ignore_convention
	pGlyph->m_AdvanceX = MSDFScale * GlyphAdvance * Scale; // ignore_convention
	pGlyph->m_Rendered = Render;

	return true;
}

CGlyph *CGlyphMap::GetGlyph(int Chr, bool Render)
{
	CGlyphIndex Index;
	Index.m_ID = Chr;

	sorted_array<CGlyphIndex>::range r = ::find_binary(m_Glyphs.all(), Index);

	// couldn't find glyph, render a new one
	if(r.empty())
	{
		Index.m_pGlyph = new CGlyph();
		Index.m_pGlyph->m_Rendered = false;
		Index.m_pGlyph->m_ID = Chr;
		if(RenderGlyph(Index.m_pGlyph, Render))
		{
			m_Glyphs.add(Index);
			return Index.m_pGlyph;
		}
		return NULL;
	}

	CGlyph *pMatch = r.front().m_pGlyph;
	if(Render)
		RenderGlyph(pMatch, true);
	return pMatch;
}

float CGlyphMap::Kerning(CGlyph *pLeft, CGlyph *pRight)
{
	double Kerning = 0.0;
	if(pLeft && pRight && pLeft->m_pFace == pRight->m_pFace)
		msdfgen::getKerning(Kerning, pLeft->m_pFace, pLeft->m_ID, pRight->m_ID);
	return (float)Kerning;
}

void CGlyphMap::PagesAccessReset()
{
	for(int i = 0; i < PAGE_COUNT * PAGE_COUNT; ++i)
		m_aAtlasPages[i].Update();
}

CWordWidthHint CTextRender::MakeWord(CTextCursor *pCursor, const char *pText, const char *pEnd, 
								float Size, int PixelSize, vec2 ScreenScale)
{
	bool Render = !(pCursor->m_Flags & TEXTFLAG_NO_RENDER);
	bool BreakWord = !(pCursor->m_Flags & TEXTFLAG_WORD_WRAP);
	CWordWidthHint Hint;
	const char *pCur = pText;
	int NextChr = str_utf8_decode(&pCur);
	CGlyph *pNextGlyph = NULL;
	if(NextChr > 0)
	{
		if(NextChr == '\n' || NextChr == '\t')
			pNextGlyph = m_pGlyphMap->GetGlyph(' ', Render);
		else
			pNextGlyph = m_pGlyphMap->GetGlyph(NextChr, Render);
	}

	float Scale = 1.0f/PixelSize;
	float MaxWidth = pCursor->m_MaxWidth;
	if(MaxWidth < 0)
		MaxWidth = INFINITY;

	float WordStartAdvanceX = pCursor->m_Advance.x;

	Hint.m_CharCount = 0;
	Hint.m_GlyphCount = 0;
	Hint.m_EffectiveAdvanceX = pCursor->m_Advance.x;
	Hint.m_EndsWithNewline = false;
	Hint.m_EndOfWord = false;
	Hint.m_IsBroken = false;

	if(*pText == '\0' || pCur > pEnd)
	{
		Hint.m_CharCount = -1;
		return Hint;
	}

	while(1)
	{
		int Chr = NextChr;
		CGlyph *pGlyph = pNextGlyph;
		int CharCount = pCur - pText;
		int NumChars = CharCount - Hint.m_CharCount;
		Hint.m_CharCount = CharCount;

		if(Chr == 0 || pCur > pEnd)
		{
			Hint.m_CharCount--;
			break;
		}

		if(!pGlyph || Chr < 0)
		{
			Hint.m_CharCount = -1;
			return Hint;
		}

		NextChr = str_utf8_decode(&pCur);
		pNextGlyph = NULL;
		if(NextChr > 0)
		{
			if(NextChr == '\n' || NextChr == '\t')
				pNextGlyph = m_pGlyphMap->GetGlyph(' ', Render);
			else
				pNextGlyph = m_pGlyphMap->GetGlyph(NextChr, Render);
		}

		float Kerning = m_pGlyphMap->Kerning(pGlyph, pNextGlyph) * Scale;
		float AdvanceX = (pGlyph->m_AdvanceX + Kerning) * Size;
	
		bool IsSpace = Chr == '\n' || Chr == '\t' || Chr == ' ';
		bool CanBreak = !IsSpace && (BreakWord || pCursor->m_StartOfLine);
		if(Hint.m_EffectiveAdvanceX - WordStartAdvanceX > MaxWidth || (CanBreak && pCursor->m_Advance.x + AdvanceX > MaxWidth))
		{
			Hint.m_CharCount--;
			Hint.m_IsBroken = true;
			break;
		}

		if(Render)
		{
			CScaledGlyph Scaled;
			Scaled.m_pGlyph = pGlyph;
			Scaled.m_Advance = pCursor->m_Advance;
			Scaled.m_Size = Size;
			Scaled.m_Line = pCursor->m_LineCount - 1;
			Scaled.m_TextColor = vec4(m_TextR, m_TextG, m_TextB, m_TextA);
			Scaled.m_SecondaryColor = vec4(m_TextSecondaryR, m_TextSecondaryG, m_TextSecondaryB, m_TextSecondaryA);
			Scaled.m_NumChars = NumChars;
			pCursor->m_Glyphs.add(Scaled);
		}
		
		pCursor->m_Advance.x += AdvanceX;
		Hint.m_GlyphCount++;
		pCursor->m_GlyphCount++;

		if(IsSpace || Chr == 0)
		{
			Hint.m_EndOfWord = true;
			Hint.m_EndsWithNewline = Chr == '\n';
			break;
		}

		Hint.m_EffectiveAdvanceX = pCursor->m_Advance.x;

		// break every char on non latin/greek characters
		if(!isWestern(Chr))
		{
			Hint.m_EndOfWord = true;
			break;
		}
	}

	return Hint;
}

void CTextRender::TextRefreshGlyphs(CTextCursor *pCursor)
{
	int NumQuads = pCursor->m_Glyphs.size();
	if(NumQuads <= 0)
		return;

	int NumTotalPages = m_pGlyphMap->NumTotalPages();

	if(NumTotalPages != pCursor->m_PageCountWhenDrawn)
	{
		// pages were dropped, rerender glyphs
		for(int i = 0; i < pCursor->m_Glyphs.size(); ++i)
			m_pGlyphMap->RenderGlyph(pCursor->m_Glyphs[i].m_pGlyph, true);
		pCursor->m_PageCountWhenDrawn = m_pGlyphMap->NumTotalPages();
	}

	if(NumTotalPages != m_pGlyphMap->NumTotalPages())
	{
		dbg_msg("textrender", "page %d mismatch after refresh, atlas might be too small.", NumTotalPages);
	}
}

int CTextRender::LoadFontCollection(const void *pFilename, const void *pBuf, long FileSize)
{
	msdfgen::FontHandle *pFace = msdfgen::loadFont(m_pFT, pBuf, FileSize, -1);
	if(!pFace)
		return -1;

	int NumFaces = msdfgen::getNumFaces(pFace);
	msdfgen::destroyFont(pFace);

	int i;
	for(i = 0; i < NumFaces; ++i)
	{
		pFace = msdfgen::loadFont(m_pFT, pBuf, FileSize, i);
		if(!pFace)
			break;

		if(m_pGlyphMap->AddFace(pFace))
			break;
	}

	dbg_msg("textrender", "loaded %d faces from font file '%s'", i, (char *)pFilename);

	return 0;
}

CTextRender::CTextRender()
{
	m_pGraphics = 0;

	m_TextR = 1.0f;
	m_TextG = 1.0f;
	m_TextB = 1.0f;
	m_TextA = 1.0f;
	m_TextSecondaryR = 0.0f;
	m_TextSecondaryG = 0.0f;
	m_TextSecondaryB = 0.0f;
	m_TextSecondaryA = 0.3f;

	m_pGlyphMap = 0;
	m_NumVariants = 0;
	m_CurrentVariant = -1;
	m_apVariants = 0;

	m_pFT = 0;

	mem_zero(m_apFontData, sizeof(m_apFontData));
}

CTextRender::~CTextRender()
{
	for(int i = 0; i < MAX_FACES; ++i)
		if(m_apFontData[i])
			mem_free(m_apFontData[i]);
}

void CTextRender::RenderAtlasTex()
{
	Graphics()->TextureSet(m_pGlyphMap->GetTexture());
	Graphics()->MapScreen(0,0,Graphics()->ScreenWidth(),Graphics()->ScreenHeight());
	Graphics()->QuadsBegin();
	IGraphics::CQuadItem QuadItem(0, Graphics()->ScreenHeight()-TEXTURE_SIZE, TEXTURE_SIZE, TEXTURE_SIZE);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();
}

void CTextRender::Init()
{
	m_pGraphics = Kernel()->RequestInterface<IGraphics>();
	m_pFT = msdfgen::initializeFreetype();
	m_pGlyphMap = new CGlyphMap(m_pGraphics);
}

void CTextRender::Update()
{
	if(m_pGlyphMap) m_pGlyphMap->PagesAccessReset();
}

void CTextRender::Shutdown()
{
	delete m_pGlyphMap;
	if(m_apVariants) delete[] m_apVariants;
	if(m_pFT) msdfgen::deinitializeFreetype(m_pFT);
}

void CTextRender::LoadFonts(IStorage *pStorage, IConsole *pConsole)
{
	// read file data into buffer
	const char *pFilename = "fonts/index.json";
	IOHANDLE File = pStorage->OpenFile(pFilename, IOFLAG_READ, IStorage::TYPE_ALL);
	if(!File)
	{
		pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "textrender", "couldn't open fonts index file");
		return;
	}
	int FileSize = (int)io_length(File);
	char *pFileData = (char *)mem_alloc(FileSize, 1);
	io_read(File, pFileData, FileSize);
	io_close(File);

	// parse json data
	json_settings JsonSettings;
	mem_zero(&JsonSettings, sizeof(JsonSettings));
	char aError[256];
	json_value *pJsonData = json_parse_ex(&JsonSettings, pFileData, FileSize, aError);
	mem_free(pFileData);

	if(pJsonData == 0)
	{
		pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, pFilename, aError);
		return;
	}

	// extract font file definitions
	const json_value &rFiles = (*pJsonData)["font files"];
	if(rFiles.type == json_array)
	{
		for(unsigned i = 0; i < rFiles.u.array.length && i < MAX_FACES; ++i)
		{
			char aFontName[IO_MAX_PATH_LENGTH];
			str_format(aFontName, sizeof(aFontName), "fonts/%s", (const char *)rFiles[i]);
			char aFilename[IO_MAX_PATH_LENGTH];
			IOHANDLE File = pStorage->OpenFile(aFontName, IOFLAG_READ, IStorage::TYPE_ALL, aFilename, sizeof(aFilename));
			if(File)
			{
				long FileSize = io_length(File);
				m_apFontData[i] = mem_alloc(FileSize, 1);
				io_read(File, m_apFontData[i], FileSize);
				io_close(File);
				if(LoadFontCollection(aFilename, m_apFontData[i], FileSize))
				{
					char aBuf[256];	
					str_format(aBuf, sizeof(aBuf), "failed to load font. filename='%s'", aFontName);	
					pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "textrender", aBuf);
				}
			}
		}
	}

	// extract default family name
	const json_value &rDefaultFace = (*pJsonData)["default"];
	if(rDefaultFace.type == json_string)
	{
		m_pGlyphMap->SetDefaultFaceByName((const char *)rDefaultFace);
	}

	// extract fallback family names
	const json_value &rFallbackFaces = (*pJsonData)["fallbacks"];
	if(rFallbackFaces.type == json_array)
	{
		for(unsigned i = 0; i < rFallbackFaces.u.array.length; ++i)
		{
			m_pGlyphMap->AddFallbackFaceByName((const char *)rFallbackFaces[i]);
		}
	}

	// extract language variant family names
	const json_value &rVariant = (*pJsonData)["language variants"];
	if(rVariant.type == json_object)
	{
		m_NumVariants = rVariant.u.object.length;
		json_object_entry *Entries = rVariant.u.object.values;
		m_apVariants = new CFontLanguageVariant[m_NumVariants];
		for(int i = 0; i < m_NumVariants; ++i)
		{
			char aFileName[128];
			str_format(aFileName, sizeof(aFileName), "languages/%s.json", (const char *)Entries[i].name);
			str_copy(m_apVariants[i].m_aLanguageFile, aFileName, sizeof(m_apVariants[i].m_aLanguageFile));

			json_value *pFamilyName = rVariant.u.object.values[i].value;
			if(pFamilyName->type == json_string)
				str_copy(m_apVariants[i].m_aFamilyName, pFamilyName->u.string.ptr, sizeof(m_apVariants[i].m_aFamilyName));
			else
				m_apVariants[i].m_aFamilyName[0] = 0;
		}
	}

	json_value_free(pJsonData);
}

void CTextRender::SetFontLanguageVariant(const char *pLanguageFile)
{
	if(!m_pGlyphMap) return;	

	char *FamilyName = NULL;

	if(m_apVariants)
	{
		for(int i = 0; i < m_NumVariants; ++i)
		{
			if(str_comp_filenames(pLanguageFile, m_apVariants[i].m_aLanguageFile) == 0)
			{
				FamilyName = m_apVariants[i].m_aFamilyName;
				m_CurrentVariant = i;
				break;
			}
		}
	}

	m_pGlyphMap->SetVariantFaceByName(FamilyName);
}

void CTextRender::TextColor(float r, float g, float b, float a)
{
	m_TextR = r;
	m_TextG = g;
	m_TextB = b;
	m_TextA = a;
}

void CTextRender::TextSecondaryColor(float r, float g, float b, float a)
{
	m_TextSecondaryR = r;
	m_TextSecondaryG = g;
	m_TextSecondaryB = b;
	m_TextSecondaryA = a;
}

float CTextRender::TextWidth(float FontSize, const char *pText, int Length)
{
	static CTextCursor s_Cursor;
	s_Cursor.m_FontSize = FontSize;
	s_Cursor.m_Flags = TEXTFLAG_NO_RENDER;
	s_Cursor.Reset();
	TextDeferred(&s_Cursor, pText, Length);
	return s_Cursor.m_Width;
}

void CTextRender::TextDeferred(CTextCursor *pCursor, const char *pText, int Length)
{
	if(pCursor->m_Truncated || pCursor->m_SkipTextRender)
		return;

	if(!m_pGlyphMap->GetDefaultFace())
		return;

	bool Render = !(pCursor->m_Flags & TEXTFLAG_NO_RENDER);

	// Sizes
	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	int ScreenWidth = Graphics()->ScreenWidth();
	int ScreenHeight = Graphics()->ScreenHeight();
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);

	vec2 ScreenScale = vec2(ScreenWidth/(ScreenX1-ScreenX0), ScreenHeight/(ScreenY1-ScreenY0));
	float Size = pCursor->m_FontSize;
	int PixelSize = (int)(Size * ScreenScale.y);
	Size = PixelSize / ScreenScale.y;

	// Cursor current states
	int Flags = pCursor->m_Flags;
	float MaxWidth = pCursor->m_MaxWidth;
	if(MaxWidth < 0)
		MaxWidth = (ScreenX1-ScreenX0);
	int MaxLines = pCursor->m_MaxLines;
	if(MaxLines < 0)
		MaxLines = (ScreenY1-ScreenY0) / pCursor->m_FontSize;

	if(Length < 0)
		Length = str_length(pText);

	const char *pCur = (char *)pText;
	const char *pEnd = (char *)pText + Length;

	float NextAdvanceY = pCursor->m_Advance.y + pCursor->m_FontSize;
	NextAdvanceY = (int)(NextAdvanceY * ScreenScale.y) / ScreenScale.y;
	pCursor->m_NextLineAdvanceY = max(NextAdvanceY, pCursor->m_NextLineAdvanceY);

	float WordStartAdvanceX = pCursor->m_Advance.x;
	CWordWidthHint WordWidth = MakeWord(pCursor, pCur, pEnd, Size, PixelSize, ScreenScale);
	const char *pWordEnd = pCur + WordWidth.m_CharCount;

	while(pWordEnd <= pEnd && WordWidth.m_CharCount >= 0)
	{
		if(WordWidth.m_EffectiveAdvanceX > MaxWidth)
		{
			int NumGlyphs = pCursor->m_GlyphCount;
			int WordStartGlyphIndex = NumGlyphs - WordWidth.m_GlyphCount;
			// Do not let space create new line.
			if(WordWidth.m_GlyphCount == 1 && (*pCur == ' ' || *pCur == '\n' || *pCur == '\t'))
			{
				if(Render)
					pCursor->m_Glyphs.remove_index(NumGlyphs-1);
				pCursor->m_GlyphCount--;
				pCursor->m_Advance.x = WordStartAdvanceX;
			}
			else
			{
				pCursor->m_LineCount++;
				float AdvanceY = pCursor->m_Advance.y;
				pCursor->m_Advance.y = pCursor->m_LineSpacing + pCursor->m_NextLineAdvanceY;
				pCursor->m_Advance.x -= WordStartAdvanceX;

				float NextAdvanceY = pCursor->m_Advance.y + pCursor->m_FontSize;
				NextAdvanceY = (int)(NextAdvanceY * ScreenScale.y) / ScreenScale.y;
				pCursor->m_NextLineAdvanceY = max(NextAdvanceY, pCursor->m_NextLineAdvanceY);

				if(pCursor->m_LineCount > MaxLines)
				{
					for(int i = NumGlyphs - 1; i >= WordStartGlyphIndex; --i)
					{
						if(Render)
							pCursor->m_Glyphs.remove_index(i);
						pCursor->m_GlyphCount--;
					}
					pCursor->m_Truncated = true;
				}
				else if(Render)
				{
					for(int i = NumGlyphs - 1; i >= WordStartGlyphIndex; --i)
					{
						pCursor->m_Glyphs[i].m_Advance.x -= WordStartAdvanceX;
						pCursor->m_Glyphs[i].m_Advance.y += pCursor->m_Advance.y - AdvanceY;
						pCursor->m_Glyphs[i].m_Line = pCursor->m_LineCount - 1;
					}
				}

				pCursor->m_StartOfLine = false;
			}
		}

		pCursor->m_Width = max(pCursor->m_Advance.x, pCursor->m_Width);

		bool ForceNewLine = WordWidth.m_EndsWithNewline && (Flags & TEXTFLAG_ALLOW_NEWLINE);
		if(ForceNewLine || WordWidth.m_IsBroken)
		{
			pCursor->m_LineCount++;
			pCursor->m_Advance.y = pCursor->m_LineSpacing + pCursor->m_NextLineAdvanceY;
			pCursor->m_Advance.x = 0;
			pCursor->m_StartOfLine = true;

			float NextAdvanceY = pCursor->m_Advance.y + pCursor->m_FontSize;
			NextAdvanceY = (int)(NextAdvanceY * ScreenScale.y) / ScreenScale.y;
			pCursor->m_NextLineAdvanceY = max(NextAdvanceY, pCursor->m_NextLineAdvanceY);
		}

		WordStartAdvanceX = pCursor->m_Advance.x;

		pCur = pWordEnd;
		WordWidth = MakeWord(pCursor, pCur, pEnd, Size, PixelSize, ScreenScale);
		pWordEnd = pWordEnd + WordWidth.m_CharCount;
		pCursor->m_StartOfLine = false;

		if(pCursor->m_LineCount > MaxLines)
		{
			pCursor->m_LineCount--;
			if(WordWidth.m_CharCount > 0)
			{
				int NumGlyphs = pCursor->m_GlyphCount;
				int WordStartGlyphIndex = NumGlyphs - WordWidth.m_GlyphCount;
				for(int i = NumGlyphs - 1; i >= WordStartGlyphIndex; --i)
				{
					if(Render)
						pCursor->m_Glyphs.remove_index(i);
					pCursor->m_GlyphCount--;
				}
			}
			pCursor->m_Truncated = true;
			break;
		}
	}

	pCursor->m_Height = pCursor->m_NextLineAdvanceY + 0.35f * Size;
	pCursor->m_CharCount = pCur - pText;

	// insert "..." at the end
	if(pCursor->m_Truncated && pCursor->m_Flags & TEXTFLAG_ELLIPSIS)
	{
		const char ellipsis[] = "...";
		if(pCursor->m_Glyphs.size() > 0)
		{
			CScaledGlyph *pLastGlyph = &pCursor->m_Glyphs[pCursor->m_Glyphs.size()-1];
			pCursor->m_Advance.x = pLastGlyph->m_Advance.x + pLastGlyph->m_pGlyph->m_AdvanceX * pLastGlyph->m_Size;
			pCursor->m_Advance.y = pLastGlyph->m_Advance.y;
		}

		int OldMaxWidth = pCursor->m_MaxWidth;
		pCursor->m_MaxWidth = -1;
		WordWidth = MakeWord(pCursor, ellipsis, ellipsis+sizeof(ellipsis), Size, PixelSize, ScreenScale);
		pCursor->m_MaxWidth = OldMaxWidth;
		if(WordWidth.m_EffectiveAdvanceX > MaxWidth)
		{
			int NumDots = WordWidth.m_GlyphCount;
			int NumGlyphs = pCursor->m_Glyphs.size() - NumDots;
			float BackAdvanceX = 0;
			for(int i = NumGlyphs - 1; i > 1; --i)
			{
				CScaledGlyph *pScaled = &pCursor->m_Glyphs[i];
				if(pScaled->m_Advance.x + pScaled->m_pGlyph->m_AdvanceX * pScaled->m_Size > MaxWidth)
				{
					BackAdvanceX = pCursor->m_Advance.x - pScaled->m_Advance.x;
					pCursor->m_Glyphs.remove_index(i);
				}
				else
					break;
			}
			for(int i = pCursor->m_Glyphs.size() - NumDots; i < pCursor->m_Glyphs.size(); ++i)
				pCursor->m_Glyphs[i].m_Advance.x -= BackAdvanceX;
		}
	}

	TextRefreshGlyphs(pCursor);
}

void CTextRender::TextNewline(CTextCursor *pCursor)
{
	if(pCursor->m_Truncated || pCursor->m_SkipTextRender)
		return;

	// Sizes
	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	int ScreenWidth = Graphics()->ScreenWidth();
	int ScreenHeight = Graphics()->ScreenHeight();
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);

	vec2 ScreenScale = vec2(ScreenWidth/(ScreenX1-ScreenX0), ScreenHeight/(ScreenY1-ScreenY0));
	float Size = pCursor->m_FontSize;
	int PixelSize = (int)(Size * ScreenScale.y);
	Size = PixelSize / ScreenScale.y;

	pCursor->m_LineCount++;
	pCursor->m_Advance.y = pCursor->m_LineSpacing + pCursor->m_NextLineAdvanceY;
	pCursor->m_Advance.x = 0;
	pCursor->m_StartOfLine = true;
	float NextAdvanceY = pCursor->m_Advance.y + pCursor->m_FontSize;
	NextAdvanceY = (int)(NextAdvanceY * ScreenScale.y) / ScreenScale.y;
	pCursor->m_NextLineAdvanceY = NextAdvanceY;

	int MaxLines = pCursor->m_MaxLines;
	if(MaxLines < 0)
		MaxLines = INT32_MAX;

	if(pCursor->m_LineCount > MaxLines)
	{
		pCursor->m_LineCount = MaxLines;
		pCursor->m_Truncated = true;
	}
}

void CTextRender::TextAdvance(CTextCursor *pCursor, float AdvanceX)
{
	int LineWidth = pCursor->m_Advance.x + AdvanceX;
	float MaxWidth = pCursor->m_MaxWidth;
	if(MaxWidth < 0)
		MaxWidth = INFINITY;
	if(LineWidth > MaxWidth)
	{
		TextNewline(pCursor);
		pCursor->m_Advance.x = LineWidth - MaxWidth;
	}
	else
	{
		pCursor->m_Advance.x = LineWidth;
	}
}

void CTextRender::DrawText(CTextCursor *pCursor, vec2 Offset, bool IsSecondary, float Alpha, int StartGlyph = 0, int NumGlyphs = -1)
{
	int NumQuads = pCursor->m_Glyphs.size();
	if(NumQuads <= 0)
		return;
	
	if(NumGlyphs < 0)
		NumGlyphs = NumQuads;
	
	int EndGlyphs = StartGlyph + NumGlyphs;

	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	int ScreenWidth = Graphics()->ScreenWidth();
	int ScreenHeight = Graphics()->ScreenHeight();
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);

	vec2 ScreenScale = vec2(ScreenWidth/(ScreenX1-ScreenX0), ScreenHeight/(ScreenY1-ScreenY0));

	int HorizontalAlign = pCursor->m_Align & TEXTALIGN_MASK_HORI;
	CTextBoundingBox AlignBox = pCursor->AlignedBoundingBox();
	vec2 AlignOffset = vec2(AlignBox.x, AlignBox.y);
	float BoxWidth = (int)(pCursor->m_Width * ScreenScale.x) / ScreenScale.x;

	vec4 LastColor = vec4(-1, -1, -1, -1);
	Graphics()->TextureSet(m_pGlyphMap->GetTexture());
	Graphics()->SetMSDF(true);
	Graphics()->QuadsBegin();

	int Line = -1;
	vec2 LineOffset = vec2(0, 0);
	vec2 Anchor = (pCursor->m_CursorPos + AlignOffset) * ScreenScale;
	Anchor.x = (int)Anchor.x / ScreenScale.x;
	Anchor.y = (int)Anchor.y / ScreenScale.y;

	Anchor += Offset / ScreenScale;

	for(int i = NumQuads - 1; i >= 0; --i)
	{
		const CScaledGlyph& rScaled = pCursor->m_Glyphs[i];
		const CGlyph *pGlyph = rScaled.m_pGlyph;

		if(Line != rScaled.m_Line)
		{
			Line = rScaled.m_Line;
			if(HorizontalAlign == TEXTALIGN_RIGHT)
				LineOffset.x = BoxWidth - (rScaled.m_Advance.x + pGlyph->m_AdvanceX * rScaled.m_Size);
			else if(HorizontalAlign == TEXTALIGN_CENTER)
				LineOffset.x = (BoxWidth - (rScaled.m_Advance.x + pGlyph->m_AdvanceX * rScaled.m_Size)) / 2.0f;
			else
				LineOffset.x = 0;
		}

		if(i < StartGlyph || i >= EndGlyphs)
			continue;

		vec4 Color;
		if(IsSecondary)
			Color = vec4(rScaled.m_SecondaryColor.r, rScaled.m_SecondaryColor.g, rScaled.m_SecondaryColor.b, rScaled.m_SecondaryColor.a);
		else
			Color = vec4(rScaled.m_TextColor.r, rScaled.m_TextColor.g, rScaled.m_TextColor.b, rScaled.m_TextColor.a);

		if(Color != LastColor)
		{
			Graphics()->SetColor(Color.r, Color.g, Color.b, Color.a * Alpha);
			LastColor = Color;
		}

		Graphics()->QuadsSetSubset(pGlyph->m_aUvCoords[0], pGlyph->m_aUvCoords[1], pGlyph->m_aUvCoords[2], pGlyph->m_aUvCoords[3]);

		vec2 QuadPosition = Anchor + LineOffset + rScaled.m_Advance + vec2(pGlyph->m_BearingX, pGlyph->m_BearingY) * rScaled.m_Size;
		IGraphics::CQuadItem QuadItem = IGraphics::CQuadItem(QuadPosition.x, QuadPosition.y, pGlyph->m_Width * rScaled.m_Size, pGlyph->m_Height * rScaled.m_Size);
		Graphics()->QuadsDrawTL(&QuadItem, 1);
	}
	Graphics()->QuadsEnd();
	Graphics()->SetMSDF(false);
}

void CTextRender::TextPlain(CTextCursor *pCursor, const char *pText, int Length)
{
	TextDeferred(pCursor, pText, Length);
	DrawTextPlain(pCursor, 1.0f, 0, -1);
}

void CTextRender::TextOutlined(CTextCursor *pCursor, const char *pText, int Length)
{
	TextDeferred(pCursor, pText, Length);
	DrawTextOutlined(pCursor, 1.0f, 0, -1);
}

void CTextRender::TextShadowed(CTextCursor *pCursor, const char *pText, int Length, vec2 ShadowOffset)
{
	TextDeferred(pCursor, pText, Length);
	DrawTextShadowed(pCursor, ShadowOffset, 1.0f, 0, -1);
}

void CTextRender::DrawTextPlain(CTextCursor *pCursor, float Alpha, int StartGlyph, int NumGlyphs)
{
	TextRefreshGlyphs(pCursor);
	DrawText(pCursor, vec2(0, 0), false, Alpha, StartGlyph, NumGlyphs);
}

void CTextRender::DrawTextOutlined(CTextCursor *pCursor, float Alpha, int StartGlyph, int NumGlyphs)
{
	TextRefreshGlyphs(pCursor);
	// TODO: outline
	//DrawText(pCursor, vec2(0, 0), true, Alpha, StartGlyph, NumGlyphs);
	DrawText(pCursor, vec2(0, 0), false, Alpha, StartGlyph, NumGlyphs);
}

void CTextRender::DrawTextShadowed(CTextCursor *pCursor, vec2 ShadowOffset, float Alpha, int StartGlyph, int NumGlyphs)
{
	TextRefreshGlyphs(pCursor);
	DrawText(pCursor, ShadowOffset, true, Alpha, StartGlyph, NumGlyphs);
	DrawText(pCursor, vec2(0, 0), false, Alpha, StartGlyph, NumGlyphs);
}

vec2 CTextRender::CaretPosition(CTextCursor *pCursor, int NumChars)
{
	int CursorChars = 0;
	int NumGlpyhs = pCursor->m_Glyphs.size();
	if(NumGlpyhs == 0 || NumChars == 0)
		return pCursor->m_CursorPos;

	for(int i = 0; i < NumGlpyhs; ++i)
	{
		CursorChars += pCursor->m_Glyphs[i].m_NumChars;
		if(CursorChars > NumChars)
			return pCursor->m_CursorPos + pCursor->m_Glyphs[i].m_Advance;
	}

	CScaledGlyph *pLastScaled = &pCursor->m_Glyphs[NumGlpyhs-1];
	return pCursor->m_CursorPos + pLastScaled->m_Advance + vec2(pLastScaled->m_pGlyph->m_AdvanceX, 0) * pLastScaled->m_Size;
}

IEngineTextRender *CreateEngineTextRender() { return new CTextRender; }
