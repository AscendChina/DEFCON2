#ifndef _included_allianceswindow_h
#define _included_allianceswindow_h

#include "interface/components/core.h"

#include "world/world.h"


class AlliancesWindow : public InterfaceWindow
{
public:
    int     m_teamOrder [MAX_TEAMS];
    int     m_votes     [MAX_TEAMS];
    int     m_selectionTeamId;

public:
    AlliancesWindow();

    void Create();
    void Update();
    void Render( bool _hasFocus );
};


// ============================================================================


class OldAlliancesWindow : public InterfaceWindow
{
public:
    OldAlliancesWindow();

    void Create();
    void Render( bool _hasFocus );
};




// ============================================================================

class VotingWindow : public InterfaceWindow
{
public:
    int m_voteId;

public:
    VotingWindow();

    void Create();
    void Render( bool _hasFocus );
};

// ============================================================================

class CeaseFireWindow: public InterfaceWindow
{
public:
    int m_teamId;

public:
    CeaseFireWindow( int teamId );
    void Create();
    void Render( bool hasFocus );
};


// ============================================================================

class ShareRadarWindow: public InterfaceWindow
{
public:
    int m_teamId;

public:
    ShareRadarWindow( int teamId );
    void Create();
    void Render( bool hasFocus );
};
#endif
