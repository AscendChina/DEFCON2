#include "lib/universal_include.h"

#include "lib/netlib/net_lib.h"
#include "lib/netlib/net_udp_packet.h"
#include "lib/netlib/net_socket_listener.h"
#include "lib/netlib/net_mutex.h"
#include "lib/netlib/net_thread.h"
#include "lib/netlib/net_socket.h"
#include "lib/metaserver/matchmaker.h"
#include "lib/metaserver/metaserver_defines.h"
#include "lib/metaserver/authentication.h"
#include "lib/metaserver/metaserver.h"
#include "lib/hi_res_time.h"
#include "lib/gucci/input.h"
#include "lib/debug_utils.h"
#include "lib/preferences.h"
#include "lib/string_utils.h"
#include "lib/language_table.h"

#include "app/app.h"
#include "app/globals.h"
#include "app/game.h"
#include "app/version_manager.h"

#include "network/Server.h"
#include "network/ClientToServer.h"
#include "network/network_defines.h"

#include "world/world.h"


// ***ListenCallback
static NetCallBackRetType ListenCallback(NetUdpPacket *udpdata)
{
    static int s_bytesReceived = 0;
    static float s_timer = 0.0f;
    static float s_interval = 5.0f;

    if (udpdata)
    {
        NetIpAddress fromAddr = udpdata->m_clientAddress;
        char newip[16];
		IpAddressToStr( fromAddr, newip );

        Directory *letter = new Directory();
        bool success = letter->Read(udpdata->m_data, udpdata->m_length);

        if( success )
        {
            g_app->GetClientToServer()->ReceiveLetter( letter );
        }
        else
        {
            AppDebugOut( "Client received bogus data, discarded (1)\n" );
            delete letter;
        }

        s_bytesReceived += udpdata->m_length;
        s_bytesReceived += UDP_HEADER_SIZE;

        delete udpdata;
    }

    float timeNow = GetHighResTime();
    if( timeNow > s_timer + s_interval )
    {
        g_app->GetClientToServer()->m_receiveRate = s_bytesReceived / s_interval;
        s_timer = timeNow;
        s_bytesReceived = 0;
    }

    return 0;
}

// ***ListenThread
static NetCallBackRetType ListenThread(void *ignored)
{   
    NetRetCode retCode = g_app->GetClientToServer()->m_listener->StartListening( ListenCallback );
    AppAssert( retCode == NetOk );
    return 0;
}


ClientToServer::ClientToServer()
:   m_sendSocket(NULL),
    m_retryTimer(0.0f),
    m_clientId(-1),
    m_sendRate(0.0f),
    m_receiveRate(0.0f),
    m_serverIp(NULL),
    m_serverPort(-1),
    m_nextChatMessageId(0),
    m_chatResetTimer(0.0f),
    m_resynchronising(-1.0f),
    m_synchronising(false),
    m_connectionAttempts(0),
    m_listener(NULL)
{
    m_lastValidSequenceIdFromServer = -1;
    m_serverSequenceId = -1;
    m_connectionState = StateDisconnected;

    m_inboxMutex = new NetMutex();
    m_outboxMutex = new NetMutex();

    m_password[0] = '\x0';
    strcpy( m_serverVersion, "1.0" );

    m_netLib = new NetLib();
    m_netLib->Initialise ();
}


ClientToServer::~ClientToServer()
{
    while( m_outbox.Size() > 0 ) {}
}


int ClientToServer::GetLocalPort()
{
    if( !m_listener ) return -1;

    return m_listener->GetPort();
}


void ClientToServer::OpenConnections()
{
    if( m_listener )
    {
        m_listener->StopListening();
        //delete m_listener;
        m_listener = NULL;
    }
    

    //
    // Try the requested port number

    int port = g_preferences->GetInt( PREFS_NETWORKCLIENTPORT );
    m_listener = new NetSocketListener( port );
    NetRetCode result = m_listener->Bind();

    
    //
    // If that failed, try port 0

    if( result != NetOk && port != 0 )
    {
        AppDebugOut( "Client failed to bind to port %d\n", port );
        AppDebugOut( "Client looking for any available port number...\n" );
        delete m_listener;
        m_listener = new NetSocketListener(0);
        result = m_listener->Bind();
    }


    //
    // If it still failed, bail out
    // (we're in trouble)

    if( result != NetOk )
    {
        AppDebugOut( "ERROR : Client failed to bind to open Listener\n" );
        delete m_listener;
        m_listener = NULL;
        return;
    }


    //
    // All ok

    NetStartThread( ListenThread );
    AppDebugOut( "Client listening on port %d\n", GetLocalPort() );        
}



// *** AdvanceSender
void ClientToServer::AdvanceSender()
{
    static int s_bytesSent = 0;
    static float s_timer = 0;
    static float s_interval = 5.0f;
 
    g_app->GetClientToServer()->m_outboxMutex->Lock();
    
    while (g_app->GetClientToServer()->m_outbox.Size())
    {
        Directory *letter = g_app->GetClientToServer()->m_outbox[0];
        AppAssert(letter);
        
        letter->CreateData( NET_DEFCON_LASTSEQID, m_lastValidSequenceIdFromServer );

        if( m_connectionState > StateDisconnected )
        {
            int letterSize = 0;
            char *byteStream = letter->Write(letterSize);
            NetSocketSession *socket = g_app->GetClientToServer()->m_sendSocket;
            int writtenData = 0;
            NetRetCode result = socket->WriteData( byteStream, letterSize, &writtenData );
            
            if( result != NetOk )           AppDebugOut("CLIENT write data bad result %d\n", (int) result );
            if( writtenData != 0 &&
                writtenData < letterSize )  AppDebugOut("CLIENT wrote less data than requested\n" );
            
            s_bytesSent += letterSize;
            s_bytesSent += UDP_HEADER_SIZE;
            
            delete letter;
            delete byteStream;            
        }
        g_app->GetClientToServer()->m_outbox.RemoveData(0);
    }

    g_app->GetClientToServer()->m_outboxMutex->Unlock();

    
    float timeNow = GetHighResTime();
    if( timeNow > s_timer + s_interval )
    {
        m_sendRate = s_bytesSent / s_interval;
        s_bytesSent = 0;
        s_timer = timeNow;
    }
}


