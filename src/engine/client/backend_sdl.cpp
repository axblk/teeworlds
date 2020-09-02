#include <base/detect.h>
#include "SDL.h"

#include <base/tl/threading.h>

#include <engine/external/glad/glad.h>

#include "graphics_threaded.h"
#include "backend_sdl.h"

// TODO:
// add tileset fallback system
// rename 3d tex to 2d tex array
// mipmaps for 2d tex arrays
// improve streaming
// add vertex buffers
// configurable layer number for 2d tex array? (usage for countryflags, icons, ...)

// legacy gl

#define GL_GENERATE_MIPMAP 0x8191

inline int GetGLVersion() { return GLVersion.major * 100 + GLVersion.minor * 10; }
inline bool IsLegacyGL() { return GetGLVersion() < 330; }

// shaders

static const GLchar *s_pVertexShader120 =
	"#version 120\n"
	"attribute vec2 a_Pos;\n"
	"attribute vec3 a_TexCoord;\n"
	"attribute vec4 a_Color;\n"
	"varying vec3 v_TexCoord;\n"
	"varying vec4 v_Color;\n"
	"uniform mat4 u_Transform;\n"
	"void main()\n"
	"{\n"
	"    v_TexCoord = a_TexCoord;\n"
	"    v_Color = a_Color;\n"
	"    gl_Position = u_Transform * vec4(a_Pos.xy, 0.0, 1.0);\n"
	"}\n";


static const GLchar *s_pVertexShader330 =
	"#version 330 core\n"
	"layout(location = 0) in vec2 a_Pos;\n"
	"layout(location = 1) in vec3 a_TexCoord;\n"
	"layout(location = 2) in vec4 a_Color;\n"
	"out vec3 v_TexCoord;\n"
	"out vec4 v_Color;\n"
	"uniform mat4 u_Transform;\n"
	"void main()\n"
	"{\n"
	"    v_TexCoord = a_TexCoord;\n"
	"    v_Color = a_Color;\n"
	"    gl_Position = u_Transform * vec4(a_Pos.xy, 0.0, 1.0);\n"
	"}\n";

static const GLchar *s_pFragmentNoTexShader120 =
	"#version 120\n"
	"varying vec3 v_TexCoord;\n"
	"varying vec4 v_Color;\n"
	"void main()\n"
	"{\n"
	"    gl_FragColor = v_Color;\n"
	"}\n";

static const GLchar *s_pFragmentNoTexShader330 =
	"#version 330 core\n"
	"in vec3 v_TexCoord;\n"
	"in vec4 v_Color;\n"
	"out vec4 f_Color;\n"
	"void main()\n"
	"{\n"
	"    f_Color = v_Color;\n"
	"}\n";

static const GLchar *s_pFragment2DTexShader120 =
	"#version 120\n"
	"uniform sampler2D s_Texture;\n"
	"varying vec3 v_TexCoord;\n"
	"varying vec4 v_Color;\n"
	"void main()\n"
	"{\n"
	"    gl_FragColor = texture2D(s_Texture, v_TexCoord.xy) * v_Color;\n"
	"}\n";

static const GLchar *s_pFragment2DTexShader330 =
	"#version 330 core\n"
	"uniform sampler2D s_Texture;\n"
	"in vec3 v_TexCoord;\n"
	"in vec4 v_Color;\n"
	"out vec4 f_Color;\n"
	"void main()\n"
	"{\n"
	"    f_Color = texture(s_Texture, v_TexCoord.xy) * v_Color;\n"
	"}\n";

static const GLchar *s_pFragment3DTexShader120 =
	"#version 120\n"
	"uniform sampler3D s_Texture;\n"
	"varying vec3 v_TexCoord;\n"
	"varying vec4 v_Color;\n"
	"void main()\n"
	"{\n"
	"    gl_FragColor = texture3D(s_Texture, vec3(v_TexCoord.xy, (v_TexCoord.z + 0.5f) / 256.f)) * v_Color;\n"
//	"    gl_FragColor = texture3D(s_Texture, v_TexCoord) * v_Color;\n"
	"}\n";

static const GLchar *s_pFragment2DTexArrayShader330 =
	"#version 330 core\n"
	"uniform sampler2DArray s_Texture;\n"
	"in vec3 v_TexCoord;\n"
	"in vec4 v_Color;\n"
	"out vec4 f_Color;\n"
	"void main()\n"
	"{\n"
	"    f_Color = texture(s_Texture, v_TexCoord) * v_Color;\n"
	"}\n";

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

