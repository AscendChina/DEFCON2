
#ifndef _included_movingobject_h
#define _included_movingobject_h

#include "world/worldobject.h"

class Blip;

class MovingObject : public WorldObject
{
protected:
    Fixed   m_speed;     
    Fixed   m_turnRate;

    LList   <Vector3<Fixed> *> m_history;
    Fixed   m_historyTimer;
    int     m_maxHistorySize;

public:
    enum
    {
        MovementTypeLand,
        MovementTypeSea,
        MovementTypeAir
    };
    int     m_movementType;

    Fixed   m_targetLongitude;
    Fixed   m_targetLatitude;
    Fixed   m_range;

    Fixed   m_finalTargetLongitude;
    Fixed   m_finalTargetLatitude;
    int     m_targetNodeId;

    Fixed   m_targetLongitudeAcrossSeam;
    Fixed   m_targetLatitudeAcrossSeam;

    Fixed   m_pathCalcTimer;
    bool    m_blockHistory;
    int     m_isLanding;

    bool    m_turning;          //  Used to turn the object when
    Fixed   m_angleTurned;      //  Its target is directly behind it

public:
    MovingObject();
    ~MovingObject();

    void            InitialiseTimers();

    bool            Update();

    bool            IsValidPosition ( Fixed longitude, Fixed latitude );
    bool            MoveToWaypoint  ();                                   // returns true upon arrival
    void            Render          ();
    void            RenderGhost     ( int teamId );
    void            RenderHistory   ();

    virtual void    SetWaypoint     ( Fixed longitude, Fixed latitude );

    void            CalculateNewPosition( Fixed *newLongitude, Fixed *newLatitude, Fixed *newDistance );
    void            CrossSeam();

    virtual int     GetAttackState  ();
    virtual bool    IsIdle          ();

    virtual void    Land            ( int targetId );

    void            ClearWaypoints  ();
    void            AutoLand        ();
    int             GetClosestLandingPad();

    virtual int     IsValidMovementTarget( Fixed longitude, Fixed latitude );

    virtual void    Retaliate       ( int attackerId );

    void            Ping            ();
    void            SetSpeed        ( Fixed speed );

    char            *LogState();

    virtual int     GetTarget( Fixed range );
};

/*
class Blip
{
public:

    enum
    {
        TargetNode,
        TargetEnd, 
        TargetSeam,
        TargetNodeAcrossSeam,
        TargetFinalAcrossSeam
    };
    float   m_longitude;
    float   m_latitude;
    float   m_targetLongitude;
    float   m_targetLatitude;
    Vector3 m_blipVel;
    int     m_targetNodeId;
    int     m_currentNodeId;
    float   m_nodeCheckTimer;
    int     m_targetType;

    LList   <Vector3 *> m_history;
    float   m_historyTimer;
};
*/

#endif
