#include <base/detect.h>

#include <base/tl/threading.h>

#include "SDL.h"
#include "SDL_syswm.h"

#include "graphics_threaded.h"
#include "backend_sdl.h"

static const unsigned char s_aVert[] = {
	0x03, 0x02, 0x23, 0x07, 0x00, 0x00, 0x01, 0x00, 0x07, 0x00, 0x08, 0x00, 0x26, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x02, 0x00, 0x01, 0x00, 0x00, 0x00, 0x0B, 0x00, 0x06, 0x00, 
	0x01, 0x00, 0x00, 0x00, 0x47, 0x4C, 0x53, 0x4C, 0x2E, 0x73, 0x74, 0x64, 0x2E, 0x34, 0x35, 0x30, 
	0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 
	0x0F, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x6D, 0x61, 0x69, 0x6E, 
	0x00, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x19, 0x00, 0x00, 0x00, 0x03, 0x00, 0x03, 0x00, 
	0x02, 0x00, 0x00, 0x00, 0xC2, 0x01, 0x00, 0x00, 0x05, 0x00, 0x04, 0x00, 0x04, 0x00, 0x00, 0x00, 
	0x6D, 0x61, 0x69, 0x6E, 0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x06, 0x00, 0x08, 0x00, 0x00, 0x00, 
	0x67, 0x6C, 0x5F, 0x50, 0x65, 0x72, 0x56, 0x65, 0x72, 0x74, 0x65, 0x78, 0x00, 0x00, 0x00, 0x00, 
	0x06, 0x00, 0x06, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x67, 0x6C, 0x5F, 0x50, 
	0x6F, 0x73, 0x69, 0x74, 0x69, 0x6F, 0x6E, 0x00, 0x05, 0x00, 0x03, 0x00, 0x0A, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x06, 0x00, 0x19, 0x00, 0x00, 0x00, 0x67, 0x6C, 0x5F, 0x56, 
	0x65, 0x72, 0x74, 0x65, 0x78, 0x49, 0x6E, 0x64, 0x65, 0x78, 0x00, 0x00, 0x05, 0x00, 0x05, 0x00, 
	0x1C, 0x00, 0x00, 0x00, 0x69, 0x6E, 0x64, 0x65, 0x78, 0x61, 0x62, 0x6C, 0x65, 0x00, 0x00, 0x00, 
	0x48, 0x00, 0x05, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0B, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x47, 0x00, 0x03, 0x00, 0x08, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 
	0x47, 0x00, 0x04, 0x00, 0x19, 0x00, 0x00, 0x00, 0x0B, 0x00, 0x00, 0x00, 0x2A, 0x00, 0x00, 0x00, 
	0x13, 0x00, 0x02, 0x00, 0x02, 0x00, 0x00, 0x00, 0x21, 0x00, 0x03, 0x00, 0x03, 0x00, 0x00, 0x00, 
	0x02, 0x00, 0x00, 0x00, 0x16, 0x00, 0x03, 0x00, 0x06, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 
	0x17, 0x00, 0x04, 0x00, 0x07, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 
	0x1E, 0x00, 0x03, 0x00, 0x08, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x20, 0x00, 0x04, 0x00, 
	0x09, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x3B, 0x00, 0x04, 0x00, 
	0x09, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x15, 0x00, 0x04, 0x00, 
	0x0B, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x2B, 0x00, 0x04, 0x00, 
	0x0B, 0x00, 0x00, 0x00, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x17, 0x00, 0x04, 0x00, 
	0x0D, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x15, 0x00, 0x04, 0x00, 
	0x0E, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2B, 0x00, 0x04, 0x00, 
	0x0E, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x1C, 0x00, 0x04, 0x00, 
	0x10, 0x00, 0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x2B, 0x00, 0x04, 0x00, 
	0x06, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2B, 0x00, 0x04, 0x00, 
	0x06, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xBF, 0x2C, 0x00, 0x05, 0x00, 
	0x0D, 0x00, 0x00, 0x00, 0x13, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00, 
	0x2B, 0x00, 0x04, 0x00, 0x06, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 
	0x2C, 0x00, 0x05, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x15, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 
	0x14, 0x00, 0x00, 0x00, 0x2C, 0x00, 0x05, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x16, 0x00, 0x00, 0x00, 
	0x12, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x2C, 0x00, 0x06, 0x00, 0x10, 0x00, 0x00, 0x00, 
	0x17, 0x00, 0x00, 0x00, 0x13, 0x00, 0x00, 0x00, 0x15, 0x00, 0x00, 0x00, 0x16, 0x00, 0x00, 0x00, 
	0x20, 0x00, 0x04, 0x00, 0x18, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x0B, 0x00, 0x00, 0x00, 
	0x3B, 0x00, 0x04, 0x00, 0x18, 0x00, 0x00, 0x00, 0x19, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 
	0x20, 0x00, 0x04, 0x00, 0x1B, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 
	0x20, 0x00, 0x04, 0x00, 0x1D, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 
	0x2B, 0x00, 0x04, 0x00, 0x06, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 
	0x20, 0x00, 0x04, 0x00, 0x24, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 
	0x36, 0x00, 0x05, 0x00, 0x02, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x03, 0x00, 0x00, 0x00, 0xF8, 0x00, 0x02, 0x00, 0x05, 0x00, 0x00, 0x00, 0x3B, 0x00, 0x04, 0x00, 
	0x1B, 0x00, 0x00, 0x00, 0x1C, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x3D, 0x00, 0x04, 0x00, 
	0x0B, 0x00, 0x00, 0x00, 0x1A, 0x00, 0x00, 0x00, 0x19, 0x00, 0x00, 0x00, 0x3E, 0x00, 0x03, 0x00, 
	0x1C, 0x00, 0x00, 0x00, 0x17, 0x00, 0x00, 0x00, 0x41, 0x00, 0x05, 0x00, 0x1D, 0x00, 0x00, 0x00, 
	0x1E, 0x00, 0x00, 0x00, 0x1C, 0x00, 0x00, 0x00, 0x1A, 0x00, 0x00, 0x00, 0x3D, 0x00, 0x04, 0x00, 
	0x0D, 0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00, 0x1E, 0x00, 0x00, 0x00, 0x51, 0x00, 0x05, 0x00, 
	0x06, 0x00, 0x00, 0x00, 0x21, 0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x51, 0x00, 0x05, 0x00, 0x06, 0x00, 0x00, 0x00, 0x22, 0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00, 
	0x01, 0x00, 0x00, 0x00, 0x50, 0x00, 0x07, 0x00, 0x07, 0x00, 0x00, 0x00, 0x23, 0x00, 0x00, 0x00, 
	0x21, 0x00, 0x00, 0x00, 0x22, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 
	0x41, 0x00, 0x05, 0x00, 0x24, 0x00, 0x00, 0x00, 0x25, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00, 
	0x0C, 0x00, 0x00, 0x00, 0x3E, 0x00, 0x03, 0x00, 0x25, 0x00, 0x00, 0x00, 0x23, 0x00, 0x00, 0x00, 
	0xFD, 0x00, 0x01, 0x00, 0x38, 0x00, 0x01, 0x00
};