// ------------ CCommandProcessorFragment_OpenGL

int CCommandProcessorFragment_OpenGL::TexFormatToOpenGLFormat(int TexFormat)
{
	if(TexFormat == CCommandBuffer::TEXFORMAT_RGB) return GL_RGB;
	if(TexFormat == CCommandBuffer::TEXFORMAT_ALPHA) return IsLegacyGL() ? GL_RGBA : GL_RED;
	if(TexFormat == CCommandBuffer::TEXFORMAT_RGBA) return GL_RGBA;
	return GL_RGBA;
}

unsigned char CCommandProcessorFragment_OpenGL::Sample(int w, int h, const unsigned char *pData, int u, int v, int Offset, int ScaleW, int ScaleH, int Bpp)
{
	int Value = 0;
	for(int x = 0; x < ScaleW; x++)
		for(int y = 0; y < ScaleH; y++)
			Value += pData[((v+y)*w+(u+x))*Bpp+Offset];
	return Value/(ScaleW*ScaleH);
}

unsigned char *CCommandProcessorFragment_OpenGL::Rescale(int Width, int Height, int NewWidth, int NewHeight, int Format, const unsigned char *pData)
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

void CCommandProcessorFragment_OpenGL::SetState(const CCommandBuffer::CState &State)
{
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
	int RenderMode = RENDER_NO_TEX;
	if(State.m_Texture >= 0 && State.m_Texture < CCommandBuffer::MAX_TEXTURES)
	{
		glActiveTexture(GL_TEXTURE0);
		if(State.m_Dimension == 2 && (m_aTextures[State.m_Texture].m_State&CTexture::STATE_TEX2D))
		{
			RenderMode = RENDER_2D_TEX;
			glBindTexture(GL_TEXTURE_2D, m_aTextures[State.m_Texture].m_Tex2D);
		}
		else if(State.m_Dimension == 3 && (m_aTextures[State.m_Texture].m_State&CTexture::STATE_TEX3D))
		{
			RenderMode = RENDER_2D_TEX_ARRAY;
			GLenum Target = IsLegacyGL() ? GL_TEXTURE_3D : GL_TEXTURE_2D_ARRAY;
			glBindTexture(Target, m_aTextures[State.m_Texture].m_Tex3D[0]);
			//glBindTexture(Target, m_aTextures[State.m_Texture].m_Tex3D[State.m_TextureArrayIndex]);
		}
		else
			dbg_msg("render", "invalid texture %d %d %d\n", State.m_Texture, State.m_Dimension, m_aTextures[State.m_Texture].m_State);

		if(m_aTextures[State.m_Texture].m_Format == CCommandBuffer::TEXFORMAT_RGBA)
			SrcBlendMode = GL_ONE;
		else
			SrcBlendMode = GL_SRC_ALPHA;
	}

	const CShaderProgram *pPrograms[] = {&m_NoTexProgram, &m_2DTexProgram, &m_2DTexArrayProgram};

	glUseProgram(pPrograms[RenderMode]->m_Program);

	glUniform1i(pPrograms[RenderMode]->m_TextureLoc, 0);

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
	if(RenderMode == RENDER_2D_TEX)
	{
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
	}

	if(RenderMode == RENDER_2D_TEX_ARRAY)
	{
		GLenum Target = IsLegacyGL() ? GL_TEXTURE_3D : GL_TEXTURE_2D_ARRAY;
		glTexParameteri(Target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(Target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(Target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(Target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(Target, GL_TEXTURE_WRAP_R, GL_REPEAT);
	}

	// screen mapping
	const float Near = -1.0f;
	const float Far = 1.0f;

	// orthographic projection matrix
	float Mat4[4*4] = {
		2.f / (State.m_ScreenBR.x - State.m_ScreenTL.x), 0, 0, - (State.m_ScreenBR.x + State.m_ScreenTL.x) / (State.m_ScreenBR.x - State.m_ScreenTL.x),
		0, 2.f / (State.m_ScreenTL.y - State.m_ScreenBR.y), 0, - (State.m_ScreenTL.y + State.m_ScreenBR.y) / (State.m_ScreenTL.y - State.m_ScreenBR.y),
		0, 0, - 2.f / (Far - Near), - (Far + Near) / (Far - Near),
		0, 0, 0, 1.0f
	};

	glUniformMatrix4fv(pPrograms[RenderMode]->m_TransformLoc, 1, true, Mat4);
}

static bool CheckShader(GLuint Shader, const char *pName)
{
	GLint Status = 0;
	GLint LogLength = 0;
	glGetShaderiv(Shader, GL_COMPILE_STATUS, &Status);
	glGetShaderiv(Shader, GL_INFO_LOG_LENGTH, &LogLength);
	if((GLboolean)Status == GL_FALSE)
		dbg_msg("shader", "failed to compile shader: %s", pName);
	if(LogLength > 1)
	{
		char *pLog = (char*)mem_alloc(LogLength + 1, 1);
		glGetShaderInfoLog(Shader, LogLength, NULL, pLog);
		dbg_msg("shader", "%s", pLog);
		mem_free(pLog);
	}
	return (GLboolean)Status == GL_TRUE;
}

static bool CheckProgram(GLuint Program, const char *pName)
{
	GLint Status = 0;
	GLint LogLength = 0;
	glGetProgramiv(Program, GL_LINK_STATUS, &Status);
	glGetProgramiv(Program, GL_INFO_LOG_LENGTH, &LogLength);
	if((GLboolean)Status == GL_FALSE)
		dbg_msg("shader", "failed to link program: %s", pName);
	if(LogLength > 1)
	{
		char *pLog = (char*)mem_alloc(LogLength + 1, 1);
		glGetProgramInfoLog(Program, LogLength, NULL, pLog);
		dbg_msg("shader", "%s", pLog);
		mem_free(pLog);
	}
	return (GLboolean)Status == GL_TRUE;
}

bool CCommandProcessorFragment_OpenGL::CShaderProgram::Create(GLuint VertexShader, GLuint FragmentShader, const char *pName)
{
	m_Program = glCreateProgram();
	glAttachShader(m_Program, VertexShader);
	glAttachShader(m_Program, FragmentShader);
	if(IsLegacyGL())
	{
		glBindAttribLocation(m_Program, 0, "a_Pos");
		glBindAttribLocation(m_Program, 1, "a_TexCoord");
		glBindAttribLocation(m_Program, 2, "a_Color");
	}
	glLinkProgram(m_Program);
	glDetachShader(m_Program, VertexShader);
	glDetachShader(m_Program, FragmentShader);
	m_TransformLoc = glGetUniformLocation(m_Program, "u_Transform");
	m_TextureLoc = glGetUniformLocation(m_Program, "s_Texture");
	return CheckProgram(m_Program, pName);
}

void CCommandProcessorFragment_OpenGL::Cmd_Init(const CInitCommand *pCommand)
{
	// set some default settings
	glEnable(GL_BLEND);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	/*glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();*/

	/*glAlphaFunc(GL_GREATER, 0);
	glEnable(GL_ALPHA_TEST);*/
	glDepthMask(GL_FALSE);

	m_pTextureMemoryUsage = pCommand->m_pTextureMemoryUsage;
	*m_pTextureMemoryUsage = 0;
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &m_MaxTexSize);
	if(IsLegacyGL())
	{
		glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &m_Max3DTexSize);
		m_MaxArrayTexLayers = m_Max3DTexSize;
	}
	else
	{
		glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &m_MaxArrayTexLayers);
		m_Max3DTexSize = m_MaxTexSize;
	}
	dbg_msg("render", "opengl max texture sizes: %d, %d(3D)", m_MaxTexSize, m_Max3DTexSize);
	if(m_MaxArrayTexLayers < IGraphics::NUMTILES_DIMENSION * IGraphics::NUMTILES_DIMENSION)
		dbg_msg("render", "*** warning *** max 3D texture size is too low - using the fallback system");
	m_TextureArraySize = IGraphics::NUMTILES_DIMENSION * IGraphics::NUMTILES_DIMENSION / min(m_MaxArrayTexLayers, IGraphics::NUMTILES_DIMENSION * IGraphics::NUMTILES_DIMENSION);
	*pCommand->m_pTextureArraySize = m_TextureArraySize;

	bool LegacyGL = IsLegacyGL();
	const GLchar *pVertexShaderSrc = LegacyGL ? s_pVertexShader120 : s_pVertexShader330;
	const GLchar *pFragmentNoTexShaderSrc = LegacyGL ? s_pFragmentNoTexShader120 : s_pFragmentNoTexShader330;
	const GLchar *pFragment2DTexShaderSrc = LegacyGL ? s_pFragment2DTexShader120 : s_pFragment2DTexShader330;
	const GLchar *pFragment3DTexShaderSrc = LegacyGL ? s_pFragment3DTexShader120 : s_pFragment2DTexArrayShader330;

	GLuint VertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(VertexShader, 1, &pVertexShaderSrc, NULL);
	glCompileShader(VertexShader);
	CheckShader(VertexShader, "vertex shader");

	GLuint FragmentNoTexShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(FragmentNoTexShader, 1, &pFragmentNoTexShaderSrc, NULL);
	glCompileShader(FragmentNoTexShader);
	CheckShader(FragmentNoTexShader, "no tex fragment shader");

	GLuint Fragment2DTexShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(Fragment2DTexShader, 1, &pFragment2DTexShaderSrc, NULL);
	glCompileShader(Fragment2DTexShader);
	CheckShader(Fragment2DTexShader, "2d tex fragment shader");

	GLuint Fragment3DTexShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(Fragment3DTexShader, 1, &pFragment3DTexShaderSrc, NULL);
	glCompileShader(Fragment3DTexShader);
	CheckShader(Fragment3DTexShader, "3d tex fragment shader");

	m_NoTexProgram.Create(VertexShader, FragmentNoTexShader, "no tex shader program");
	m_2DTexProgram.Create(VertexShader, Fragment2DTexShader, "2d tex shader program");
	m_2DTexArrayProgram.Create(VertexShader, Fragment3DTexShader, "2d tex array shader program");

	glDeleteShader(VertexShader);
	glDeleteShader(FragmentNoTexShader);
	glDeleteShader(Fragment2DTexShader);
	glDeleteShader(Fragment3DTexShader);

	if(!IsLegacyGL())
	{
		GLuint VertexArray;
		glGenVertexArrays(1, &VertexArray);
		glBindVertexArray(VertexArray);
	}

	glGenBuffers(1, &m_StreamingBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, m_StreamingBuffer);

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);

	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(IGraphics::CVertex), 0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(IGraphics::CVertex), (void*)(sizeof(float) * 2));
	glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(IGraphics::CVertex), (void*)(sizeof(float) * 5));
}

