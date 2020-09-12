/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/shared/config.h>
#include <engine/graphics.h>
#include <generated/protocol.h>
#include <generated/client_data.h>

#include <game/client/gameclient.h>

#include "broadcast.h"
#include "chat.h"
#include "scoreboard.h"
#include "motd.h"

#define BROADCAST_FONTSIZE_BIG 11.0f
#define BROADCAST_FONTSIZE_SMALL 6.5f

inline bool IsCharANum(char c)
{
	return c >= '0' && c <= '9';
}

inline int WordLengthBack(const char *pText, int MaxChars)
{
	int s = 0;
	while(MaxChars--)
	{
		if((*pText == '\n' || *pText == '\t' || *pText == ' '))
			return s;
		pText--;
		s++;
	}
	return 0;
}

inline bool IsCharWhitespace(char c)
{
	return c == '\n' || c == '\t' || c == ' ';
}

void CBroadcast::RenderServerBroadcast()
{
	if(!Config()->m_ClShowServerBroadcast || m_pClient->m_MuteServerBroadcast)
		return;

	const bool ColoredBroadcastEnabled = Config()->m_ClColoredBroadcast;
	const float Height = 300;
	const float Width = Height*Graphics()->ScreenAspect();

	const float DisplayDuration = 10.0f;
	const float DisplayStartFade = 9.0f;
	const float DeltaTime = Client()->LocalTime() - m_SrvBroadcastReceivedTime;

	if(m_aSrvBroadcastMsg[0] == 0 || DeltaTime > DisplayDuration)
		return;

	if(m_pClient->m_pChat->IsActive() || m_pClient->Client()->State() != IClient::STATE_ONLINE)
		return;

	Graphics()->MapScreen(0, 0, Width, Height);

	const float Fade = 1.0f - max(0.0f, (DeltaTime - DisplayStartFade) / (DisplayDuration - DisplayStartFade));

	CUIRect ScreenRect = {0, 0, Width, Height};

	CUIRect BcView = ScreenRect;
	BcView.x += Width * 0.25f;
	BcView.y += Height * 0.8f;
	BcView.w *= 0.5f;
	BcView.h *= 0.2f;

	vec4 ColorTop(0, 0, 0, 0);
	vec4 ColorBot(0, 0, 0, 0.4f * Fade);
	CUIRect BgRect;
	BcView.HSplitBottom(10.0f, 0, &BgRect);
	BcView.HSplitBottom(6.0f, &BcView, 0);

	// draw bottom bar
	const float CornerWidth = 10.0f;
	const float CornerHeight = BgRect.h;
	BgRect.VMargin(CornerWidth, &BgRect);

	Graphics()->TextureClear();
	Graphics()->QuadsBegin();

	// make round corners
	enum { CORNER_MAX_QUADS=4 };
	IGraphics::CFreeformItem LeftCornerQuads[CORNER_MAX_QUADS];
	IGraphics::CFreeformItem RightCornerQuads[CORNER_MAX_QUADS];
	const float AngleStep = (pi * 0.5f)/(CORNER_MAX_QUADS * 2);

	for(int q = 0; q < CORNER_MAX_QUADS; q++)
	{
		const float Angle = AngleStep * q * 2;
		const float ca = cosf(Angle);
		const float ca1 = cosf(Angle + AngleStep);
		const float ca2 = cosf(Angle + AngleStep * 2);
		const float sa = sinf(Angle);
		const float sa1 = sinf(Angle + AngleStep);
		const float sa2 = sinf(Angle + AngleStep * 2);

		IGraphics::CFreeformItem LQuad(
			BgRect.x + ca * -CornerWidth,
			BgRect.y + CornerHeight + sa * -CornerHeight,

			BgRect.x, BgRect.y + CornerHeight,

			BgRect.x + ca1 * -CornerWidth,
			BgRect.y + CornerHeight + sa1 * -CornerHeight,

			BgRect.x + ca2 * -CornerWidth,
			BgRect.y + CornerHeight + sa2 *- CornerHeight
		);
		LeftCornerQuads[q] = LQuad;

		IGraphics::CFreeformItem RQuad(
			BgRect.x + BgRect.w + ca * CornerWidth,
			BgRect.y + CornerHeight + sa * -CornerHeight,

			BgRect.x + BgRect.w, BgRect.y + CornerHeight,

			BgRect.x + BgRect.w + ca1 * CornerWidth,
			BgRect.y + CornerHeight + sa1 * -CornerHeight,

			BgRect.x + BgRect.w + ca2 * CornerWidth,
			BgRect.y + CornerHeight + sa2 *- CornerHeight
		);
		RightCornerQuads[q] = RQuad;
	}

	IGraphics::CColorVertex aColorVert[4] = {
		IGraphics::CColorVertex(0, 0,0,0, 0.0f),
		IGraphics::CColorVertex(1, 0,0,0, 0.4f * Fade),
		IGraphics::CColorVertex(2, 0,0,0, 0.0f),
		IGraphics::CColorVertex(3, 0,0,0, 0.0f)
	};

	Graphics()->SetColorVertex(aColorVert, 4);
	Graphics()->QuadsDrawFreeform(LeftCornerQuads, CORNER_MAX_QUADS);
	Graphics()->QuadsDrawFreeform(RightCornerQuads, CORNER_MAX_QUADS);

	Graphics()->QuadsEnd();

	RenderTools()->DrawUIRect4(&BgRect, ColorTop, ColorTop,
							   ColorBot, ColorBot, 0, 0);


	BcView.VMargin(5.0f, &BcView);
	BcView.HSplitBottom(2.0f, &BcView, 0);

	// draw lines
	const float FontSize = m_SrvBroadcastFontSize;
	const int LineCount = m_SrvBroadcastLineCount;
	const char* pBroadcastMsg = m_aSrvBroadcastMsg;
	CTextCursor Cursor(FontSize);

	const vec2 ShadowOff(1.0f, 2.0f);
	const vec4 ShadowColorBlack(0, 0, 0, 0.9f * Fade);
	const vec4 TextColorWhite(1, 1, 1, Fade);
	float y = BcView.y + BcView.h - LineCount * FontSize;

	// TODO: ADDBACK: do broadcast

	TextRender()->TextColor(1, 1, 1, 1);
	TextRender()->TextSecondaryColor(0, 0, 0, 0.3f);
}