static const unsigned char s_aFrag[] = {
	0x03, 0x02, 0x23, 0x07, 0x00, 0x00, 0x01, 0x00, 0x07, 0x00, 0x08, 0x00, 0x0D, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x02, 0x00, 0x01, 0x00, 0x00, 0x00, 0x0B, 0x00, 0x06, 0x00, 
	0x01, 0x00, 0x00, 0x00, 0x47, 0x4C, 0x53, 0x4C, 0x2E, 0x73, 0x74, 0x64, 0x2E, 0x34, 0x35, 0x30, 
	0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 
	0x0F, 0x00, 0x06, 0x00, 0x04, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x6D, 0x61, 0x69, 0x6E, 
	0x00, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x10, 0x00, 0x03, 0x00, 0x04, 0x00, 0x00, 0x00, 
	0x07, 0x00, 0x00, 0x00, 0x03, 0x00, 0x03, 0x00, 0x02, 0x00, 0x00, 0x00, 0xC2, 0x01, 0x00, 0x00, 
	0x05, 0x00, 0x04, 0x00, 0x04, 0x00, 0x00, 0x00, 0x6D, 0x61, 0x69, 0x6E, 0x00, 0x00, 0x00, 0x00, 
	0x05, 0x00, 0x05, 0x00, 0x09, 0x00, 0x00, 0x00, 0x6F, 0x75, 0x74, 0x43, 0x6F, 0x6C, 0x6F, 0x72, 
	0x00, 0x00, 0x00, 0x00, 0x47, 0x00, 0x04, 0x00, 0x09, 0x00, 0x00, 0x00, 0x1E, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x13, 0x00, 0x02, 0x00, 0x02, 0x00, 0x00, 0x00, 0x21, 0x00, 0x03, 0x00, 
	0x03, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x16, 0x00, 0x03, 0x00, 0x06, 0x00, 0x00, 0x00, 
	0x20, 0x00, 0x00, 0x00, 0x17, 0x00, 0x04, 0x00, 0x07, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 
	0x04, 0x00, 0x00, 0x00, 0x20, 0x00, 0x04, 0x00, 0x08, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 
	0x07, 0x00, 0x00, 0x00, 0x3B, 0x00, 0x04, 0x00, 0x08, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 
	0x03, 0x00, 0x00, 0x00, 0x2B, 0x00, 0x04, 0x00, 0x06, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x80, 0x3F, 0x2B, 0x00, 0x04, 0x00, 0x06, 0x00, 0x00, 0x00, 0x0B, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x2C, 0x00, 0x07, 0x00, 0x07, 0x00, 0x00, 0x00, 0x0C, 0x00, 0x00, 0x00, 
	0x0A, 0x00, 0x00, 0x00, 0x0B, 0x00, 0x00, 0x00, 0x0B, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00, 
	0x36, 0x00, 0x05, 0x00, 0x02, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x03, 0x00, 0x00, 0x00, 0xF8, 0x00, 0x02, 0x00, 0x05, 0x00, 0x00, 0x00, 0x3E, 0x00, 0x03, 0x00, 
	0x09, 0x00, 0x00, 0x00, 0x0C, 0x00, 0x00, 0x00, 0xFD, 0x00, 0x01, 0x00, 0x38, 0x00, 0x01, 0x00, 
};

void FequestAdapterCallback(WGPUAdapterId Received, void *pUserdata)
{
    *(WGPUAdapterId*)pUserdata = Received;
}

// ------------ CGraphicsBackend_Threaded

void CGraphicsBackend_Threaded::ThreadFunc(void *pUser)
{
	CGraphicsBackend_Threaded *pThis = (CGraphicsBackend_Threaded *)pUser;

	while(!pThis->m_Shutdown)
	{
		pThis->m_Activity.wait();
		if(pThis->m_pBuffer)
		{
			#ifdef CONF_PLATFORM_MACOSX
				CAutoreleasePool AutoreleasePool;
			#endif
			pThis->m_pProcessor->RunBuffer(pThis->m_pBuffer);
			sync_barrier();
			pThis->m_pBuffer = 0x0;
			pThis->m_BufferDone.signal();
		}
	}
}

CGraphicsBackend_Threaded::CGraphicsBackend_Threaded()
{
	m_pBuffer = 0x0;
	m_pProcessor = 0x0;
	m_pThread = 0x0;
}

void CGraphicsBackend_Threaded::StartProcessor(ICommandProcessor *pProcessor)
{
	m_Shutdown = false;
	m_pProcessor = pProcessor;
	m_pThread = thread_init(ThreadFunc, this);
	m_BufferDone.signal();
}

void CGraphicsBackend_Threaded::StopProcessor()
{
	m_Shutdown = true;
	m_Activity.signal();
	thread_wait(m_pThread);
	thread_destroy(m_pThread);
}

void CGraphicsBackend_Threaded::RunBuffer(CCommandBuffer *pBuffer)
{
	WaitForIdle();
	m_pBuffer = pBuffer;
	m_Activity.signal();
}

bool CGraphicsBackend_Threaded::IsIdle() const
{
	return m_pBuffer == 0x0;
}

void CGraphicsBackend_Threaded::WaitForIdle()
{
	while(m_pBuffer != 0x0)
		m_BufferDone.wait();
}


// ------------ CCommandProcessorFragment_General

void CCommandProcessorFragment_General::Cmd_Signal(const CCommandBuffer::CSignalCommand *pCommand)
{
	pCommand->m_pSemaphore->signal();
}

bool CCommandProcessorFragment_General::RunCommand(const CCommandBuffer::CCommand * pBaseCommand)
{
	switch(pBaseCommand->m_Cmd)
	{
	case CCommandBuffer::CMD_NOP: break;
	case CCommandBuffer::CMD_SIGNAL: Cmd_Signal(static_cast<const CCommandBuffer::CSignalCommand *>(pBaseCommand)); break;
	default: return false;
	}

	return true;
}

// ------------ CCommandProcessorFragment_WGPU

WGPUTextureFormat CCommandProcessorFragment_WGPU::TexFormatToWGPUFormat(int TexFormat)
{
	//if(TexFormat == CCommandBuffer::TEXFORMAT_RGB) return GL_RGB;
	if(TexFormat == CCommandBuffer::TEXFORMAT_ALPHA) return WGPUTextureFormat_R8Unorm;
	if(TexFormat == CCommandBuffer::TEXFORMAT_RGBA) return WGPUTextureFormat_Rgba8Unorm;
	return WGPUTextureFormat_Rgba8Unorm;
}

unsigned char CCommandProcessorFragment_WGPU::Sample(int w, int h, const unsigned char *pData, int u, int v, int Offset, int ScaleW, int ScaleH, int Bpp)
{
	int Value = 0;
	for(int x = 0; x < ScaleW; x++)
		for(int y = 0; y < ScaleH; y++)
			Value += pData[((v+y)*w+(u+x))*Bpp+Offset];
	return Value/(ScaleW*ScaleH);
}

