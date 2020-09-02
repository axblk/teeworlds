/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/system.h>
#include <base/math.h>
#include <base/tl/sorted_array.h>

#include <engine/graphics.h>
#include <engine/textrender.h>

#ifdef CONF_FAMILY_WINDOWS
	#include <windows.h>
#endif

// ft2 texture
#include <ft2build.h>
#include FT_FREETYPE_H

static int s_aFontSizes[] = {8,9,10,11,12,13,14,15,16,17,18,19,20,36,64};
#define NUM_FONT_SIZES (sizeof(s_aFontSizes)/sizeof(int))

enum
{
	MAX_CHARACTERS = 64,

	VERTICES_PER_QUAD = 4,
	VERTEX_BUFFER_QUADS = 4096,
	VERTEX_BUFFER_SIZE = VERTEX_BUFFER_QUADS * VERTICES_PER_QUAD
};

static int GetFontSizeIndex(int Pixelsize)
{
	for(unsigned i = 0; i < NUM_FONT_SIZES; i++)
	{
		if(s_aFontSizes[i] >= Pixelsize)
			return i;
	}

	return NUM_FONT_SIZES-1;
}

struct CFontChar
{
	int m_ID;
	int m_TexSlot;

	// these values are scaled to the pFont size
	// width * font_size == real_size
	float m_Width;
	float m_Height;
	float m_OffsetX;
	float m_OffsetY;
	float m_AdvanceX;

	float m_aUvs[8];
	int64 m_TouchTime;

	bool operator <(const CFontChar &Other) const { return m_ID < Other.m_ID; }
	bool operator ==(const CFontChar &Other) const { return m_ID == Other.m_ID; }
};

class CFontSizeData
{
	// 32k of data used for rendering glyphs
	static unsigned char ms_aGlyphData[(1024/8) * (1024/8)];
	static unsigned char ms_aGlyphDataOutlined[(1024/8) * (1024/8)];

	static int ms_FontMemoryUsage;

	IGraphics *m_pGraphics;
	FT_Face m_FtFace;

	IGraphics::CTextureHandle m_Texture;
	int m_TextureWidth;
	int m_TextureHeight;

	int m_NumXChars;
	int m_NumYChars;

	int m_CharMaxWidth;
	int m_CharMaxHeight;

	sorted_array<CFontChar> m_lCharacters;

	int64 m_LastCharsModified; // existing characters were modified

	void FreeTexture()
	{
		if(m_pGraphics && m_Texture.IsValid())
		{
			m_pGraphics->UnloadTexture(&m_Texture);
			ms_FontMemoryUsage -= m_TextureWidth*m_TextureHeight;
		}
	}

	void InitTexture(int CharWidth, int CharHeight, int Xchars, int Ychars)
	{
		int Width = CharWidth*Xchars;
		int Height = CharHeight*Ychars;
		void *pMem = mem_alloc(Width*Height, 1);
		mem_zero(pMem, Width*Height);

		FreeTexture();

		m_Texture = m_pGraphics->LoadTextureRaw(Width, Height, CImageInfo::FORMAT_ALPHA, pMem, CImageInfo::FORMAT_ALPHA, IGraphics::TEXLOAD_NOMIPMAPS);
		ms_FontMemoryUsage += Width*Height;

		m_NumXChars = Xchars;
		m_NumYChars = Ychars;
		m_TextureWidth = Width;
		m_TextureHeight = Height;
		m_LastCharsModified = time_get();

		m_lCharacters.clear();
		m_lCharacters.hint_size(m_NumXChars*m_NumYChars/2);

		dbg_msg("textrender", "font memory usage: %d", ms_FontMemoryUsage);

		mem_free(pMem);
	}

	void IncreaseTextureSize()
	{
		if(m_TextureWidth < m_TextureHeight)
			m_NumXChars <<= 1;
		else
			m_NumYChars <<= 1;
		InitTexture(m_CharMaxWidth, m_CharMaxHeight, m_NumXChars, m_NumYChars);
	}

	static int AdjustOutlineThicknessToFontSize(int OutlineThickness, int FontSize)
	{
		if(FontSize > 36)
			OutlineThickness *= 4;
		else if(FontSize >= 18)
			OutlineThickness *= 2;
		return OutlineThickness;
	}

