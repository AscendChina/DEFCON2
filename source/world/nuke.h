
#ifndef _included_nuke_h
#define _included_nuke_h

#include "world/movingobject.h"


class Nuke : public MovingObject
{
protected:
    Fixed   m_totalDistance;
    Fixed   m_curveDirection;
    Fixed   m_prevDistanceToTarget;
    
    Fixed   m_newLongitude;             // Used to slowly turn towards new target
    Fixed   m_newLatitude;
	bool	m_isMirv;
    bool    m_targetLocked;
	void Nuke::InsertIntoCityListOrderd(LList<City *> *llist, City *city);
	WorldObject *Nuke::GetTargettedObject();
    City *Nuke::GetTargettedCity();

public:
    Nuke();

    void    Action          ( int targetObjectId, Fixed longitude, Fixed latitude );
    bool    Update          ();
    void    Render          ();
	void	SetMirv			(bool isMirv = true);
    void    SetWaypoint     ( Fixed longitude, Fixed latitude );
    
    static void FindTarget  ( int team, int targetTeam, int launchedBy, Fixed range, Fixed *longitude, Fixed *latitude );
    static void FindTarget  ( int team, int targetTeam, int launchedBy, Fixed range, Fixed *longitude, Fixed *latitude, int *objectId );
    static int CountTargetedNukes( int teamId, Fixed longitude, Fixed latitude );

    void    CeaseFire       ( int teamId );
    int     IsValidMovementTarget( Fixed longitude, Fixed latitude );

    void    LockTarget();
}; 


#endif
