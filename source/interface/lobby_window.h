
#ifndef _included_lobbywindow_h
#define _included_lobbywindow_h

#include "lib/tosser/bounded_array.h"
#include "lib/tosser/directory.h"

#include "interface/components/core.h"

#include "world/world.h"

class GameOption;

// ============================================================================


class LobbyWindow : public InterfaceWindow
{   
public:
    BoundedArray    <GameOption *> m_gameOptions;    
    float           m_updateTimer;
    int             m_teamOrder[MAX_TEAMS];
    int             m_selectionId;
    int             m_gameMode;

public:
    LobbyWindow();
    ~LobbyWindow();

    bool StartNewServer();
    
    void Create();
    void Update();
    void Render( bool _hasFocus );

    void OrderTeams();

    void CreateAggressionControl( char *name, int x, int y, int w, int h,
                                  int dataType, void *value, float change, float _lowBound, float _highBound);
};


// ============================================================================

class LobbyOptionsWindow : public InterfaceWindow
{
protected:
    BoundedArray    <GameOption *> m_gameOptions;    
    float           m_updateTimer;
    int             m_gameMode;

public:
    LobbyOptionsWindow();

    void Create();
    void Update();
    void Render( bool _hasFocus );
};

// ============================================================================


class TeamOptionsWindow : public InterfaceWindow
{
public:
    int m_teamId;

public:
    TeamOptionsWindow( int teamId );

    void Create();
    void Update();
};


// ============================================================================

class PasswordWindow : public InterfaceWindow
{
public:
    char m_password[128];

public:
    PasswordWindow();

    void Create();
    void Render( bool hasFocus );
};


// ============================================================================

class RenameTeamWindow : public InterfaceWindow
{
public:
    char m_teamName[128];
    int  m_teamId;

public:
    RenameTeamWindow( int _teamId = -1 );

    void Create();
    void Render( bool hasFocus );
};



#endif