void *CCommandProcessorFragment_WGPU::Rescale(int Width, int Height, int NewWidth, int NewHeight, int Format, const unsigned char *pData)
{
	unsigned char *pTmpData;
	int ScaleW = Width/NewWidth;
	int ScaleH = Height/NewHeight;

	int Bpp = 3;
	if(Format == CCommandBuffer::TEXFORMAT_RGBA)
		Bpp = 4;

	pTmpData = (unsigned char *)mem_alloc(NewWidth*NewHeight*Bpp, 1);

	int c = 0;
	for(int y = 0; y < NewHeight; y++)
		for(int x = 0; x < NewWidth; x++, c++)
		{
			pTmpData[c*Bpp] = Sample(Width, Height, pData, x*ScaleW, y*ScaleH, 0, ScaleW, ScaleH, Bpp);
			pTmpData[c*Bpp+1] = Sample(Width, Height, pData, x*ScaleW, y*ScaleH, 1, ScaleW, ScaleH, Bpp);
			pTmpData[c*Bpp+2] = Sample(Width, Height, pData, x*ScaleW, y*ScaleH, 2, ScaleW, ScaleH, Bpp);
			if(Bpp == 4)
				pTmpData[c*Bpp+3] = Sample(Width, Height, pData, x*ScaleW, y*ScaleH, 3, ScaleW, ScaleH, Bpp);
		}

	return pTmpData;
}

void CCommandProcessorFragment_WGPU::SetState(const CCommandBuffer::CState &State)
{
	/*
	// clip
	if(State.m_ClipEnable)
	{
		glScissor(State.m_ClipX, State.m_ClipY, State.m_ClipW, State.m_ClipH);
		glEnable(GL_SCISSOR_TEST);
	}
	else
		glDisable(GL_SCISSOR_TEST);


	// texture
	int SrcBlendMode = GL_ONE;
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_TEXTURE_3D);
	if(State.m_Texture >= 0 && State.m_Texture < CCommandBuffer::MAX_TEXTURES)
	{
		if(State.m_Dimension == 2 && (m_aTextures[State.m_Texture].m_State&CTexture::STATE_TEX2D))
		{
			glEnable(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D, m_aTextures[State.m_Texture].m_Tex2D);
		}
		else if(State.m_Dimension == 3 && (m_aTextures[State.m_Texture].m_State&CTexture::STATE_TEX3D))
		{
			glEnable(GL_TEXTURE_3D);
			glBindTexture(GL_TEXTURE_3D, m_aTextures[State.m_Texture].m_Tex3D);
		}
		else
			dbg_msg("render", "invalid texture %d %d %d\n", State.m_Texture, State.m_Dimension, m_aTextures[State.m_Texture].m_State);

		if(m_aTextures[State.m_Texture].m_Format == CCommandBuffer::TEXFORMAT_RGBA)
			SrcBlendMode = GL_ONE;
		else
			SrcBlendMode = GL_SRC_ALPHA;
	}

	// blend
	switch(State.m_BlendMode)
	{
	case CCommandBuffer::BLEND_NONE:
		glDisable(GL_BLEND);
		break;
	case CCommandBuffer::BLEND_ALPHA:
		glEnable(GL_BLEND);
		glBlendFunc(SrcBlendMode, GL_ONE_MINUS_SRC_ALPHA);
		break;
	case CCommandBuffer::BLEND_ADDITIVE:
		glEnable(GL_BLEND);
		glBlendFunc(SrcBlendMode, GL_ONE);
		break;
	default:
		dbg_msg("render", "unknown blendmode %d\n", State.m_BlendMode);
	};

	// wrap mode
	switch(State.m_WrapModeU)
	{
	case IGraphics::WRAP_REPEAT:
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		break;
	case IGraphics::WRAP_CLAMP:
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		break;
	default:
		dbg_msg("render", "unknown wrapmode %d\n", State.m_WrapModeU);
	};

	switch(State.m_WrapModeV)
	{
	case IGraphics::WRAP_REPEAT:
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		break;
	case IGraphics::WRAP_CLAMP:
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		break;
	default:
		dbg_msg("render", "unknown wrapmode %d\n", State.m_WrapModeV);
	};

	if(State.m_Texture >= 0 && State.m_Texture < CCommandBuffer::MAX_TEXTURES && State.m_Dimension == 3)
	{
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);

	}

	// screen mapping
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(State.m_ScreenTL.x, State.m_ScreenBR.x, State.m_ScreenBR.y, State.m_ScreenTL.y, -1.0f, 1.0f);
	*/
}

