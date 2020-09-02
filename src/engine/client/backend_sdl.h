#pragma once

#include "graphics_threaded.h"

extern "C" {
	#include <wgpu.h>
}

#if defined(CONF_PLATFORM_MACOSX)
	#include <objc/objc-runtime.h>

	class semaphore
	{
		SDL_sem *sem;
	public:
		semaphore() { sem = SDL_CreateSemaphore(0); }
		~semaphore() { SDL_DestroySemaphore(sem); }
		void wait() { SDL_SemWait(sem); }
		void signal() { SDL_SemPost(sem); }
	};

	class CAutoreleasePool
	{
	private:
		id m_Pool;

	public:
		CAutoreleasePool()
		{
			Class NSAutoreleasePoolClass = (Class) objc_getClass("NSAutoreleasePool");
			m_Pool = class_createInstance(NSAutoreleasePoolClass, 0);
			SEL selector = sel_registerName("init");
			((id (*)(id, SEL))objc_msgSend)(m_Pool, selector);
		}

		~CAutoreleasePool()
		{
			SEL selector = sel_registerName("drain");
			((id (*)(id, SEL))objc_msgSend)(m_Pool, selector);
		}
	};
#endif

struct CScreen { float TL_x, TL_y, BR_x, BR_y; };
struct CMat4 { float m_Value[16]; };

// basic threaded backend, abstract, missing init and shutdown functions
class CGraphicsBackend_Threaded : public IGraphicsBackend
{
public:
	// constructed on the main thread, the rest of the functions is run on the render thread
	class ICommandProcessor
	{
	public:
		virtual ~ICommandProcessor() {}
		virtual void RunBuffer(CCommandBuffer *pBuffer) = 0;
	};

	CGraphicsBackend_Threaded();

	virtual void RunBuffer(CCommandBuffer *pBuffer);
	virtual bool IsIdle() const;
	virtual void WaitForIdle();

protected:
	void StartProcessor(ICommandProcessor *pProcessor);
	void StopProcessor();

private:
	ICommandProcessor *m_pProcessor;
	CCommandBuffer * volatile m_pBuffer;
	volatile bool m_Shutdown;
	semaphore m_Activity;
	semaphore m_BufferDone;
	void *m_pThread;

	static void ThreadFunc(void *pUser);
};

// takes care of implementation independent operations
class CCommandProcessorFragment_General
{
	void Cmd_Nop();
	void Cmd_Signal(const CCommandBuffer::CSignalCommand *pCommand);
public:
	bool RunCommand(const CCommandBuffer::CCommand * pBaseCommand);
};

// takes care of wgpu related rendering
class CCommandProcessorFragment_WGPU
{
	class CTextureData
	{
	public:
		WGPUTextureId m_Tex;
		WGPUTextureViewId m_TexView;
		WGPUBindGroupId m_BindGroups[4];
		bool m_LinearMipmaps;
	};

	class CTexture
	{
	public:
		enum
		{
			STATE_EMPTY = 0,
			STATE_TEX2D = 1,
			STATE_TEX3D = 2,
		};
		CTextureData m_Tex2D;
		CTextureData m_Tex3D;
		int m_State;
		int m_Format;
		int m_MemSize;
	};

	class CVertexBuffer
	{
	public:
		WGPUBufferId m_VertexBuffer;
		int m_MemSize;
	};

	WGPUDeviceId m_Device;
	WGPUSurfaceId m_Surface;
	WGPUSwapChainId m_SwapChain;
	WGPUPresentMode m_PresentMode;
	WGPUQueueId m_Queue;
	WGPUSwapChainOutput m_NextTexture;
	WGPUCommandEncoderId m_CmdEncoder;
	WGPURenderPassId m_RPass;
	WGPUBufferId m_StreamingBuffer;
	WGPUBufferId m_TransformBuffer;
	WGPUBindGroupId m_TransformBindGroup;
	WGPURenderPipelineId m_RenderPipeline[3];
	WGPURenderPipelineId m_Render2DPipeline[3];
	WGPURenderPipelineId m_Render2DArrayPipeline[3];
	WGPURenderPipelineId m_RenderPipelineLines[3];
	WGPURenderPipelineId m_RenderPipelineMipmap;
	WGPUBindGroupLayoutId m_BindGroupTransformLayout;
	WGPUBindGroupLayoutId m_BindGroup2DLayout;
	WGPUBindGroupLayoutId m_BindGroup2DArrayLayout;
	WGPUSamplerId m_Sampler[2][2][2];
	unsigned m_ScreenWidth;
	unsigned m_ScreenHeight;
	bool m_UpdatePresentMode;
	bool m_Ready;

	int m_ScreenCount;
	CScreen m_LastScreen;

	CTexture m_aTextures[CCommandBuffer::MAX_TEXTURES];
	CVertexBuffer m_aVertexBuffers[CCommandBuffer::MAX_VERTEX_BUFFERS];
	volatile int *m_pTextureMemoryUsage;
	volatile int *m_pVertexBufferMemoryUsage;
	int m_MaxTexSize;
	int m_Max3DTexSize;

public:
	enum
	{
		CMD_INIT = CCommandBuffer::CMDGROUP_PLATFORM_WGPU,
	};

