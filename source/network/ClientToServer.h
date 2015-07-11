#ifndef _CLIENTTOSERVER_H
#define _CLIENTTOSERVER_H


#include "lib/tosser/llist.h"
#include "lib/tosser/directory.h"

class NetLib;
class NetSocketListener;
class NetSocketSession;
class NetMutex;
class ChatMessage;


class ClientToServer
{
private:    
    NetLib              *m_netLib;    
    char                *m_serverIp;
    int                 m_serverPort;

public:
    NetSocketListener   *m_listener;
    NetSocketSession    *m_sendSocket; 
    NetMutex            *m_inboxMutex;
    NetMutex            *m_outboxMutex;
    LList               <Directory *> m_inbox;
    LList               <Directory *> m_outbox;

    LList               <ChatMessage *> m_chatSendQueue;
    int                 m_nextChatMessageId;
    float               m_chatResetTimer;

    int                 m_lastValidSequenceIdFromServer;                    // eg if we have 11,12,13,15,18 then this is 13
    int                 m_serverSequenceId;
    float               m_sendRate;
    float               m_receiveRate;
    bool                m_synchronising;                                    // Set to true when the "connecting" window is open
    float               m_resynchronising;
    
    LList               <int> m_outOfSyncClients;
    LList               <int> m_demoClients;

    enum
    {
        StateDisconnected,
        StateConnecting,
        StateHandshaking,
        StateConnected
    };

    int     m_connectionState;
    float   m_retryTimer;
    int     m_connectionAttempts;
    int     m_clientId;
    char    m_password[128];
    char    m_serverVersion[128];

public:
    ClientToServer();
    ~ClientToServer();

    Directory               *GetNextLetter();
    int                      GetNextLetterSeqID();
    
	void Advance				();
	void AdvanceSender	        ();

    void OpenConnections        ();                                 // Close and re-open listeners

    void ReceiveLetter          ( Directory *letter );
    void SendLetter             ( Directory *letter );
    void ProcessServerUpdates   ( Directory *letter );

    bool ClientJoin             ( char *serverIP, int _serverPort );
    void ClientLeave            ( bool _notifyServer );
    bool IsConnected            ();

    void RequestStartGame       ( unsigned char teamId, unsigned char randSeed );
    void RequestTeam            ( int teamType );
    void RequestRemoveAI        ( unsigned char teamId );
    void RequestSpectate        ();
    void RequestAlliance        ( unsigned char teamId, unsigned char allianceId );
    void RequestOptionChange    ( unsigned char optionId, int newValue );
    void RequestOptionChange    ( unsigned char optionId, char *newValue );
    void RequestGameSpeed       ( unsigned char teamId, unsigned char gameSpeed );
    
    void RequestPlacement       ( unsigned char teamId, unsigned char unitType, Fixed longitude, Fixed latitude, unsigned char fleetId = 255 );
    void RequestStateChange     ( int objId, unsigned char state );
    void RequestAction          ( int objId, int targetObjectId, Fixed longitude, Fixed latitude );
    void RequestSpecialAction   ( int objId, int targetObjectId, unsigned char specialActionType );
    void RequestSetWaypoint     ( int objId, Fixed longitude, Fixed latitude );
    void RequestClearActionQueue( int objId );
    void RequestClearLastAction ( int objId );
    void RequestFleet           ( unsigned char teamId );
    void RequestFleetMovement   ( unsigned char teamId, int fleetId, Fixed longitude, Fixed latitude );
    void RequestCeaseFire       ( unsigned char teamId, unsigned char targetTeam );
    void RequestShareRadar      ( unsigned char teamId, unsigned char targetTeam );

    void RequestSetTeamName     ( unsigned char teamId, char *teamName, bool spectator=false );
    void RequestTerritory       ( unsigned char teamId, unsigned char territoryId, int addOrRemove=0 );               // -1=remove, 0=toggle, 1=add
    void RequestAggressionChange( unsigned char teamId, unsigned char value );

	void RequestWhiteBoard      ( unsigned char teamId, unsigned char action, int pointId = -1, Fixed longitude = 0, Fixed latitude = 0, Fixed longitude2 = 0, Fixed latitude2 = 0 );

	void SendTeamScore          ( unsigned char teamId, int teamScore );

    void BeginVote              ( unsigned char teamId, unsigned char voteType, unsigned char voteData );
    void CastVote               ( unsigned char teamId, unsigned char voteId, unsigned char myVote );

    void SendChatMessage        ( unsigned char teamId, unsigned char channel, char *msg, int messageId, bool spectator=false );
    void SendChatMessageReliably( unsigned char teamId, unsigned char channel, char *msg, bool spectator );
    void ReceiveChatMessage     ( unsigned char teamId, int messageId );

    void SendSyncronisation     ( int _lastProcessedId, unsigned char _sync );
    void SetSyncState           ( int _clientId, bool _synchronised );
    bool IsSynchronised         ( int _clientId );
    void Resynchronise          ();

    void SetClientDemo          ( int _clientId );
    bool IsClientDemo           ( int _clientId );
    bool AmIDemoClient          ();
    void GetDemoLimitations     ( int &_maxGameSize, int &_maxDemoPlayers, bool &_allowDemoServers );            // Uses defaults if not yet known
    void GetPurchasePrice       ( char *_prices );
    void GetPurchaseUrl         ( char *_purchaseUrl );

    int     GetEstimatedServerSeqId();
    float   GetEstimatedLatency();
    bool    IsSequenceIdInQueue( int _seqId );
    int     CountPacketLoss();

    void StartIdentifying   ();
    void StopIdentifying    ();
    bool GetIdentity        ( char *_ip, int *_port );
    bool GetServerDetails   ( char *_ip, int *_port );   
    int  GetLocalPort();                                        // Returns local port client is listening on
};


#endif