void CCommandProcessorFragment_WGPU::Cmd_Init(const CInitCommand *pCommand)
{
	// set some default settings
	/*glEnable(GL_BLEND);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glAlphaFunc(GL_GREATER, 0);
	glEnable(GL_ALPHA_TEST);
	glDepthMask(0);*/

	m_Device = pCommand->m_Device;
	m_SwapChain = pCommand->m_SwapChain;
	m_pTextureMemoryUsage = pCommand->m_pTextureMemoryUsage;
	*m_pTextureMemoryUsage = 0;

	WGPUShaderModuleId VertexShader = wgpu_device_create_shader_module(m_Device,
        &WGPUShaderModuleDescriptor {
            .code = WGPUU32Array {
				.bytes = (uint32_t*) s_aVert,
				.length = sizeof(s_aVert) / 4,
			},
        });
	
	dbg_msg("wgpu", "created vertex shader");

    WGPUShaderModuleId FragmentShader = wgpu_device_create_shader_module(m_Device,
        &WGPUShaderModuleDescriptor {
            .code = WGPUU32Array {
                .bytes = (uint32_t*) s_aFrag,
                .length = sizeof(s_aFrag) / 4,
            },
        });
	
	dbg_msg("wgpu", "created fragment shader");

	WGPUBindGroupLayoutEntry aEntries2D[2] = {
		{
			.binding = 0,
			.visibility = WGPUShaderStage_FRAGMENT,
			.ty = WGPUBindingType_SampledTexture,
			.multisampled = false,
			.view_dimension = WGPUTextureViewDimension_D2,
			.texture_component_type = WGPUTextureComponentType_Float
		},
		{
			.binding = 1,
			.visibility = WGPUShaderStage_FRAGMENT,
			.ty = WGPUBindingType_Sampler,
		}
	};

	WGPUBindGroupLayoutEntry aEntries2DArray[2] = {
		{
			.binding = 0,
			.visibility = WGPUShaderStage_FRAGMENT,
			.ty = WGPUBindingType_SampledTexture,
			.multisampled = false,
			.view_dimension = WGPUTextureViewDimension_D2Array,
			.texture_component_type = WGPUTextureComponentType_Float
		},
		{
			.binding = 1,
			.visibility = WGPUShaderStage_FRAGMENT,
			.ty = WGPUBindingType_Sampler,
		}
	};

    m_BindGroup2DLayout = wgpu_device_create_bind_group_layout(m_Device,
        &WGPUBindGroupLayoutDescriptor {
            .label = "bind group 2D layout",
            .entries = aEntries2D,
            .entries_length = 2,
        });
	
	m_BindGroup2DArrayLayout = wgpu_device_create_bind_group_layout(m_Device,
        &WGPUBindGroupLayoutDescriptor {
            .label = "bind group 2D array layout",
            .entries = aEntries2DArray,
            .entries_length = 2,
        });

	dbg_msg("wgpu", "created bind group layout");

    WGPUPipelineLayoutId PipelineLayout = wgpu_device_create_pipeline_layout(m_Device,
        &WGPUPipelineLayoutDescriptor {
            .bind_group_layouts = NULL,
            .bind_group_layouts_length = 0,
        });

	WGPUBindGroupLayoutId aBindGroup2DLayouts[1] = {m_BindGroup2DLayout};
	WGPUPipelineLayoutId Pipeline2DLayout = wgpu_device_create_pipeline_layout(m_Device,
        &WGPUPipelineLayoutDescriptor {
            .bind_group_layouts = aBindGroup2DLayouts,
            .bind_group_layouts_length = 1,
        });

	WGPUBindGroupLayoutId aBindGroup2DArrayLayouts[1] = {m_BindGroup2DArrayLayout};
	WGPUPipelineLayoutId Pipeline2DArrayLayout = wgpu_device_create_pipeline_layout(m_Device,
        &WGPUPipelineLayoutDescriptor {
            .bind_group_layouts = {aBindGroup2DArrayLayouts},
            .bind_group_layouts_length = 1,
        });
	
	dbg_msg("wgpu", "created pipeline layout");

    m_RenderPipeline = wgpu_device_create_render_pipeline(m_Device,
        &WGPURenderPipelineDescriptor {
            .layout = PipelineLayout,
            .vertex_stage =
                WGPUProgrammableStageDescriptor {
                    .module = VertexShader,
                    .entry_point = "main",
                },
            .fragment_stage =
                &WGPUProgrammableStageDescriptor {
                    .module = FragmentShader,
                    .entry_point = "main",
                },
            .primitive_topology = WGPUPrimitiveTopology_TriangleList,
            .rasterization_state =
                &WGPURasterizationStateDescriptor {
                    .front_face = WGPUFrontFace_Ccw,
                    .cull_mode = WGPUCullMode_None,
                    .depth_bias = 0,
                    .depth_bias_slope_scale = 0.0,
                    .depth_bias_clamp = 0.0,
                },
            .color_states =
                &WGPUColorStateDescriptor {
                    .format = WGPUTextureFormat_Bgra8Unorm,
                    .alpha_blend =
                        WGPUBlendDescriptor {
                            .src_factor = WGPUBlendFactor_One,
                            .dst_factor = WGPUBlendFactor_Zero,
                            .operation = WGPUBlendOperation_Add,
                        },
                    .color_blend =
                        WGPUBlendDescriptor {
                            .src_factor = WGPUBlendFactor_One,
                            .dst_factor = WGPUBlendFactor_Zero,
                            .operation = WGPUBlendOperation_Add,
                        },
                    .write_mask = WGPUColorWrite_ALL,
                },
            .color_states_length = 1,
            .depth_stencil_state = NULL,
            .vertex_state =
                WGPUVertexStateDescriptor {
                    .index_format = WGPUIndexFormat_Uint16,
                    .vertex_buffers = NULL,
                    .vertex_buffers_length = 0,
                },
            .sample_count = 1,
        });

    m_Render2DPipeline = wgpu_device_create_render_pipeline(m_Device,
        &WGPURenderPipelineDescriptor {
            .layout = Pipeline2DLayout,
            .vertex_stage =
                WGPUProgrammableStageDescriptor {
                    .module = VertexShader,
                    .entry_point = "main",
                },
            .fragment_stage =
                &WGPUProgrammableStageDescriptor {
                    .module = FragmentShader,
                    .entry_point = "main",
                },
            .primitive_topology = WGPUPrimitiveTopology_TriangleList,
            .rasterization_state =
                &WGPURasterizationStateDescriptor {
                    .front_face = WGPUFrontFace_Ccw,
                    .cull_mode = WGPUCullMode_None,
                    .depth_bias = 0,
                    .depth_bias_slope_scale = 0.0,
                    .depth_bias_clamp = 0.0,
                },
            .color_states =
                &WGPUColorStateDescriptor {
                    .format = WGPUTextureFormat_Bgra8Unorm,
                    .alpha_blend =
                        WGPUBlendDescriptor {
                            .src_factor = WGPUBlendFactor_One,
                            .dst_factor = WGPUBlendFactor_Zero,
                            .operation = WGPUBlendOperation_Add,
                        },
                    .color_blend =
                        WGPUBlendDescriptor {
                            .src_factor = WGPUBlendFactor_One,
                            .dst_factor = WGPUBlendFactor_Zero,
                            .operation = WGPUBlendOperation_Add,
                        },
                    .write_mask = WGPUColorWrite_ALL,
                },
            .color_states_length = 1,
            .depth_stencil_state = NULL,
            .vertex_state =
                WGPUVertexStateDescriptor {
                    .index_format = WGPUIndexFormat_Uint16,
                    .vertex_buffers = NULL,
                    .vertex_buffers_length = 0,
                },
            .sample_count = 1,
        });

	m_Render2DArrayPipeline = wgpu_device_create_render_pipeline(m_Device,
        &WGPURenderPipelineDescriptor {
            .layout = Pipeline2DArrayLayout,
            .vertex_stage =
                WGPUProgrammableStageDescriptor {
                    .module = VertexShader,
                    .entry_point = "main",
                },
            .fragment_stage =
                &WGPUProgrammableStageDescriptor {
                    .module = FragmentShader,
                    .entry_point = "main",
                },
            .primitive_topology = WGPUPrimitiveTopology_TriangleList,
            .rasterization_state =
                &WGPURasterizationStateDescriptor {
                    .front_face = WGPUFrontFace_Ccw,
                    .cull_mode = WGPUCullMode_None,
                    .depth_bias = 0,
                    .depth_bias_slope_scale = 0.0,
                    .depth_bias_clamp = 0.0,
                },
            .color_states =
                &WGPUColorStateDescriptor {
                    .format = WGPUTextureFormat_Bgra8Unorm,
                    .alpha_blend =
                        WGPUBlendDescriptor {
                            .src_factor = WGPUBlendFactor_One,
                            .dst_factor = WGPUBlendFactor_Zero,
                            .operation = WGPUBlendOperation_Add,
                        },
                    .color_blend =
                        WGPUBlendDescriptor {
                            .src_factor = WGPUBlendFactor_One,
                            .dst_factor = WGPUBlendFactor_Zero,
                            .operation = WGPUBlendOperation_Add,
                        },
                    .write_mask = WGPUColorWrite_ALL,
                },
            .color_states_length = 1,
            .depth_stencil_state = NULL,
            .vertex_state =
                WGPUVertexStateDescriptor {
                    .index_format = WGPUIndexFormat_Uint16,
                    .vertex_buffers = NULL,
                    .vertex_buffers_length = 0,
                },
            .sample_count = 1,
        });
	
	dbg_msg("wgpu", "created pipeline");

	m_Sampler = wgpu_device_create_sampler(m_Device,
        &WGPUSamplerDescriptor {
            .address_mode_u = WGPUAddressMode_ClampToEdge,
			.address_mode_v = WGPUAddressMode_ClampToEdge,
			.address_mode_w = WGPUAddressMode_ClampToEdge,
			.mag_filter = WGPUFilterMode_Linear,
			.min_filter = WGPUFilterMode_Linear,
			.mipmap_filter = WGPUFilterMode_Nearest,
			.lod_min_clamp = -100.f,
			.lod_max_clamp = 100.0,
            .compare = WGPUCompareFunction_Undefined,
        });

	m_NextTexture = wgpu_swap_chain_get_next_texture(m_SwapChain);

	m_Ready = true;
	
	/*glGetIntegerv(GL_MAX_TEXTURE_SIZE, &m_MaxTexSize);
	glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &m_Max3DTexSize);
	dbg_msg("render", "opengl max texture sizes: %d, %d(3D)", m_MaxTexSize, m_Max3DTexSize);*/
}

