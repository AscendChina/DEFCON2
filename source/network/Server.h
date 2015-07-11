#ifndef SERVER_H
#define SERVER_H

#include "lib/tosser/llist.h"
#include "lib/tosser/darray.h"
#include "lib/tosser/directory.h"


class NetLib;
class NetMutex;
class NetSocketListener;
class ServerToClient;
class ServerToClientLetter;
class ServerTeam;

#define UDP_HEADER_SIZE     32           // 12 bytes for UDP header, 20 bytes for IP header




class Server
{
protected:
    NetLib          *m_netLib;   
    LList           <ServerToClientLetter *> m_history;

protected:
    int             CountEmptyMessages  ( int _startingSeqId );    
    void            AuthenticateClients ();

public:
    int             m_sequenceId;
    int             m_nextClientId;
    
    DArray          <ServerToClient *>  m_clients;
    DArray          <ServerToClient *>  m_disconnectedClients;
    DArray          <ServerTeam *>      m_teams;

    NetMutex        *m_inboxMutex;
    NetMutex        *m_outboxMutex;
    LList           <Directory *> m_inbox;
    LList           <ServerToClientLetter *> m_outbox;
    
    bool            m_syncronised;
    float           m_sendRate;
    float           m_receiveRate;

public:
    Server();
    ~Server();

    bool Initialise			();
    void Shutdown           ();
    bool IsRunning          ();

    Directory *GetNextLetter();

    void ReceiveLetter      ( Directory *update, char *fromIP, int _fromPort );
    void SendLetter         ( ServerToClientLetter *letter );
    
    int  GetClientId        ( char *_ip, int _port );
    int  RegisterNewClient  ( Directory *_client, int _clientId=-1 );
    void RemoveClient       ( int _clientId, int _reason );
    void AuthenticateClient ( int _clientId );
    void SendClientId       ( int _clientId );
    void RegisterNewTeam    ( int _clientId, int _teamType );    
    void RemoveAI           ( Directory *_message );
    void RegisterSpectator  ( int _clientId );
    void ResynchroniseClient( int _clientId );
    
    void NotifyNetSyncError ( int _clientId );
    void NotifyNetSyncFixed ( int _clientId );

    void Advance			();
    void AdvanceSender		();
    void Advertise          ();
    void UpdateClients      ();

    void SendModPath        ();

    int  GetNumTeams        ( int _clientId );
    int  GetNumSpectators   ();
    int  GetNumDemoTeams    ();

    ServerToClient          *GetClient( int _id );
    ServerToClient          *GetDisconnectedClient( int _id );

    bool IsDisconnectedClient       ( char *_ip, int _port );    
    void ForgetDisconnectedClient   ( char *_ip, int _port );
    bool HandleDisconnectedClient   ( Directory *_message );
    bool CanClientRejoin            ( Directory *_message );
    int  HandleClientRejoin         ( Directory *_message );
    void HandleSyncMessage          ( Directory *_message );
    bool CheckForExploits           ( Directory *_message );

    bool GetIdentity        ( char *_ip, int *_port );
    int  GetLocalPort();                                        // Returns local port server is listening on

    bool TestBedReadyToContinue();

    int  GetHistoryByteSize ();
};




// ==========================================================


enum            // Disconnection types
{
    Disconnect_ClientLeave,
    Disconnect_ClientDrop,
    Disconnect_ServerShutdown,
    Disconnect_InvalidKey,
    Disconnect_DuplicateKey,
    Disconnect_KeyAuthFailed,
    Disconnect_BadPassword,
    Disconnect_GameFull,
    Disconnect_KickedFromGame,
    Disconnect_DemoFull,
    Disconnect_ReadyForReconnect
};


// ==========================================================


class ServerToClientLetter
{
public:
    int             m_receiverId;
    bool            m_clientDisconnected;
    Directory       *m_data;

    ServerToClientLetter()
    :   m_receiverId(-1),
        m_data(NULL),
        m_clientDisconnected(false)
    {}

    ~ServerToClientLetter()
    {
        if( m_data ) delete m_data;        
    }
};


// ==========================================================


class ServerTeam
{
public:
    int m_clientId;
    int m_teamType;

    ServerTeam(int _clientId, int _teamType);
};

#endif

