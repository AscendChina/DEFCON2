
#ifndef _included_bomber_h
#define _included_saucer_h

#include "world/movingobject.h"


class Saucer : public MovingObject
{
    
public:

    Fixed m_explosionSize;
    Fixed m_damageTimer;
    float m_angle;

    bool  m_leavingWorld;

    Saucer();

    void    Action          ( int targetObjectId, Fixed longitude, Fixed latitude );
    Fixed   GetActionRange  ();
    bool    Update          ();
    void    Render          ();

    void    GetNewTarget    ();

};


#endif
