#include "lib/universal_include.h"
#include "lib/debug_utils.h"

#include "net_lib.h"
#include "net_socket.h"
#include "net_socket_listener.h"
#include "net_thread.h"
#include "net_udp_packet.h"


NetSocketListener::NetSocketListener(unsigned short port)
:   m_sockfd(-1), 
    m_binding(false), 
    m_listening(false),
    m_port(port)
{
}

NetSocketListener::~NetSocketListener()
{
	NetCloseSocket(m_sockfd);
}

NetSocketHandle NetSocketListener::GetBoundSocketHandle()
{
	if (Bind() == NetOk)
		return m_sockfd;
	else
		return -1;
}

int	NetSocketListener::GetPort() const
{
	return m_port;
}

NetRetCode NetSocketListener::Bind()
{
	if (int(m_sockfd) >= 0)
		return NetOk;
		 
	if ((m_sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
	{
		return NetFailed;
	}

	// We are starting the binding process (should really get this atomically)
	// The whole point of m_binding and m_listening are flags which can be set
	// from another thread to help the socket close down gracefully.
	m_binding = true;
	
	// Set up the server address
	NetIpAddress servaddr;
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(m_port);

	// Bind socket to port
	int bindAttempts = 0;
	while ((bind(m_sockfd, (sockaddr *)&servaddr, sizeof(servaddr)) == -1) && m_binding)
	{
		if ((bindAttempts++ == 5) || (!NetIsAddrInUse))
		{
			m_binding = false;
			return NetFailed;
		}
		else
		{
			AppDebugOut("NETLIB: Failed to bind to port %d\n", m_port);
			NetSleep(bindAttempts * 100);
		}
	}
	
	// If port was 0, let's see what the system assigned
	if (m_port == 0) {
		NetSocketLenType addr_len = sizeof(servaddr);
		if (getsockname(m_sockfd, (sockaddr *)&servaddr, &addr_len) == -1) {
			AppDebugOut("NETLIB: Failed to get system assigned port number\n");
			return NetFailed;
		}
		m_port = ntohs(servaddr.sin_port);
	}
	
	if (m_binding) 
	{
		m_binding = false;
		return NetOk;
	}
	else
		return NetFailed;
}

NetIpAddress NetSocketListener::GetListenAddress() const
{
	NetIpAddress addr;
	socklen_t size = sizeof(addr);

	getsockname(m_sockfd, (sockaddr *) &addr, &size );
	return addr;
}


NetRetCode NetSocketListener::EnableBroadcast()
{
    int opt = 1;
    int result = setsockopt(m_sockfd, SOL_SOCKET, SO_BROADCAST, (char *)&opt, sizeof(int));

    if( result == 0 ) return NetOk;

    return NetFailed;
}


void DebugOutNetSocketError( int _errorCode )
{
#ifdef WIN32
    switch( _errorCode )
    {
        case WSAEINTR:                          
            // Interrupted function call : not really an error, we've just been shut down            
            break;

        case WSAECONNRESET:
            AppDebugOut( "NetSocketListener : Connection Reset by Peer\n" );
            break;

        default:
            AppDebugOut( "NetSocketListener : Error reading UDP data (errno=%d)\n", _errorCode );            
            break;
    }
#else
	// TODO
#endif
}


NetRetCode NetSocketListener::StartListening(NetCallBack functionPointer)
{
	// Ensure that we are bound 
	NetRetCode ret = NetOk;
	if ((ret = Bind()) != NetOk)
		return ret;

	NetIpAddress clientaddr;
	memset(&clientaddr, 0, sizeof(clientaddr));
		
	// Make sure incoming arguments make sense
	if (functionPointer == (NetCallBack)NULL)
	{
		return NetBadArgs;
	}
	
	// Signal that we should be listening
	m_listening = true;

	NetUdpPacket *packet = new NetUdpPacket();
	
	while (m_listening)
	{
		NetSocketLenType clientAddrLen = sizeof(packet->m_clientAddress);
		
		packet->m_length = recvfrom(m_sockfd, packet->m_data, MAX_PACKET_SIZE, 0, 
			                        (struct sockaddr *)&packet->m_clientAddress, &clientAddrLen);
		            
	    if(packet->m_length <= 0)
	    {   
            int errorCode = NetGetLastError();            
	        DebugOutNetSocketError( NetGetLastError() );
			// Cycle round and reuse the packet
	    }
	    else
	    {
			// Call function pointer with datagram data (type is NetUdpPacket) -
			// the function pointed to must free NetUdpPacket passed in
			(*functionPointer)(packet);
			packet = new NetUdpPacket();
	    }
	}
	
	delete packet;

	return NetOk;
}

   
void NetSocketListener::StopListening()
{
	m_listening = false;
	m_binding = false;
	NetSleep(250);
	if (int(m_sockfd) >= 0)
	{
		shutdown(m_sockfd, 0);
	}

	NetCloseSocket(m_sockfd);
}
