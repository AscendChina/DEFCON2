#include "lib/universal_include.h"

#include "lib/netlib/net_lib.h"
#include "lib/tosser/directory.h"
#include "lib/debug_utils.h"
#include "lib/hi_res_time.h"
#include "lib/math/random_number.h"
#include "lib/language_table.h"

#include "metaserver.h"
#include "authentication.h"
#include "metaserver_defines.h"

using namespace std;


// ============================================================================
// Static data for MetaServer lib


struct GameServer
{
public:
    char        m_identity[256];                // usually IP:port
    double      m_updateTime;
    int         m_authResult;
    bool        m_receivedWan;
    bool        m_receivedLan;
    Directory   *m_data;

public:
    GameServer()
        :   m_updateTime(0.0f),
            m_authResult(0),
            m_receivedWan(false),
            m_receivedLan(false),
            m_data(NULL)
    {
        m_identity[0] = '\x0';
    }
};



static NetLib               *s_netLib = NULL;

static NetSocketListener    *s_socketListener = NULL;
static NetSocketSession     *s_socketToWAN = NULL;
static NetSocketSession     *s_socketToLAN = NULL;

static bool                 s_connectedToWAN = false;
static bool                 s_registeringOverWAN = false;

static Directory            *s_serverProperties = NULL;
static NetMutex             s_serverPropertiesMutex;

static Directory            *s_clientProperties = NULL;
static NetMutex             s_clientPropertiesMutex;

static LList<GameServer *>  s_serverList;
static NetMutex             s_serverListMutex;

static float                s_serverTTL = 30;
static bool                 s_awaitingServerListWAN = false;

static int                  s_bytesSent = 0;

#define                    UDP_HEADER_SIZE    32



struct MetaServerData
{
public:
    char        m_dataType[256];
    Directory   *m_data;
    double      m_timeOfExpire;                 // < 0 = never expires
    int         m_retries;

public:
    MetaServerData()
        :   m_data(NULL),
            m_timeOfExpire(0.0f),
            m_retries(0)
    {
        m_dataType[0] = '\x0';
    }
};


static LList<MetaServerData *>  s_metaServerData;
static NetMutex                 s_metaServerDataMutex;


// ============================================================================


bool MetaServer_SendDirectory( Directory *_directory, NetSocket *_socket )
{    
    int byteStreamLen = 0;
    char *byteStream = _directory->Write( byteStreamLen );

    int numActualBytes = 0;
    NetRetCode result = _socket->WriteData( byteStream, byteStreamLen, &numActualBytes );

    s_bytesSent += byteStreamLen;
    s_bytesSent += UDP_HEADER_SIZE;

    delete[] byteStream;

    return( result == NetOk && numActualBytes == byteStreamLen );
}


bool MetaServer_SendDirectory( Directory *_directory, NetSocketSession *_socket )
{        
    int byteStreamLen = 0;
    char *byteStream = _directory->Write( byteStreamLen );

    int numActualBytes = 0;
    NetRetCode result = _socket->WriteData( byteStream, byteStreamLen, &numActualBytes );

    s_bytesSent += byteStreamLen;
    s_bytesSent += UDP_HEADER_SIZE;

    delete[] byteStream;

    return( result == NetOk && numActualBytes == byteStreamLen );
}


bool MetaServer_SendToMetaServer( Directory *_directory )
{
#ifdef WAN_PLAY_ENABLED
    if( s_connectedToWAN )
    {
        return MetaServer_SendDirectory( _directory, s_socketToWAN );
    }
#endif

    return false;
}


void MetaServer_Initialise()
{
    if( !s_netLib )
    {
        s_netLib = new NetLib();
        bool result = s_netLib->Initialise();
        AppDebugAssert( result );
    }
}