	void UploadGlyph(int Texnum, int SlotID, const void *pData)
	{
		int x = (SlotID%m_NumXChars) * (m_TextureWidth/m_NumXChars);
		int y = (SlotID/m_NumXChars) * (m_TextureHeight/m_NumYChars);

		m_pGraphics->LoadTextureRawSub(m_Texture, x, y,
			m_TextureWidth/m_NumXChars,
			m_TextureHeight/m_NumYChars,
			CImageInfo::FORMAT_ALPHA, pData);
	}

	int GetSlot(int Chr, int64 Now)
	{
		CFontChar New;
		New.m_ID = Chr;

		int CharCount = m_NumXChars*m_NumYChars/2;
		if(m_lCharacters.size() < CharCount)
		{
			New.m_TexSlot = m_lCharacters.size();
			return m_lCharacters.add(New);
		}

		// kick out the oldest
		// TODO: remove this linear search
		{
			int Oldest = 0;
			for(int i = 1; i < m_lCharacters.size(); i++)
			{
				if(m_lCharacters[i].m_TouchTime < m_lCharacters[Oldest].m_TouchTime)
					Oldest = i;
			}

			if(Now-m_lCharacters[Oldest].m_TouchTime < time_freq() &&
				(m_NumXChars < MAX_CHARACTERS || m_NumYChars < MAX_CHARACTERS))
			{
				IncreaseTextureSize();
				return GetSlot(Chr, Now);
			}

			m_LastCharsModified = time_get();

			New.m_TexSlot = m_lCharacters[Oldest].m_TexSlot;
			m_lCharacters.remove_index(Oldest);
			return m_lCharacters.add(New);
		}
	}

	static void Grow(unsigned char *pIn, unsigned char *pOut, int w, int h)
	{
		for(int y = 0; y < h; y++)
			for(int x = 0; x < w; x++)
			{
				int c = pIn[y*w+x];

				for(int sy = -1; sy <= 1; sy++)
					for(int sx = -1; sx <= 1; sx++)
					{
						int GetX = x+sx;
						int GetY = y+sy;
						if (GetX >= 0 && GetY >= 0 && GetX < w && GetY < h)
						{
							int Index = GetY*w+GetX;
							if(pIn[Index] > c)
								c = pIn[Index];
						}
					}

				pOut[y*w+x] = c;
			}
	}