static void ConvertTexture(int Width, int Height, int Format, unsigned char **ppData)
{
	unsigned char *pTexels = *ppData;

	if(Format == CCommandBuffer::TEXFORMAT_ALPHA && IsLegacyGL())
	{
		unsigned char *pTmpData = (unsigned char *)mem_alloc(Width*Height*4, 1);
		for(int i = 0; i < Width * Height; ++i)
		{
			pTmpData[i*4+0] = 255;
			pTmpData[i*4+1] = 255;
			pTmpData[i*4+2] = 255;
			pTmpData[i*4+3] = pTexels[i];
		}
		mem_free(pTexels);
		*ppData = pTmpData;
	}
	// use premultiplied alpha for rgba textures
	else if(Format == CCommandBuffer::TEXFORMAT_RGBA)
	{	
		for(int i = 0; i < Width * Height; ++i)
		{
			const float a = (pTexels[i*4+3]/255.0f);
			pTexels[i*4+0] = (unsigned char)(pTexels[i*4+0] * a);
			pTexels[i*4+1] = (unsigned char)(pTexels[i*4+1] * a);
			pTexels[i*4+2] = (unsigned char)(pTexels[i*4+2] * a);
		}
	}
}

void CCommandProcessorFragment_OpenGL::Cmd_Texture_Update(const CCommandBuffer::CTextureUpdateCommand *pCommand)
{
	unsigned char *pTexData = (unsigned char*)pCommand->m_pData;
	if(m_aTextures[pCommand->m_Slot].m_State&CTexture::STATE_TEX2D)
	{
		ConvertTexture(pCommand->m_Width, pCommand->m_Height, pCommand->m_Format, &pTexData);
		glBindTexture(GL_TEXTURE_2D, m_aTextures[pCommand->m_Slot].m_Tex2D);
		glTexSubImage2D(GL_TEXTURE_2D, 0, pCommand->m_X, pCommand->m_Y, pCommand->m_Width, pCommand->m_Height,
			TexFormatToOpenGLFormat(pCommand->m_Format), GL_UNSIGNED_BYTE, pTexData);
	}
	mem_free(pTexData);
}

