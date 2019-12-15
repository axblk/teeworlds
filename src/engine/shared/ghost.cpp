/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/system.h>

#include <engine/console.h>
#include <engine/storage.h>

#include "ghost.h"
#include "compression.h"
#include "network.h"

static const unsigned char gs_aHeaderMarker[8] = {'T', 'W', 'G', 'H', 'O', 'S', 'T', 0};
static const unsigned char gs_ActVersion = 5;
static const int gs_NumTicksOffset = 93;

CGhostRecorder::CGhostRecorder()
{
	m_File = 0;
	ResetBuffer();

	m_Huffman.Init();
}

void CGhostRecorder::Init(IConsole *pConsole, IStorage *pStorage)
{
	m_pStorage = pStorage;
	m_pConsole = pConsole;
}

// Record
int CGhostRecorder::Start(const char *pFilename, const char *pMap, unsigned Crc, const char* pName)
{
	m_File = m_pStorage->OpenFile(pFilename, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(!m_File)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "Unable to open '%s' for ghost recording", pFilename);
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ghost_recorder", aBuf);
		return -1;
	}

	// write header
	CGhostHeader Header;
	mem_zero(&Header, sizeof(Header));
	mem_copy(Header.m_aMarker, gs_aHeaderMarker, sizeof(Header.m_aMarker));
	Header.m_Version = gs_ActVersion;
	str_copy(Header.m_aOwner, pName, sizeof(Header.m_aOwner));
	str_copy(Header.m_aMap, pMap, sizeof(Header.m_aMap));
	uint_to_bytes_be(Header.m_aCrc, Crc);
	io_write(m_File, &Header, sizeof(Header));

	m_LastItem.Reset();
	ResetBuffer();

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "Ghost recording to '%s'", pFilename);
	m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ghost_recorder", aBuf);
	return 0;
}

void CGhostRecorder::ResetBuffer()
{
	m_pBufferPos = m_aBuffer;
	m_BufferNumItems = 0;
}

static void DiffItem(int *pPast, int *pCurrent, int *pOut, int Size)
{
	while(Size)
	{
		*pOut = *pCurrent - *pPast;
		pOut++;
		pPast++;
		pCurrent++;
		Size--;
	}
}

void CGhostRecorder::WriteData(int Type, const char *pData, int Size)
{
	if(!m_File || (unsigned)Size > MAX_ITEM_SIZE || Size <= 0 || Type == -1)
		return;

	CGhostItem Data(Type);
	mem_copy(Data.m_aData, pData, Size);

	if(m_LastItem.m_Type == Data.m_Type)
		DiffItem((int*)m_LastItem.m_aData, (int*)Data.m_aData, (int*)m_pBufferPos, Size/4);
	else
	{
		FlushChunk();
		mem_copy(m_pBufferPos, Data.m_aData, Size);
	}

	m_LastItem = Data;
	m_pBufferPos += Size;
	m_BufferNumItems++;
	if(m_BufferNumItems >= NUM_ITEMS_PER_CHUNK)
		FlushChunk();
}

void CGhostRecorder::FlushChunk()
{
	static char s_aBuffer[MAX_ITEM_SIZE * NUM_ITEMS_PER_CHUNK];
	static char s_aBuffer2[MAX_ITEM_SIZE * NUM_ITEMS_PER_CHUNK];
	unsigned char aChunk[4];

	int Size = m_pBufferPos - m_aBuffer;
	int Type = m_LastItem.m_Type;

	if(!m_File || Size == 0)
		return;

	while(Size&3)
		m_aBuffer[Size++] = 0;

	Size = CVariableInt::Compress(m_aBuffer, Size, s_aBuffer, sizeof(s_aBuffer));
	if(Size < 0)
		return;

	Size = m_Huffman.Compress(s_aBuffer, Size, s_aBuffer2, sizeof(s_aBuffer2));
	if(Size < 0)
		return;

	aChunk[0] = Type&0xff;
	aChunk[1] = m_BufferNumItems&0xff;
	aChunk[2] = (Size>>8)&0xff;
	aChunk[3] = (Size)&0xff;

	io_write(m_File, aChunk, sizeof(aChunk));
	io_write(m_File, s_aBuffer2, Size);

	m_LastItem.Reset();
	ResetBuffer();
}

int CGhostRecorder::Stop(int Ticks, int Time)
{
	if(!m_File)
		return -1;

	m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ghost_recorder", "Stopped ghost recording");

	FlushChunk();

	unsigned char aNumTicks[4];
	unsigned char aTime[4];

	uint_to_bytes_be(aNumTicks, Ticks);
	uint_to_bytes_be(aTime, Time);

	// write down num shots and time
	io_seek(m_File, gs_NumTicksOffset, IOSEEK_START);
	io_write(m_File, &aNumTicks, sizeof(aNumTicks));
	io_write(m_File, &aTime, sizeof(aTime));

	io_close(m_File);
	m_File = 0;
	return 0;
}

CGhostLoader::CGhostLoader()
{
	m_File = 0;
	ResetBuffer();

	m_Huffman.Init();
}

void CGhostLoader::Init(IConsole *pConsole, IStorage *pStorage)
{
	m_pStorage = pStorage;
	m_pConsole = pConsole;
}