	int RenderGlyph(int Chr, int64 Now)
	{
		FT_Bitmap *pBitmap;
		int SlotID = 0;
		int SlotW = m_TextureWidth / m_NumXChars;
		int SlotH = m_TextureHeight / m_NumYChars;
		int SlotSize = SlotW*SlotH;
		int x = 1;
		int y = 1;
		unsigned int px, py;

		FT_Set_Pixel_Sizes(m_FtFace, 0, m_FontSize);

		if(FT_Load_Char(m_FtFace, Chr, FT_LOAD_RENDER|FT_LOAD_NO_BITMAP))
		{
			dbg_msg("textrender", "error loading glyph %d", Chr);
			return -1;
		}

		pBitmap = &m_FtFace->glyph->bitmap; // ignore_convention

		// fetch slot
		SlotID = GetSlot(Chr, Now);
		if(SlotID < 0)
			return -1;

		// adjust spacing
		int OutlineThickness = AdjustOutlineThicknessToFontSize(1, m_FontSize);
		x += OutlineThickness;
		y += OutlineThickness;

		// prepare glyph data
		mem_zero(ms_aGlyphData, SlotSize);

		if(pBitmap->pixel_mode == FT_PIXEL_MODE_GRAY) // ignore_convention
		{
			for(py = 0; py < pBitmap->rows; py++) // ignore_convention
				for(px = 0; px < pBitmap->width; px++) // ignore_convention
					ms_aGlyphData[(py+y)*SlotW+px+x] = pBitmap->buffer[py*pBitmap->pitch+px]; // ignore_convention
		}
		else if(pBitmap->pixel_mode == FT_PIXEL_MODE_MONO) // ignore_convention
		{
			for(py = 0; py < pBitmap->rows; py++) // ignore_convention
				for(px = 0; px < pBitmap->width; px++) // ignore_convention
				{
					if(pBitmap->buffer[py*pBitmap->pitch+px/8]&(1<<(7-(px%8)))) // ignore_convention
						ms_aGlyphData[(py+y)*SlotW+px+x] = 255;
				}
		}

		// upload the glyph
		int TextureSlot = m_lCharacters[SlotID].m_TexSlot * 2;
		int TextureOutlineSlot = m_lCharacters[SlotID].m_TexSlot * 2 + 1;
		UploadGlyph(0, TextureSlot, ms_aGlyphData);

		if(OutlineThickness == 1)
		{
			Grow(ms_aGlyphData, ms_aGlyphDataOutlined, SlotW, SlotH);
			UploadGlyph(1, TextureOutlineSlot, ms_aGlyphDataOutlined);
		}
		else
		{
			for(int i = OutlineThickness; i > 0; i-=2)
			{
				Grow(ms_aGlyphData, ms_aGlyphDataOutlined, SlotW, SlotH);
				Grow(ms_aGlyphDataOutlined, ms_aGlyphData, SlotW, SlotH);
			}
			UploadGlyph(1, TextureOutlineSlot, ms_aGlyphData);
		}

		// set char info
		{
			CFontChar *pFontchr = &m_lCharacters[SlotID];
			float Scale = 1.0f/m_FontSize;
			float Uscale = 1.0f/m_TextureWidth;
			float Vscale = 1.0f/m_TextureHeight;
			int Height = pBitmap->rows + OutlineThickness*2 + 2; // ignore_convention
			int Width = pBitmap->width + OutlineThickness*2 + 2; // ignore_convention

			pFontchr->m_ID = Chr;
			pFontchr->m_Height = Height * Scale;
			pFontchr->m_Width = Width * Scale;
			pFontchr->m_OffsetX = (m_FtFace->glyph->bitmap_left-2) * Scale; // ignore_convention
			pFontchr->m_OffsetY = (m_FontSize - m_FtFace->glyph->bitmap_top) * Scale; // ignore_convention
			pFontchr->m_AdvanceX = (m_FtFace->glyph->advance.x>>6) * Scale; // ignore_convention

			pFontchr->m_aUvs[0] = (TextureSlot%m_NumXChars) / (float)(m_NumXChars);
			pFontchr->m_aUvs[1] = (TextureSlot/m_NumXChars) / (float)(m_NumYChars);
			pFontchr->m_aUvs[2] = pFontchr->m_aUvs[0] + Width*Uscale;
			pFontchr->m_aUvs[3] = pFontchr->m_aUvs[1] + Height*Vscale;

			// outline
			pFontchr->m_aUvs[4] = (TextureOutlineSlot%m_NumXChars) / (float)(m_NumXChars);
			pFontchr->m_aUvs[5] = (TextureOutlineSlot/m_NumXChars) / (float)(m_NumYChars);
			pFontchr->m_aUvs[6] = pFontchr->m_aUvs[4] + Width*Uscale;
			pFontchr->m_aUvs[7] = pFontchr->m_aUvs[5] + Height*Vscale;
		}

		return SlotID;
	}

public:
	int m_FontSize;

	~CFontSizeData() { FreeTexture(); }

	IGraphics::CTextureHandle GetTexture() const { return m_Texture; }
	int64 LastCharsModified() const { return m_LastCharsModified; }

	void Init(IGraphics *pGraphics, FT_Face FtFace, int FontSize)
	{
		m_pGraphics = pGraphics;
		m_FtFace = FtFace;
		m_FontSize = FontSize;

		FT_Set_Pixel_Sizes(m_FtFace, 0, m_FontSize);

		int OutlineThickness = AdjustOutlineThicknessToFontSize(1, m_FontSize);

		{
			unsigned GlyphIndex;
			int MaxH = 0;
			int MaxW = 0;

			int Charcode = FT_Get_First_Char(m_FtFace, &GlyphIndex);
			while(GlyphIndex != 0)
			{
				// do stuff
				FT_Load_Glyph(m_FtFace, GlyphIndex, FT_LOAD_DEFAULT);

				if(m_FtFace->glyph->metrics.width > MaxW) MaxW = m_FtFace->glyph->metrics.width; // ignore_convention
				if(m_FtFace->glyph->metrics.height > MaxH) MaxH = m_FtFace->glyph->metrics.height; // ignore_convention
				Charcode = FT_Get_Next_Char(m_FtFace, Charcode, &GlyphIndex);
			}

			MaxW = (MaxW>>6)+2+OutlineThickness*2;
			MaxH = (MaxH>>6)+2+OutlineThickness*2;

			for(m_CharMaxWidth = 1; m_CharMaxWidth < MaxW; m_CharMaxWidth <<= 1);
			for(m_CharMaxHeight = 1; m_CharMaxHeight < MaxH; m_CharMaxHeight <<= 1);
		}

		InitTexture(m_CharMaxWidth, m_CharMaxHeight, 16, 16);
	}