void CCommandProcessorFragment_WGPU::Cmd_Texture_Update(const CCommandBuffer::CTextureUpdateCommand *pCommand)
{
	/*if(m_aTextures[pCommand->m_Slot].m_State&CTexture::STATE_TEX2D)
	{
		glBindTexture(GL_TEXTURE_2D, m_aTextures[pCommand->m_Slot].m_Tex2D);
		glTexSubImage2D(GL_TEXTURE_2D, 0, pCommand->m_X, pCommand->m_Y, pCommand->m_Width, pCommand->m_Height,
			TexFormatToWGPUFormat(pCommand->m_Format), GL_UNSIGNED_BYTE, pCommand->m_pData);
	}
	mem_free(pCommand->m_pData);*/
}

void CCommandProcessorFragment_WGPU::Cmd_Texture_Destroy(const CCommandBuffer::CTextureDestroyCommand *pCommand)
{
	if(m_aTextures[pCommand->m_Slot].m_State&CTexture::STATE_TEX2D)
		wgpu_bind_group_destroy(m_aTextures[pCommand->m_Slot].m_Tex2D);
	if(m_aTextures[pCommand->m_Slot].m_State&CTexture::STATE_TEX3D)
		wgpu_bind_group_destroy(m_aTextures[pCommand->m_Slot].m_Tex3D);
	*m_pTextureMemoryUsage -= m_aTextures[pCommand->m_Slot].m_MemSize;
	m_aTextures[pCommand->m_Slot].m_State = CTexture::STATE_EMPTY;
	m_aTextures[pCommand->m_Slot].m_MemSize = 0;
}

void CCommandProcessorFragment_WGPU::Cmd_Texture_Create(const CCommandBuffer::CTextureCreateCommand *pCommand)
{
	int Width = pCommand->m_Width;
	int Height = pCommand->m_Height;
	int Depth = 1;
	void *pTexData = pCommand->m_pData;

	// resample if needed
	/*if(pCommand->m_Format == CCommandBuffer::TEXFORMAT_RGBA || pCommand->m_Format == CCommandBuffer::TEXFORMAT_RGB)
	{
		int MaxTexSize = m_MaxTexSize;
		if(pCommand->m_Flags&CCommandBuffer::TEXFLAG_TEXTURE3D)
		{
			if(pCommand->m_Flags&CCommandBuffer::TEXFLAG_TEXTURE2D)
				MaxTexSize = min(MaxTexSize, m_Max3DTexSize * IGraphics::NUMTILES_DIMENSION);
			else
				MaxTexSize = m_Max3DTexSize * IGraphics::NUMTILES_DIMENSION;
		}
		if(Width > MaxTexSize || Height > MaxTexSize)
		{
			do
			{
				Width>>=1;
				Height>>=1;
			}
			while(Width > MaxTexSize || Height > MaxTexSize);

			void *pTmpData = Rescale(pCommand->m_Width, pCommand->m_Height, Width, Height, pCommand->m_Format, static_cast<const unsigned char *>(pCommand->m_pData));
			mem_free(pTexData);
			pTexData = pTmpData;
		}
		else if(Width > IGraphics::NUMTILES_DIMENSION && Height > IGraphics::NUMTILES_DIMENSION && (pCommand->m_Flags&CCommandBuffer::TEXFLAG_QUALITY) == 0)
		{
			Width>>=1;
			Height>>=1;

			void *pTmpData = Rescale(pCommand->m_Width, pCommand->m_Height, Width, Height, pCommand->m_Format, static_cast<const unsigned char *>(pCommand->m_pData));
			mem_free(pTexData);
			pTexData = pTmpData;
		}
	}*/

	// use premultiplied alpha for rgba textures
	if(pCommand->m_Format == CCommandBuffer::TEXFORMAT_RGBA)
	{
		unsigned char *pTexels = (unsigned char *)pTexData;
		for(int i = 0; i < Width * Height; ++i)
		{
			const float a = (pTexels[i*4+3]/255.0f);
			pTexels[i*4+0] = (unsigned char)(pTexels[i*4+0] * a);
			pTexels[i*4+1] = (unsigned char)(pTexels[i*4+1] * a);
			pTexels[i*4+2] = (unsigned char)(pTexels[i*4+2] * a);
		}
	}
	m_aTextures[pCommand->m_Slot].m_Format = pCommand->m_Format;

	//
	WGPUTextureFormat WGPUFormat = TexFormatToWGPUFormat(pCommand->m_Format);

	// 2D texture
	if(pCommand->m_Flags&CCommandBuffer::TEXFLAG_TEXTURE2D)
	{
		//bool Mipmaps = !(pCommand->m_Flags&CCommandBuffer::TEXFLAG_NOMIPMAPS);
		WGPUTextureId Tex = wgpu_device_create_texture(m_Device, 
			&WGPUTextureDescriptor {
				.size = {
					.width = (unsigned)Width,
					.height = (unsigned)Height,
					.depth = (unsigned)Depth,
				},
				.array_layer_count = 1,
				.mip_level_count = 1,
				.sample_count = 1,
				.dimension = WGPUTextureDimension_D2,
				.format = WGPUFormat,
				.usage = WGPUTextureUsage_SAMPLED | WGPUTextureUsage_COPY_DST,
			});
		
		WGPUTextureViewId TexView = wgpu_texture_create_view(Tex, NULL);

		WGPUBindGroupEntry aEntries[2] = {
			{
				.binding = 0,
				.resource = {
					.tag = WGPUBindingResource_TextureView,
					.texture_view = TexView,
				},
			},
			{
				.binding = 1,
				.resource = {
					.tag = WGPUBindingResource_Sampler,
					.sampler = m_Sampler,
				},
			}
		};
		m_aTextures[pCommand->m_Slot].m_Tex2D = wgpu_device_create_bind_group(m_Device,
			&WGPUBindGroupDescriptor {
				.layout = m_BindGroup2DLayout,
				.entries = aEntries,
				.entries_length = 2,
			});
		m_aTextures[pCommand->m_Slot].m_State |= CTexture::STATE_TEX2D;

		/*if(!Mipmaps)
		{
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexImage2D(GL_TEXTURE_2D, 0, StoreWGPUFormat, Width, Height, 0, WGPUFormat, GL_UNSIGNED_BYTE, pTexData);
		}
		else
		{
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			if(pCommand->m_Flags&CCommandBuffer::TEXTFLAG_LINEARMIPMAPS)
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			else
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
			glTexImage2D(GL_TEXTURE_2D, 0, StoreWGPUFormat, Width, Height, 0, WGPUFormat, GL_UNSIGNED_BYTE, pTexData);
		}

		// calculate memory usage
		m_aTextures[pCommand->m_Slot].m_MemSize = Width*Height*pCommand->m_PixelSize;
		if(Mipmaps)
		{
			int TexWidth = Width;
			int TexHeight = Height;
			while(TexWidth > 2 && TexHeight > 2)
			{
				TexWidth>>=1;
				TexHeight>>=1;
				m_aTextures[pCommand->m_Slot].m_MemSize += TexWidth*TexHeight*pCommand->m_PixelSize;
			}
		}*/
	}

	// 3D texture
	/*if((pCommand->m_Flags&CCommandBuffer::TEXFLAG_TEXTURE3D) && m_Max3DTexSize >= CTexture::MIN_GL_MAX_3D_TEXTURE_SIZE)
	{
		Width /= IGraphics::NUMTILES_DIMENSION;
		Height /= IGraphics::NUMTILES_DIMENSION;
		Depth = min(m_Max3DTexSize, IGraphics::NUMTILES_DIMENSION * IGraphics::NUMTILES_DIMENSION);

		// copy and reorder texture data
		int MemSize = Width*Height*IGraphics::NUMTILES_DIMENSION*IGraphics::NUMTILES_DIMENSION*pCommand->m_PixelSize;
		char *pTmpData = (char *)mem_alloc(MemSize, sizeof(void*));

		const int TileSize = (Height * Width) * pCommand->m_PixelSize;
		const int TileRowSize = Width * pCommand->m_PixelSize;
		const int ImagePitch = Width * IGraphics::NUMTILES_DIMENSION * pCommand->m_PixelSize;
		mem_zero(pTmpData, MemSize);
		for(int i = 0; i < IGraphics::NUMTILES_DIMENSION * IGraphics::NUMTILES_DIMENSION; i++)
		{
			const int px = (i%IGraphics::NUMTILES_DIMENSION) * Width;
			const int py = (i/IGraphics::NUMTILES_DIMENSION) * Height;
			const char *pTileData = (const char *)pTexData + (py * Width * IGraphics::NUMTILES_DIMENSION + px) * pCommand->m_PixelSize;
			for(int y = 0; y < Height; y++)
				mem_copy(pTmpData + i*TileSize + y*TileRowSize, pTileData + y * ImagePitch, TileRowSize);
		}

		mem_free(pTexData);

		//
		glGenTextures(1, m_aTextures[pCommand->m_Slot].m_Tex3D);
		m_aTextures[pCommand->m_Slot].m_State |= CTexture::STATE_TEX3D;

		glBindTexture(GL_TEXTURE_3D, m_aTextures[pCommand->m_Slot].m_Tex3D);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		pTexData = pTmpData;
		glTexImage3D(GL_TEXTURE_3D, 0, StoreWGPUFormat, Width, Height, Depth, 0, WGPUFormat, GL_UNSIGNED_BYTE, pTexData);

		m_aTextures[pCommand->m_Slot].m_MemSize += Width*Height*pCommand->m_PixelSize;

		pTexData = pTmpData;
	}*/

	*m_pTextureMemoryUsage += m_aTextures[pCommand->m_Slot].m_MemSize;

	mem_free(pTexData);
}