// *** Advance
void ClientToServer::Advance()
{
    //
    // Set our basic Client properties

    char authKey[256];
    Authentication_GetKey( authKey );
    Directory clientProperties;
    clientProperties.CreateData( NET_METASERVER_AUTHKEY, authKey );
    clientProperties.CreateData( NET_METASERVER_GAMENAME, APP_NAME );
    clientProperties.CreateData( NET_METASERVER_GAMEVERSION, APP_VERSION );
    MetaServer_SetClientProperties( &clientProperties );


    //
    // If we are disconnected burn all messages in the inbox

    if( m_connectionState == StateDisconnected )
    {
        m_inboxMutex->Lock();
        m_inbox.EmptyAndDelete();
        m_inboxMutex->Unlock();
    }


    //
    // Auto-retry to connect to the server until successful

    if( m_connectionState == StateConnecting )
    {
        float timeNow = GetHighResTime();
        if( timeNow > m_retryTimer )
        {
            //
            // Attempt to hole punch

            char ourIp[256];
            int ourPort = -1;
            bool identityKnown = GetIdentity( ourIp, &ourPort );

            if( identityKnown )
            {
                char authKey[256];
                Authentication_GetKey( authKey );

                AppAssert( m_serverIp );
                Directory ourDetails;
                
                ourDetails.CreateData( NET_METASERVER_GAMENAME, APP_NAME );
                ourDetails.CreateData( NET_METASERVER_GAMEVERSION, APP_VERSION );
                ourDetails.CreateData( NET_METASERVER_AUTHKEY, authKey );
                ourDetails.CreateData( NET_METASERVER_IP, ourIp );
                ourDetails.CreateData( NET_METASERVER_PORT, ourPort );

                MatchMaker_RequestConnection( m_serverIp, m_serverPort, &ourDetails );
            }


            //
            // Attempt to directly connect

            Directory *letter = new Directory();
            letter->CreateData( NET_DEFCON_COMMAND, NET_DEFCON_CLIENT_JOIN );
            letter->CreateData( NET_METASERVER_GAMEVERSION, APP_VERSION );
            letter->CreateData( NET_DEFCON_SYSTEMTYPE, APP_SYSTEM );

            char authKey[256];
            Authentication_GetKey(authKey);
            bool isDemoKey = Authentication_IsDemoKey(authKey);

            if( g_app->GetServer() || isDemoKey )
            {
                letter->CreateData( NET_METASERVER_AUTHKEY, authKey );
            }
            else
            {
                char authKeyHash[256];
                Authentication_GetKeyHash( authKeyHash );
                int keyId = Authentication_GetKeyId(authKey);

                letter->CreateData( NET_METASERVER_AUTHKEY, authKeyHash );
                letter->CreateData( NET_METASERVER_AUTHKEYID, keyId );
            }            
            
            if( strlen(m_password) > 0 )
            {
                letter->CreateData( NET_METASERVER_PASSWORD, m_password );
            }

            SendLetter( letter );
            m_retryTimer = timeNow + 1.0f;
            m_connectionAttempts++;
        }
    }

    
    //
    // Send our desired team name 

    if( m_connectionState == StateConnected )
    {
        float timeNow = GetHighResTime();
        if( timeNow > m_retryTimer )
        {
            m_retryTimer = timeNow + 1.0f;

            if( GetEstimatedLatency() < 10.0f )
            {
                char *requestedTeamName = g_preferences->GetString( "PlayerName" );
				if( g_languageTable->DoesPhraseExist( "gameoption_PlayerName" ) && 
					strcmp( requestedTeamName, g_languageTable->LookupBasePhrase( "gameoption_PlayerName" ) ) == 0 )
				{
					if( g_languageTable->DoesTranslationExist( "gameoption_PlayerName" ) )
					{
						requestedTeamName = LANGUAGEPHRASE("gameoption_PlayerName");
					}
				}
				requestedTeamName = newStr( requestedTeamName );
                SafetyString( requestedTeamName );
                ReplaceExtendedCharacters( requestedTeamName );

                int specIndex = g_app->GetWorld()->IsSpectating( m_clientId );

                if( specIndex != -1 )
                {
                    Spectator *spec = g_app->GetWorld()->m_spectators[specIndex];
                    if( strcmp(spec->m_name, requestedTeamName) != 0 )
                    {
                        RequestSetTeamName( m_clientId, requestedTeamName, true );            
                    }
                }
                else
                {
                    Team *team = g_app->GetWorld()->GetMyTeam();
                    if( team &&
                        team->m_type == Team::TypeLocalPlayer &&
                        strcmp(team->m_name, requestedTeamName) != 0 )
                    {
                        RequestSetTeamName( team->m_teamId, requestedTeamName, false );            
                    }
                }

				delete [] requestedTeamName;
            }
        }
    }



    //
    // Re-send chat messages

    if( m_connectionState == StateConnected )
    {
        if( m_chatSendQueue.Size() && GetHighResTime() > m_chatResetTimer )
        {
            ChatMessage *msg = m_chatSendQueue[0];
            SendChatMessage( msg->m_fromTeamId, 
                             msg->m_channel, 
                             msg->m_message, 
                             msg->m_messageId, 
                             msg->m_spectator );
            m_chatResetTimer = GetHighResTime() + 0.5f;
        }
    }
        

    //
    // Send resynchronise requests if it hasn't happened yet

    if( m_resynchronising > 0.0f )
    {
        float timeNow = GetHighResTime();
        if( timeNow > m_resynchronising )
        {
            AppDebugOut( "Client requesting Resynchronisation...\n" );

            Directory *letter = new Directory();
            letter->CreateData( NET_DEFCON_COMMAND, NET_DEFCON_RESYNCHRONISE );
            SendLetter( letter );

            m_resynchronising = timeNow + 1.0f;
        }
    }


    //
    // Advance the sender

	AdvanceSender();
}


void ClientToServer::SendChatMessageReliably( unsigned char teamId, unsigned char channel, char *msg, bool spectator )
{
    int messageId = m_nextChatMessageId;
    ++m_nextChatMessageId;


    //
    // Put the message into our send queue

    ChatMessage *message = new ChatMessage();
    message->m_fromTeamId = teamId;
    message->m_channel = channel;
    message->m_messageId = messageId;
    message->m_spectator = spectator;
    message->m_message = newStr(msg);
    m_chatSendQueue.PutDataAtEnd( message );

    m_chatResetTimer = GetHighResTime() + 0.5f;
    

    //
    // Send it right now

    g_app->GetClientToServer()->SendChatMessage( teamId, channel, msg, messageId, spectator );
}


void ClientToServer::ReceiveChatMessage( unsigned char teamId, int messageId )
{
    //
    // Remove the chat message from the send queue

    for( int i = 0; i < m_chatSendQueue.Size(); ++i )
    {
        ChatMessage *thisChat = m_chatSendQueue[i];
        if( thisChat->m_fromTeamId == teamId &&
            thisChat->m_messageId == messageId )
        {
            m_chatSendQueue.RemoveData(i);
            delete thisChat;
            --i;
        }
    }
}


int ClientToServer::GetNextLetterSeqID()
{
    int result = -1;

    m_inboxMutex->Lock();
    if( m_inbox.Size() > 0 )
    {
        result = m_inbox[0]->GetDataInt( NET_DEFCON_SEQID );
    }    
    m_inboxMutex->Unlock();

    return result;
}


Directory *ClientToServer::GetNextLetter()
{
    m_inboxMutex->Lock();
    Directory *letter = NULL;
    
    if( m_inbox.Size() > 0 )
    {
        letter = m_inbox[0];
        int seqId = letter->GetDataInt( NET_DEFCON_SEQID );
        
        if( seqId == g_lastProcessedSequenceId+1 ||
            seqId == -1 )
        {
            // This is the next letter, remove from list
            // (assume its now dealt with)
            m_inbox.RemoveData(0);
        }
        else if( seqId < g_lastProcessedSequenceId+1 )
        {
            // This is a duplicate old letter, throw it away
            m_inbox.RemoveData(0);
            letter = NULL;
        }
        else
        {
            // We're not ready to read this letter yet
            letter = NULL;
        }
    }

    m_inboxMutex->Unlock();

    return letter;
}


int ClientToServer::GetEstimatedServerSeqId()
{    
    float timeFactor = g_app->m_gameRunning ? 
                            SERVER_ADVANCE_PERIOD.DoubleValue() : 
                            SERVER_ADVANCE_PERIOD.DoubleValue() * 5.0f;

    float timePassed = GetHighResTime() - g_startTime;

    int estimatedSequenceId = timePassed / timeFactor;
    
    estimatedSequenceId = max( estimatedSequenceId, m_serverSequenceId );
    
    return estimatedSequenceId;
}