void CGhostLoader::ResetBuffer()
{
	m_pBufferPos = m_aBuffer;
	m_BufferNumItems = 0;
	m_BufferCurItem = 0;
	m_BufferPrevItem = -1;
}

int CGhostLoader::Load(const char *pFilename, const char *pMap)
{
	m_File = m_pStorage->OpenFile(pFilename, IOFLAG_READ, IStorage::TYPE_SAVE);
	if(!m_File)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "could not open '%s'", pFilename);
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ghost_loader", aBuf);
		return -1;
	}

	// read the header
	mem_zero(&m_Header, sizeof(m_Header));
	io_read(m_File, &m_Header, sizeof(CGhostHeader));
	if(mem_comp(m_Header.m_aMarker, gs_aHeaderMarker, sizeof(gs_aHeaderMarker)) != 0)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "'%s' is not a ghost file", pFilename);
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ghost_loader", aBuf);
		io_close(m_File);
		m_File = 0;
		return -1;
	}

	if(m_Header.m_Version != gs_ActVersion && m_Header.m_Version != 4)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "ghost version %d is not supported", m_Header.m_Version);
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ghost_loader", aBuf);
		io_close(m_File);
		m_File = 0;
		return -1;
	}

	if(str_comp(m_Header.m_aMap, pMap) != 0)
	{
		io_close(m_File);
		m_File = 0;
		return -1;
	}

	m_LastItem.Reset();
	ResetBuffer();

	return 0;
}

int CGhostLoader::ReadChunk(int *pType)
{
	static char s_aCompresseddata[MAX_ITEM_SIZE * NUM_ITEMS_PER_CHUNK];
	static char s_aDecompressed[MAX_ITEM_SIZE * NUM_ITEMS_PER_CHUNK];
	unsigned char aChunk[4];

	if(m_Header.m_Version != 4)
		m_LastItem.Reset();
	ResetBuffer();

	if(io_read(m_File, aChunk, sizeof(aChunk)) != sizeof(aChunk))
		return -1;

	*pType = aChunk[0];
	int Size = (aChunk[2] << 8) | aChunk[3];
	m_BufferNumItems = aChunk[1];

	if(Size > MAX_ITEM_SIZE * NUM_ITEMS_PER_CHUNK || Size <= 0)
		return -1;

	if(io_read(m_File, s_aCompresseddata, Size) != (unsigned)Size)
	{
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ghost", "error reading chunk");
		return -1;
	}

	Size = m_Huffman.Decompress(s_aCompresseddata, Size, s_aDecompressed, sizeof(s_aDecompressed));
	if(Size < 0)
	{
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ghost", "error during network decompression");
		return -1;
	}

	Size = CVariableInt::Decompress(s_aDecompressed, Size, m_aBuffer, sizeof(m_aBuffer));
	if(Size < 0)
	{
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ghost", "error during intpack decompression");
		return -1;
	}

	return 0;
}

bool CGhostLoader::ReadNextType(int *pType)
{
	if(!m_File)
		return false;

	if(m_BufferCurItem != m_BufferPrevItem && m_BufferCurItem < m_BufferNumItems)
	{
		*pType = m_LastItem.m_Type;
	}
	else
	{
		if(ReadChunk(pType))
			return false; // error or eof
	}

	m_BufferPrevItem = m_BufferCurItem;

	return true;
}

static void UndiffItem(int *pPast, int *pDiff, int *pOut, int Size)
{
	while(Size)
	{
		*pOut = *pPast + *pDiff;
		pOut++;
		pPast++;
		pDiff++;
		Size--;
	}
}

bool CGhostLoader::ReadData(int Type, char *pData, int Size)
{
	if(!m_File || Size > MAX_ITEM_SIZE || Size <= 0 || Type == -1)
		return false;

	CGhostItem Data(Type);

	if(m_LastItem.m_Type == Data.m_Type)
		UndiffItem((int*)m_LastItem.m_aData, (int*)m_pBufferPos, (int*)Data.m_aData, Size/4);
	else
		mem_copy(Data.m_aData, m_pBufferPos, Size);

	mem_copy(pData, Data.m_aData, Size);

	m_LastItem = Data;
	m_pBufferPos += Size;
	m_BufferCurItem++;
	return true;
}

void CGhostLoader::Close()
{
	if(!m_File)
		return;
	io_close(m_File);
	m_File = 0;
}

bool CGhostLoader::GetGhostInfo(const char *pFilename, CGhostHeader *pGhostHeader) const
{
	if(!pGhostHeader)
		return false;

	mem_zero(pGhostHeader, sizeof(CGhostHeader));

	IOHANDLE File = m_pStorage->OpenFile(pFilename, IOFLAG_READ, IStorage::TYPE_SAVE);
	if(!File)
		return false;

	io_read(File, pGhostHeader, sizeof(CGhostHeader));
	io_close(File);

	if(mem_comp(pGhostHeader->m_aMarker, gs_aHeaderMarker, sizeof(gs_aHeaderMarker)) || (pGhostHeader->m_Version != gs_ActVersion && pGhostHeader->m_Version != 4))
		return false;
	return true;
}
