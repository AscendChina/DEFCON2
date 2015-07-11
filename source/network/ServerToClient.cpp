#include "lib/universal_include.h"

#include <string.h>

#include "lib/debug_utils.h"
#include "lib/netlib/net_socket.h"
#include "lib/metaserver/authentication.h"
#include "lib/metaserver/metaserver_defines.h"

#include "app/app.h"
#include "app/globals.h"

#include "network/ServerToClient.h"
#include "network/ClientToServer.h"
#include "network/Server.h"
#include "network/network_defines.h"



ServerToClient::ServerToClient( char *_ip, int _port,
                                NetSocketListener *_listener )
:   m_socket(NULL),
    m_clientId(-1),
    m_lastKnownSequenceId(-1),
    m_lastSentSequenceId(-1),
    m_port(_port),
    m_basicAuthCheck(0),
    m_lastMessageReceived(0.0f),
    m_caughtUp(false),
    m_disconnected(-1),
    m_spectator(false),
    m_authKeyId(0),
    m_syncErrorSeqId(-1),
    m_lastBackedUp(0.0f)
{   
    strcpy ( m_ip, _ip );
    strcpy ( m_system, " " );

    strcpy ( m_version, "1.0" );

    m_socket = new NetSocketSession(*_listener, _ip, _port);

    m_chatMessages.SetSize(100);
    m_chatMessages.SetStepSize(100);

    m_sync.SetSize(1000);
    m_sync.SetStepSize(1000);
}


ServerToClient::~ServerToClient()
{
    delete m_socket;
}


char *ServerToClient::GetIP()
{
    return m_ip;
}

NetSocketSession *ServerToClient::GetSocket ()
{
    return m_socket;
}


