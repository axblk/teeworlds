#include "gamecontext.h"

#include "score.h"

void IScore::UpdateThrottling(int ClientID)
{
    m_LastRequest[ClientID] = Server()->TickSpeed();
}

bool IScore::IsThrottled(int ClientID)
{
    return m_LastRequest[ClientID] != -1 && m_LastRequest[ClientID] + Server()->TickSpeed() * 3 > Server()->Tick();
}
