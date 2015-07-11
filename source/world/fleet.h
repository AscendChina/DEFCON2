
#ifndef _included_fleet_h
#define _included_fleet_h

#include "lib/tosser/bounded_array.h"
#include "lib/render/renderer.h"
#include "lib/math/fixed.h"

class Fleet
{
public:
    
    int         m_fleetId;
    int         m_teamId;
    //int         m_fleetLeader;
    LList<int>  m_fleetMembers;
    LList<int>  m_memberType;
    LList<int>  m_lastHitByTeamId;
    bool        m_active;

    Fixed       m_longitude;        // average longitude of all fleet members
    Fixed       m_latitude;         // average latitude of all fleet members

    Fixed       m_targetLongitude;
    Fixed       m_targetLatitude;
    int         m_targetNodeId;
    Fixed       m_pathCheckTimer;
    bool        m_crossingSeam;
    bool        m_atTarget;
    Fixed       m_speedUpdateTimer;

    int         m_currentState; // combat state (aggressive/defensive/passive)
    bool        m_holdPosition;

    int         m_fleetType;
    int         m_aiState;
    bool        m_defensive;

    int         m_targetObjectId;  // used for the AI intercept state

    bool        m_pursueTarget; //
    int         m_targetTeam;   //  } used when a fleet needs to follow a target
    int         m_targetFleet;  //

    int         m_subNukesLaunched;

    Fixed       m_blipTimer;
    LList       <Blip *> m_movementBlips;

    LList<int>  m_pointIgnoreList;  // list of nuke points that the fleet should ignore because no targets were found within range
    bool        m_niceTryChecked;

    enum
    {
        FleetStateAggressive,
        FleetStateDefensive,
        FleetStatePassive
    };

    enum
    {
        FleetTypeBattleships,
        FleetTypeScout,
        FleetTypeNuke,
        FleetTypeSubs,
        FleetTypeMixed,
        FleetTypeAntiSub,
        FleetTypeDefender,
        FleetTypeDefender2,
        FleetTypeRandom,
        NumFleetTypes
    };

    enum
    {
        AIStateHunting,         // generic aggressive state. moves toward enemy territory to engage hostile ships
        AIStateScouting,        // launching fighters over enemy territory to reveal buildings
        AIStateDefending,       // moving around friendly territory looking for hostile ships
        AIStateSneaking,        // trying to covertly move into enemy territory, preferably close to coastline
        AIStateNuking,          // launching nukes/bombers at enemy territory.
        AIStateRetreating,      // retreating back to friendly territory. use after nuke strikes, or by sub hunters tracking subs
        AIStateIntercepting,    // intercepting a specified unit/fleet
    };

public:

    Fleet();
    void Update();
    void GetFormationPosition( int memberCount, int memberId, Fixed *longitude, Fixed *latitude );
    int  GetFleetMemberId( int objectId );

    void AlterWaypoints             ( Fixed longitude, Fixed latitude, bool checkNewWaypoint );
    void MoveFleet                  ( Fixed longitude, Fixed latitude, bool cancelPursuits = true );
    void FleetAction                ( int targetObjectId );
    bool IsValidFleetPosition       ( Fixed longitude, Fixed latitude );
    bool ValidFleetPlacement        ( Fixed longitude, Fixed latitude );
    bool FindGoodAttackSpotSlow     ( Fixed &_longitude, Fixed &_latitude );
    bool FindGoodAttackSpot         ( Fixed &_longitude, Fixed &_latitude );
    void StopFleet();

    void GetFleetPosition( Fixed *longitude, Fixed *latitude );
    bool IsAtTarget();
    void CheckFleetFormation();
    void CounterSubs();

    // Ai functions
    void PlaceFleet( Fixed longitude, Fixed latitude );

    void RunAI();
    void RunAIHunting();
    void RunAISneaking();
    void RunAIIntercepting();

    int CountSurvivingMembers();
    
    bool IsIgnoringPoint( int pointId );
    bool IsFleetMoving();
    bool IsFleetIdle();
    bool IsInCombat();
    bool IsInFleet( int objectType );
    bool IsOnSameSideOfSeam(); // checks to make sure all ships are on the same side of the seam
    int  CountNukesInFleet();
    bool IsInNukeRange();
    bool IgnoreSpeedDropoff(); // fleet ignores speed dropoff rules because it consists entirely of hidden subs

    bool FindBlip( int type );
    void CreateBlip();
    void CreateBlip( Fixed longitude, Fixed latitude, int type );

    Fixed GetSpeedDropoff();    // returns the % speed reduction based on nearby enemy fleets, caped a 80%

    char *LogState();

    static void GetFleetMembers( int fleetType, int *ships, int *subs, int *carriers );
};

#endif
