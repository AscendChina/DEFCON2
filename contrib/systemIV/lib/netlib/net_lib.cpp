#include "lib/universal_include.h"

#include <stdarg.h>
#include <stdio.h>

#include "net_lib.h"
#include "lib/debug_utils.h"


// ****************************************************************************
// Class NetLib
// ****************************************************************************

NetLib::NetLib()
{
}


NetLib::~NetLib()
{
#ifdef WIN32
    WSACleanup();
#endif
}


bool NetLib::Initialise()
{
#ifdef WIN32
    WORD versionRequested;
    WSADATA wsaData;
    
    versionRequested = MAKEWORD(2, 2);
 
    if (WSAStartup(versionRequested, &wsaData))
    {
        AppDebugOut("WinSock startup failed\n");
        return false;
    }
 
    // Confirm that the WinSock DLL supports 2.2. Note that if the DLL supports
	// versions greater than 2.2 in addition to 2.2, it will still return
    // 2.2 in wVersion since that is the version we requested.                                       
    if ((LOBYTE(wsaData.wVersion) != 2) || (HIBYTE(wsaData.wVersion) != 2))
    {
        // Tell the user that we could not find a usable WinSock DLL
        WSACleanup();
        AppDebugOut("No valid WinSock DLL found\n");
        return false; 
    }

    AppDebugOut( "WinSock started\n" );
#endif
    
    return true;
}


int GetLocalHost()
{
    //
    // Get our host name

    char str[512];
    int result = gethostname( str, sizeof(str) );
    if (result < 0) 
    {
        return 0;
    }


    //
    // Convert it to a hostent

    struct hostent *lpHostEnt;
    lpHostEnt = gethostbyname (str);

    //
    // Print it to an int

    NetIpAddress addr;
    memcpy( &addr.sin_addr, lpHostEnt->h_addr_list[0], sizeof(addr.sin_addr) );
    addr.sin_port = 0;

    const unsigned char *a = (const unsigned char *) &addr.sin_addr;
    result = (a[0] << 24) + (a[1] << 16) + (a[2] << 8) + a[3];

    return result;
}

#ifdef TARGET_MSVC
void GetLocalHostIP( char *str, int len )
{
	static bool retrievedAddress = false;
	static char localHostIP[16] = "127.0.0.1";

	if (!retrievedAddress)
	{
		retrievedAddress = true;

		//
		// Get our host name

		char hostname[256];
		int result = gethostname( hostname, sizeof(hostname) );
		if (result >= 0) 
		{

			//
			// Convert it to a hostent

			struct hostent *lpHostEnt;
			lpHostEnt = gethostbyname( hostname );
			if (lpHostEnt)
			{

				//
				// Print it to an IP

				NetIpAddress addr;
				memcpy( &addr.sin_addr, lpHostEnt->h_addr_list[0], sizeof(addr.sin_addr) );
				addr.sin_port = 0;

				IpAddressToStr( addr, localHostIP );

				//
				// Bogus IP address if begins by '0', most probably the user don't have a configured network card 

				if( localHostIP[0] == '0' )
				{
					strncpy( localHostIP, "127.0.0.1", sizeof(localHostIP) );
					localHostIP[ sizeof(localHostIP) - 1 ] = '\0';
				}
			}
		}

		if (strcmp( localHostIP, "127.0.0.1" ) == 0)
		{
			AppDebugOut( "Local IP address is 127.0.0.1, only local games possible\n" );
		}
		else
		{
			AppDebugOut( "Local IP address is: %s\n", localHostIP );
		}
	}

	strncpy( str, localHostIP, len );
	str[ len - 1 ] = '\0';
}	
#else
void GetLocalHostIP( char *str, int len )
{
	static bool retrievedAddress = false;
	static char localHostIP[16];

	if (!retrievedAddress) 
	{
		// Try to find the IP address of the default route (without using name lookup)
		NetSocket s;
	
		// It doesn't actually matter whether the host is reachable
		// Since UDP is a connectionless protocol, the only effect 
		// of connect is to see if there is a route to the specified
		// host.
		
		if (s.Connect( "80.175.29.66", 5000 ) == NetOk) 
		{
			NetIpAddress localAddress = s.GetLocalAddress();

			// On some systems (Win98) this can return 0.0.0.0 
			// which is not a valid IP to connect to.
			if (* (unsigned long *) (&localAddress.sin_addr) != 0) {
				IpAddressToStr( localAddress, localHostIP );
				retrievedAddress = true;
			}

			// It seems that on some systems this can return 0.0.0.0, strange, strange!
			if (strcmp( localHostIP, "0.0.0.0" ) != 0)
				retrievedAddress = true;
		}

		if (!retrievedAddress) {
			// Not connected to the internet (or 0.0.0.0 returned), revert to 
			// host name lookup method

			// Default fallback
			strcpy( localHostIP, "127.0.0.1" );

			// Get our host name
			char hostname[256];
			int result = gethostname( hostname, sizeof(hostname) );
			if (result >= 0) {
				struct hostent *lpHostEnt = gethostbyname( hostname );

				if (lpHostEnt) {
					NetIpAddress addr;
					memcpy( &addr.sin_addr, lpHostEnt->h_addr_list[0], sizeof(addr.sin_addr) );
					addr.sin_port = 0;

					IpAddressToStr( addr, localHostIP );
				}
			}
			
		}

		AppDebugOut("Local IP address is: %s\n", localHostIP );
		retrievedAddress = true;
	}

	strncpy( str, localHostIP, len );
	str[ len - 1 ] = '\0';
}
#endif

void IpAddressToStr(const NetIpAddress &address, char *str)
{
#ifdef TARGET_MSVC
	sprintf ( str, "%u.%u.%u.%u", address.sin_addr.S_un.S_un_b.s_b1,
			  address.sin_addr.S_un.S_un_b.s_b2,
			  address.sin_addr.S_un.S_un_b.s_b3,
			  address.sin_addr.S_un.S_un_b.s_b4 );
#else
	const unsigned char *a = (const unsigned char *) &address.sin_addr;
	sprintf ( str, "%u.%u.%u.%u", a[0], a[1], a[2], a[3] );
#endif	
}
