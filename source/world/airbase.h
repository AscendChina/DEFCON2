
#ifndef _included_airbase_h
#define _included_airbase_h

#include "world/worldobject.h"
#include "lib/math/fixed.h"


class AirBase : public WorldObject
{
protected:
    Fixed m_fighterRegenTimer;

public:
    AirBase();

    void RequestAction( ActionOrder *_order );
    void Action( int targetObjectId, Fixed longitude, Fixed latitude );
    void RunAI();
    bool IsActionQueueable();
    bool UsingNukes();
    void NukeStrike();
    bool Update();
    void Render();
    bool CanLaunchFighter();
    bool CanLaunchBomber();
    
    int  GetAttackOdds           ( int _defenderType );

    int  IsValidCombatTarget     ( int _objectId );                                      // returns TargetType...
    int  IsValidMovementTarget   ( Fixed longitude, Fixed latitude );                    //
};



#endif