float ClientToServer::GetEstimatedLatency()
{
    int estimatedServerSeqId = GetEstimatedServerSeqId();

    float seqIdDifference = estimatedServerSeqId - g_lastProcessedSequenceId;

    seqIdDifference = max( seqIdDifference, 0.0f );

    float timeFactor = g_app->m_gameRunning ? 
                            SERVER_ADVANCE_PERIOD.DoubleValue() : 
                            SERVER_ADVANCE_PERIOD.DoubleValue() * 5.0f;


    float estimatedLatency = seqIdDifference * timeFactor;

    return estimatedLatency;
}


void ClientToServer::ReceiveLetter( Directory *letter )
{
    //
    // Simulate network packet loss
    
#ifdef _DEBUG
    if( g_keys[KEY_F] )
    {    
        delete letter;
        return;
    }
#endif


    //
    // Is this part of the MatchMaker service?

    if( strcmp( letter->m_name, NET_MATCHMAKER_MESSAGE ) == 0 )
    {
        MatchMaker_ReceiveMessage( m_listener, letter );
        return;
    }


    //
    // If we are disconnected, chuck this message in the bin

    if( m_connectionState == StateDisconnected )
    {
        delete letter;
        return;
    }


    //
    // Ensure the message looks valid

    if( strcmp( letter->m_name, NET_DEFCON_MESSAGE ) != 0 )
    {
        AppDebugOut( "Client received bogus data, discarded (2)\n" );
        delete letter;
        return;
    }
    

    //
    // If there is a previous update included in this letter,
    // take it out and deal with it now

    Directory *prevUpdate = letter->GetDirectory( NET_DEFCON_PREVUPDATE );
    if( prevUpdate )
    {
        letter->RemoveDirectory( NET_DEFCON_PREVUPDATE );
        prevUpdate->SetName( NET_DEFCON_MESSAGE );
        ReceiveLetter( prevUpdate );
    }


    //
    // Record how far behind the server we are

    if( letter->HasData(NET_DEFCON_LASTSEQID, DIRECTORY_TYPE_INT) )
    {
        int serverSeqId = letter->GetDataInt( NET_DEFCON_LASTSEQID );
        m_serverSequenceId = max( m_serverSequenceId, serverSeqId );
    }

    
    //
    // If this letter is just for us put it to the front of the queue
        
    if( !letter->HasData( NET_DEFCON_SEQID, DIRECTORY_TYPE_INT ) )
    {
        AppDebugOut( "Client received bogus data, discarded (3)\n" );
        delete letter;
        return;
    }
    

    int seqId = letter->GetDataInt( NET_DEFCON_SEQID );
    if( seqId == -1 )
    {
        m_inboxMutex->Lock();
        m_inbox.PutDataAtStart( letter );
        m_inboxMutex->Unlock();
        return;
    }


    //
    // Check for duplicates
    // Throw away stuff if we're not supposed to be connected
    
    if( seqId <= m_lastValidSequenceIdFromServer ||
        m_connectionState == StateDisconnected )
    {
        delete letter;
        return;
    }


    //
    // Work out our start time

    double newStartTime = GetHighResTime() - (float)seqId * SERVER_ADVANCE_PERIOD.DoubleValue();
    if( !g_app->m_gameRunning ) newStartTime = GetHighResTime() - (float)seqId * SERVER_ADVANCE_PERIOD.DoubleValue() * 5.0f;
    if( newStartTime < g_startTime ||
        newStartTime > g_startTime + 0.1f ) 
    {
        g_startTime = newStartTime;     
    }


    //
    // Do a sorted insert of the letter into the inbox

    m_inboxMutex->Lock();

    bool inserted = false;
    for( int i = m_inbox.Size()-1; i >= 0; --i )
    {
        Directory *thisLetter = m_inbox[i];
        int thisSeqId = thisLetter->GetDataInt(NET_DEFCON_SEQID);
        if( seqId > thisSeqId )
        {
            m_inbox.PutDataAtIndex( letter, i+1 );
            inserted = true;
            break;
        }
        else if( seqId == thisSeqId )
        {
            // Throw this letter away, it's a duplicate
            delete letter;
            inserted = true;
            break;
        }
    }
    if( !inserted )
    {
        m_inbox.PutDataAtStart( letter );
    }


    //
    // Recalculate our last Known Sequence Id

    for( int i = 0; i < m_inbox.Size(); ++i )
    {
        Directory *thisLetter = m_inbox[i];
        int thisSeqId = thisLetter->GetDataInt( NET_DEFCON_SEQID );
        if( thisSeqId > m_lastValidSequenceIdFromServer+1 )
        {
            break;
        }
        m_lastValidSequenceIdFromServer = max( m_lastValidSequenceIdFromServer, thisSeqId );

        if( thisLetter->m_subDirectories.Size() == 0 &&
            thisLetter->HasData( NET_DEFCON_NUMEMPTYUPDATES, DIRECTORY_TYPE_INT ) )
        {
            int numEmptyUpdates = thisLetter->GetDataInt( NET_DEFCON_NUMEMPTYUPDATES );
            AppAssert( numEmptyUpdates != -1 );
            m_lastValidSequenceIdFromServer = max( m_lastValidSequenceIdFromServer, 
                                                   thisSeqId + numEmptyUpdates - 1 );
        }
    }

    m_inboxMutex->Unlock();
}

void ClientToServer::SendLetter( Directory *letter )
{
    if( letter )
    {
        AppAssert( letter->HasData(NET_DEFCON_COMMAND,DIRECTORY_TYPE_STRING) );

        letter->SetName     ( NET_DEFCON_MESSAGE );

        m_outboxMutex->Lock();
        m_outbox.PutDataAtEnd( letter );
        m_outboxMutex->Unlock();
    }
}


bool ClientToServer::IsSequenceIdInQueue( int _seqId )
{
    bool found = false;

    m_inboxMutex->Lock();
    for( int i = 0; i < m_inbox.Size(); ++i )
    {
        Directory *thisLetter = m_inbox[i];
        int thisSeqId = thisLetter->GetDataInt( NET_DEFCON_SEQID );
        if( thisSeqId == _seqId )
        {
            found = true;
            break;
        }

        if( thisLetter->m_subDirectories.Size() == 0 &&
            thisLetter->HasData( NET_DEFCON_NUMEMPTYUPDATES, DIRECTORY_TYPE_INT ) )
        {
            int numEmptyUpdates = thisLetter->GetDataInt( NET_DEFCON_NUMEMPTYUPDATES );
            if( _seqId >= thisSeqId &&
                _seqId < thisSeqId + numEmptyUpdates )
            {
                found = true;
                break;
            }
        }
    }
    m_inboxMutex->Unlock();

    return found;
}


int ClientToServer::CountPacketLoss()
{
    int packetLoss = 0;
    int lastSeqId = g_lastProcessedSequenceId;

    m_inboxMutex->Lock();
    for( int i = 0; i < m_inbox.Size(); ++i )
    {
        Directory *thisLetter = m_inbox[i];
        int thisSeqId = thisLetter->GetDataInt( NET_DEFCON_SEQID );

        if( thisSeqId > lastSeqId + 1 )
        {
            ++packetLoss;
        }

        lastSeqId = thisSeqId;
    }
    m_inboxMutex->Unlock();

    return packetLoss;
}