void ReceiveMetaServerData( Directory *_directory )
{
    char *thisDataType = _directory->GetDataString( NET_METASERVER_DATATYPE );
 
    AppDebugOut( "Received data from MetaServer : %s\n", thisDataType );

    
    bool found = false;
    MetaServerData *target = NULL;

    s_metaServerDataMutex.Lock();

    //
    // Look for an existing entry to replace

    for( int i = 0; i < s_metaServerData.Size(); ++i )
    {
        MetaServerData *data = s_metaServerData[i];               
        if( stricmp( data->m_dataType, thisDataType ) == 0 ) 
        {
            delete data->m_data;
            target = data;
            found = true;
            break;
        }
    }

    //
    // If no entry found, create a new one

    if( !found )
    {
        target = new MetaServerData();
        strcpy( target->m_dataType, thisDataType );
        s_metaServerData.PutData( target );
    }


    //
    // Update the entry with the latest data
        
    target->m_data = _directory;
    int ttl = _directory->GetDataInt( NET_METASERVER_DATATIMETOLIVE );
    if( ttl < 0 ) target->m_timeOfExpire = -1;
    else          target->m_timeOfExpire = GetHighResTime() + ttl;

    s_metaServerDataMutex.Unlock();
}


void ReceiveDirectory( Directory *_directory )
{
    if( strcmp(_directory->m_name, NET_METASERVER_MESSAGE) != 0 ||
        !_directory->HasData(NET_METASERVER_COMMAND, DIRECTORY_TYPE_STRING) )
    {
        AppDebugOut( "Received bogus message, discarded (6)\n" );
        delete _directory;
        return;
    }

    char *cmd = _directory->GetDataString(NET_METASERVER_COMMAND);


    //
    // LIST command received
    // This is from a WAN MetaServer, and tells us about one game server somewhere

    if( strcmp( cmd, NET_METASERVER_LISTRESULT ) == 0 )
    {        
        MetaServer_UpdateServerList( _directory->GetDataString( NET_METASERVER_IP ), 
                                     _directory->GetDataInt( NET_METASERVER_PORT ),
                                     true,
                                     _directory );
        
        s_awaitingServerListWAN = false;
    }
    

    //
    // LIST ENVELOPE received
    // This is essentially a directory that contains a number of LIST commands
    // Each one tells us about a single game server somewhere

    else if( strcmp( cmd, NET_METASERVER_LISTRESULT_ENVELOPE ) == 0 )
    {
        for( int i = 0; i < _directory->m_data.Size(); ++i )
        {
            if( _directory->m_data.ValidIndex(i) )
            {
                DirectoryData *data = _directory->m_data[i];
                if( data->IsVoid() )
                {
                    Directory *result = new Directory();
                    bool success = result->Read( data->m_void, data->m_voidLen );
                    if( success )
                    {                
                        ReceiveDirectory( result );
                    }
                }
            }
        }
    }


    //
    // REGISTER command received
    // This is from a LAN broadcast, and tells us about a game server on the LAN

    else if( strcmp( cmd, NET_METASERVER_REGISTER ) == 0 )
    {
        char *fromIP = _directory->GetDataString( NET_METASERVER_FROMIP );
        int localPort = _directory->GetDataInt( NET_METASERVER_LOCALPORT );

        _directory->CreateData( NET_METASERVER_IP, fromIP );
        _directory->CreateData( NET_METASERVER_PORT, localPort );

        MetaServer_UpdateServerList( fromIP, localPort, false, _directory );
    }


    //
    // AUTHRESULT command received
    // This is a message from the MetaServer, informing us of the results
    // of an Authentication test

    else if( strcmp( cmd, NET_METASERVER_AUTHRESULT ) == 0 )
    {
        int result = _directory->GetDataInt( NET_METASERVER_AUTHRESULT );
        char *key = _directory->GetDataString( NET_METASERVER_AUTHKEY );
        int keyId = _directory->GetDataInt( NET_METASERVER_AUTHKEYID );
        Authentication_SetStatus( key, keyId, result );

        if( _directory->HasData(NET_METASERVER_HASHTOKEN, DIRECTORY_TYPE_INT) )
        {
            int hashToken = _directory->GetDataInt( NET_METASERVER_HASHTOKEN );
            Authentication_SetHashToken( hashToken );
        }

        s_awaitingServerListWAN = false;
    }

    //
    // REQUESTDATA received
    // This is a message from the MetaServer,
    // And gives us a small piece of usefull data eg TheTime, LatestVersion etc

    else if( strcmp( cmd, NET_METASERVER_REQUEST_DATA ) == 0 )
    {   
        ReceiveMetaServerData( _directory );
    }


    //
    // No idea what this message is supposed to be

    else
    {
        AppDebugOut( "Received bogus message from the MetaServer, discarded (7)\n" );
        delete _directory;
    }
}


