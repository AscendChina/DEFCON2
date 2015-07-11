
#ifndef _included_explosion_h
#define _included_explosion_h

#include "world/worldobject.h"


class Explosion : public WorldObject
{
public:
    Fixed m_initialIntensity;
    Fixed m_intensity;
    bool  m_underWater;
    int   m_targetTeamId;

public:
    Explosion();

    bool Update();
    void Render();

    bool IsActionable() { return false; }

};


#endif