bool ClientToServer::ClientJoin( char *ip, int _serverPort )
{
    AppDebugOut( "CLIENT : Attempting connection to Server at %s:%d...\n", ip, _serverPort );
    
    if( !m_listener )
    {
        AppDebugOut( "CLIENT: Can't join because Listener isn't open\n" );
        return false;
    }

    m_serverIp = strdup(ip);
    m_serverPort = _serverPort;
    m_lastValidSequenceIdFromServer = -1;
    m_serverSequenceId = -1;
    m_connectionAttempts = 0;

    m_sendSocket = new NetSocketSession( *m_listener, ip, _serverPort );	
    m_connectionState = StateConnecting;
    m_retryTimer = 0.0f;


    return true;
}


void ClientToServer::ClientLeave( bool _notifyServer )
{
    if( _notifyServer )
    {
        AppDebugOut( "CLIENT : Sending disconnect...\n" );

        Directory *letter = new Directory();
        letter->CreateData( NET_DEFCON_COMMAND, NET_DEFCON_CLIENT_LEAVE );
        SendLetter( letter );

        AdvanceSender();
    }
    
    NetSleep(100);

    delete m_sendSocket;
    m_sendSocket = NULL;

    delete m_serverIp;
    m_serverIp = NULL;
    m_serverPort = -1;

    m_chatSendQueue.EmptyAndDelete();
    m_nextChatMessageId = 0;
    m_chatResetTimer = 0.0f;
    
    m_clientId = -1;
    m_lastValidSequenceIdFromServer = -1;
    m_serverSequenceId = -1;
    m_connectionState = StateDisconnected;
    
    m_outOfSyncClients.Empty();
    m_demoClients.Empty();

    m_inboxMutex->Lock();
    m_inbox.EmptyAndDelete();
    m_inboxMutex->Unlock();

    g_lastProcessedSequenceId = -2;

    AppDebugOut( "CLIENT : Disconnected\n" );
}

void ClientToServer::RequestStartGame( unsigned char teamId, unsigned char randSeed )
{
    Directory *letter = new Directory();

    letter->CreateData( NET_DEFCON_COMMAND,     NET_DEFCON_START_GAME );
    letter->CreateData( NET_DEFCON_TEAMID,      teamId );
    letter->CreateData( NET_DEFCON_RANDSEED,    randSeed );
    
    SendLetter( letter );
}

bool ClientToServer::IsConnected()
{    
    return( m_connectionState == StateHandshaking ||
            m_connectionState == StateConnected );
}

void ClientToServer::RequestTeam( int teamType )
{
    Directory *letter = new Directory();
    
    letter->CreateData( NET_DEFCON_COMMAND,     NET_DEFCON_REQUEST_TEAM );
    letter->CreateData( NET_DEFCON_TEAMTYPE,    teamType );
    
    SendLetter( letter );
}

void ClientToServer::RequestSpectate()
{
    Directory *letter = new Directory();
    
    letter->CreateData( NET_DEFCON_COMMAND, NET_DEFCON_REQUEST_SPECTATE );
    
    SendLetter( letter );
}

void ClientToServer::RequestRemoveAI( unsigned char teamId )
{
    Directory *letter = new Directory();

    letter->CreateData( NET_DEFCON_COMMAND, NET_DEFCON_REMOVEAI );
    letter->CreateData( NET_DEFCON_TEAMID, teamId );

    SendLetter( letter );
}

void ClientToServer::RequestAlliance( unsigned char teamId, unsigned char allianceId )
{
    Directory *letter = new Directory();

    letter->CreateData( NET_DEFCON_COMMAND,     NET_DEFCON_REQUEST_ALLIANCE );
    letter->CreateData( NET_DEFCON_TEAMID,      teamId );
    letter->CreateData( NET_DEFCON_ALLIANCEID,  allianceId );

    SendLetter( letter );
}


void ClientToServer::RequestStateChange ( int objId, unsigned char state )
{
    Directory *letter = new Directory();

    letter->CreateData( NET_DEFCON_COMMAND,     NET_DEFCON_OBJSTATECHANGE );
    letter->CreateData( NET_DEFCON_OBJECTID,    objId );
    letter->CreateData( NET_DEFCON_STATE,       state );

    SendLetter( letter );
}

void ClientToServer::RequestAction( int objId, int targetObjectId, Fixed longitude, Fixed latitude )
{
    Directory *letter = new Directory();

    letter->CreateData( NET_DEFCON_COMMAND,         NET_DEFCON_OBJACTION );
    letter->CreateData( NET_DEFCON_OBJECTID,        objId );
    letter->CreateData( NET_DEFCON_TARGETOBJECTID,  targetObjectId );
    letter->CreateData( NET_DEFCON_LONGITUDE,       longitude );
    letter->CreateData( NET_DEFCON_LATTITUDE,       latitude );

    SendLetter( letter );
}


void ClientToServer::RequestPlacement( unsigned char teamId, unsigned char unitType, Fixed longitude, Fixed latitude, unsigned char fleetId )
{
    Directory *letter = new Directory();
    
    letter->CreateData( NET_DEFCON_COMMAND,     NET_DEFCON_OBJPLACEMENT );
    letter->CreateData( NET_DEFCON_TEAMID,      teamId );
    letter->CreateData( NET_DEFCON_UNITTYPE,    unitType );
    letter->CreateData( NET_DEFCON_LONGITUDE,   longitude );
    letter->CreateData( NET_DEFCON_LATTITUDE,   latitude );
    letter->CreateData( NET_DEFCON_FLEETID,     fleetId );

    SendLetter( letter );
}


void ClientToServer::RequestSpecialAction( int objId, int targetObjectId, unsigned char specialActionType )
{
    Directory *letter = new Directory();
    
    letter->CreateData( NET_DEFCON_COMMAND,         NET_DEFCON_OBJSPECIALACTION );
    letter->CreateData( NET_DEFCON_OBJECTID,        objId );
    letter->CreateData( NET_DEFCON_TARGETOBJECTID,  targetObjectId );
    letter->CreateData( NET_DEFCON_ACTIONTYPE,      specialActionType );

    SendLetter( letter );
}


void ClientToServer::RequestSetWaypoint( int objId, Fixed longitude, Fixed latitude )
{
    Directory *letter = new Directory();

    letter->CreateData( NET_DEFCON_COMMAND,     NET_DEFCON_OBJSETWAYPOINT );
    letter->CreateData( NET_DEFCON_OBJECTID,    objId );
    letter->CreateData( NET_DEFCON_LONGITUDE,   longitude );
    letter->CreateData( NET_DEFCON_LATTITUDE,   latitude );

    SendLetter( letter );
}


void ClientToServer::RequestClearActionQueue( int objId )
{
    Directory *letter = new Directory();

    letter->CreateData( NET_DEFCON_COMMAND,     NET_DEFCON_OBJCLEARACTIONQUEUE );
    letter->CreateData( NET_DEFCON_OBJECTID,    objId );

    SendLetter( letter );
}

void ClientToServer::RequestClearLastAction( int objId )
{
    Directory *letter = new Directory();

    letter->CreateData( NET_DEFCON_COMMAND,     NET_DEFCON_OBJCLEARLASTACTION );
    letter->CreateData( NET_DEFCON_OBJECTID,    objId );

    SendLetter( letter );
}

