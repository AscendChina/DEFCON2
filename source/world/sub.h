
#ifndef _included_sub_h
#define _included_sub_h

#include "world/movingobject.h"


class Sub : public MovingObject
{    
protected:
    bool    m_hidden;

    WorldObject *FindTarget      ();
    bool         IsSafeTarget    ( Fleet *_fleet );

public:

    Sub();

    void    Action          ( int targetObjectId, Fixed longitude, Fixed latitude );
    bool    IsHiddenFrom    ();
    bool    Update          ();
    void    Render          ();   
    void    RunAI           ();
    int     GetAttackState  ();
    bool    IsIdle          ();
    bool    UsingGuns       ();
    bool    UsingNukes      ();
    void    SetState        ( int state );
    void    ChangePosition  ();
    bool    IsActionQueueable();
    bool    IsGroupable     ( int unitType );
    bool    IsPinging       ();
    void    FleetAction     ( int targetObjectId );

    void    RequestAction   ( ActionOrder *_action );

    int     GetAttackOdds           ( int _defenderType );
    int     IsValidCombatTarget     ( int _objectId );
    int     IsValidMovementTarget   ( Fixed longitude, Fixed latitude );
};


#endif
