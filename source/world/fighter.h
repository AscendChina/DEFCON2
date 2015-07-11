
#ifndef _included_fighter_h
#define _included_fighter_h

#include "world/movingobject.h"


class Fighter : public MovingObject
{  
public:
    bool    m_playerSetWaypoint;

public:
    Fighter();

    void    Action          ( int targetObjectId, Fixed longitude, Fixed latitude );
    bool    Update          ();
    void    Render          ();

	void	RunAI			();

    int     GetAttackState  ();

    void    Land            ( int targetId );
    bool    UsingGuns       ();
    int     CountTargettedFighters( int targetId );

    int     IsValidCombatTarget( int _objectId );                                      // returns TargetType...
    bool    SetWaypointOnAction();
};


#endif
