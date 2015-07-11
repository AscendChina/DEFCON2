
#ifndef _included_world_h
#define _included_world_h

#include "network/ClientToServer.h"

#include "lib/tosser/llist.h"
#include "lib/tosser/fast_darray.h"
#include "lib/tosser/bounded_array.h"
#include "lib/math/vector3.h"

#include "renderer/animated_icon.h"

#include "world/worldobject.h"
#include "world/team.h"
#include "world/date.h"
#include "world/node.h"
#include "world/gunfire.h"
#include "world/explosion.h"
#include "world/radargrid.h"
#include "world/votingsystem.h"
#include "world/whiteboard.h"



class Island;
class Silo;
class City;
class WorldMessage;
class ChatMessage;
class Spectator;
struct CityLocation;


#define MAX_TEAMS				7
#define TEAMID_SPECIALOBJECTS	255
#define OBJECTID_CITYS          100000
#define COLOUR_SPECIALOBJECTS   Colour(50,50,200,200)

#define GAMESPEED_PAUSED        0
#define GAMESPEED_REALTIME      1
#define GAMESPEED_SLOW          5
#define GAMESPEED_MEDIUM        10
#define GAMESPEED_FAST          20


class World
{
protected:    
    int         m_timeScaleFactor;
    int         m_nextUniqueId;
    
public:
    enum
    {
        TerritoryNorthAmerica,
        TerritorySouthAmerica,
        TerritoryEurope,
        TerritoryRussia,
        TerritorySouthAsia,
        TerritoryAfrica,
        NumTerritories
    };
    
    enum 
    {
        SpecialActionLandingAircraft,
        SpecialActionFormingSquad
    };

    enum
    {
        CityWashingtonDC,
        CityNewYork,
        CitySanFransisco,
        CityMexico,
        CitySaoPaolo,
        CityLima,
        CityLondon,
        CityRome,
        CityBerlin,
        CityCairo,
        CityKinshasa,
        CityMoscow,
        CityLeningrad,
        CityTokyo,
        CityHonkKong,
        CityShanghai,
        NumAchievementCities
    };


    LList           <City *>            m_cities;
    FastDArray      <WorldObject *>     m_objects;
    LList           <Node *>            m_nodes;
    FastDArray      <GunFire *>         m_gunfire;
    FastDArray      <Explosion *>       m_explosions;    
    LList           <WorldMessage *>    m_messages;   
    LList           <ChatMessage *>     m_chat;   
    LList           <Team *>            m_teams;
    LList           <Spectator *>       m_spectators;
    BoundedArray    <Vector3<Fixed> >   m_populationCenter;     // Indedex on territory

    LList           <Vector3<Fixed> *>  m_aiPlacementPoints;
    LList           <Vector3<Fixed> *>  m_aiTargetPoints;

    LList           <Vector3<Fixed> *>  m_radiation;
    LList           <int>               m_populationTotals;     // used for resetting cities in the tutorial

    int             m_myTeamId;
    Date            m_theDate;
    RadarGrid       m_radarGrid;
    VotingSystem    m_votingSystem;

    BoundedArray<int>   m_defconTime;     // time in minutes when each defcon starts
    BoundedArray<bool>  m_firstLaunch;    // 

	WhiteBoard      m_whiteBoards[MAX_TEAMS];

    bool            m_achievementCitiesNuked[NumAchievementCities];

    int             m_numNukesGivenToEachTeam;

public:   
    World();

    void Init();
    void Shutdown();
    void LoadCoastlines();

    int  GenerateUniqueId   ();
    
    void InitialiseTeam     ( int teamId, int teamType, int clientId );
    void RemoveTeams        ( int clientId, int reason );
    void RemoveSpectator    ( int clientId );
    void ReassignTeams      ( int clientId );
    void RemoveAITeam       ( int _teamId );
    void RemoveTeam         ( int _teamId );    
    void InitialiseSpectator( int _clientId );

    void LoadNodes          ();
    void AssignCities       ();
    void RandomObjects      ( int teamId );
    
    Team *GetTeam           ( int teamId );
    Team *GetMyTeam         ();

    bool AmISpectating          ();
    int  IsSpectating           ( int _clientId );
    Spectator *GetSpectator     ( int _clientId );

    static Colour GetAllianceColour    ( int _allianceId );
    char  *GetAllianceName      ( int _allianceId );
    bool   IsFriend             ( int _teamId1, int _teamId2 );
    int    CountAllianceMembers ( int _allianceId );
    int    FindFreeAllianceId   ();

    int  AddWorldObject     ( WorldObject *wobj );
    WorldObject *GetWorldObject( int _uniqueId );

    bool IsValidPlacement   ( int teamId, Fixed longitude, Fixed latitude, int objectType );    
    int  GetNearestObject   ( int teamId, Fixed longitude, Fixed latitude, int objectType=-1, bool enemyTeam = false );
    void LaunchNuke         ( int teamId, int objId, Fixed longitude, Fixed latitude, Fixed range );
    void CreateExplosion    ( int teamId, Fixed longitude, Fixed latitude, Fixed intensity, int targetTeamId=-1 );

    bool IsVisible          ( Fixed longitude, Fixed latitude, int teamId );

    void ObjectPlacement        ( int teamId, int unitType, Fixed longitude, Fixed latitude, int fleetId );
    void ObjectStateChange      ( int objectId, int newState );
    void ObjectAction           ( int objectId, int targetObjectId, Fixed longitude, Fixed latitude, bool pursue = false );
    void ObjectSpecialAction    ( int objectId, int targetObjectId, int specialActionType );
    void ObjectSetWaypoint      ( int objectId, Fixed longitude, Fixed latitude );
    void ObjectClearActionQueue ( int objectId );
    void ObjectClearLastAction  ( int objectId );
    void FleetSetWaypoint       ( int teamId, int fleetId, Fixed longitude, Fixed latitude );
    void TeamCeaseFire          ( int teamId, int targetTeam );
    void TeamShareRadar         ( int teamId, int targetTeam );
    void WhiteBoardAction       ( int teamId, int action, int pointId, Fixed longitude, Fixed latitude, Fixed longitude2, Fixed latitude2 );

