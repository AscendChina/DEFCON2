
#ifndef _included_silo_h
#define _included_silo_h

#include "world/worldobject.h"

class Silo : public WorldObject
{
public:
    int m_numNukesLaunched;

public:

    Silo();

    void    Action          ( int targetObjectId, Fixed longitude, Fixed latitude );
    void    Render          ();    
    bool    Update          ();
	void	RunAI			();
    int     GetTargetObjectId();
    bool    IsActionQueueable();
    bool    UsingGuns       ();
    bool    UsingNukes      ();
    void    NukeStrike      ();
    void    AirDefense      ();
    void    SetState        ( int state );
    Image   *GetBmpImage    ( int state );

    int     GetAttackOdds       ( int _defenderType );
    int     IsValidCombatTarget ( int _objectId );                                      // returns TargetType...

    void    CeaseFire       ( int teamId );

};


#endif
