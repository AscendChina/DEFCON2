
#ifndef _included_game_h
#define _included_game_h

#include "lib/tosser/llist.h"
#include "lib/tosser/bounded_array.h"
#include "lib/math/fixed.h"


class GameOption;

#define GAMEMODE_STANDARD      0
#define GAMEMODE_OFFICEMODE    1
#define GAMEMODE_SPEEDDEFCON   2
#define GAMEMODE_DIPLOMACY     3
#define GAMEMODE_BIGWORLD      4
#define GAMEMODE_TOURNAMENT    5
#define GAMEMODE_CUSTOM        6

// ============================================================================
// class Game


class Game
{
public:
    Fixed           m_recalcTimer;
    Fixed           m_victoryTimer;                                     // Triggered once nuke count is low enough
    Fixed           m_maxGameTime;
    bool            m_gameTimeWarning;
    bool            m_lockVictoryTimer;
    int             m_winner;
    int             m_gameMode;

    int             m_lastKnownDefcon;
    
    LList           <GameOption *>  m_options;
    
    BoundedArray    <int>           m_score;                            // Indexed on teamID
    BoundedArray    <int>           m_nukeCount;                        
    BoundedArray    <int>           m_totalNukes;                       // the total nukes a player has, calculated once defcon 3 hits

    int             m_pointsPerSurvivor;
    int             m_pointsPerDeath;
    int             m_pointsPerKill;
    int             m_pointsPerNuke;
    int             m_pointsPerCollatoral;

protected:
    void            CountNukes();
    void            CalculateScores();

public:
    Game();

    void            Update();

    int             GetScore        ( int _teamId );

    void            ResetOptions    ();
    int             GetOptionValue  ( char *_name );
    void            SetOptionValue  ( char *_name, int _value );
    int             GetOptionIndex  ( char *_name );    
    GameOption      *GetOption      ( char *_name );

    void            SetGameMode     ( int _mode );
    bool            IsOptionEditable( int _optionId );

    void            ResetGame       ();
};



// ============================================================================
// class GameOption

class GameOption
{
public:
    char    m_name[256];
    float   m_min;
    float   m_max;
    float   m_default;
    int     m_change;    
    int     m_currentValue;
    char    m_currentString[256];
    
    LList   <char *> m_subOptions;

    void    Copy( GameOption *_option );

	static char* TranslateValue( char *_value );
};



#endif
