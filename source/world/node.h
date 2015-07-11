
#ifndef _included_node_h
#define _included_node_h

#include "world/worldobject.h"

struct Route;

class Node : public WorldObject
{

public:
    LList<Route *>    m_routeTable;

public:
    
    Node();
    void AddRoute(int targetNodeId, int nextNode, Fixed distance);
    int  GetRouteId( int targetNodeId );

    
};

struct Route
{
    int m_targetNodeId;            // The ultimate target
    int m_nextNodeId;             // Where to go first to reach the ultimate target
    Fixed m_totalDistance;
};

#endif