static NetCallBackRetType ListenCallback(NetUdpPacket *udpdata)
{
    if (udpdata)
    {
	// **** addresstostr 
        NetIpAddress fromAddr = udpdata->m_clientAddress;
        char newip[16];
		IpAddressToStr(fromAddr, newip);

        Directory *directory = new Directory();
        bool success = directory->Read( udpdata->m_data, udpdata->m_length );
        if( success )
        {            
            directory->CreateData( NET_METASERVER_FROMIP, newip );        
            ReceiveDirectory( directory );
        }
        else
        {
            AppDebugOut( "Received bogus message, discarded (8)\n" );
			Directory test;
			test.Read( udpdata->m_data, udpdata->m_length );
            delete directory;
        }

        delete udpdata;
    }

    return 0;
}


static NetCallBackRetType ListenThread(void *ignored)
{
    s_socketListener->StartListening( ListenCallback );    
    return 0;
}


void MetaServer_Connect( char *_metaServerIp, int _metaserverPort, int _listenPort )
{
    if( s_connectedToWAN )
    {
        return;
    }


    //
    // Try the requested port number first
    
    s_socketListener = new NetSocketListener( _listenPort );    
    NetRetCode result = s_socketListener->Bind();


    //
    // If that failed, try port 0 and see if we do any better

    if( result != NetOk && _listenPort != 0 )
    {        
        AppDebugOut( "MetaServer failed to open listener on Port %d\n", _listenPort );
        AppDebugOut( "MetaServer looking for any free Port number...\n" );
        
        delete s_socketListener;
        s_socketListener = new NetSocketListener(0);
        result = s_socketListener->Bind();
    }


    //
    // If it still didn't work, bail out now

    if( result != NetOk )
    {
        delete s_socketListener;
        s_socketListener = NULL;
        AppDebugOut( "ERROR: Failed to connect to MetaServer\n" );
        return;
    }


    NetStartThread( ListenThread );

#ifdef WAN_PLAY_ENABLED   
    s_socketToWAN = new NetSocketSession( *s_socketListener, _metaServerIp, _metaserverPort );
    s_connectedToWAN = true;
#endif

    AppDebugOut( "Connection established to MetaServer on port %d. Listening on Port %d\n", _metaserverPort, s_socketListener->GetPort() );

}


void MetaServer_Disconnect()
{
    s_connectedToWAN = false;
    s_registeringOverWAN = false;
    
    delete s_socketToWAN;
    s_socketToWAN = NULL;

    if( s_socketListener )
    {
        s_socketListener->StopListening();
        //delete s_socketListener;
        s_socketListener = NULL;
    }

    AppDebugOut( "Disconnected from MetaServer\n" );
}


bool MetaServer_IsConnected()
{
    return ( s_connectedToWAN );
}


void MetaServer_SetServerProperties( Directory *_properties )
{
    //
    // Copy the incoming game properties so we have a thread safe version to read

    Directory *propertiesCopy = new Directory();
    propertiesCopy->CopyData( _properties );

    s_serverPropertiesMutex.Lock();

    if( s_serverProperties )
    {
        delete s_serverProperties;
        s_serverProperties = NULL;
    }

    s_serverProperties = propertiesCopy;

    s_serverPropertiesMutex.Unlock();
}


void MetaServer_SetClientProperties( Directory *_properties )
{
    //
    // Copy the incoming game properties so we have a thread safe version to read

    Directory *propertiesCopy = new Directory();
    propertiesCopy->CopyData( _properties );

    s_clientPropertiesMutex.Lock();

    if( s_clientProperties )
    {
        delete s_clientProperties;
        s_clientProperties = NULL;
    }

    s_clientProperties = propertiesCopy;

    s_clientPropertiesMutex.Unlock();
}


