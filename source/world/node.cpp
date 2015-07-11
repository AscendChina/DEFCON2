#include "lib/universal_include.h"

#include <string.h>

#include "lib/resource/resource.h"
#include "lib/resource/image.h"

#include "app/app.h"
#include "app/globals.h"

#include "world/node.h"
#include "world/world.h"

#include "interface/interface.h"

#include "renderer/map_renderer.h"


Node::Node()
:   WorldObject()
{
    
}

void Node::AddRoute( int targetNodeId, int nextNodeId, Fixed distance )
{
    // Do we already have a quicker route to this node?
    for( int i = 0; i < m_routeTable.Size(); ++i )
    {
        if( m_routeTable[i]->m_targetNodeId == targetNodeId &&
            m_routeTable[i]->m_totalDistance <= distance )
        {
            // We already have a quicker or identical route
            return;
        }
    }

    int routeId = GetRouteId( targetNodeId );
    if( routeId != -1 )
    {
        Route *oldRoute = m_routeTable[routeId];
        m_routeTable.RemoveData(routeId);
        delete oldRoute;
    }

    Route *newRoute = new Route;
    newRoute->m_nextNodeId = nextNodeId;
    newRoute->m_targetNodeId = targetNodeId;
    newRoute->m_totalDistance = distance;
    m_routeTable.PutData( newRoute );
}

int Node::GetRouteId( int targetNodeId )
{
    for( int i = 0; i < m_routeTable.Size(); ++i )
    {
        if( m_routeTable[i]->m_targetNodeId == targetNodeId )
        {
            return i;
        }
    }
    return -1;
}