    void AssignTerritory    ( int territoryId, int teamId, int addOrRemove );
    void RequestAlliance    ( int teamId, int allianceId );

    int  GetAttackOdds      ( int attackerType, int defenderType, int attackerId );
    int  GetAttackOdds      ( int attackerType, int defenderType );
    
    void  SetTimeScaleFactor( Fixed factor );
    Fixed GetTimeScaleFactor();
    bool  CanSetTimeFactor  ( Fixed factor );

    int   GetDefcon();

    void AddWorldMessage        ( Fixed longitude, Fixed latitude, int teamId, char *msg, int msgType, bool showOnToolbar=true );
    void AddWorldMessage        ( int objectId, int teamId, char *msg, int msgType, bool showOnToolbar=true );
    void AddDestroyedMessage    ( int attackerId, int defenderId, bool explosion = false );
    void AddOutOfFueldMessage   ( int objectId );

    void AddChatMessage         ( int teamId, int channel, char *_msg, int msgId, bool spectator );
    bool IsChatMessageVisible   ( int teamId, int channel, bool spectator );

    void Update         ();
    void UpdateRadar    ();

	void GenerateWorldEvent ();
    

    bool  IsSailable                ( Fixed const &fromLongitude, Fixed const &fromLatitude, Fixed const &toLongitude, Fixed const &toLatitude);
    bool  IsSailableSlow            ( Fixed const &fromLongitude, Fixed const &fromLatitude, Fixed const &toLongitude, Fixed const &toLatitude);
    int   GetClosestNode            ( Fixed const &longitude, Fixed const &latitude );
    int   GetClosestNodeSlow        ( Fixed const &longitude, Fixed const &latitude );

    Fixed GetDistanceAcrossSeam     ( Fixed const &fromLongitude, Fixed const &fromLatitude, Fixed const &toLongitude, Fixed const &toLatitude );
    Fixed GetDistanceAcrossSeamSqd  ( Fixed const &fromLongitude, Fixed const &fromLatitude, Fixed const &toLongitude, Fixed const &toLatitude );
    Fixed GetDistance               ( Fixed const &fromLongitude, Fixed const &fromLatitude, Fixed const &toLongitude, Fixed const &toLatitude, bool ignoreSeam = false );
    Fixed GetDistanceSqd            ( Fixed const &fromLongitude, Fixed const &fromLatitude, Fixed const &toLongitude, Fixed const &toLatitude, bool ignoreSeam = false );
    Fixed GetSailDistance           ( Fixed const &fromLongitude, Fixed const &fromLatitude, Fixed const &toLongitude, Fixed const &toLatitude );
    Fixed GetSailDistanceSlow       ( Fixed const &fromLongitude, Fixed const &fromLatitude, Fixed const &toLongitude, Fixed const &toLatitude );
    
    void  GetSeamCrossLatitude  ( Vector3<Fixed> _to, Vector3<Fixed> _from, Fixed *longitude, Fixed *latitude );
    int   GetTerritoryOwner     ( int territoryId );

    int  GetUnitValue( int _type );

    static Fixed GetGameScale();

    void ClearWorld();      // used in the tutorial to move between missions

    static void ParseCityDataFile();
    void        WriteNodeCoverageFile();

    void    GetNumNukers            ( int objectId, int *inFlight, int *queued );
    void    CreatePopList           ( LList<int> *popList );
};




// ============================================================================


class WorldMessage
{
public:
    WorldMessage()
        :   m_longitude(0), 
            m_latitude(0), 
            m_timer(0),
            m_teamId(-1),
            m_objectId(-1),
            m_messageType(-1), 
            m_deaths(0), 
            m_renderFull(false)
    {}

    enum
    {
        TypeObjectState,
        TypeFuel, 
        TypeLaunch,
        TypeDestroyed,
        TypeDirectHit,
        TypeInvalidAction,
        NumMessageTypes
    };

    void    SetMessage( char *_message );

    Fixed   m_longitude;
    Fixed   m_latitude;
    Fixed   m_timer;
    int     m_teamId;
    int     m_objectId;
    char    m_message[512];
    int     m_messageType;
    int     m_deaths;
    bool    m_renderFull;   // bypass the gradual drawing and render the full message
};


// ============================================================================

/* 
    Channel field :
    0 = private message to team 0
    1 = private message to team 1
        etc
    100 = public
    101 = alliance

  */

#define CHATCHANNEL_PUBLIC      100
#define CHATCHANNEL_ALLIANCE    101
#define CHATCHANNEL_SPECTATORS  102


class ChatMessage
{
public:
    int     m_fromTeamId; 
    int     m_channel;
    char    m_playerName[256];
    bool    m_spectator;
    int     m_messageId;
    char    *m_message;
    bool    m_visible;

public:
    ChatMessage()
        :   m_fromTeamId(-1),
            m_channel(0),
            m_spectator(false),
            m_messageId(-1),
            m_message(NULL),
            m_visible(false)
    {
        m_playerName[0] = '\x0';
    }
};




// ============================================================================

class Spectator
{
public:
    char    m_name[256];
    int     m_clientId;

public:
    Spectator()
        : m_clientId(-1)
    {
        m_name[0] = '\x0';
    }
};


#endif