Directory *MetaServer_GetClientProperties()
{
    Directory *result = NULL;

    s_clientPropertiesMutex.Lock();

    if( s_clientProperties )
    {
        result = new Directory();
        result->CopyData( s_clientProperties );
    }

    s_clientPropertiesMutex.Unlock();

    return result;
}


static NetCallBackRetType ServerRegisterWANThread(void *ignored)
{
#ifdef WAN_PLAY_ENABLED
    AppDebugOut( "MetaServer : WAN registration enabled\n" );

    while( s_connectedToWAN && s_registeringOverWAN )
    {
        bool success = true;
        bool registrationPermitted = true;

        //
        // If we're a Demo user, make sure we are permitted to advertise

        char authKey[256];
        Authentication_GetKey( authKey );
        if( Authentication_IsDemoKey( authKey ) )
        {
            Directory *limits = MetaServer_RequestData( NET_METASERVER_DATA_DEMOLIMITS );
            if( limits &&
                limits->HasData( NET_METASERVER_ALLOWDEMOSERVERS, DIRECTORY_TYPE_BOOL ) )
            {
                bool allowDemoServers = limits->GetDataBool( NET_METASERVER_ALLOWDEMOSERVERS );
                registrationPermitted = allowDemoServers;
            }
            delete limits;
			limits = NULL;
        }

        //
        // Send our Registration

        if( registrationPermitted )
        {
            s_serverPropertiesMutex.Lock();
            if( s_serverProperties )
            {    
                s_serverProperties->SetName( NET_METASERVER_MESSAGE );
                s_serverProperties->CreateData( NET_METASERVER_COMMAND, NET_METASERVER_REGISTER );

                success = MetaServer_SendDirectory( s_serverProperties, s_socketToWAN );
            }        
            s_serverPropertiesMutex.Unlock();
        }

        if( !success ) 
        {
            AppDebugOut( "MetaServer encountered error sending data\n" );
            break;
        }

        float timeToSleep = MetaServer_GetServerTTL();
        timeToSleep /= 3.0f;
        timeToSleep += sfrand(3.0f);
        timeToSleep *= 1000.0f;
        
        NetSleep( (int) timeToSleep );
    }

    AppDebugOut( "MetaServer : WAN registration disabled\n" );
#else
    AppDebugOut( "MetaServer WARNING : WAN Play is disabled\n" );
#endif

    return 0;
}


void MetaServer_StartRegisteringOverWAN()
{
#ifdef WAN_PLAY_ENABLED
    if( s_connectedToWAN && !s_registeringOverWAN )
    {
        s_registeringOverWAN = true;
        NetStartThread( ServerRegisterWANThread );
    }
#else
    AppDebugOut( "MetaServer WARNING : WAN Play is disabled\n" );
#endif
}


void MetaServer_StopRegisteringOverWAN()
{
    s_registeringOverWAN = false;
}


static NetCallBackRetType ServerRegisterLANThread(void *ignored)
{
#ifdef LAN_PLAY_ENABLED
    AppDebugOut( "MetaServer : LAN registration enabled\n" );

    while( s_socketToLAN )
    {
        bool success = true;

        s_serverPropertiesMutex.Lock();

        if( s_serverProperties )
        {
            s_serverProperties->SetName( NET_METASERVER_MESSAGE );
            s_serverProperties->CreateData( NET_METASERVER_COMMAND, NET_METASERVER_REGISTER );

            success = MetaServer_SendDirectory( s_serverProperties, s_socketToLAN );
        }        

        s_serverPropertiesMutex.Unlock();
        
        if( !success ) 
        {
            AppDebugOut( "MetaServer encountered error sending data\n" );
            break;
        }

        NetSleep( PERIOD_SERVER_REGISTER_LAN );
    }
    AppDebugOut( "MetaServer : LAN registration disabled\n" );

#else
    AppDebugOut( "MetaServer WARNING : LAN Play is disabled\n" );
#endif

    return 0;
}