	struct CInitCommand : public CCommandBuffer::CCommand
	{
		CInitCommand() : CCommand(CMD_INIT) {}
		WGPUSurfaceId m_Surface;
		unsigned m_ScreenWidth;
		unsigned m_ScreenHeight;
		bool m_VSync;
		volatile int *m_pTextureMemoryUsage;
		volatile int *m_pVertexBufferMemoryUsage;
	};

private:
	static unsigned char Sample(int w, int h, const unsigned char *pData, int u, int v, int Offset, int ScaleW, int ScaleH, int Bpp);
	static unsigned char *Rescale(int Width, int Height, int NewWidth, int NewHeight, int Format, const unsigned char *pData);
	static void ConvertToRGBA(int Width, int Height, int Format, unsigned char **ppData);

	void GenerateMipmapsForLayer(WGPUTextureId Tex, unsigned BaseLayer, unsigned MipLevels);

	WGPUSwapChainId CreateSwapChain(WGPUPresentMode PresentMode);
	WGPUShaderModuleId CreateShaderModule(const uint32_t *pBytes, uintptr_t Length);
	WGPURenderPipelineId CreateRenderPipeline(WGPUPipelineLayoutId PipelineLayout, WGPUShaderModuleId VertexShader, WGPUShaderModuleId FragmentShader, WGPUPrimitiveTopology PrimTopology, WGPUBlendDescriptor BlendInfo, bool Mipmap = false);
	WGPUSamplerId CreateSampler(WGPUAddressMode ModeU, WGPUAddressMode ModeV, bool LinearMipmaps);
	WGPUBindGroupId CreateTexBindGroup(WGPUTextureViewId TexView, WGPUSamplerId Sampler, bool Array = false);
	WGPURenderPassId CreateRenderPass(WGPUCommandEncoderId CmdEncoder, WGPUTextureViewId TexView, WGPULoadOp LoadOp, WGPUColor ClearColor);

	WGPUBindGroupId GetTexBindGroup(CTextureData *pTex, int WrapModeU, int WrapModeV, bool Array);
	WGPUCommandEncoderId GetCommandEncoder();
	WGPURenderPassId GetRenderPass(bool Clear = false, IGraphics::CColor ClearColor = {0.0f, 0.0f, 0.0f, 1.0f});
	void EndRenderPass();

	void SetState(const CCommandBuffer::CState &State, int PrimType, WGPURenderPassId RPass);

	void Cmd_Init(const CInitCommand *pCommand);
	void Cmd_Texture_Update(const CCommandBuffer::CTextureUpdateCommand *pCommand);
	void Cmd_Texture_Destroy(const CCommandBuffer::CTextureDestroyCommand *pCommand);
	void Cmd_Texture_Create(const CCommandBuffer::CTextureCreateCommand *pCommand);
	void Cmd_VertexBuffer_Update(const CCommandBuffer::CVertexBufferUpdateCommand *pCommand);
	void Cmd_VertexBuffer_Destroy(const CCommandBuffer::CVertexBufferDestroyCommand *pCommand);
	void Cmd_VertexBuffer_Create(const CCommandBuffer::CVertexBufferCreateCommand *pCommand);
	void Cmd_Clear(const CCommandBuffer::CClearCommand *pCommand);
	void Cmd_Render(const CCommandBuffer::CRenderCommand *pCommand);
	void Cmd_Swap(const CCommandBuffer::CSwapCommand *pCommand);
	void Cmd_Screenshot(const CCommandBuffer::CScreenshotCommand *pCommand);
	void Cmd_VSync(const CCommandBuffer::CVSyncCommand *pCommand);

public:
	CCommandProcessorFragment_WGPU();

	bool RunCommand(const CCommandBuffer::CCommand * pBaseCommand);

	void SubmitCommandBuffer();

	void UploadStreamingData(const void *pData, unsigned Size, const CScreen *pScreens, int NumScreens);
};

// command processor impelementation, uses the fragments to combine into one processor
class CCommandProcessor_SDL_WGPU : public CGraphicsBackend_Threaded::ICommandProcessor
{
	CCommandProcessorFragment_WGPU m_WGPU;
	CCommandProcessorFragment_General m_General;
 public:
	virtual void RunBuffer(CCommandBuffer *pBuffer);
};

// graphics backend implemented with SDL and WGPU
class CGraphicsBackend_SDL_WGPU : public CGraphicsBackend_Threaded
{
	SDL_Window *m_pWindow;
	//SDL_GLContext m_GLContext;
	ICommandProcessor *m_pProcessor;
	volatile int m_TextureMemoryUsage;
	volatile int m_VertexBufferMemoryUsage;
	int m_NumScreens;
public:
	virtual int Init(const char *pName, int *pScreen, int *pWindowWidth, int *pWindowHeight, int *pScreenWidth, int *pScreenHeight, int FsaaSamples, int Flags, int *pDesktopWidth, int *pDesktopHeight);
	virtual int Shutdown();

	virtual int MemoryUsage() const;

	virtual int GetNumScreens() const { return m_NumScreens; }

	virtual void Minimize();
	virtual void Maximize();
	virtual bool Fullscreen(bool State);		// on=true/off=false
	virtual void SetWindowBordered(bool State);	// on=true/off=false
	virtual bool SetWindowScreen(int Index);
	virtual int GetWindowScreen();
	virtual int GetVideoModes(CVideoMode *pModes, int MaxModes, int Screen);
	virtual bool GetDesktopResolution(int Index, int *pDesktopWidth, int* pDesktopHeight);
	virtual int WindowActive();
	virtual int WindowOpen();
};