void ClientToServer::RequestOptionChange( unsigned char optionId, int newValue )
{
    Directory *letter = new Directory();

    letter->CreateData( NET_DEFCON_COMMAND,         NET_DEFCON_CHANGEOPTION );
    letter->CreateData( NET_DEFCON_OPTIONID,        optionId );
    letter->CreateData( NET_DEFCON_OPTIONVALUE,     newValue );

    SendLetter( letter );
}


void ClientToServer::RequestOptionChange( unsigned char optionId, char *newValue )
{
    Directory *letter = new Directory();

    letter->CreateData( NET_DEFCON_COMMAND,         NET_DEFCON_CHANGEOPTION );
    letter->CreateData( NET_DEFCON_OPTIONID,        optionId );
    letter->CreateData( NET_DEFCON_OPTIONVALUE,     newValue );
    
    SendLetter( letter );
}


void ClientToServer::RequestGameSpeed( unsigned char teamId, unsigned char gameSpeed )
{
    Directory *letter = new Directory();

    letter->CreateData( NET_DEFCON_COMMAND,         NET_DEFCON_SETGAMESPEED );
    letter->CreateData( NET_DEFCON_TEAMID,          teamId );
    letter->CreateData( NET_DEFCON_OPTIONVALUE,     gameSpeed );

    SendLetter( letter );
}


void ClientToServer::RequestTerritory( unsigned char teamId, unsigned char territoryId, int addOrRemove )
{
    Directory *letter = new Directory();

    letter->CreateData( NET_DEFCON_COMMAND,         NET_DEFCON_REQUEST_TERRITORY );
    letter->CreateData( NET_DEFCON_TEAMID,          teamId );
    letter->CreateData( NET_DEFCON_TERRITORYID,     territoryId );
    letter->CreateData( NET_DEFCON_OPTIONVALUE,     addOrRemove );

    SendLetter( letter );
}

void ClientToServer::RequestFleet( unsigned char teamId )
{
    Directory *letter = new Directory();

    letter->CreateData( NET_DEFCON_COMMAND,     NET_DEFCON_REQUEST_FLEET );
    letter->CreateData( NET_DEFCON_TEAMID,      teamId );

    SendLetter( letter );
}

void ClientToServer::RequestFleetMovement( unsigned char teamId, int fleetId, Fixed longitude, Fixed latitude )
{    
    Directory *letter = new Directory();

    letter->CreateData( NET_DEFCON_COMMAND,     NET_DEFCON_FLEETMOVE );
    letter->CreateData( NET_DEFCON_TEAMID,      teamId );
    letter->CreateData( NET_DEFCON_FLEETID,     fleetId );
    letter->CreateData( NET_DEFCON_LONGITUDE,   longitude );
    letter->CreateData( NET_DEFCON_LATTITUDE,   latitude );

    SendLetter( letter );
}


void ClientToServer::RequestSetTeamName( unsigned char teamId, char *teamName, bool spectator )
{
    Directory *letter = new Directory();

    letter->CreateData( NET_DEFCON_COMMAND,     NET_DEFCON_SETTEAMNAME );
    letter->CreateData( NET_DEFCON_TEAMID,      teamId );
    letter->CreateData( NET_DEFCON_OPTIONVALUE, teamName );

    if( spectator )
    {
        letter->CreateData( NET_DEFCON_SPECTATOR, 1 );
    }

    SendLetter(letter);
}

void ClientToServer::RequestCeaseFire( unsigned char teamId, unsigned char targetTeam )
{
    Directory *letter = new Directory();

    letter->CreateData( NET_DEFCON_COMMAND,         NET_DEFCON_CEASEFIRE );
    letter->CreateData( NET_DEFCON_TEAMID,          teamId );
    letter->CreateData( NET_DEFCON_TARGETTEAMID,    targetTeam );

    SendLetter( letter );
}

void ClientToServer::RequestShareRadar( unsigned char teamId, unsigned char targetTeam )
{
    Directory *letter = new Directory();

    letter->CreateData( NET_DEFCON_COMMAND,         NET_DEFCON_SHARERADAR );
    letter->CreateData( NET_DEFCON_TEAMID,          teamId );
    letter->CreateData( NET_DEFCON_TARGETTEAMID,    targetTeam );

    SendLetter( letter );
}

void ClientToServer::RequestAggressionChange( unsigned char teamId, unsigned char value )
{
    Directory *letter = new Directory();

    letter->CreateData( NET_DEFCON_COMMAND,         NET_DEFCON_AGGRESSIONCHANGE );
    letter->CreateData( NET_DEFCON_TEAMID,          teamId );
    letter->CreateData( NET_DEFCON_OPTIONVALUE,     value );

    SendLetter( letter );
}

void ClientToServer::SendChatMessage( unsigned char teamId, unsigned char channel, char *msg, int messageId, bool spectator )
{
    Directory *letter = new Directory();

    letter->CreateData( NET_DEFCON_COMMAND,         NET_DEFCON_CHATMESSAGE );
    letter->CreateData( NET_DEFCON_TEAMID,          teamId );
    letter->CreateData( NET_DEFCON_CHATCHANNEL,     channel );
    letter->CreateData( NET_DEFCON_CHATMSG,         msg );
    letter->CreateData( NET_DEFCON_CHATMSGID,       messageId );

    if( spectator )
    {
        letter->CreateData( NET_DEFCON_SPECTATOR, 1 );
    }

    SendLetter(letter);
}


void ClientToServer::RequestWhiteBoard( unsigned char teamId, unsigned char action, int pointId, Fixed longitude, Fixed latitude, Fixed longitude2, Fixed latitude2 )
{
    if( VersionManager::DoesSupportWhiteBoard( m_serverVersion ) )
	{
		Directory *letter = new Directory();

		letter->CreateData( NET_DEFCON_COMMAND,         NET_DEFCON_WHITEBOARD );
		letter->CreateData( NET_DEFCON_TEAMID,          teamId );
		letter->CreateData( NET_DEFCON_ACTIONTYPE,      action );
		letter->CreateData( NET_DEFCON_OBJECTID,        pointId );
		letter->CreateData( NET_DEFCON_LONGITUDE,       longitude );
		letter->CreateData( NET_DEFCON_LATTITUDE,       latitude );
		letter->CreateData( NET_DEFCON_LONGITUDE2,      longitude2 );
		letter->CreateData( NET_DEFCON_LATTITUDE2,      latitude2 );

		SendLetter( letter );
	}
}


void ClientToServer::SendTeamScore( unsigned char teamId, int teamScore )
{
	if( VersionManager::DoesSupportSendTeamScore( m_serverVersion ) )
	{
		Directory *letter = new Directory();

		letter->CreateData( NET_DEFCON_COMMAND,  NET_DEFCON_TEAM_SCORE );
		letter->CreateData( NET_DEFCON_TEAMID,   teamId );
		letter->CreateData( NET_DEFCON_SCORE,    teamScore );

		SendLetter( letter );
	}
}


void ClientToServer::BeginVote( unsigned char teamId, unsigned char voteType, unsigned char voteData )
{
    Directory *letter = new Directory();

    letter->CreateData( NET_DEFCON_COMMAND,         NET_DEFCON_BEGINVOTE );
    letter->CreateData( NET_DEFCON_TEAMID,          teamId );
    letter->CreateData( NET_DEFCON_VOTETYPE,        voteType );
    letter->CreateData( NET_DEFCON_VOTEDATA,        voteData );

    SendLetter( letter );
}

