#ifndef _included_serverdetailswindow_h
#define _included_serverdetailswindow_h

#include "interface/components/core.h"

class Directory;


class ServerDetailsWindow : public InterfaceWindow
{
public:
    char        m_serverIp[256];
    int         m_serverPort;

    double      m_refreshTimer;
    int         m_numRefreshes;
    Directory   *m_details;

public:
    ServerDetailsWindow( char *_serverIp, int _serverPort );

    void Create();
    void Render( bool _hasFocus );
    void Update();
};


#endif