CBroadcast::CBroadcast()
{
	OnReset();
}

void CBroadcast::DoBroadcast(const char *pText)
{
	str_copy(m_aBroadcastText, pText, sizeof(m_aBroadcastText));
	// m_Cursor.Clear();
	
	// m_Cursor.m_Align = TEXTALIGN_BC;
	// m_Cursor.m_FontSize = BIG
	// CTextCursor Cursor;
	// TextRender()->SetCursor(&Cursor, 0, 0, 12.0f, TEXTFLAG_STOP_AT_END);
	// Cursor.m_LineWidth = 300*Graphics()->ScreenAspect();
	// TextRender()->TextEx(&Cursor, m_aBroadcastText, -1);
	// m_BroadcastRenderOffset = 150*Graphics()->ScreenAspect()-Cursor.m_X/2;
	// m_BroadcastTime = Client()->LocalTime() + 10.0f;
}

void CBroadcast::OnReset()
{
	m_BroadcastTime = 0;
	m_SrvBroadcastReceivedTime = 0;
}

void CBroadcast::OnMessage(int MsgType, void* pRawMsg)
{
	// process server broadcast message
	if(MsgType == NETMSGTYPE_SV_BROADCAST && Config()->m_ClShowServerBroadcast &&
	   !m_pClient->m_MuteServerBroadcast)
	{
		CNetMsg_Sv_Broadcast *pMsg = (CNetMsg_Sv_Broadcast *)pRawMsg;

		// new broadcast message
		int RcvMsgLen = str_length(pMsg->m_pMessage);
		mem_zero(m_aSrvBroadcastMsg, sizeof(m_aSrvBroadcastMsg));
		m_aSrvBroadcastMsgLen = 0;
		m_SrvBroadcastReceivedTime = Client()->LocalTime();

		const CBcColor White = { 255, 255, 255, 0 };
		m_aSrvBroadcastColorList[0] = White;
		m_SrvBroadcastColorCount = 1;

		CBcLineInfo UserLines[MAX_BROADCAST_LINES];
		int UserLineCount = 0;
		int LastUserLineStartPoint = 0;

		// parse colors
		for(int i = 0; i < RcvMsgLen; i++)
		{
			const char* c = pMsg->m_pMessage + i;
			const char* pTmp = c;
			int CharUtf8 = str_utf8_decode(&pTmp);
			const int Utf8Len = pTmp-c;

			if(*c == CharUtf8 && *c == '^')
			{
				if(i+3 < RcvMsgLen && IsCharANum(c[1]) && IsCharANum(c[2])  && IsCharANum(c[3]))
				{
					u8 r = (c[1] - '0') * 24 + 39;
					u8 g = (c[2] - '0') * 24 + 39;
					u8 b = (c[3] - '0') * 24 + 39;
					CBcColor Color = { r, g, b, m_aSrvBroadcastMsgLen };
					if(m_SrvBroadcastColorCount < MAX_BROADCAST_COLORS)
						m_aSrvBroadcastColorList[m_SrvBroadcastColorCount++] = Color;
					i += 3;
					continue;
				}
			}

			if(*c == CharUtf8 && *c == '\\')
			{
				if(i+1 < RcvMsgLen && c[1] == 'n' && UserLineCount < MAX_BROADCAST_LINES)
				{
					CBcLineInfo Line = { m_aSrvBroadcastMsg+LastUserLineStartPoint,
										 m_aSrvBroadcastMsgLen-LastUserLineStartPoint, 0 };
					if(Line.m_StrLen > 0)
						UserLines[UserLineCount++] = Line;
					LastUserLineStartPoint = m_aSrvBroadcastMsgLen;
					i++;
					continue;
				}
			}

			if(*c == '\n')
			{
				CBcLineInfo Line = { m_aSrvBroadcastMsg+LastUserLineStartPoint,
									 m_aSrvBroadcastMsgLen-LastUserLineStartPoint, 0 };
				if(Line.m_StrLen > 0)
					UserLines[UserLineCount++] = Line;
				LastUserLineStartPoint = m_aSrvBroadcastMsgLen;
				continue;
			}

			if(m_aSrvBroadcastMsgLen+Utf8Len < MAX_BROADCAST_MSG_LENGTH)
				m_aSrvBroadcastMsg[m_aSrvBroadcastMsgLen++] = *c;
		}

		// last user defined line
		if(LastUserLineStartPoint > 0 && UserLineCount < 3)
		{
			CBcLineInfo Line = { m_aSrvBroadcastMsg+LastUserLineStartPoint,
								 m_aSrvBroadcastMsgLen-LastUserLineStartPoint, 0 };
			if(Line.m_StrLen > 0)
				UserLines[UserLineCount++] = Line;
		}

		const float Height = 300;
		const float Width = Height*Graphics()->ScreenAspect();
		const float LineMaxWidth = Width * 0.5f - 10.0f;

		// process boradcast message
		const char* pBroadcastMsg = m_aSrvBroadcastMsg;
		const int MsgLen = m_aSrvBroadcastMsgLen;

		Graphics()->MapScreen(0, 0, Width, Height);

		// one line == big font
		// 2+ lines == small font
		m_SrvBroadcastLineCount = 0;
		float FontSize = BROADCAST_FONTSIZE_BIG;
		CTextCursor Cursor(FontSize);

		// TODO: ADDBACK: broadcast lines

		m_SrvBroadcastFontSize = FontSize;
	}
}

void CBroadcast::OnRender()
{
	if(Client()->State() < IClient::STATE_ONLINE)
		return;

	// server broadcast
	RenderServerBroadcast();

	// client broadcast
	if(m_pClient->m_pScoreboard->IsActive() || m_pClient->m_pMotd->IsActive())
		return;

	Graphics()->MapScreen(0, 0, 300*Graphics()->ScreenAspect(), 300);

	if(Client()->LocalTime() < m_BroadcastTime)
	{
		// TODO: ADDBACK: render broadcast
	}
}