	const CFontChar *GetChar(int Chr, int64 Now)
	{
		CFontChar *pFontchr = NULL;

		// search for the character
		CFontChar Tmp;
		Tmp.m_ID = Chr;
		sorted_array<CFontChar>::range r = ::find_binary(m_lCharacters.all(), Tmp);

		// check if we need to render the character
		if(r.empty())
		{
			int Index = RenderGlyph(Chr, Now);
			if(Index >= 0)
				pFontchr = &m_lCharacters[Index];
		}
		else
		{
			pFontchr = &r.front();
		}

		// touch the character
		if(pFontchr)
			pFontchr->m_TouchTime = Now;

		return pFontchr;
	}
};

unsigned char CFontSizeData::ms_aGlyphData[] = {};
unsigned char CFontSizeData::ms_aGlyphDataOutlined[] = {};

int CFontSizeData::ms_FontMemoryUsage = 0;

class CFont
{
	IGraphics *m_pGraphics;
	FT_Face m_FtFace;
	char m_aFilename[IO_MAX_PATH_LENGTH];
	CFontSizeData m_aSizes[NUM_FONT_SIZES];

public:
	virtual ~CFont()
	{
		if(m_FtFace != 0)
			FT_Done_Face(m_FtFace);
	}

	bool Init(IGraphics *pGraphics, FT_Library FtLibrary, const char *pFilename)
	{
		m_pGraphics = pGraphics;
		str_copy(m_aFilename, pFilename, sizeof(m_aFilename));

		if(FT_New_Face(FtLibrary, m_aFilename, 0, &m_FtFace))
			return false;

		for(unsigned i = 0; i < NUM_FONT_SIZES; i++)
			m_aSizes[i].m_FontSize = -1;
		
		return true;
	}

	CFontSizeData *GetSizeByIndex(int Index)
	{
		if(m_aSizes[Index].m_FontSize != s_aFontSizes[Index])
			m_aSizes[Index].Init(m_pGraphics, m_FtFace, s_aFontSizes[Index]);
		return &m_aSizes[Index];
	}

	CFontSizeData *GetSize(int Pixelsize)
	{
		int Index = GetFontSizeIndex(Pixelsize);
		return GetSizeByIndex(Index);
	}

	// must only be called from the rendering function as the Font must be set to the correct size
	void RenderSetup(int Size)
	{
		FT_Set_Pixel_Sizes(m_FtFace, 0, Size);
	}

	float Kerning(int Left, int Right)
	{
		FT_Vector Kerning = {0,0};
		FT_Get_Kerning(m_FtFace, Left, Right, FT_KERNING_DEFAULT, &Kerning);
		return (Kerning.x>>6);
	}
};

class CTextParams
{
public:
	enum
	{
		TEXTTYPE_SIMPLE=0,
		TEXTTYPE_OUTLINED,
		TEXTTYPE_SHADOWED
	};

	IGraphics::CColor m_aColor[2];
	vec2 m_Offset;
	int m_Type;

	CTextParams(int Type = TEXTTYPE_SIMPLE) : m_Offset(vec2(0.f, 0.f)), m_Type(Type)
	{
		m_aColor[0].r = m_aColor[0].g = m_aColor[0].b = m_aColor[0].a = 1.f;
		m_aColor[1] = m_aColor[0];
	}

