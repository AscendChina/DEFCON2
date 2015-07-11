// ****************************************************************************
// A UDP socket listener implementation. This class blocks until data is
// received, so you probably want to put it in its own thread.
// ****************************************************************************

#ifndef INCLUDED_NET_SOCKET_LISTENER_H
#define INCLUDED_NET_SOCKET_LISTENER_H


#include "net_lib.h"


class NetSocketListener
{
public:
	NetSocketListener(unsigned short port);
	~NetSocketListener();

	NetRetCode	StartListening(NetCallBack fnptr);
	
	// Asynchronously stops the listener after next accept call
	void		StopListening();

	// Called by NetSocketSession to get the socket
	NetSocketHandle		GetBoundSocketHandle();

	NetRetCode	Bind();
	
    NetRetCode  EnableBroadcast();

	int			GetPort() const;

	NetIpAddress	GetListenAddress() const;
	
protected:
	NetSocketHandle 	m_sockfd;
	bool				m_binding;
	bool				m_listening;
	unsigned short	 	m_port;
};

#endif
