
#ifndef _included_team_h
#define _included_team_h

#include "lib/tosser/bounded_array.h"
#include "lib/render/renderer.h"

#include "world/worldobject.h"
#include "world/fighter.h"

#define MAX_TEAM_NAME 48

class Fleet;
class Event;

class Team
{
public:
    enum
    {
        TypeUnassigned,
        TypeLocalPlayer,
        TypeRemotePlayer,
        TypeAI
    };

    enum
    {
        DifficultyEasy,
        DifficultyNormal,
        DifficultyHard
    };

    enum
    {
        StatePlacement,     // initial unit placement
        StateScouting,      // no silo launches, airbases/carriers only launch fighters
        StateAssault,       // tactical strikes against enemy bases using subs/carriers
        StateStrike,         // Full strike against civs/remaining bases
        StatePanic,         // no silos left, all airbases/carriers switch to bomber launches only, try to cause as much damage as possible
        StateFinal         // Assault finished - now either reteating to own territory for defensive, or going fully offensive, depending on aggression value
    };

    enum
    {
        SubStateExploration,
        SubStateLaunchScouts,
        SubStateLaunchBombers,
        SubStateLaunchNukes,
        SubStateActivateSubs,
        NoSubState
    };


    int     m_type;
    int     m_clientId;
    int     m_teamId;
    int     m_allianceId;
    int     m_randSeed;
    char    m_name[MAX_TEAM_NAME];
    bool    m_nameSet;
    bool    m_readyToStart;
    int     m_desiredGameSpeed;
    
    int     m_enemyKills;
    int     m_friendlyDeaths;
    int     m_collatoralDamage;

    Fixed   m_aiActionTimer;    // Tracks time since last AI action. Used to tweak AI difficulty
	Fixed   m_aiActionSpeed;    // Fixed time between AI actions, determined by AI difficulty (when implemented). Set to 0.0f for now
    int     m_aggression;           // AI aggression from 1 to 10 - 10 being very aggressive
    Fixed   m_aiAssaultTimer;   // Timer left before AI is forced into assault mode
    int     m_targetsVisible;    // number of targets currently visible on enemy team
    int     m_maxTargetsSeen;   // maximum number of targets seen by the AI    

	BoundedArray<int> m_unitsAvailable; // used as remaining units in fixed army mode, or as the max cap for each type in variable mode
    BoundedArray<int> m_unitsInPlay;  // list of how many of each type of unit is still alive
    int     m_unitCredits;      // number of 'credits' for units in VariableUnit mode

    int     m_numTerritories;
    LList<int> m_territories;

    int     m_currentState;
    int     m_subState;
    int     m_previousState;
    Fixed   m_aiStateTimer;
    Fixed   m_siloSwitchTimer;      // used to stagger silo nuke launches

    int     m_targetTeam;
    int     m_validTargetPopulation;

    LList<Fleet *>  m_fleets;

    float   m_teamColourFader;           // Used to fade the team colours based on deaths

    LList<Event *>  m_eventLog;

    BoundedArray<bool> m_ceaseFire;
    BoundedArray<bool> m_sharingRadar;

    BoundedArray<float> m_leftAllianceTimer;    // achievement tracking only
    bool    m_alwaysSolo;
    
public:
    Team();

    void SetTeamType        ( int type );
    void SetTeamId          ( int teamId );
    void SetTeamName        ( char *name );
	static bool IsValidName ( char *name);


    void CheckPanicState    ();
	void RunAI			    ();
    void PlacementAI        ();
    void ScoutingAI         ();
    void AssaultAI          ();
    void StrikeAI           ();
    void HandleAIEvents     ();

	int GetPlacementPriority();
    int GetNumTargets       ();

    Fixed GetClosestObject  ( int cityId, int objectType ); // returns the distance to the closest friendly object of type objectType, -1 if none found
    bool CheckCityCoverage  ( int objectType, Fixed *longitude, Fixed *latitude ); // checks if a city is covered by radar/silo, and fills the long/lat with the city's location if is isnt
    bool IsSafeLocation     ( Fixed longitude, Fixed latitude); // returns false if the location is within nuke range (2.0f) of a city
    bool GetUncoveredArea   ( bool radar, Fixed *longitude, Fixed *latitude );
    int  CountTargettedUnits( int targetId );

    bool AssembleFleet      ( int fleetType );
    bool CanAssembleFleet   (); // checks all fleet constructions to make sure at least one valid fleet type is available
    bool CanAssembleFleet   ( int fleetType );

    Colour GetTeamColour    ();
    char *GetTeamType       ();
    char *GetTeamName       ();

    void AddTerritory       ( int territoryId );
    bool OwnsTerritory      ( int territoryId );
    void RemoveTerritory    ( int territoryId );
    void AssignAITerritory  ();
    void ChangeState        ( int newState );

    void CreateFleet        ();
    Fleet *GetFleet( int fleetId );

    int  GetRemainingPopulation();

    void AddEvent( int type, int objectId, int teamId, int fleetId, Fixed longitude, Fixed latitude );
    void DeleteEvent( int id );
    Event *GetEvent( int id );
};


// ============================================================================


class Event
{
public:
    enum
    {
        TypeEnemyIncursion,
        TypeSubDetected,
        TypeIncomingAircraft,
        TypeNukeLaunch
    };

    int m_type;
    int m_objectId;
    int m_actionTaken;  // incremented each time action is taken (ie. retaliatory nuke fired)
    int m_teamId;       // team of the object that caused the event
    int m_fleetId;

    Fixed m_longitude;
    Fixed m_latitude;

public:
    Event()
        :   m_type(-1),
            m_teamId(-1),
            m_fleetId(-1),
            m_objectId(-1),
            m_actionTaken(0),
            m_longitude(0),
            m_latitude(0)
        {
        }
};

#endif