	CTextParams(int Type, IGraphics::CColor Color, IGraphics::CColor SecondaryColor) : m_Offset(vec2(0.f, 0.f)), m_Type(Type)
	{
		m_aColor[0] = Color;
		m_aColor[1] = SecondaryColor;
	}

	inline int QuadsPerChar() const { return m_Type == TEXTTYPE_SIMPLE ? 1 : 2; }
};

inline void FillQuad(IGraphics::CVertex *pVert, float x, float y, float w, float h, const float aUvs[4], IGraphics::CColor Color)
{
	pVert[0].m_Pos.x = x;
	pVert[0].m_Pos.y = y;
	pVert[0].m_Tex.u = aUvs[0];
	pVert[0].m_Tex.v = aUvs[1];
	pVert[0].m_Color = Color;

	pVert[1].m_Pos.x = x + w;
	pVert[1].m_Pos.y = y;
	pVert[1].m_Tex.u = aUvs[2];
	pVert[1].m_Tex.v = aUvs[1];
	pVert[1].m_Color = Color;

	pVert[2].m_Pos.x = x + w;
	pVert[2].m_Pos.y = y + h;
	pVert[2].m_Tex.u = aUvs[2];
	pVert[2].m_Tex.v = aUvs[3];
	pVert[2].m_Color = Color;

	pVert[3].m_Pos.x = x;
	pVert[3].m_Pos.y = y + h;
	pVert[3].m_Tex.u = aUvs[0];
	pVert[3].m_Tex.v = aUvs[3];
	pVert[3].m_Color = Color;
}

class CTextRender : public IEngineTextRender
{
	static IGraphics::CVertex ms_aTextVertices[VERTEX_BUFFER_SIZE];

	IGraphics *m_pGraphics;
	IGraphics *Graphics() { return m_pGraphics; }

	IGraphics::CColor m_TextColor;
	IGraphics::CColor m_TextOutlineColor;

	int64 m_CurTime;

	CFont *m_pDefaultFont;

	FT_Library m_FTLibrary;

	static int WordLength(const char *pText)
	{
		int s = 1;
		while(1)
		{
			if(*pText == 0)
				return s-1;
			if(*pText == '\n' || *pText == '\t' || *pText == ' ')
				return s;
			pText++;
			s++;
		}
	}

	int GetActualSize(float FontSize)
	{
		float ScreenX0, ScreenY0, ScreenX1, ScreenY1, FakeToScreenY;
		Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
		FakeToScreenY = (Graphics()->ScreenHeight()/(ScreenY1-ScreenY0));
		return FontSize * FakeToScreenY;
	}

	CFont *GetFont(const CTextCursor *pCursor)
	{
		CFont *pFont = pCursor->m_pFont;
		if(!pFont)
			pFont = m_pDefaultFont;
		return pFont;
	}

	int TextDeferredRenderEx(CTextCursor *pCursor, const char *pText, int Length,
		const CTextParams *pParams, IGraphics::CVertex *pVertices = 0, int MaxQuadCount = 0)
	{
		CFont *pFont = GetFont(pCursor);
		CFontSizeData *pSizeData = NULL;

		float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
		float FakeToScreenX, FakeToScreenY;
		int ActualX, ActualY;

		int ActualSize;
		int GotNewLine = 0;
		float DrawX = 0.0f, DrawY = 0.0f;
		int LineCount = 0;
		float CursorX, CursorY;

		float Size = pCursor->m_FontSize;

		const int QuadsPerChar = pParams->QuadsPerChar();
		int QuadCount = 0;

		if(!pFont)
			return 0;

		// to correct coords, convert to screen coords, round, and convert back
		Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);

		FakeToScreenX = (Graphics()->ScreenWidth()/(ScreenX1-ScreenX0));
		FakeToScreenY = (Graphics()->ScreenHeight()/(ScreenY1-ScreenY0));
		ActualX = (int)(pCursor->m_X * FakeToScreenX);
		ActualY = (int)(pCursor->m_Y * FakeToScreenY);

		CursorX = ActualX / FakeToScreenX;
		CursorY = ActualY / FakeToScreenY;

		// same with size
		ActualSize = (int)(Size * FakeToScreenY);
		Size = ActualSize / FakeToScreenY;

		pSizeData = pFont->GetSize(ActualSize);
		pFont->RenderSetup(ActualSize);