void ClientToServer::CastVote( unsigned char teamId, unsigned char voteId, unsigned char myVote )
{
    Directory *letter = new Directory();

    letter->CreateData( NET_DEFCON_COMMAND,         NET_DEFCON_CASTVOTE );
    letter->CreateData( NET_DEFCON_TEAMID,          teamId );
    letter->CreateData( NET_DEFCON_VOTETYPE,        voteId );
    letter->CreateData( NET_DEFCON_VOTEDATA,        myVote );

    SendLetter( letter );
}


void ClientToServer::SendSyncronisation( int _lastProcessedId, unsigned char _sync )
{
    Directory *letter = new Directory();

    letter->CreateData( NET_DEFCON_COMMAND,             NET_DEFCON_SYNCHRONISE );
    letter->CreateData( NET_DEFCON_LASTPROCESSEDSEQID,  _lastProcessedId );
    letter->CreateData( NET_DEFCON_SYNCVALUE,           _sync );

    SendLetter( letter );
}


void ClientToServer::SetSyncState( int _clientId, bool _synchronised )
{
    if( !_synchronised && IsSynchronised(_clientId) )
    {
        m_outOfSyncClients.PutData( _clientId );
    }

    if( _synchronised )
    {
        for( int i = 0; i < m_outOfSyncClients.Size(); ++i )
        {
            if( m_outOfSyncClients[i] == _clientId )
            {
                m_outOfSyncClients.RemoveData(i);
                --i;
            }
        }
    }
}


bool ClientToServer::IsSynchronised( int _clientId )
{
    for( int i = 0; i < m_outOfSyncClients.Size(); ++i )
    {
        if( m_outOfSyncClients[i] == _clientId )
        {
            return false;
        }
    }

    return true;
}


void ClientToServer::SetClientDemo( int _clientId )
{
    if( !IsClientDemo(_clientId) )
    {
        m_demoClients.PutData( _clientId );
    }
}


bool ClientToServer::IsClientDemo( int _clientId )
{
    for( int i = 0; i < m_demoClients.Size(); ++i )
    {
        if( m_demoClients[i] == _clientId )
        {
            return true;
        }
    }

    return false;
}


bool ClientToServer::AmIDemoClient()
{
    if( IsConnected() )
    {
        return IsClientDemo( m_clientId );
    }
    else
    {
        char authKey[256];
        Authentication_GetKey( authKey );
        return Authentication_IsDemoKey( authKey );
    }
}


void ClientToServer::GetDemoLimitations( int &_maxGameSize, int &_maxDemoPlayers, bool &_allowDemoServers )
{
    Directory *demoLims = MetaServer_RequestData( NET_METASERVER_DATA_DEMOLIMITS );

   if( demoLims &&
        demoLims->HasData( NET_METASERVER_MAXDEMOGAMESIZE, DIRECTORY_TYPE_INT ) &&
        demoLims->HasData( NET_METASERVER_MAXDEMOPLAYERS, DIRECTORY_TYPE_INT ) &&
        demoLims->HasData( NET_METASERVER_ALLOWDEMOSERVERS, DIRECTORY_TYPE_BOOL) )
    {
        // The MetaServer gave us these numbers
        _maxGameSize = demoLims->GetDataInt( NET_METASERVER_MAXDEMOGAMESIZE );
        _maxDemoPlayers = demoLims->GetDataInt( NET_METASERVER_MAXDEMOPLAYERS );
        _allowDemoServers = demoLims->GetDataBool( NET_METASERVER_ALLOWDEMOSERVERS );
        delete demoLims;
    }
   else
   {
        // We don't have a clear answer from the MetaServer
        // So go with sensible defaults
        _maxGameSize = 6;
        _maxDemoPlayers = 6;
        _allowDemoServers = true;
   }    
}


void ClientToServer::GetPurchasePrice( char *_prices )
{
    Directory *prices = MetaServer_RequestData( NET_METASERVER_DATA_DEMOPRICES );

    if( _prices )
    {
        strcpy( _prices, "£10     $15     €12" );

		if( prices &&
            prices->HasData( NET_METASERVER_DATA_DEMOPRICES, DIRECTORY_TYPE_STRING ) )
        {
            char *msprices = prices->GetDataString( NET_METASERVER_DATA_DEMOPRICES );
            strcpy( _prices, msprices );
        }
    }
}


void ClientToServer::GetPurchaseUrl( char *_purchaseUrl )
{
    Directory *purchaseUrl = MetaServer_RequestData( NET_METASERVER_DATA_BUYURL );

    if( _purchaseUrl )
    {
        strcpy( _purchaseUrl, "http://www.introversion.co.uk/defcon/purchase/GlengaryDEFCON.htm?source=demo" );

        if( purchaseUrl &&
            purchaseUrl->HasData( NET_METASERVER_DATA_BUYURL, DIRECTORY_TYPE_STRING ) )
        {
            char *msPurchaseUrl = purchaseUrl->GetDataString( NET_METASERVER_DATA_BUYURL );
            strcpy( _purchaseUrl, msPurchaseUrl );
        }
    }
}


