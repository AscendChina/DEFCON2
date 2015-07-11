
#ifndef _included_animatedicon_h
#define _included_animatedicon_h

#include "world/world.h"

class AnimatedIcon
{
public:
    float   m_startTime;
    int     m_animationType;
    float   m_longitude;
    float   m_latitude;
    int     m_fromObjectId;

    BoundedArray   <bool>m_visible;

public:
    AnimatedIcon();
    virtual bool Render(); // returns true when finished rendering, and the object is deleted.

};


// ============================================================================


class ActionMarker : public AnimatedIcon
{
public:
    bool    m_combatTarget;
    int     m_targetType;
    
public:
    ActionMarker();
    bool Render();
};


// ============================================================================


class SonarPing : public AnimatedIcon
{
public:
    int m_teamId;
    
public:
    SonarPing();
    bool Render();
};



// ============================================================================


class NukePointer : public AnimatedIcon 
{
public:
    float m_lifeTime;
    int   m_targetId;
    int   m_mode;
    bool  m_offScreen;
    bool  m_seen;

public:
    NukePointer();
    
    void Merge();               // Look for nearby nuke pointers, merge in
    bool Render();
};



#endif
