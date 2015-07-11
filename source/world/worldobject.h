
#ifndef _included_worldobject_h
#define _included_worldobject_h

#include "lib/tosser/llist.h"
#include "lib/tosser/bounded_array.h"
#include "lib/math/vector3.h"
#include "lib/math/fixed.h"

class Image;
class WorldObjectState;
class ActionOrder;

class WorldObject
{
public:
    enum
    {
        TypeInvalid,
        TypeCity,
        TypeSilo,
        TypeRadarStation,        
        TypeNuke,
        TypeExplosion,
        TypeSub,
        TypeBattleShip,
        TypeAirBase,
        TypeFighter,
        TypeBomber,
        TypeCarrier,
		TypeTornado,
        TypeSaucer,
        NumObjectTypes
    };

    enum
    {
        TargetTypeDefconRequired=-3,
        TargetTypeOutOfStock=-2,
        TargetTypeOutOfRange=-1,
        TargetTypeInvalid=0,
        TargetTypeValid,
        TargetTypeLaunchNuke,
        TargetTypeLaunchFighter,
        TargetTypeLaunchBomber,
        TargetTypeLand
    };

    int     m_type;
    int     m_teamId;
    int     m_objectId;
    Fixed   m_longitude;
    Fixed   m_latitude;
    int     m_life;                                     // Cities population, or 1/0 for sea units, or 0-30 for ground units
	int		m_lastHitByTeamId;
    bool    m_selectable;
    
    Vector3<Fixed> m_vel;

    LList   <WorldObjectState *> m_states;
    int     m_currentState;
    int     m_previousState;
    Fixed   m_stateTimer;                               // Time until we're in this state
    Fixed   m_previousRadarRange;                       // The size we previously added into the radar grid

    BoundedArray<bool>      m_visible;
    BoundedArray<bool>      m_seen;                     // seen objects memory
    BoundedArray<Vector3<Fixed> >   m_lastKnownPosition;
    BoundedArray<Vector3<Fixed> >   m_lastKnownVelocity;
    BoundedArray<Fixed>     m_lastSeenTime;
    BoundedArray<int>       m_lastSeenState;
    
    LList<ActionOrder *> m_actionQueue;

    int     m_fleetId;
    int     m_nukeSupply;               // for reloading bombers
    bool    m_offensive;                // used by AI for tracking whether unit is offensive or defensive
    Fixed   m_aiTimer;
    Fixed   m_aiSpeed;
    Fixed   m_ghostFadeTime;
    int     m_targetObjectId;
    bool    m_isRetaliating;
    bool    m_forceAction;              // forces ai unit to act on a certain state regardless of team/fleet state

    int     m_numNukesInFlight;
    int     m_numNukesInQueue;
    double  m_nukeCountTimer;

    int     m_maxFighters;
    int     m_maxBombers;               // max number of aircraft this unit can hold


protected:
    char    bmpImageFilename[256];
    Fixed   m_radarRange;
    Fixed   m_retargetTimer;            // object is allowed to search for a new target when this = 0
    
public:
    WorldObject();
    ~WorldObject();

    virtual void        InitialiseTimers();

    void                SetType         ( int type );
    void                SetTeamId       ( int teamId );
    void                SetPosition     ( Fixed longitude, Fixed latitude );
    
    void                SetRadarRange   ( Fixed radarRange );
    virtual Fixed       GetRadarRange   ();

    void                AddState        ( char *stateName, Fixed prepareTime, Fixed reloadTime, Fixed radarRange, 
                                          Fixed actionRange, bool isActionable, int numTimesPermitted = -1, int defconPermitted = 5 );
    
    virtual bool        CanSetState     ( int state );
    virtual void        SetState        ( int state );
    virtual bool        IsActionable    ();
    virtual Fixed       GetActionRange  ();
    virtual Fixed       GetActionRangeSqd();
    virtual void        Action          ( int targetObjectId, Fixed longitude, Fixed latitude );
    virtual void        FleetAction     ( int targetObjectId );
    virtual bool        IsHiddenFrom    ();

    virtual bool        Update          ();
    virtual void        Render          ();
    
	virtual void		RunAI			();

    void                RenderCounter   ( int counter );
    virtual void        RenderGhost     ( int teamId );

    virtual Fixed       GetSize         ();
    bool                CheckCurrentState();
    bool                IsMovingObject  ();

    virtual void        RequestAction   ( ActionOrder *_action );
    virtual bool        IsActionQueueable();
    void                ClearActionQueue();
    void                ClearLastAction();

    virtual void        FireGun         ( Fixed range );

    virtual bool        UsingNukes      ();
    virtual bool        UsingGuns       ();
    virtual void        NukeStrike      ();

    virtual void        Retaliate       ( int attackerId );

    void                SetTargetObjectId ( int targetObjectId );
    int                 GetTargetObjectId ();

    virtual bool        IsPinging();

    virtual int         GetAttackOdds   ( int _defenderType );

    static char         *GetName        ( int _type );
    static int          GetType         ( char *_name );
	static char         *GetTypeName    ( int _type );
	static WorldObject  *CreateObject   ( int _type );

    virtual int         IsValidCombatTarget     ( int _objectId );                                      // returns TargetType...
    virtual int         IsValidMovementTarget   ( Fixed longitude, Fixed latitude );                    //

    virtual bool        CanLaunchFighter();
    virtual bool        CanLaunchBomber ();
    bool                LaunchBomber    ( int targetObjectId, Fixed longitude, Fixed latitude );
    bool                LaunchFighter   ( int targetObjectId, Fixed longitude, Fixed latitude );
    virtual bool        SetWaypointOnAction();

    virtual void        CeaseFire       ( int teamId );
    virtual int         CountIncomingFighters();
    
    virtual char        *LogState();

    virtual Image       *GetBmpImage     ( int state );

    char                *GetBmpBlurFilename();
};



/*
 * ============================================================================
 * class WorldObjectState
 */

class WorldObjectState
{
public:
    char    *m_stateName;                   // eg "Launch ICBM"
    Fixed   m_timeToPrepare;                // ie time to get ready for launch
    Fixed   m_timeToReload;                 // ie minimum time between launches

    Fixed   m_radarRange;
    Fixed   m_actionRange;
    bool    m_isActionable;

    int     m_numTimesPermitted;            // Number of times a particular state can be used
    int     m_defconPermitted;              // The required defcon level before this state is usable

    char *GetStateName();                   // "Launch ICBM"
    char *GetPrepareName();                 // "Prepare to Launch ICBM"
    char *GetPreparingName();               // "Preparing to Launch ICBM"
    char *GetReadyName();                   // "Ready to Launch ICBM"
};



// ============================================================================

class ActionOrder
{
public:
    int     m_targetObjectId;
    Fixed   m_longitude;
    Fixed   m_latitude;
    bool    m_pursueTarget;   // tells the objects fleet to pursue target

public:
    ActionOrder()
        :   m_targetObjectId(-1),
            m_longitude(0),
            m_latitude(0),
            m_pursueTarget(false)
    {
    }
};

#endif