		float Scale = 1.0f/pSizeData->m_FontSize;

		// set length
		if(Length < 0)
			Length = str_length(pText);


		const char *pCurrent = (char *)pText;
		const char *pEnd = pCurrent+Length;
		DrawX = CursorX;
		DrawY = CursorY;
		LineCount = pCursor->m_LineCount;

		while(pCurrent < pEnd && (pCursor->m_MaxLines < 1 || LineCount <= pCursor->m_MaxLines))
		{
			int NewLine = 0;
			const char *pBatchEnd = pEnd;
			if(pCursor->m_LineWidth > 0 && !(pCursor->m_Flags&TEXTFLAG_STOP_AT_END))
			{
				int Wlen = min(WordLength((char *)pCurrent), (int)(pEnd-pCurrent));
				CTextCursor Compare = *pCursor;
				Compare.m_X = DrawX;
				Compare.m_Y = DrawY;
				Compare.m_Flags &= ~TEXTFLAG_RENDER;
				Compare.m_LineWidth = -1;
				TextDeferredRenderEx(&Compare, pCurrent, Wlen, pParams);

				if(Compare.m_X-DrawX > pCursor->m_LineWidth)
				{
					// word can't be fitted in one line, cut it
					CTextCursor Cutter = *pCursor;
					Cutter.m_GlyphCount = 0;
					Cutter.m_X = DrawX;
					Cutter.m_Y = DrawY;
					Cutter.m_Flags &= ~TEXTFLAG_RENDER;
					Cutter.m_Flags |= TEXTFLAG_STOP_AT_END;

					TextDeferredRenderEx(&Cutter, pCurrent, Wlen, pParams);
					Wlen = Cutter.m_GlyphCount;
					NewLine = 1;

					if(Wlen <= 3) // if we can't place 3 chars of the word on this line, take the next
						Wlen = 0;
				}
				else if(Compare.m_X-pCursor->m_StartX > pCursor->m_LineWidth)
				{
					NewLine = 1;
					Wlen = 0;
				}

				pBatchEnd = pCurrent + Wlen;
			}

			const char *pTmp = pCurrent;
			int NextCharacter = str_utf8_decode(&pTmp);
			while(pCurrent < pBatchEnd)
			{
				pCursor->m_CharCount += pTmp-pCurrent;
				int Character = NextCharacter;
				pCurrent = pTmp;
				NextCharacter = str_utf8_decode(&pTmp);

				if(Character == '\n')
				{
					DrawX = pCursor->m_StartX;
					DrawY += Size;
					GotNewLine = 1;
					DrawX = (int)(DrawX * FakeToScreenX) / FakeToScreenX; // realign
					DrawY = (int)(DrawY * FakeToScreenY) / FakeToScreenY;
					++LineCount;
					if(pCursor->m_MaxLines > 0 && LineCount > pCursor->m_MaxLines)
						break;
					continue;
				}

				const CFontChar *pChr = pSizeData->GetChar(Character, m_CurTime);
				if(pChr)
				{
					float Advance = pChr->m_AdvanceX + pFont->Kerning(Character, NextCharacter)*Scale;
					if(pCursor->m_Flags&TEXTFLAG_STOP_AT_END && DrawX+Advance*Size-pCursor->m_StartX > pCursor->m_LineWidth)
					{
						// we hit the end of the line, no more to render or count
						pCurrent = pEnd;
						break;
					}

					bool IsWhiteSpace = Character == ' ' || Character == '\t';
					if(!IsWhiteSpace && pCursor->m_Flags&TEXTFLAG_RENDER && QuadCount < MaxQuadCount)
					{
						if(pVertices)
						{
							//dbg_assert(CharCount < MaxCharCount, "aQuadChar size is too small");
							// background
							int Index = QuadCount * VERTICES_PER_QUAD;

							if(pParams->m_Type != CTextParams::TEXTTYPE_SIMPLE)
							{
								const float *pUvs = pParams->m_Type == CTextParams::TEXTTYPE_OUTLINED ? pChr->m_aUvs+4 : pChr->m_aUvs;
								FillQuad(&pVertices[Index],
									DrawX+pChr->m_OffsetX*Size+pParams->m_Offset.x, DrawY+pChr->m_OffsetY*Size+pParams->m_Offset.y,
									pChr->m_Width*Size, pChr->m_Height*Size,
									pUvs, pParams->m_aColor[1]);
								Index += VERTICES_PER_QUAD;
							}

							// foreground
							FillQuad(&pVertices[Index],
								DrawX+pChr->m_OffsetX*Size, DrawY+pChr->m_OffsetY*Size,
								pChr->m_Width*Size, pChr->m_Height*Size,
								pChr->m_aUvs, pParams->m_aColor[0]);
						}

						QuadCount += QuadsPerChar;
					}

					DrawX += Advance*Size;
					pCursor->m_GlyphCount++;
				}
			}

			if(NewLine)
			{
				DrawX = pCursor->m_StartX;
				DrawY += Size;
				GotNewLine = 1;
				DrawX = (int)(DrawX * FakeToScreenX) / FakeToScreenX; // realign
				DrawY = (int)(DrawY * FakeToScreenY) / FakeToScreenY;
				++LineCount;
			}
		}