void MetaServer_StartRegisteringOverLAN()
{
#ifdef LAN_PLAY_ENABLED
    if( !s_socketToLAN && s_socketListener )
    {
        //
        // Turn on broadcast on the listener socket

        AppDebugOut( "MetaServer enabling UDP broadcast for LAN discovery..." );
        NetRetCode result = s_socketListener->EnableBroadcast();
        if( result == NetOk ) AppDebugOut( "SUCCESS\n" );
        else                  AppDebugOut( "FAILED\n" );

        //
        // Open a socket session to send to the LAN

        int port = PORT_METASERVER_CLIENT_LISTEN;
        char *host = "255.255.255.255";
        s_socketToLAN = new NetSocketSession( *s_socketListener, host, port );

        NetStartThread( ServerRegisterLANThread );
    }
#else
    AppDebugOut( "MetaServer WARNING : LAN Play is disabled\n" );
#endif
}


void MetaServer_StopRegisteringOverLAN()
{
    if( s_socketToLAN ) 
    {
        delete s_socketToLAN;
        s_socketToLAN = NULL;
    }
}


bool MetaServer_IsRegisteringOverWAN()
{
    return( s_connectedToWAN && s_registeringOverWAN );
}


bool MetaServer_IsRegisteringOverLAN()
{
    return( s_socketToLAN != NULL );
}


void MetaServer_RequestServerListWAN()
{
    s_awaitingServerListWAN = true;

#ifdef WAN_PLAY_ENABLED
    if( !s_connectedToWAN )
    {
        return;
    }

    if( !s_clientProperties )
    {
        AppDebugOut( "MetaServer failed to RequestServerList over WAN : client properties not set\n" );
        return;
    }

    char authKey[256];
    Authentication_GetKey( authKey );
    if( Authentication_SimpleKeyCheck(authKey) < 0 )
    {
        AppDebugOut( "MetaServer refused to RequestServerList over WAN : authentication key invalid\n" );
        return;
    }

    s_clientPropertiesMutex.Lock();
    
    s_clientProperties->SetName( NET_METASERVER_MESSAGE );
    s_clientProperties->CreateData( NET_METASERVER_COMMAND, NET_METASERVER_REQUEST_LIST );

    MetaServer_SendDirectory( s_clientProperties, s_socketToWAN );

    s_clientPropertiesMutex.Unlock();
    
#else
    AppDebugOut( "MetaServer WARNING : WAN Play is disabled\n" );
#endif
}


void MetaServer_RequestServerDetails( char *_ip, int _port )
{
#ifdef WAN_PLAY_ENABLED
    if( !s_connectedToWAN )
    {
        return;
    }

    if( !s_clientProperties )
    {
        AppDebugOut( "MetaServer failed to RequestServerDetails over WAN : client properties not set\n" );
        return;
    }

    s_clientPropertiesMutex.Lock();

    s_clientProperties->SetName( NET_METASERVER_MESSAGE );
    s_clientProperties->CreateData( NET_METASERVER_COMMAND, NET_METASERVER_REQUESTSERVERDETAILS );
    s_clientProperties->CreateData( NET_METASERVER_IP, _ip );
    s_clientProperties->CreateData( NET_METASERVER_PORT, _port );

    MetaServer_SendDirectory( s_clientProperties, s_socketToWAN );

    s_clientPropertiesMutex.Unlock();
#endif
}



bool MetaServer_HasReceivedListWAN()
{
    return !s_awaitingServerListWAN;
}


int MetaServer_GetNumServers( bool _wanServers, bool _lanServers )
{
    int result = 0;

    s_serverListMutex.Lock();

    if( _wanServers && _lanServers )
    {
        result = s_serverList.Size();
    }
    else
    {
        for( int i = 0; i < s_serverList.Size(); ++i )
        {
            GameServer *server = s_serverList[i];

            bool lanMatch = ( server->m_receivedLan && _lanServers );
            bool wanMatch = ( server->m_receivedWan && _wanServers );

            if( lanMatch || wanMatch ) ++result;
        }
    }

    s_serverListMutex.Unlock();

    return result;
}


void MetaServer_ClearServerList()
{
    s_serverListMutex.Lock();
    s_serverList.EmptyAndDelete();
    s_serverListMutex.Unlock();
}


