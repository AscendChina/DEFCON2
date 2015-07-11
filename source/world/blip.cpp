#include "lib/universal_include.h"

#include <math.h>

#include "lib/resource/resource.h"
#include "lib/resource/image.h"
#include "lib/render/renderer.h"
#include "lib/math/vector3.h"
 
#include "app/app.h"
#include "app/globals.h"

#include "renderer/map_renderer.h"

#include "world/world.h"
#include "world/blip.h"
#include "world/fleet.h"


Blip::Blip()
:   MovingObject(),
    m_origin(-1),
    m_arrived(false),
    m_finalTargetLongitude(0),
    m_finalTargetLatitude(0),
    m_pathCheckTimer(0),
    m_targetNodeId(-1)
{
    m_range = Fixed::MAX;
    m_speed = 4;
    m_turnRate = 40;
    m_maxHistorySize = -1;
    m_movementType = MovementTypeSea;
    m_pathCalcTimer = 0;
    m_historyTimer = 0;
    
    strcpy( bmpImageFilename, "graphics/blip.bmp" );
   
    AddState("Blip", 0, 0, 0, 0, false, -1, -1 );
}

bool Blip::Update()
{
    m_speed = 4 / g_app->GetWorld()->GetTimeScaleFactor();
    m_turnRate = 40 / g_app->GetWorld()->GetTimeScaleFactor();
    if( m_type == BlipTypeSelect &&
        m_origin != g_app->GetMapRenderer()->GetCurrentSelectionId() )
    {
        return true;
    }
    if( m_type == BlipTypeHighlight )
    {
        WorldObject *highlight = g_app->GetWorld()->GetWorldObject( g_app->GetMapRenderer()->GetCurrentHighlightId() );
        if( highlight )
        {
            if( m_origin != highlight->m_fleetId )
            {
                return true;
            }
        }

        WorldObject *obj = g_app->GetWorld()->GetWorldObject( g_app->GetMapRenderer()->GetCurrentSelectionId() );
        if( obj )
        {
            if( m_origin != obj->m_fleetId )
            {
                return true;
            }
        }

        if( g_app->GetMapRenderer()->GetCurrentSelectionId() == -1 &&
            g_app->GetMapRenderer()->GetCurrentHighlightId() == -1 )
        {
            return true;
        }
    }
    if( m_type == BlipTypeMouseTracker )
    {
        if( g_app->GetMapRenderer()->m_mouseIdleTime < 3.0f )
        {
            return true;
        }
        WorldObject *obj = g_app->GetWorld()->GetWorldObject( g_app->GetMapRenderer()->GetCurrentSelectionId() );
        if( !obj ||
            obj->m_fleetId != m_origin )
        {
            return true;
        }
    }

    m_historyTimer -= SERVER_ADVANCE_PERIOD / 10;
    if( m_historyTimer <= 0 )
    {
        if( m_longitude > -180 ||
            m_longitude < 180 )
        {
            m_history.PutDataAtStart( new Vector3<Fixed>(m_longitude, m_latitude, 0) );
            m_historyTimer = Fixed::Hundredths(1);
        }
    }

    Render();
    m_pathCheckTimer -= SERVER_ADVANCE_PERIOD;
    if( m_pathCheckTimer <= 0 )
    {
        m_pathCheckTimer = 1;

        if( m_finalTargetLongitude != 0 && m_finalTargetLatitude != 0 )
        {
            if( g_app->GetWorld()->IsSailable( m_longitude, m_latitude, m_finalTargetLongitude, m_finalTargetLatitude ) )
            {
                // We can now sail directly to our target
                SetWaypoint( m_finalTargetLongitude, m_finalTargetLatitude );
                m_finalTargetLongitude = 0;
                m_finalTargetLatitude = 0;
                m_targetNodeId = -1;
            }
            else
            {

                // Query all directly sailable nodes for the quickest route to targetNodeId;
                Fixed currentBestDistance = Fixed::MAX;

                Fixed newLong(0);
                Fixed newLat(0);

                for( int n = 0; n < g_app->GetWorld()->m_nodes.Size(); ++n )
                {
                    Node *node = g_app->GetWorld()->m_nodes[n];
                    if( g_app->GetWorld()->IsSailable( m_longitude, m_latitude, node->m_longitude, node->m_latitude ) )
                    {
                        int routeId = node->GetRouteId( m_targetNodeId );
                        if( routeId != -1 )
                        {
                            Fixed totalDistance = node->m_routeTable[routeId]->m_totalDistance;

                            totalDistance += g_app->GetWorld()->GetDistance( m_longitude, m_latitude, node->m_longitude, node->m_latitude );

                            if( totalDistance < currentBestDistance )
                            {
                                currentBestDistance = totalDistance;               
                                newLong = node->m_longitude;
                                newLat = node->m_latitude;
                            }
                        }
                    }
                }
                if( newLong != 0 && newLat != 0 )
                {
                    SetWaypoint( newLong, newLat );
                }
            }
        }
    }

    return MoveToWaypoint();
}

void Blip::SetWaypoint( Fixed longitude, Fixed latitude )
{
    Fixed directDistance = g_app->GetWorld()->GetDistance( m_longitude, m_latitude, longitude, latitude, true);

    Fixed targetSeamLatitude;
    Fixed targetSeamLongitude;
    g_app->GetWorld()->GetSeamCrossLatitude( Vector3<Fixed>( longitude, latitude, 0 ), Vector3<Fixed>(m_longitude, m_latitude, 0), &targetSeamLongitude, &targetSeamLatitude);
    Fixed distanceAcrossSeam = g_app->GetWorld()->GetDistanceAcrossSeam( m_longitude, m_latitude, longitude, latitude);

    if( distanceAcrossSeam < directDistance )
    {
        if( targetSeamLongitude < 0 )
        {
            longitude -= 360;
        }
        else
        {
            longitude += 360;
        }      
    }
    MovingObject::SetWaypoint( longitude, latitude );
}


Fixed Blip::GetSize()
{
    return MovingObject::GetSize() / 2;
}


void Blip::Render()
{
    MovingObject::Render();

    Team *team = g_app->GetWorld()->GetMyTeam();
    Fleet *fleet = team->GetFleet( m_origin );
    if( fleet )
    {
        for( int i = 0; i < fleet->m_fleetMembers.Size(); ++i )
        {
            WorldObject *member = g_app->GetWorld()->GetWorldObject( fleet->m_fleetMembers[i] );
            if( member )
            {
                float size = MovingObject::GetSize().DoubleValue();
                Fixed longitude = 0;
                Fixed latitude = 0;
                fleet->GetFormationPosition( fleet->m_fleetMembers.Size(), i, &longitude, &latitude );
                longitude += m_vel.x * Fixed::FromDouble(g_predictionTime);
                latitude += m_vel.y * Fixed::FromDouble(g_predictionTime);
                Image *img = g_resource->GetImage( g_app->GetMapRenderer()->m_imageFiles[ member->m_type ] );
                g_renderer->Blit( img, m_longitude.DoubleValue() + longitude.DoubleValue() - size,
								  m_latitude.DoubleValue() + latitude.DoubleValue() + size,
								  size*2, size*-2, Colour(100,100,100,200) );
            }
        }
    }    
}