		pCursor->m_X = DrawX;
		pCursor->m_LineCount = LineCount;

		if(GotNewLine)
			pCursor->m_Y = DrawY;

		return QuadCount;
	}

	void TextExInternal(CTextCursor *pCursor, const char *pText, int Length, const CTextParams *pParams)
	{
		int ActualSize = GetActualSize(pCursor->m_FontSize);
		int SizeIndex = GetFontSizeIndex(ActualSize);

		CFont *pFont = GetFont(pCursor);
		if(!pFont)
			return;

		int NumQuads = TextDeferredRenderEx(pCursor, pText, Length, pParams, ms_aTextVertices, VERTEX_BUFFER_QUADS);

		if(pCursor->m_Flags&TEXTFLAG_RENDER)
		{
			Graphics()->TextureSet(pFont->GetSizeByIndex(SizeIndex)->GetTexture());
			Graphics()->RenderQuads(ms_aTextVertices, NumQuads);
		}
	}

public:
	CTextRender()
	{
		m_pGraphics = 0;

		m_TextColor.r = 1.0f;
		m_TextColor.g = 1.0f;
		m_TextColor.b = 1.0f;
		m_TextColor.a = 1.0f;

		m_TextOutlineColor.r = 0.0f;
		m_TextOutlineColor.g = 0.0f;
		m_TextOutlineColor.b = 0.0f;
		m_TextOutlineColor.a = 0.3f;

		m_pDefaultFont = 0;
		m_FTLibrary = 0;
	}

	virtual ~CTextRender()
	{
		if(m_pDefaultFont)
			delete m_pDefaultFont;

		if(m_FTLibrary != 0)
			FT_Done_FreeType(m_FTLibrary);
	}

	virtual void Init()
	{
		m_pGraphics = Kernel()->RequestInterface<IGraphics>();
		FT_Init_FreeType(&m_FTLibrary);
	}

	virtual void UpdateTime(int64 Now)
	{
		m_CurTime = Now;
	}

	virtual CFont *LoadFont(const char *pFilename)
	{
		CFont *pFont = new CFont();
		if(!pFont->Init(Graphics(), m_FTLibrary, pFilename))
		{
			delete pFont;
			return 0;
		}

		dbg_msg("textrender", "loaded font from '%s'", pFilename);
		if(!m_pDefaultFont)
			m_pDefaultFont = pFont;

		return pFont;
	}

	virtual void SetDefaultFont(CFont *pFont)
	{
		m_pDefaultFont = pFont;
	}

	virtual void SetCursor(CTextCursor *pCursor, float x, float y, float FontSize, int Flags)
	{
		mem_zero(pCursor, sizeof(*pCursor));
		pCursor->m_FontSize = FontSize;
		pCursor->m_StartX = x;
		pCursor->m_StartY = y;
		pCursor->m_X = x;
		pCursor->m_Y = y;
		pCursor->m_LineCount = 1;
		pCursor->m_LineWidth = -1.0f;
		pCursor->m_Flags = Flags;
		pCursor->m_GlyphCount = 0;
		pCursor->m_CharCount = 0;
	}

	virtual void Text(void *pFontSetV, float x, float y, float Size, const char *pText, float LineWidth, bool MultiLine)
	{
		CTextCursor Cursor;
		SetCursor(&Cursor, x, y, Size, TEXTFLAG_RENDER);
		if(!MultiLine)
			Cursor.m_Flags |= TEXTFLAG_STOP_AT_END;
		Cursor.m_LineWidth = LineWidth;
		TextEx(&Cursor, pText, -1);
	}

	virtual float TextWidth(void *pFontSetV, float Size, const char *pText, int StrLength, float LineWidth)
	{
		CTextCursor Cursor;
		SetCursor(&Cursor, 0, 0, Size, 0);
		Cursor.m_LineWidth = LineWidth;
		TextEx(&Cursor, pText, StrLength);
		return Cursor.m_X;
	}

	virtual void TextColor(float r, float g, float b, float a)
	{
		m_TextColor.r = r;
		m_TextColor.g = g;
		m_TextColor.b = b;
		m_TextColor.a = a;
	}

	virtual void TextOutlineColor(float r, float g, float b, float a)
	{
		m_TextOutlineColor.r = r;
		m_TextOutlineColor.g = g;
		m_TextOutlineColor.b = b;
		m_TextOutlineColor.a = a;
	}

	virtual void TextShadowed(CTextCursor *pCursor, const char *pText, int Length, vec2 ShadowOffset, vec4 ShadowColor, vec4 TextColor)
	{
		float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
		float FakeToScreenX, FakeToScreenY;

		// to correct coords, convert to screen coords, round, and convert back
		Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);

		FakeToScreenX = (Graphics()->ScreenWidth()/(ScreenX1-ScreenX0));
		FakeToScreenY = (Graphics()->ScreenHeight()/(ScreenY1-ScreenY0));
		ShadowOffset.x /= FakeToScreenX;
		ShadowOffset.y /= FakeToScreenY;

		const IGraphics::CColor ShadowC = { ShadowColor.r, ShadowColor.g, ShadowColor.b, ShadowColor.a };
		const IGraphics::CColor TextC = { TextColor.r, TextColor.g, TextColor.b, TextColor.a };
		CTextParams Params(CTextParams::TEXTTYPE_SHADOWED, TextC, ShadowC);
		Params.m_Offset = ShadowOffset;

		TextExInternal(pCursor, pText, Length, &Params);
	}

	virtual void TextEx(CTextCursor *pCursor, const char *pText, int Length)
	{
		CTextParams Params(CTextParams::TEXTTYPE_OUTLINED, m_TextColor, m_TextOutlineColor);
		TextExInternal(pCursor, pText, Length, &Params);
	}

	virtual void TextSimple(CTextCursor *pCursor, const char *pText, int Length, vec4 TextColor)
	{
		const IGraphics::CColor TextC = { TextColor.r, TextColor.g, TextColor.b, TextColor.a };
		CTextParams Params(CTextParams::TEXTTYPE_SIMPLE, TextC, IGraphics::CColor());
		TextExInternal(pCursor, pText, Length, &Params);
	}

	virtual float TextGetLineBaseY(const CTextCursor *pCursor)
	{
		CFont *pFont = GetFont(pCursor);
		CFontSizeData *pSizeData = NULL;

		float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
		float Size = pCursor->m_FontSize;

		if(!pFont)
			return 0;

		// to correct coords, convert to screen coords, round, and convert back
		Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);

		float FakeToScreenY = (Graphics()->ScreenHeight()/(ScreenY1-ScreenY0));
		int ActualY = (int)(pCursor->m_Y * FakeToScreenY);
		float CursorY = ActualY / FakeToScreenY;

		// same with size
		int ActualSize = (int)(Size * FakeToScreenY);
		Size = ActualSize / FakeToScreenY;

		pSizeData = pFont->GetSize(ActualSize);
		pFont->RenderSetup(ActualSize);
		const CFontChar *pChr = pSizeData->GetChar(' ', m_CurTime);
		return CursorY + pChr->m_OffsetY*Size + pChr->m_Height*Size;
	}
};

IGraphics::CVertex CTextRender::ms_aTextVertices[] = {};

IEngineTextRender *CreateEngineTextRender() { return new CTextRender; }