void MetaServer_UpdateServerList( char *_ip, int _port, bool _lanOrWan, Directory *_server )
{
    char serverIdentity[512];
    sprintf( serverIdentity, "%s:%d", _ip, _port );


    char *authKey = NULL;

    if( _server->HasData( NET_METASERVER_AUTHKEY ) )
    {
        authKey = _server->GetDataString( NET_METASERVER_AUTHKEY );
    }


    //
    // Is this server already listed?
    // Look for matching identitys (IP:port)
    // Also look for matching keys, as one key can only run one server at once
    
    s_serverListMutex.Lock();
    
    for( int i = 0; i < s_serverList.Size(); ++i )
    {        
        GameServer *thisServer = s_serverList[i]; 
        bool authKeyMatch = false;


        if( authKey &&
            thisServer->m_data &&
            thisServer->m_data->HasData( NET_METASERVER_AUTHKEY ))
        {
            const char *thisAuthKey = thisServer->m_data->GetDataString( NET_METASERVER_AUTHKEY );

            authKeyMatch = ( authKey && 
                             thisAuthKey && 
                             strcmp( thisAuthKey, authKey ) == 0 );
        }


        if( strcmp( thisServer->m_identity, serverIdentity) == 0 ||
            authKeyMatch )
        {
            delete thisServer->m_data;
            thisServer->m_data = _server;
            thisServer->m_updateTime = GetHighResTime();
            if( _lanOrWan==0 ) thisServer->m_receivedLan = true;
            if( _lanOrWan==1 ) thisServer->m_receivedWan = true;
            s_serverListMutex.Unlock();
            return;
        }
    }

    s_serverListMutex.Unlock();


    //
    // List the server    

    GameServer *server = new GameServer();
    strcpy( server->m_identity, serverIdentity );
    server->m_data = _server;
    server->m_updateTime = GetHighResTime();
    server->m_authResult = AuthenticationUnknown;
    server->m_receivedLan = (_lanOrWan==0);
    server->m_receivedWan = (_lanOrWan==1);

    s_serverListMutex.Lock();
    s_serverList.PutData( server );
    s_serverListMutex.Unlock();
}


void MetaServer_SetServerTTL( int _seconds )
{
    s_serverTTL = _seconds;
}


Directory *MetaServer_GetServer( char *_ip, int _port, bool _authRequired )
{
    char serverIdentity[512];
    sprintf( serverIdentity, "%s:%d", _ip, _port );

    Directory *result = NULL;


    s_serverListMutex.Lock();

    for( int i = 0; i < s_serverList.Size(); ++i )
    {
        GameServer *server = s_serverList[i];
        bool authOk = !_authRequired ||
                       server->m_authResult == AuthenticationAccepted;

        if( authOk && stricmp( server->m_identity, serverIdentity ) == 0 )
        {
            result = new Directory();
            result->CopyData( server->m_data );
            break;
        }
    }

    s_serverListMutex.Unlock();


    return result;
}


void MetaServer_PurgeServerList()
{
    double discardTime = GetHighResTime() - s_serverTTL;

    s_serverListMutex.Lock();

    for( int i = 0; i < s_serverList.Size(); ++i )
    {
        GameServer *server = s_serverList[i];
        if( server->m_updateTime < discardTime )
        {            
            s_serverList.RemoveData(i);
            delete server->m_data;
            delete server;        
            --i;
        }
    }    

    s_serverListMutex.Unlock();
}


LList<Directory *> *MetaServer_GetServerList( bool _authRequired, bool _wanServers, bool _lanServers, int _maxListSize )
{
    //
    // First we purge this list for servers that have been silent for too long

    MetaServer_PurgeServerList();


    //
    // We must copy the list to ensure safe mutex use
    // Only copy servers that have been authenticated

    LList<Directory *> *copyServerList = new LList<Directory *>();

    int numAdded = 0;

    s_serverListMutex.Lock();    

    for( int i = 0; i < s_serverList.Size(); ++i )
    {
        GameServer *server = s_serverList[i];
        if( !_authRequired ||
            server->m_authResult == AuthenticationAccepted )
        {
            bool lanMatch = ( server->m_receivedLan && _lanServers );
            bool wanMatch = ( server->m_receivedWan && _wanServers );
            
            if( lanMatch || wanMatch )
            {
                Directory *copyMe = new Directory();
                copyMe->CopyData( server->m_data );
                copyMe->CreateData( NET_METASERVER_AUTHRESULT, server->m_authResult );
                copyServerList->PutData( copyMe );

                ++numAdded;
                if( _maxListSize != -1 && numAdded >= _maxListSize )
                {
                    // We have reached the max server list size we want
                    break;
                }
            }
        }
    }
    s_serverListMutex.Unlock();    

    return copyServerList;
}


