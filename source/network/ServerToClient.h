/* Represents one connection to one client. Server is expected to have a list of these. */


#ifndef _SERVERTOCLIENT_H
#define _SERVERTOCLIENT_H

class NetSocketSession;
class NetSocketListener;
class ServerToClientLetter;

#include "lib/tosser/darray.h"




class ServerToClient
{
public:
    char                m_ip[16];
    int                 m_port;
    int                 m_clientId;

    char                m_version[256];
    char                m_system[256];
    char                m_authKey[256];
    int                 m_authKeyId;
    char                m_password[128];
    int                 m_basicAuthCheck;                   // 0=not done, 1=pass, -1=basicfail, -2=duplicate, -3=keybad, -4=badpass, -5=full, -6=game started
    int                 m_disconnected;                     // Can be Disconnect_ClientLeave etc
    bool                m_spectator;
    
    int                 m_lastKnownSequenceId;
    int                 m_lastSentSequenceId;
    double              m_lastMessageReceived;
    bool                m_caughtUp;
    float               m_lastBackedUp;
    int                 m_syncErrorSeqId;                   // SeqID where we got out of sync, or -1 if in sync

    DArray<bool>            m_chatMessages;
    DArray<unsigned char>   m_sync;

protected:
    NetSocketSession   *m_socket;
    
public:
    ServerToClient( char *_ip, int _port, 
                    NetSocketListener *_listener );

    ~ServerToClient();
    
    char                *GetIP ();
    NetSocketSession    *GetSocket ();
};



#endif