void CCommandProcessorFragment_OpenGL::Cmd_Texture_Destroy(const CCommandBuffer::CTextureDestroyCommand *pCommand)
{
	if(m_aTextures[pCommand->m_Slot].m_State&CTexture::STATE_TEX2D)
		glDeleteTextures(1, &m_aTextures[pCommand->m_Slot].m_Tex2D);
	if(m_aTextures[pCommand->m_Slot].m_State&CTexture::STATE_TEX3D)
		glDeleteTextures(m_TextureArraySize, m_aTextures[pCommand->m_Slot].m_Tex3D);
	*m_pTextureMemoryUsage -= m_aTextures[pCommand->m_Slot].m_MemSize;
	m_aTextures[pCommand->m_Slot].m_State = CTexture::STATE_EMPTY;
	m_aTextures[pCommand->m_Slot].m_MemSize = 0;
}

void CCommandProcessorFragment_OpenGL::Cmd_Texture_Create(const CCommandBuffer::CTextureCreateCommand *pCommand)
{
	int Width = pCommand->m_Width;
	int Height = pCommand->m_Height;
	int Depth = 1;
	unsigned char *pTexData = (unsigned char*)pCommand->m_pData;

	// resample if needed
	if(pCommand->m_Format == CCommandBuffer::TEXFORMAT_RGBA || pCommand->m_Format == CCommandBuffer::TEXFORMAT_RGB)
	{
		int MaxTexSize = m_MaxTexSize;
		if((pCommand->m_Flags&CCommandBuffer::TEXFLAG_TEXTURE3D) && m_MaxArrayTexLayers >= CTexture::MIN_GL_MAX_3D_TEXTURE_SIZE)
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

			unsigned char *pTmpData = Rescale(pCommand->m_Width, pCommand->m_Height, Width, Height, pCommand->m_Format, pTexData);
			mem_free(pTexData);
			pTexData = pTmpData;
		}
		else if(Width > IGraphics::NUMTILES_DIMENSION && Height > IGraphics::NUMTILES_DIMENSION && (pCommand->m_Flags&CCommandBuffer::TEXFLAG_QUALITY) == 0)
		{
			Width>>=1;
			Height>>=1;

			unsigned char *pTmpData = Rescale(pCommand->m_Width, pCommand->m_Height, Width, Height, pCommand->m_Format, pTexData);
			mem_free(pTexData);
			pTexData = pTmpData;
		}
	}

	ConvertTexture(Width, Height, pCommand->m_Format, &pTexData);
	m_aTextures[pCommand->m_Slot].m_Format = pCommand->m_Format;

	//
	int Oglformat = TexFormatToOpenGLFormat(pCommand->m_Format);
	int StoreOglformat = TexFormatToOpenGLFormat(pCommand->m_StoreFormat);

	if(pCommand->m_Flags&CCommandBuffer::TEXFLAG_COMPRESSED)
	{
		switch(StoreOglformat)
		{
			case GL_RGB: StoreOglformat = GL_COMPRESSED_RGB; break;
			case GL_RED: StoreOglformat = GL_COMPRESSED_RED; break;
			case GL_RGBA: StoreOglformat = GL_COMPRESSED_RGBA; break;
			default: StoreOglformat = GL_COMPRESSED_RGBA;
		}
	}

	// 2D texture
	if(pCommand->m_Flags&CCommandBuffer::TEXFLAG_TEXTURE2D)
	{
		bool Mipmaps = !(pCommand->m_Flags&CCommandBuffer::TEXFLAG_NOMIPMAPS);
		glGenTextures(1, &m_aTextures[pCommand->m_Slot].m_Tex2D);
		m_aTextures[pCommand->m_Slot].m_State |= CTexture::STATE_TEX2D;
		glBindTexture(GL_TEXTURE_2D, m_aTextures[pCommand->m_Slot].m_Tex2D);
		if(m_aTextures[pCommand->m_Slot].m_Format == CCommandBuffer::TEXFORMAT_ALPHA && !IsLegacyGL())
		{
			GLint aSwizzleMask[] = {GL_ONE, GL_ONE, GL_ONE, GL_RED};
			glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, aSwizzleMask);
		}
		if(!Mipmaps)
		{
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexImage2D(GL_TEXTURE_2D, 0, StoreOglformat, Width, Height, 0, Oglformat, GL_UNSIGNED_BYTE, pTexData);
		}
		else
		{
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			if(pCommand->m_Flags&CCommandBuffer::TEXTFLAG_LINEARMIPMAPS)
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			else
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
			if(IsLegacyGL())
				glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
			glTexImage2D(GL_TEXTURE_2D, 0, StoreOglformat, Width, Height, 0, Oglformat, GL_UNSIGNED_BYTE, pTexData);
			if(!IsLegacyGL())
				glGenerateMipmap(GL_TEXTURE_2D);
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
		}
	}

	// 3D texture
	if((pCommand->m_Flags&CCommandBuffer::TEXFLAG_TEXTURE3D) && m_MaxArrayTexLayers >= CTexture::MIN_GL_MAX_3D_TEXTURE_SIZE)
	{
		Width /= IGraphics::NUMTILES_DIMENSION;
		Height /= IGraphics::NUMTILES_DIMENSION;
		Depth = min(m_MaxArrayTexLayers, IGraphics::NUMTILES_DIMENSION * IGraphics::NUMTILES_DIMENSION);

		// copy and reorder texture data
		int MemSize = Width*Height*IGraphics::NUMTILES_DIMENSION*IGraphics::NUMTILES_DIMENSION*pCommand->m_PixelSize;
		unsigned char *pTmpData = (unsigned char *)mem_alloc(MemSize, sizeof(void*));

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
		glGenTextures(m_TextureArraySize, m_aTextures[pCommand->m_Slot].m_Tex3D);
		m_aTextures[pCommand->m_Slot].m_State |= CTexture::STATE_TEX3D;
		for(int i = 0; i < m_TextureArraySize; ++i)
		{
			GLenum Target = IsLegacyGL() ? GL_TEXTURE_3D : GL_TEXTURE_2D_ARRAY;
			glBindTexture(Target, m_aTextures[pCommand->m_Slot].m_Tex3D[i]);
			glTexParameteri(Target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(Target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			pTexData = pTmpData+i*(Width*Height*Depth*pCommand->m_PixelSize);
			glTexImage3D(Target, 0, StoreOglformat, Width, Height, Depth, 0, Oglformat, GL_UNSIGNED_BYTE, pTexData);

			m_aTextures[pCommand->m_Slot].m_MemSize += Width*Height*pCommand->m_PixelSize;
		}
		pTexData = pTmpData;
	}

	*m_pTextureMemoryUsage += m_aTextures[pCommand->m_Slot].m_MemSize;

	mem_free(pTexData);
}

void CCommandProcessorFragment_OpenGL::Cmd_Clear(const CCommandBuffer::CClearCommand *pCommand)
{
	glClearColor(pCommand->m_Color.r, pCommand->m_Color.g, pCommand->m_Color.b, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void CCommandProcessorFragment_OpenGL::Cmd_Render(const CCommandBuffer::CRenderCommand *pCommand)
{
	SetState(pCommand->m_State);

	int First = pCommand->m_Offset / sizeof(IGraphics::CVertex);

	switch(pCommand->m_PrimType)
	{
	case CCommandBuffer::PRIMTYPE_TRIANGLES:
		glDrawArrays(GL_TRIANGLES, First, pCommand->m_PrimCount*3);
		break;
	case CCommandBuffer::PRIMTYPE_LINES:
		glDrawArrays(GL_LINES, First, pCommand->m_PrimCount*2);
		break;
	default:
		dbg_msg("render", "unknown primtype %d\n", pCommand->m_Cmd);
	};
}

void CCommandProcessorFragment_OpenGL::Cmd_Screenshot(const CCommandBuffer::CScreenshotCommand *pCommand)
{
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

	// flip the pixel because opengl works from bottom left corner
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
	pCommand->m_pImage->m_pData = pPixelData;
}

CCommandProcessorFragment_OpenGL::CCommandProcessorFragment_OpenGL()
{
	mem_zero(m_aTextures, sizeof(m_aTextures));
	m_pTextureMemoryUsage = 0;
}

bool CCommandProcessorFragment_OpenGL::RunCommand(const CCommandBuffer::CCommand * pBaseCommand)
{
	switch(pBaseCommand->m_Cmd)
	{
	case CMD_INIT: Cmd_Init(static_cast<const CInitCommand *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_TEXTURE_CREATE: Cmd_Texture_Create(static_cast<const CCommandBuffer::CTextureCreateCommand *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_TEXTURE_DESTROY: Cmd_Texture_Destroy(static_cast<const CCommandBuffer::CTextureDestroyCommand *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_TEXTURE_UPDATE: Cmd_Texture_Update(static_cast<const CCommandBuffer::CTextureUpdateCommand *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_CLEAR: Cmd_Clear(static_cast<const CCommandBuffer::CClearCommand *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_RENDER: Cmd_Render(static_cast<const CCommandBuffer::CRenderCommand *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_SCREENSHOT: Cmd_Screenshot(static_cast<const CCommandBuffer::CScreenshotCommand *>(pBaseCommand)); break;
	default: return false;
	}

	return true;
}

void CCommandProcessorFragment_OpenGL::UploadStreamingData(const void *pData, unsigned Size)
{
	glBufferData(GL_ARRAY_BUFFER, 2 * 1024 * 1024, NULL, GL_STREAM_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, Size, pData);
}

// ------------ CCommandProcessorFragment_SDL

void CCommandProcessorFragment_SDL::Cmd_Init(const CInitCommand *pCommand)
{
	m_GLContext = pCommand->m_GLContext;
	m_pWindow = pCommand->m_pWindow;
	SDL_GL_MakeCurrent(m_pWindow, m_GLContext);
}

void CCommandProcessorFragment_SDL::Cmd_Shutdown(const CShutdownCommand *pCommand)
{
	SDL_GL_MakeCurrent(NULL, NULL);
}

void CCommandProcessorFragment_SDL::Cmd_Swap(const CCommandBuffer::CSwapCommand *pCommand)
{
	SDL_GL_SwapWindow(m_pWindow);

	if(pCommand->m_Finish)
		glFinish();
}

void CCommandProcessorFragment_SDL::Cmd_VSync(const CCommandBuffer::CVSyncCommand *pCommand)
{
	*pCommand->m_pRetOk = SDL_GL_SetSwapInterval(pCommand->m_VSync) == 0;
}

CCommandProcessorFragment_SDL::CCommandProcessorFragment_SDL()
{
}

bool CCommandProcessorFragment_SDL::RunCommand(const CCommandBuffer::CCommand *pBaseCommand)
{
	switch(pBaseCommand->m_Cmd)
	{
	case CCommandBuffer::CMD_SWAP: Cmd_Swap(static_cast<const CCommandBuffer::CSwapCommand *>(pBaseCommand)); break;
	case CCommandBuffer::CMD_VSYNC: Cmd_VSync(static_cast<const CCommandBuffer::CVSyncCommand *>(pBaseCommand)); break;
	case CMD_INIT: Cmd_Init(static_cast<const CInitCommand *>(pBaseCommand)); break;
	case CMD_SHUTDOWN: Cmd_Shutdown(static_cast<const CShutdownCommand *>(pBaseCommand)); break;
	default: return false;
	}

	return true;
}

// ------------ CCommandProcessor_SDL_OpenGL

void CCommandProcessor_SDL_OpenGL::RunBuffer(CCommandBuffer *pBuffer)
{
	unsigned CmdIndex = 0;

	if(pBuffer->DataUsed() > 0)
		m_OpenGL.UploadStreamingData(pBuffer->DataPtr(), pBuffer->DataUsed());

	while(1)
	{
		const CCommandBuffer::CCommand *pBaseCommand = pBuffer->GetCommand(&CmdIndex);
		if(pBaseCommand == 0x0)
			break;

		if(m_OpenGL.RunCommand(pBaseCommand))
			continue;

		if(m_SDL.RunCommand(pBaseCommand))
			continue;

		if(m_General.RunCommand(pBaseCommand))
			continue;

		dbg_msg("graphics", "unknown command %d", pBaseCommand->m_Cmd);
	}
}

// ------------ CGraphicsBackend_SDL_OpenGL

int CGraphicsBackend_SDL_OpenGL::Init(const char *pName, int *pScreen, int *pWindowWidth, int *pWindowHeight, int* pScreenWidth, int* pScreenHeight, int FsaaSamples, int Flags, int *pDesktopWidth, int *pDesktopHeight)
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
	int SdlFlags = SDL_WINDOW_OPENGL;
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

	// set gl attributes
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
	}

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

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

	// create gl context
	m_GLContext = SDL_GL_CreateContext(m_pWindow);
	if(m_GLContext == NULL)
	{
		dbg_msg("gfx", "unable to create OpenGL context: %s", SDL_GetError());
		return -1;
	}

	SDL_GL_GetDrawableSize(m_pWindow, pScreenWidth, pScreenHeight); // drawable size may differ in high dpi mode

	if(!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress))
	{
		dbg_msg("gfx", "failed to load opengl functions");
		return -1;
	}

	dbg_msg("gfx", "OpenGL version: %s", glGetString(GL_VERSION));

	SDL_GL_SetSwapInterval(Flags&IGraphicsBackend::INITFLAG_VSYNC ? 1 : 0);

	SDL_GL_MakeCurrent(NULL, NULL);

	// print sdl version
	SDL_version Compiled;
	SDL_version Linked;

	SDL_VERSION(&Compiled);
	SDL_GetVersion(&Linked);
	dbg_msg("sdl", "SDL version %d.%d.%d (dll = %d.%d.%d)", Compiled.major, Compiled.minor, Compiled.patch, Linked.major, Linked.minor, Linked.patch);

	// start the command processor
	m_pProcessor = new CCommandProcessor_SDL_OpenGL;
	StartProcessor(m_pProcessor);

	// issue init commands for OpenGL and SDL
	CCommandBuffer CmdBuffer(1024, 512);
	CCommandProcessorFragment_SDL::CInitCommand CmdSDL;
	CmdSDL.m_pWindow = m_pWindow;
	CmdSDL.m_GLContext = m_GLContext;
	CmdBuffer.AddCommand(CmdSDL);
	CCommandProcessorFragment_OpenGL::CInitCommand CmdOpenGL;
	CmdOpenGL.m_pTextureMemoryUsage = &m_TextureMemoryUsage;
	CmdOpenGL.m_pTextureArraySize = &m_TextureArraySize;
	CmdBuffer.AddCommand(CmdOpenGL);
	RunBuffer(&CmdBuffer);
	WaitForIdle();

	// return
	return 0;
}

int CGraphicsBackend_SDL_OpenGL::Shutdown()
{
	// issue a shutdown command
	CCommandBuffer CmdBuffer(1024, 512);
	CCommandProcessorFragment_SDL::CShutdownCommand Cmd;
	CmdBuffer.AddCommand(Cmd);
	RunBuffer(&CmdBuffer);
	WaitForIdle();

	// stop and delete the processor
	StopProcessor();
	delete m_pProcessor;
	m_pProcessor = 0;

	SDL_GL_DeleteContext(m_GLContext);
	SDL_DestroyWindow(m_pWindow);
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
	return 0;
}

int CGraphicsBackend_SDL_OpenGL::MemoryUsage() const
{
	return m_TextureMemoryUsage;
}

void CGraphicsBackend_SDL_OpenGL::Minimize()
{
	SDL_MinimizeWindow(m_pWindow);
}

void CGraphicsBackend_SDL_OpenGL::Maximize()
{
	SDL_MaximizeWindow(m_pWindow);
}

bool CGraphicsBackend_SDL_OpenGL::Fullscreen(bool State)
{
#if defined(CONF_PLATFORM_MACOSX)	// Todo SDL: remove this when fixed (game freezes when losing focus in fullscreen)
	return SDL_SetWindowFullscreen(m_pWindow, State ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0) == 0;
#else
	return SDL_SetWindowFullscreen(m_pWindow, State ? SDL_WINDOW_FULLSCREEN : 0) == 0;
#endif
}

void CGraphicsBackend_SDL_OpenGL::SetWindowBordered(bool State)
{
	SDL_SetWindowBordered(m_pWindow, SDL_bool(State));
}

bool CGraphicsBackend_SDL_OpenGL::SetWindowScreen(int Index)
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

int CGraphicsBackend_SDL_OpenGL::GetWindowScreen()
{
	return SDL_GetWindowDisplayIndex(m_pWindow);
}

int CGraphicsBackend_SDL_OpenGL::GetVideoModes(CVideoMode *pModes, int MaxModes, int Screen)
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

bool CGraphicsBackend_SDL_OpenGL::GetDesktopResolution(int Index, int *pDesktopWidth, int* pDesktopHeight)
{
	SDL_DisplayMode DisplayMode;
	if(SDL_GetDesktopDisplayMode(Index, &DisplayMode))
		return false;

	*pDesktopWidth = DisplayMode.w;
	*pDesktopHeight = DisplayMode.h;
	return true;
}

int CGraphicsBackend_SDL_OpenGL::WindowActive()
{
	return SDL_GetWindowFlags(m_pWindow)&SDL_WINDOW_INPUT_FOCUS;
}

int CGraphicsBackend_SDL_OpenGL::WindowOpen()
{
	return SDL_GetWindowFlags(m_pWindow)&SDL_WINDOW_SHOWN;

}


IGraphicsBackend *CreateGraphicsBackend() { return new CGraphicsBackend_SDL_OpenGL; }
