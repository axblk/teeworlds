#ifndef GAME_SERVER_SCORE_SQLITESCORE_H
#define GAME_SERVER_SCORE_SQLITESCORE_H

#include <base/tl/array.h>

#include <engine/shared/jobs.h>

#include "../score.h"

struct sqlite3;

class CSQLiteScore : public IScoreBackend
{
	struct CSqlExecData
	{
		typedef int(*FRequestFunc)(sqlite3 *pDB, CRequestData *pRequestData);

		CSqlExecData(int Type, FRequestFunc pFunc, int Flags, CRequestData *pRequestData = 0)
			: m_Type(Type), m_pfnFunc(pFunc), m_Flags(Flags), m_pRequestData(pRequestData), m_HandlerResult(-1) { }

		CJob m_Job;
		int m_Type;
		FRequestFunc m_pfnFunc;
		int m_Flags;
		CRequestData *m_pRequestData;
		int m_HandlerResult;
	};

	class IEngine *m_pEngine;
	class IStorage *m_pStorage;

	bool m_DBValid;
	static char m_aDBFilename[512];
	array<CSqlExecData*> m_lPendingRequests;

	static int ExecSqlFunc(void *pUser);

	static int CreateTablesHandler(sqlite3 *pDB, CRequestData *pRequestData);
	static int LoadMapHandler(sqlite3 *pDB, CRequestData *pRequestData);
	static int LoadPlayerHandler(sqlite3 *pDB, CRequestData *pRequestData);
	static int SaveScoreHandler(sqlite3 *pDB, CRequestData *pRequestData);
	static int ShowRankHandler(sqlite3 *pDB, CRequestData *pRequestData);
	static int ShowTop5Handler(sqlite3 *pDB, CRequestData *pRequestData);

public:
	CSQLiteScore(IScoreResponseListener *pListener, IEngine *pEngine, IStorage *pStorage, const char *pDatabase);
	~CSQLiteScore();

	bool Ready() const { return m_DBValid; };
	void Tick();
	void AddRequest(int Type, CRequestData *pRequestData = 0);
};

#endif