void ClientToServer::ProcessServerUpdates( Directory *letter )
{
    if( strcmp(letter->m_name, NET_DEFCON_MESSAGE) != 0 ||
        !letter->HasData(NET_DEFCON_COMMAND, DIRECTORY_TYPE_STRING) )
    {
        AppDebugOut( "ClientToServer received bogus message, discarded (8)\n" );
        return;
    }

    char *cmd = letter->GetDataString(NET_DEFCON_COMMAND);    
    if(strcmp(cmd, NET_DEFCON_UPDATE) != 0 )
    {
        AppDebugOut( "ClientToServer received bogus message, discarded (9)\n" );
        return;
    }
    
    
    //
    // Special case - this may be an empty update informing us
    // of a stack of upcoming empty updates

    if( letter->m_subDirectories.Size() == 0 &&
        letter->HasData( NET_DEFCON_NUMEMPTYUPDATES, DIRECTORY_TYPE_INT ) )
    {
        int numEmptyUpdates = letter->GetDataInt( NET_DEFCON_NUMEMPTYUPDATES );
        AppAssert( numEmptyUpdates != -1 );
        
        int seqId = letter->GetDataInt( NET_DEFCON_SEQID );
        
        if( numEmptyUpdates > 1 )
        {            
            Directory *letterCopy = new Directory(letter);
            letterCopy->CreateData( NET_DEFCON_NUMEMPTYUPDATES, numEmptyUpdates-1 );
            letterCopy->CreateData( NET_DEFCON_SEQID, seqId+1 );
            
            m_inboxMutex->Lock();
            m_inbox.PutDataAtStart( letterCopy );
            m_inboxMutex->Unlock();
        }

        return;
    }
    

    //
    // Our updates are stored in the sub-directories
    
    for( int i = 0; i < letter->m_subDirectories.Size(); ++i )
    {
        if( letter->m_subDirectories.ValidIndex(i) )
        {
            Directory *update = letter->m_subDirectories[i];
            AppAssert( strcmp(update->m_name, NET_DEFCON_MESSAGE) == 0 );
            AppAssert( update->HasData( NET_DEFCON_COMMAND, DIRECTORY_TYPE_STRING ) );
    
            char *cmd = update->GetDataString( NET_DEFCON_COMMAND );

            if( strcmp( cmd, NET_DEFCON_START_GAME ) == 0 )
            {
                int teamId = update->GetDataUChar(NET_DEFCON_TEAMID);
                Team *team = g_app->GetWorld()->GetTeam( teamId );
                if( team )
                {
                    team->m_readyToStart = !team->m_readyToStart;
                    team->m_randSeed = update->GetDataUChar(NET_DEFCON_RANDSEED);
                }

				g_app->SaveGameName();
            }
            else if( strcmp( cmd, NET_DEFCON_OBJPLACEMENT ) == 0 )
            {
                g_app->GetWorld()->ObjectPlacement( update->GetDataUChar(NET_DEFCON_TEAMID), 
                                                    update->GetDataUChar(NET_DEFCON_UNITTYPE),
                                                    update->GetDataFixed(NET_DEFCON_LONGITUDE),
                                                    update->GetDataFixed(NET_DEFCON_LATTITUDE),
                                                    update->GetDataUChar(NET_DEFCON_FLEETID) );
            }
            else if( strcmp( cmd, NET_DEFCON_OBJSTATECHANGE ) == 0 )
            {
                g_app->GetWorld()->ObjectStateChange( update->GetDataInt(NET_DEFCON_OBJECTID), 
                                                      update->GetDataUChar(NET_DEFCON_STATE) );

            }
            else if( strcmp( cmd, NET_DEFCON_OBJACTION ) == 0 )
            {
                g_app->GetWorld()->ObjectAction( update->GetDataInt(NET_DEFCON_OBJECTID), 
                                                 update->GetDataInt(NET_DEFCON_TARGETOBJECTID), 
                                                 update->GetDataFixed(NET_DEFCON_LONGITUDE),
                                                 update->GetDataFixed(NET_DEFCON_LATTITUDE), 
                                                 true );
            }
            else if( strcmp( cmd, NET_DEFCON_OBJSPECIALACTION ) == 0 )
            {
                g_app->GetWorld()->ObjectSpecialAction( update->GetDataInt(NET_DEFCON_OBJECTID),
                                                        update->GetDataInt(NET_DEFCON_TARGETOBJECTID),
                                                        update->GetDataUChar(NET_DEFCON_ACTIONTYPE) );
            }
            else if( strcmp( cmd, NET_DEFCON_OBJCLEARACTIONQUEUE ) == 0 )
            {
                g_app->GetWorld()->ObjectClearActionQueue( update->GetDataInt(NET_DEFCON_OBJECTID) );
            }
            else if( strcmp( cmd, NET_DEFCON_OBJCLEARLASTACTION ) == 0 )
            {
                g_app->GetWorld()->ObjectClearLastAction( update->GetDataInt(NET_DEFCON_OBJECTID) );
            }
            else if( strcmp( cmd, NET_DEFCON_OBJSETWAYPOINT ) == 0 )
            {
                g_app->GetWorld()->ObjectSetWaypoint( update->GetDataInt(NET_DEFCON_OBJECTID), 
                                                      update->GetDataFixed(NET_DEFCON_LONGITUDE), 
                                                      update->GetDataFixed(NET_DEFCON_LATTITUDE) );
            }                
            else if( strcmp( cmd, NET_DEFCON_REQUEST_TERRITORY ) == 0 )
            {
                int teamId = update->GetDataUChar(NET_DEFCON_TEAMID);
                g_app->GetWorld()->AssignTerritory( update->GetDataUChar(NET_DEFCON_TERRITORYID), 
                                                    update->GetDataUChar(NET_DEFCON_TEAMID),
                                                    update->GetDataInt(NET_DEFCON_OPTIONVALUE) );
                Team *team = g_app->GetWorld()->GetTeam( teamId );
                if( team )
                {
                    team->m_readyToStart = false;
                }
            }
            else if( strcmp( cmd, NET_DEFCON_REQUEST_ALLIANCE ) == 0 )
            {
                int teamId = update->GetDataUChar(NET_DEFCON_TEAMID);
                g_app->GetWorld()->RequestAlliance( teamId,
                                                    update->GetDataUChar(NET_DEFCON_ALLIANCEID) );
                Team *team = g_app->GetWorld()->GetTeam( teamId );
                if( team )
                {
                    team->m_readyToStart = false;
                }
            }
            else if( strcmp( cmd, NET_DEFCON_CHANGEOPTION ) == 0 )
            {
                int optionIndex = (int)update->GetDataUChar( NET_DEFCON_OPTIONID );
                GameOption *option = g_app->GetGame()->m_options[optionIndex];
                AppAssert(option);
        
                if( update->HasData( NET_DEFCON_OPTIONVALUE, DIRECTORY_TYPE_INT ) )
                {
                    option->m_currentValue = update->GetDataInt(NET_DEFCON_OPTIONVALUE);
                }
                else if( update->HasData( NET_DEFCON_OPTIONVALUE, DIRECTORY_TYPE_STRING ) )
                {
                    strcpy( option->m_currentString, update->GetDataString(NET_DEFCON_OPTIONVALUE) );
                }
        
                if( strcmp(option->m_name, "GameMode" ) == 0 )
                {
                    g_app->GetGame()->SetGameMode( option->m_currentValue );
                }

                for( int i = 0; i < g_app->GetWorld()->m_teams.Size(); ++i )
                {
                    g_app->GetWorld()->m_teams[i]->m_readyToStart = false;
                }
            }
            else if( strcmp( cmd, NET_DEFCON_SETTEAMNAME ) == 0 )
            {
                int teamId = update->GetDataUChar(NET_DEFCON_TEAMID);
                char *teamName = update->GetDataString(NET_DEFCON_OPTIONVALUE);
				char defaultName[256];

				// If player attempts to set invalid name, use a default instead
				if( !Team::IsValidName( teamName ) )
				{
					if( update->HasData( NET_DEFCON_SPECTATOR ) )
					{
						strcpy( defaultName, LANGUAGEPHRASE("dialog_c2s_default_name_spectator") );
					}
					else
					{
						strcpy( defaultName, LANGUAGEPHRASE("dialog_c2s_default_name_player") );
						LPREPLACEINTEGERFLAG( 'T', teamId, defaultName );
					}

					teamName = defaultName;
				}

                if( update->HasData(NET_DEFCON_SPECTATOR) )
                {
                    // Spectator renaming himself
                    Spectator *spec = g_app->GetWorld()->GetSpectator(teamId);
                    
                    if( spec && strcmp( spec->m_name, teamName ) != 0 )
                    {
                        char msg[256];
						sprintf( msg, LANGUAGEPHRASE("dialog_c2s_team_renamed") );
						LPREPLACESTRINGFLAG( 'T', spec->m_name, msg );
						LPREPLACESTRINGFLAG( 'N', teamName, msg );
                        g_app->GetWorld()->AddChatMessage( -1, CHATCHANNEL_PUBLIC, msg, -1, false );

                        strcpy( spec->m_name, teamName );        

                        if( teamId == m_clientId )
                        {
                            if( strcmp( teamName, LANGUAGEPHRASE("gameoption_PlayerName") ) == 0 )
                            {
                                g_preferences->SetString( "PlayerName", g_languageTable->LookupBasePhrase( "gameoption_PlayerName" ) );
                            }
                            else
                            {
                                g_preferences->SetString( "PlayerName", teamName );
                            }
                            g_preferences->Save();
                        }
                    }
                }
                else
                {
                    // Team renaming itself
                    Team *team = g_app->GetWorld()->GetTeam( teamId );
                    
                    if( team &&
                        strcmp( team->m_name, teamName ) != 0 )
                    {
                        char msg[256];
                        if( team->m_nameSet )
                        {
							sprintf( msg, LANGUAGEPHRASE("dialog_c2s_team_renamed") );
							LPREPLACESTRINGFLAG( 'T', team->m_name, msg );
							LPREPLACESTRINGFLAG( 'N', teamName, msg );
                        }
                        else
                        {
                            sprintf( msg, LANGUAGEPHRASE("dialog_c2s_team_joined_game") );
							LPREPLACESTRINGFLAG( 'T', teamName, msg );
                            team->m_nameSet = true;
                        }
                        g_app->GetWorld()->AddChatMessage( team->m_teamId, CHATCHANNEL_PUBLIC, msg, -1, false );

                        team->SetTeamName( teamName );

                        if( teamId == g_app->GetWorld()->m_myTeamId )
                        {
                            if( strcmp( teamName, LANGUAGEPHRASE("gameoption_PlayerName") ) == 0 )
                            {
                                g_preferences->SetString( "PlayerName", g_languageTable->LookupBasePhrase( "gameoption_PlayerName" ) );
                            }
                            else
                            {
                                g_preferences->SetString( "PlayerName", teamName );
                            }
                            g_preferences->Save();
                        }
                    }
                }
            }
            else if( strcmp( cmd, NET_DEFCON_SETGAMESPEED ) == 0 )
            {
                int teamId = update->GetDataUChar(NET_DEFCON_TEAMID);
                Team *team = g_app->GetWorld()->GetTeam( teamId );                
                if( team )
                {
                    team->m_desiredGameSpeed = update->GetDataUChar(NET_DEFCON_OPTIONVALUE);
                }
            }
            else if( strcmp( cmd, NET_DEFCON_REQUEST_FLEET ) == 0 )
            {
                Team *team = g_app->GetWorld()->GetTeam( update->GetDataUChar(NET_DEFCON_TEAMID) );
                AppAssert(team);
                team->CreateFleet();
            }
            else if( strcmp( cmd, NET_DEFCON_FLEETMOVE ) == 0 )
            {
                g_app->GetWorld()->FleetSetWaypoint( update->GetDataUChar(NET_DEFCON_TEAMID), 
                                                     update->GetDataInt(NET_DEFCON_FLEETID),
                                                     update->GetDataFixed(NET_DEFCON_LONGITUDE),
                                                     update->GetDataFixed(NET_DEFCON_LATTITUDE) );
            }
            else if( strcmp( cmd, NET_DEFCON_CHATMESSAGE ) == 0 )
            {
                bool spectator = update->HasData(NET_DEFCON_SPECTATOR, DIRECTORY_TYPE_INT );

                int teamId = update->GetDataUChar(NET_DEFCON_TEAMID);
                int messageId = update->GetDataInt(NET_DEFCON_CHATMSGID);

                ReceiveChatMessage( teamId, messageId );
                g_app->GetWorld()->AddChatMessage( teamId, 
                                                   update->GetDataUChar(NET_DEFCON_CHATCHANNEL), 
                                                   update->GetDataString(NET_DEFCON_CHATMSG),
                                                   messageId,
                                                   spectator );
            }
            else if( strcmp( cmd, NET_DEFCON_BEGINVOTE ) == 0 )
            {
                g_app->GetWorld()->m_votingSystem.RegisterNewVote( update->GetDataUChar(NET_DEFCON_TEAMID),
                                                                   update->GetDataUChar(NET_DEFCON_VOTETYPE),
                                                                   update->GetDataUChar(NET_DEFCON_VOTEDATA) );
            }
            else if( strcmp( cmd, NET_DEFCON_CASTVOTE ) == 0 )
            {
                g_app->GetWorld()->m_votingSystem.CastVote( update->GetDataUChar(NET_DEFCON_TEAMID),
                                                            update->GetDataUChar(NET_DEFCON_VOTETYPE),
                                                            update->GetDataUChar(NET_DEFCON_VOTEDATA) );
            }
            else if( strcmp( cmd, NET_DEFCON_CEASEFIRE ) == 0 )
            {
                g_app->GetWorld()->TeamCeaseFire( update->GetDataUChar(NET_DEFCON_TEAMID), 
                                                  update->GetDataUChar(NET_DEFCON_TARGETTEAMID) );
            }
            else if( strcmp( cmd, NET_DEFCON_SHARERADAR ) == 0 )
            {
                g_app->GetWorld()->TeamShareRadar( update->GetDataUChar(NET_DEFCON_TEAMID),
                                                   update->GetDataUChar(NET_DEFCON_TARGETTEAMID) );
            }
            else if( strcmp( cmd, NET_DEFCON_AGGRESSIONCHANGE ) == 0 )
            {
                int teamId = update->GetDataUChar(NET_DEFCON_TEAMID);
                Team *team = g_app->GetWorld()->GetTeam( teamId );
                AppAssert(team);
                team->m_aggression = update->GetDataUChar(NET_DEFCON_OPTIONVALUE);               
            }
            else if( strcmp( cmd, NET_DEFCON_REMOVEAI ) == 0 )
            {
                int teamId = update->GetDataUChar(NET_DEFCON_TEAMID);
                g_app->GetWorld()->RemoveAITeam(teamId);
            }
			else if( strcmp( cmd, NET_DEFCON_WHITEBOARD ) == 0 )
			{
				g_app->GetWorld()->WhiteBoardAction( update->GetDataUChar(NET_DEFCON_TEAMID),
                                                     update->GetDataUChar(NET_DEFCON_ACTIONTYPE),
                                                     update->GetDataInt(NET_DEFCON_OBJECTID),
                                                     update->GetDataFixed(NET_DEFCON_LONGITUDE),
                                                     update->GetDataFixed(NET_DEFCON_LATTITUDE),
                                                     update->GetDataFixed(NET_DEFCON_LONGITUDE2),
                                                     update->GetDataFixed(NET_DEFCON_LATTITUDE2) );
			}
			else if( strcmp( cmd, NET_DEFCON_TEAM_SCORE ) == 0 )
			{
				//do nothing
			}
			else
            {
                AppDebugOut( "ClientToServer received bogus message, discarded (11)\n" );
            }
        }
    }
}


void ClientToServer::Resynchronise()
{
    m_lastValidSequenceIdFromServer = -1;
    m_serverSequenceId = -1;
    //m_clientId = -1;

    m_resynchronising = GetHighResTime();
}


void ClientToServer::StartIdentifying()
{
    MatchMaker_StartRequestingIdentity( m_listener );
}


void ClientToServer::StopIdentifying()
{
    MatchMaker_StopRequestingIdentity( m_listener );
}


bool ClientToServer::GetIdentity( char *_ip, int *_port )
{
    return MatchMaker_GetIdentity( m_listener, _ip, _port );
}


bool ClientToServer::GetServerDetails( char *_ip, int *_port )
{
    if( !m_serverIp )
    {
        return false;
    }

    strcpy( _ip, m_serverIp );
    *_port = m_serverPort;
    return true;
}


