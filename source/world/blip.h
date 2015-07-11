
#ifndef _included_blip_h
#define _included_blip_h

#include "world/movingobject.h"


class Blip : public MovingObject
{
public:
    int m_origin;
    int m_type;
    int m_targetNodeId;
    bool m_arrived;
    Fixed m_finalTargetLongitude;
    Fixed m_finalTargetLatitude;
    Fixed m_pathCheckTimer;

    enum 
    {
        BlipTypeHighlight,
        BlipTypeSelect,
        BlipTypeMouseTracker
    };


public:
    Blip();

    Fixed GetSize();
    bool Update();
    void SetWaypoint( Fixed longitude, Fixed latitude );
    void Render();
}; 

#endif
