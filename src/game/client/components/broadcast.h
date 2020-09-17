/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_BROADCAST_H
#define GAME_CLIENT_COMPONENTS_BROADCAST_H
#include <game/client/component.h>
#include <engine/textrender.h>

class CBroadcast : public CComponent
{
	struct CBcSegment
	{
		bool m_IsHighContrast;
		int m_GlyphPos;
	};

	CTextCursor m_BroadcastCursor;
	float m_BroadcastTime;

	// server broadcast
	enum {
		MAX_BROADCAST_MSG_LENGTH = 128,
		MAX_BROADCAST_LINES = 3,
	};

	CBcSegment m_aSrvBroadcastSegments[MAX_BROADCAST_MSG_LENGTH];
	int m_NumSegments;
	CTextCursor m_SrvBroadcastCursor;
	float m_SrvBroadcastReceivedTime;

	void RenderServerBroadcast();

public:
	CBroadcast();

	void DoBroadcast(const char *pText);

	virtual void OnReset();
	virtual void OnMessage(int MsgType, void *pRawMsg);
	virtual void OnRender();
};

#endif