void MetaServer_SetAuthenticationStatus( char *_ip, int _port, int _status )
{
    char serverIdentity[512];
    sprintf( serverIdentity, "%s:%d", _ip, _port );

    s_serverListMutex.Lock();

    for( int i = 0; i < s_serverList.Size(); ++i )
    {
        GameServer *server = s_serverList[i];
        if( stricmp( server->m_identity, serverIdentity ) == 0 )
        {
            server->m_authResult = _status;
            break;
        }
    }

    s_serverListMutex.Unlock();
}


Directory *MetaServer_RequestData( char *_dataType )
{
    //
    // Do we already have an answer?

    s_metaServerDataMutex.Lock();
    
    float timeNow = GetHighResTime();

    Directory *result = NULL;
    bool sendRequest = false;
    bool found = false;

    for( int i = 0; i < s_metaServerData.Size(); ++i )
    {
        MetaServerData *data = s_metaServerData[i];       
        if( stricmp( data->m_dataType, _dataType ) == 0 ) 
        {
            found = true;

            if( data->m_data )
            {
                result = new Directory( data->m_data );
                data->m_retries = 0;
            }

            if( data->m_timeOfExpire > 0.0f &&
                timeNow >= data->m_timeOfExpire )
            {
                // This data has expired so request more
                // Set the time of Expire so we don't flood the MetaServer
                // Once its failed 10 times back off and only request every minute at most

                sendRequest = true;
                float timeToWait = 1.0f;
                if( data->m_retries > 10 ) timeToWait = 60.0f;

                data->m_timeOfExpire = GetHighResTime() + timeToWait;
                
                if( data->m_data )
                {
                    int currentExpire = 0;
                    if( data->m_data->HasData( NET_METASERVER_DATAEXPIRED ) )
                    {
                        currentExpire = data->m_data->GetDataInt( NET_METASERVER_DATAEXPIRED );
                    }

                    data->m_data->CreateData( NET_METASERVER_DATAEXPIRED, int(currentExpire + timeToWait) );
                }
                else
                {
                    data->m_retries++;
                }
            }            

            break;
        }
    }


    //
    // If we didn't find anything then send a request for the data
    // Also log the request, so we dont flood

    if( !result )
    {
        if( !found )
        {
            MetaServerData *data = new MetaServerData();
            strcpy( data->m_dataType, _dataType );
            data->m_data = NULL;
            data->m_timeOfExpire = GetHighResTime() + 1.0f;
            s_metaServerData.PutData( data );
        }
    }

    s_metaServerDataMutex.Unlock();

    if( sendRequest && s_connectedToWAN )
    {
        Directory request;
        request.SetName( NET_METASERVER_MESSAGE );
        request.CreateData( NET_METASERVER_COMMAND, NET_METASERVER_REQUEST_DATA );
        request.CreateData( NET_METASERVER_DATATYPE, _dataType );

		//
		// Add current language
		if( g_languageTable->m_lang )
		{
			request.CreateData( NET_METASERVER_LANGUAGE, g_languageTable->m_lang->m_name );
		}

        //
        // Try to get our game name and version from the Client properties
        s_clientPropertiesMutex.Lock();
        if( s_clientProperties )
        {
            request.CreateData( NET_METASERVER_GAMENAME, s_clientProperties->GetDataString( NET_METASERVER_GAMENAME ) );
            request.CreateData( NET_METASERVER_GAMEVERSION, s_clientProperties->GetDataString( NET_METASERVER_GAMEVERSION ) );
        }
        s_clientPropertiesMutex.Unlock();


        MetaServer_SendToMetaServer( &request );

        AppDebugOut( "Requesting data from MetaServer : %s\n", _dataType );
    }        
    

    return result;
}


