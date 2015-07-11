#ifndef _included_gamehistory_h
#define _included_gamehistory_h

/*
 *  Simple class designed to remember all the servers
 *  you've connected to recently.
 *
 * 
 */

#include "lib/tosser/llist.h"


// ============================================================================


struct GameHistoryServer
{
public:
    char m_identity[256];
    char m_name[256];
};



// ============================================================================


class GameHistory
{
public:
    LList<GameHistoryServer *> m_servers;

public:
    GameHistory();
    
    void JoinedGame( char *_ip, int _port, const char *_name );

    int HasJoinedGame( char *_ip, int _port );                  // 0 = no, > 0 = yes, 1=recent, 10=not recent

    void Save();
    void Load();
};



extern GameHistory g_gameHistory;


#endif
