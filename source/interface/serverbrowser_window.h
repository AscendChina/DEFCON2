
#ifndef _included_serverbrowserwindow_h
#define _included_serverbrowserwindow_h

#include "interface/components/core.h"

class Directory;
class ScrollBar;



class ServerBrowserWindow : public InterfaceWindow
{
public:
    LList       <Directory *> *m_serverList;
    double      m_requestTimer;
    double      m_relistTimer;
    char        m_serverIp[512];
    int         m_selection;
    int         m_receivedServerListSize;
    double      m_doubleClickTimer;
    
    bool        m_showSinglePlayerGames;
    bool        m_showDemoGames;
    bool        m_showDefaultNamedServers;

    enum
    {
        ListTypeAll,
        ListTypeLobby,
        ListTypeInProgress,
        ListTypeRecent,
        ListTypeLAN,
        ListTypeEnterIPManually
    };
    int m_listType;                                     

    enum
    {
        SortTypeDefault,
        SortTypeName,
        SortTypeGame,
        SortTypeMod,
        SortTypeTime,
        SortTypeSpectators,
        SortTypeTeams
    };
    int     m_sortType;
    bool    m_sortInvert;

public:
    ScrollBar   *m_scrollBar;

protected:
    int ScoreServer_Default     ( Directory *_server );
    int ScoreServer_Name        ( Directory *_server );
    int ScoreServer_Time        ( Directory *_server );
    int ScoreServer_Game        ( Directory *_server );
    int ScoreServer_Mods        ( Directory *_server );
    int ScoreServer_Spectators  ( Directory *_server );
    int ScoreServer_Teams       ( Directory *_server );

public:
    ServerBrowserWindow();
    ~ServerBrowserWindow();

    void Create();
    void Update ();
    void Render( bool _hasFocus );

    void FilterServerList();                         
    void SortServerList();
    void ClearServerList();

    static bool ConnectToServer( Directory *_server, const char *_serverPassword = NULL );
    static bool IsOurServer( char *_ip, int _port );
    bool IsServerInList( char *_ip, int _port );
};


// ============================================================================


class FiltersWindow : public InterfaceWindow
{
public:
    FiltersWindow();

    void Create();
    void Render( bool _hasFocus );
};


// ============================================================================

class ServerPasswordWindow : public InterfaceWindow
{
public:
    char    m_password[128];
    char    m_ip[256];
    int     m_port;
    char    m_serverName[256];

public:
    ServerPasswordWindow();

    void Create();
    void Render( bool hasFocus );
};


// ============================================================================


class EnterIPManuallyWindow : public InterfaceWindow
{
public:
    char    m_ip[256];
    int     m_port;

public:
    EnterIPManuallyWindow();

    void Create();
    void Render( bool _hasFocus );
};


#endif