void CCommandProcessorFragment_WGPU::Cmd_Clear(const CCommandBuffer::CClearCommand *pCommand)
{
	//glClearColor(pCommand->m_Color.r, pCommand->m_Color.g, pCommand->m_Color.b, 0.0f);
	//glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void CCommandProcessorFragment_WGPU::Cmd_Render(const CCommandBuffer::CRenderCommand *pCommand)
{
	/*SetState(pCommand->m_State);

	glVertexPointer(2, GL_FLOAT, sizeof(CCommandBuffer::CVertex), (char*)pCommand->m_pVertices);
	glTexCoordPointer(3, GL_FLOAT, sizeof(CCommandBuffer::CVertex), (char*)pCommand->m_pVertices + sizeof(float)*2);
	glColorPointer(4, GL_FLOAT, sizeof(CCommandBuffer::CVertex), (char*)pCommand->m_pVertices + sizeof(float)*5);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);

	switch(pCommand->m_PrimType)
	{
	case CCommandBuffer::PRIMTYPE_QUADS:
		glDrawArrays(GL_QUADS, 0, pCommand->m_PrimCount*4);
		break;
	case CCommandBuffer::PRIMTYPE_LINES:
		glDrawArrays(GL_LINES, 0, pCommand->m_PrimCount*2);
		break;
	default:
		dbg_msg("render", "unknown primtype %d\n", pCommand->m_Cmd);
	};*/
}

void CCommandProcessorFragment_WGPU::Cmd_Swap(const CCommandBuffer::CSwapCommand *pCommand)
{
	/*SDL_GL_SwapWindow(m_pWindow);

	if(pCommand->m_Finish)
		glFinish();*/

	SubmitCommandBuffer();

	wgpu_swap_chain_present(m_SwapChain);
	//dbg_msg("wgpu", "presented swap chain");

	m_NextTexture = wgpu_swap_chain_get_next_texture(m_SwapChain);

	InitCommandBuffer();
}

void CCommandProcessorFragment_WGPU::Cmd_Screenshot(const CCommandBuffer::CScreenshotCommand *pCommand)
{
	/*
	// fetch image data
	GLint aViewport[4] = {0,0,0,0};
	glGetIntegerv(GL_VIEWPORT, aViewport);

	int w = pCommand->m_W == -1 ? aViewport[2] : pCommand->m_W;
	int h = pCommand->m_H == -1 ? aViewport[3] : pCommand->m_H;
	int x = pCommand->m_X;
	int y = aViewport[3] - pCommand->m_Y - 1 - (h - 1);

	// we allocate one more row to use when we are flipping the texture
	unsigned char *pPixelData = (unsigned char *)mem_alloc(w*(h+1)*3, 1);
	unsigned char *pTempRow = pPixelData+w*h*3;

	// fetch the pixels
	GLint Alignment;
	glGetIntegerv(GL_PACK_ALIGNMENT, &Alignment);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(x, y, w, h, GL_RGB, GL_UNSIGNED_BYTE, pPixelData);
	glPixelStorei(GL_PACK_ALIGNMENT, Alignment);

	// flip the pixel because WGPU works from bottom left corner
	for(int ty = 0; ty < h/2; ty++)
	{
		mem_copy(pTempRow, pPixelData+ty*w*3, w*3);
		mem_copy(pPixelData+ty*w*3, pPixelData+(h-ty-1)*w*3, w*3);
		mem_copy(pPixelData+(h-ty-1)*w*3, pTempRow,w*3);
	}

	// fill in the information
	pCommand->m_pImage->m_Width = w;
	pCommand->m_pImage->m_Height = h;
	pCommand->m_pImage->m_Format = CImageInfo::FORMAT_RGB;
	pCommand->m_pImage->m_pData = pPixelData;*/
}

CCommandProcessorFragment_WGPU::CCommandProcessorFragment_WGPU()
{
	mem_zero(m_aTextures, sizeof(m_aTextures));
	m_pTextureMemoryUsage = 0;
	m_RecordingBuffer = false;
	m_Ready = false;
}

bool CCommandProcessorFragment_WGPU::RunCommand(const CCommandBuffer::CCommand * pBaseCommand)
{
	switch(pBaseCommand->m_Cmd)
	{
	case CMD_INIT: Cmd_Init(static_cast<const CInitCommand *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_TEXTURE_CREATE: Cmd_Texture_Create(static_cast<const CCommandBuffer::CTextureCreateCommand *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_TEXTURE_DESTROY: Cmd_Texture_Destroy(static_cast<const CCommandBuffer::CTextureDestroyCommand *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_TEXTURE_UPDATE: Cmd_Texture_Update(static_cast<const CCommandBuffer::CTextureUpdateCommand *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_CLEAR: Cmd_Clear(static_cast<const CCommandBuffer::CClearCommand *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_RENDER: Cmd_Render(static_cast<const CCommandBuffer::CRenderCommand *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_SWAP: Cmd_Swap(static_cast<const CCommandBuffer::CSwapCommand *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_SCREENSHOT: Cmd_Screenshot(static_cast<const CCommandBuffer::CScreenshotCommand *>(pBaseCommand)); break;
	default: return false;
	}

	return true;
}

void CCommandProcessorFragment_WGPU::InitCommandBuffer()
{
	if(!m_Ready || !m_NextTexture.view_id)
		return;

    m_CmdEncoder = wgpu_device_create_command_encoder(
        m_Device, &WGPUCommandEncoderDescriptor {.label = "command encoder"});
	
	//dbg_msg("wgpu", "created command encoder");

    WGPURenderPassColorAttachmentDescriptor aColorAttachments[1] = {
            {
                .attachment = m_NextTexture.view_id,
                .load_op = WGPULoadOp_Clear,
                .store_op = WGPUStoreOp_Store,
                .clear_color = WGPUColor { .r = 0.0, .g = 0.0, .b = 1.0, .a = 1.0 },
            },
        };

    m_RPass = wgpu_command_encoder_begin_render_pass(m_CmdEncoder,
        &WGPURenderPassDescriptor {
            .color_attachments = aColorAttachments,
            .color_attachments_length = 1,
            .depth_stencil_attachment = NULL,
        });
	
	//dbg_msg("wgpu", "began render pass");

	m_RecordingBuffer = true;

    wgpu_render_pass_set_pipeline(m_RPass, m_RenderPipeline);
    //wgpu_render_pass_set_bind_group(RPass, 0, BindGroup, NULL, 0);
    wgpu_render_pass_draw(m_RPass, 3, 1, 0, 0);
    wgpu_render_pass_end_pass(m_RPass);
	//dbg_msg("wgpu", "render pass ended");
}

void CCommandProcessorFragment_WGPU::SubmitCommandBuffer()
{
	if(m_RecordingBuffer)
	{
		WGPUCommandBufferId CmdBuf =  wgpu_command_encoder_finish(m_CmdEncoder, NULL);
		//dbg_msg("wgpu", "finished command buffer");
		WGPUQueueId Queue = wgpu_device_get_default_queue(m_Device);
		wgpu_queue_submit(Queue, &CmdBuf, 1);
		//dbg_msg("wgpu", "submitted command buffer");
		m_RecordingBuffer = false;
	}
}


// ------------ CCommandProcessor_SDL_WGPU

void CCommandProcessor_SDL_WGPU::RunBuffer(CCommandBuffer *pBuffer)
{
	unsigned CmdIndex = 0;
	m_WGPU.InitCommandBuffer();
	while(1)
	{
		const CCommandBuffer::CCommand *pBaseCommand = pBuffer->GetCommand(&CmdIndex);
		if(pBaseCommand == 0x0)
			break;

		if(m_WGPU.RunCommand(pBaseCommand))
			continue;

		if(m_General.RunCommand(pBaseCommand))
			continue;

		dbg_msg("graphics", "unknown command %d", pBaseCommand->m_Cmd);
	}
	m_WGPU.SubmitCommandBuffer();
}

// ------------ CGraphicsBackend_SDL_WGPU

int CGraphicsBackend_SDL_WGPU::Init(const char *pName, int *pScreen, int *pWindowWidth, int *pWindowHeight, int* pScreenWidth, int* pScreenHeight, int FsaaSamples, int Flags, int *pDesktopWidth, int *pDesktopHeight)
{
	if(!SDL_WasInit(SDL_INIT_VIDEO))
	{
		if(SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
		{
			dbg_msg("gfx", "unable to init SDL video: %s", SDL_GetError());
			return -1;
		}
	}

	// set screen
	SDL_Rect ScreenPos;
	m_NumScreens = SDL_GetNumVideoDisplays();
	if(m_NumScreens > 0)
	{
		*pScreen = clamp(*pScreen, 0, m_NumScreens-1);
		if(SDL_GetDisplayBounds(*pScreen, &ScreenPos) != 0)
		{
			dbg_msg("gfx", "unable to retrieve screen information: %s", SDL_GetError());
			return -1;
		}
	}
	else
	{
		dbg_msg("gfx", "unable to retrieve number of screens: %s", SDL_GetError());
		return -1;
	}

	// store desktop resolution for settings reset button
	if(!GetDesktopResolution(*pScreen, pDesktopWidth, pDesktopHeight))
	{
		dbg_msg("gfx", "unable to get desktop resolution: %s", SDL_GetError());
		return -1;
	}

	// use desktop resolution as default resolution
	if (*pWindowWidth == 0 || *pWindowHeight == 0)
	{
		*pWindowWidth = *pDesktopWidth;
		*pWindowHeight = *pDesktopHeight;
	}

	// set flags
	int SdlFlags = 0;
	if(Flags&IGraphicsBackend::INITFLAG_HIGHDPI)
		SdlFlags |= SDL_WINDOW_ALLOW_HIGHDPI;
	if(Flags&IGraphicsBackend::INITFLAG_RESIZABLE)
		SdlFlags |= SDL_WINDOW_RESIZABLE;
	if(Flags&IGraphicsBackend::INITFLAG_BORDERLESS)
		SdlFlags |= SDL_WINDOW_BORDERLESS;
	if(Flags&IGraphicsBackend::INITFLAG_FULLSCREEN)
#if defined(CONF_PLATFORM_MACOSX)	// Todo SDL: remove this when fixed (game freezes when losing focus in fullscreen)
	{
		SdlFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;	// always use "fake" fullscreen
		*pWindowWidth = *pDesktopWidth;
		*pWindowHeight = *pDesktopHeight;
	}
#else
		SdlFlags |= SDL_WINDOW_FULLSCREEN;
#endif

	if(Flags&IGraphicsBackend::INITFLAG_X11XRANDR)
		SDL_SetHint(SDL_HINT_VIDEO_X11_XRANDR, "1");

	/*// set gl attributes
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	if(FsaaSamples)
	{
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, FsaaSamples);
	}
	else
	{
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
	}*/

	// calculate centered position in windowed mode
	int OffsetX = 0;
	int OffsetY = 0;
	if(!(Flags&IGraphicsBackend::INITFLAG_FULLSCREEN) && *pDesktopWidth > *pWindowWidth && *pDesktopHeight > *pWindowHeight)
	{
		OffsetX = (*pDesktopWidth - *pWindowWidth) / 2;
		OffsetY = (*pDesktopHeight - *pWindowHeight) / 2;
	}

	// create window
	m_pWindow = SDL_CreateWindow(pName, ScreenPos.x+OffsetX, ScreenPos.y+OffsetY, *pWindowWidth, *pWindowHeight, SdlFlags);
	if(m_pWindow == NULL)
	{
		dbg_msg("gfx", "unable to create window: %s", SDL_GetError());
		return -1;
	}

	SDL_GetWindowSize(m_pWindow, pWindowWidth, pWindowHeight);

	SDL_SysWMinfo WmInfo;
	SDL_VERSION(&WmInfo.version);
	SDL_GetWindowWMInfo(m_pWindow, &WmInfo);
	HWND hwnd = WmInfo.info.win.window;

	WGPUSurfaceId Surface;

	SDL_GL_GetDrawableSize(m_pWindow, pScreenWidth, pScreenHeight); // drawable size may differ in high dpi mode

	#if defined(CONF_FAMILY_WINDOWS)
	Surface = wgpu_create_surface_from_windows_hwnd(WmInfo.info.win.hinstance, WmInfo.info.win.window);
	#else
		#if defined(CONF_PLATFORM_MACOSX)
		// TODO
		#else
		surface = wgpu_create_surface_from_xlib((const void **)WmInfo.info.x11.display, WmInfo.info.x11.window);
		#endif
	#endif

	//SDL_GL_GetDrawableSize(m_pWindow, pScreenWidth, pScreenHeight); // drawable size may differ in high dpi mode

	*pScreenWidth = *pWindowWidth;
	*pScreenHeight = *pWindowHeight;

	WGPUAdapterId Adapter = { 0 };
    wgpu_request_adapter_async(
        &WGPURequestAdapterOptions {
            .power_preference = WGPUPowerPreference_HighPerformance,
            .compatible_surface = Surface,
        },
        2 | 4 | 8,
        FequestAdapterCallback,
        (void *) &Adapter
    );

	dbg_msg("wgpu", "adapter requested");

    m_Device = wgpu_adapter_request_device(Adapter,
        &WGPUDeviceDescriptor {
            .extensions =
                {
                    .anisotropic_filtering = false,
                },
            .limits =
                {
                    .max_bind_groups = 1,
                },
        });

	dbg_msg("wgpu", "device requested");
	
	WGPUSwapChainId SwapChain = wgpu_device_create_swap_chain(m_Device, Surface,
        &WGPUSwapChainDescriptor {
            .usage = WGPUTextureUsage_OUTPUT_ATTACHMENT,
            .format = WGPUTextureFormat_Bgra8Unorm,
            .width = (unsigned)*pScreenWidth,
            .height = (unsigned)*pScreenHeight,
            .present_mode = WGPUPresentMode_Fifo,
        });
	
	dbg_msg("wgpu", "created swapchain");

	// print sdl version
	SDL_version Compiled;
	SDL_version Linked;

	SDL_VERSION(&Compiled);
	SDL_GetVersion(&Linked);
	dbg_msg("sdl", "SDL version %d.%d.%d (dll = %d.%d.%d)", Compiled.major, Compiled.minor, Compiled.patch, Linked.major, Linked.minor, Linked.patch);

	// start the command processor
	m_pProcessor = new CCommandProcessor_SDL_WGPU;
	StartProcessor(m_pProcessor);

	// issue init commands for WGPU and SDL
	CCommandBuffer CmdBuffer(1024, 512);
	CCommandProcessorFragment_WGPU::CInitCommand CmdWGPU;
	CmdWGPU.m_Device = m_Device;
	CmdWGPU.m_SwapChain = SwapChain;
	CmdWGPU.m_pTextureMemoryUsage = &m_TextureMemoryUsage;
	CmdBuffer.AddCommand(CmdWGPU);
	RunBuffer(&CmdBuffer);
	WaitForIdle();

	// return
	return 0;
}

int CGraphicsBackend_SDL_WGPU::Shutdown()
{
	// TODO: issue a shutdown command

	// stop and delete the processor
	StopProcessor();
	delete m_pProcessor;
	m_pProcessor = 0;

	SDL_DestroyWindow(m_pWindow);
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
	return 0;
}

int CGraphicsBackend_SDL_WGPU::MemoryUsage() const
{
	return m_TextureMemoryUsage;
}

void CGraphicsBackend_SDL_WGPU::Minimize()
{
	SDL_MinimizeWindow(m_pWindow);
}

void CGraphicsBackend_SDL_WGPU::Maximize()
{
	SDL_MaximizeWindow(m_pWindow);
}

bool CGraphicsBackend_SDL_WGPU::Fullscreen(bool State)
{
#if defined(CONF_PLATFORM_MACOSX)	// Todo SDL: remove this when fixed (game freezes when losing focus in fullscreen)
	return SDL_SetWindowFullscreen(m_pWindow, State ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0) == 0;
#else
	return SDL_SetWindowFullscreen(m_pWindow, State ? SDL_WINDOW_FULLSCREEN : 0) == 0;
#endif
}

void CGraphicsBackend_SDL_WGPU::SetWindowBordered(bool State)
{
	SDL_SetWindowBordered(m_pWindow, SDL_bool(State));
}

bool CGraphicsBackend_SDL_WGPU::SetWindowScreen(int Index)
{
	if(Index >= 0 && Index < m_NumScreens)
	{
		SDL_Rect ScreenPos;
		if(SDL_GetDisplayBounds(Index, &ScreenPos) == 0)
		{
			SDL_SetWindowPosition(m_pWindow, ScreenPos.x, ScreenPos.y);
			return true;
		}
	}

	return false;
}

int CGraphicsBackend_SDL_WGPU::GetWindowScreen()
{
	return SDL_GetWindowDisplayIndex(m_pWindow);
}

int CGraphicsBackend_SDL_WGPU::GetVideoModes(CVideoMode *pModes, int MaxModes, int Screen)
{
	int NumModes = SDL_GetNumDisplayModes(Screen);
	if(NumModes < 0)
	{
		dbg_msg("gfx", "unable to get the number of display modes: %s", SDL_GetError());
		return 0;
	}

	if(NumModes > MaxModes)
		NumModes = MaxModes;

	int ModesCount = 0;
	for(int i = 0; i < NumModes; i++)
	{
		SDL_DisplayMode Mode;
		if(SDL_GetDisplayMode(Screen, i, &Mode) < 0)
		{
			dbg_msg("gfx", "unable to get display mode: %s", SDL_GetError());
			continue;
		}

		bool AlreadyFound = false;
		for(int j = 0; j < ModesCount; j++)
		{
			if(pModes[j].m_Width == Mode.w && pModes[j].m_Height == Mode.h)
			{
				AlreadyFound = true;
				break;
			}
		}
		if(AlreadyFound)
			continue;

		pModes[ModesCount].m_Width = Mode.w;
		pModes[ModesCount].m_Height = Mode.h;
		pModes[ModesCount].m_Red = 8;
		pModes[ModesCount].m_Green = 8;
		pModes[ModesCount].m_Blue = 8;
		ModesCount++;
	}
	return ModesCount;
}

bool CGraphicsBackend_SDL_WGPU::GetDesktopResolution(int Index, int *pDesktopWidth, int* pDesktopHeight)
{
	SDL_DisplayMode DisplayMode;
	if(SDL_GetDesktopDisplayMode(Index, &DisplayMode))
		return false;

	*pDesktopWidth = DisplayMode.w;
	*pDesktopHeight = DisplayMode.h;
	return true;
}

int CGraphicsBackend_SDL_WGPU::WindowActive()
{
	return SDL_GetWindowFlags(m_pWindow)&SDL_WINDOW_INPUT_FOCUS;
}

int CGraphicsBackend_SDL_WGPU::WindowOpen()
{
	return SDL_GetWindowFlags(m_pWindow)&SDL_WINDOW_SHOWN;

}


IGraphicsBackend *CreateGraphicsBackend() { return new CGraphicsBackend_SDL_WGPU; }