void MetaServer_RemoveData( char *_dataType )
{
    s_metaServerDataMutex.Lock();
    
    for( int i = s_metaServerData.Size() - 1; i >= 0; i-- )
    {
        MetaServerData *data = s_metaServerData[i];
        if( stricmp( data->m_dataType, _dataType ) == 0 )
        {
            s_metaServerData.RemoveData(i);
            delete data->m_data;
            delete data;
        }
    }

    s_metaServerDataMutex.Unlock();
}


int MetaServer_GetServerTTL()
{
    int serverTTL = 60;

    //
    // Do we have a definitive answer?

    Directory *serverTTLDir = MetaServer_RequestData( NET_METASERVER_DATA_SERVERTTL );

    if( serverTTLDir &&
        serverTTLDir->HasData( NET_METASERVER_DATA_SERVERTTL, DIRECTORY_TYPE_STRING ) )
    {
        serverTTL = atoi( serverTTLDir->GetDataString( NET_METASERVER_DATA_SERVERTTL ) );
    }


    //
    // Sanity check the answer

    serverTTL = max( serverTTL, 10 );

    return serverTTL;
}


int MetaServer_BytesSent()
{
    int result = s_bytesSent;
    s_bytesSent = 0;
    return result;
}


int GetServerSortScore( GameServer *server )
{
    int inProgress = server->m_data->GetDataUChar( NET_METASERVER_GAMEINPROGRESS );
    int numPlayers = server->m_data->GetDataUChar( NET_METASERVER_NUMTEAMS );
    int maxPlayers = server->m_data->GetDataUChar( NET_METASERVER_MAXTEAMS );
    int numSpectators = server->m_data->GetDataUChar( NET_METASERVER_NUMTEAMS );
    int maxSpectators = server->m_data->GetDataUChar( NET_METASERVER_MAXTEAMS );

    //char *authKey = server->m_data->GetDataString( NET_METASERVER_AUTHKEY );
    //bool demoServer = Authentication_IsDemoKey(authKey);


    if( inProgress == 0 )
    {
        // Not yet started
        if( numPlayers < maxPlayers )           return 0;
        if( numSpectators < maxSpectators )     return 10;
        return 20;
    }
    else if( inProgress == 1 )
    {
        // Game running
        if( numSpectators < maxSpectators )     return 30 - numPlayers;
        return 40;
    }
    else if( inProgress == 2 )
    {
        // Game over       
        return 50;
    }   

    // Unknown
    return 50;
}

void MetaServer_GraduallySortList()
{
    s_serverListMutex.Lock();

    if( s_serverList.Size() )
    {
        GameServer *prevServer = s_serverList[0];
        int prevScore = GetServerSortScore( prevServer );

        for( int i = 1; i < s_serverList.Size(); ++i )
        {
            GameServer *nextServer = s_serverList[i];
            int thisScore = GetServerSortScore( nextServer );

            if( prevScore > thisScore )
            {   
                // These two are the wrong way around, so swap
                s_serverList.RemoveData(i);
                s_serverList.PutDataAtIndex( nextServer, i-1 );
            }
            else
            {
                // These two are fine, move on
                prevServer = nextServer;
                prevScore = thisScore;
            }
        }
    }

    s_serverListMutex.Unlock();
}


void MetaServer_DelistServers( char *_authKey )
{
    s_serverListMutex.Lock();

    for( int i = 0; i < s_serverList.Size(); ++i )
    {
        GameServer *server = s_serverList[i];
        if( server->m_data )
        {
            char *thisAuthKey = server->m_data->GetDataString( NET_METASERVER_AUTHKEY );
            
            if( stricmp( thisAuthKey, _authKey ) == 0 )
            {
                s_serverList.RemoveData(i);
                delete server->m_data;
                delete server;        
                --i;
            }
        }
    }

    s_serverListMutex.Unlock();
}

