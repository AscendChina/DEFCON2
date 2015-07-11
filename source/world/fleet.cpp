#include "lib/universal_include.h"
#include "lib/language_table.h"
#include "lib/math/random_number.h"
#include "lib/math/math_utils.h"
#include "lib/profiler.h"

#include "app/globals.h"
#include "app/app.h"
#include "app/game.h"

#include "network/ClientToServer.h"

#include "renderer/map_renderer.h"

#include "world/team.h"
#include "world/world.h"
#include "world/fleet.h"
#include "world/blip.h"
#include "world/nuke.h"

Fleet::Fleet()
:   m_fleetId(-1),
    m_teamId(-1),
    m_active(false),
    m_holdPosition(false),
    m_currentState(0),
    m_defensive(false),
    m_aiState(-1),
    m_longitude(0),
    m_latitude(0),
    m_targetLongitude(0),
    m_targetLatitude(0),
    m_fleetType(-1),
    m_targetNodeId(-1),
    m_pathCheckTimer(0),
    m_crossingSeam(false),
    m_pursueTarget(false),
    m_targetTeam(-1),
    m_targetFleet(-1),
    m_targetObjectId(-1),
    m_atTarget(false),
    m_blipTimer(0),
    m_speedUpdateTimer(0),
    m_niceTryChecked(false),
    m_subNukesLaunched(0)
{
}

void Fleet::Update()
{   
    if( m_fleetMembers.Size() == 0 )
    {
        return;
    }  

    GetFleetPosition( &m_longitude, &m_latitude );
    CheckFleetFormation();

    m_speedUpdateTimer -= SERVER_ADVANCE_PERIOD * g_app->GetWorld()->GetTimeScaleFactor();
    if( m_speedUpdateTimer <= 0 )
    {
        m_speedUpdateTimer = 5 + syncfrand(10);
        Fixed slowestSpeed = Fixed::Hundredths(3);
        Fixed speedDropoff = GetSpeedDropoff();
        if( IsInFleet( WorldObject::TypeSub ) )
        {
            slowestSpeed = Fixed::Hundredths(2);
        }
        slowestSpeed *= speedDropoff;
        slowestSpeed /= World::GetGameScale();
        for( int i = 0; i < m_fleetMembers.Size(); ++i )
        {
            if( m_fleetMembers.ValidIndex(i) )
            {
                MovingObject *obj = (MovingObject* )g_app->GetWorld()->GetWorldObject( m_fleetMembers[i] );
                if( obj && obj->IsMovingObject() )
                {
                    obj->SetSpeed(slowestSpeed);
                }
                else
                {
                    m_fleetMembers.RemoveData(i);
                    i--;
                }
            }
        }

        bool validPersueTarget = false;

        if( m_pursueTarget )
        {
            Team *enemyTeam = g_app->GetWorld()->GetTeam( m_targetTeam );
            if( enemyTeam )
            {
                Fleet *targetFleet = enemyTeam->GetFleet( m_targetFleet );
                if( targetFleet )
                {               
                    bool visibleTargets = false;
                    for( int i = 0; i < targetFleet->m_fleetMembers.Size(); ++i)
                    {
                        WorldObject *thisTarget = g_app->GetWorld()->GetWorldObject(targetFleet->m_fleetMembers[i]);
                        if( thisTarget &&
                            thisTarget->m_visible[m_teamId] )
                        {
                            visibleTargets = true;
                            break;
                        }
                    }

                    START_PROFILE( "Persue" );
                    if( visibleTargets )
                    {
                        Vector3<Fixed> targetPos( targetFleet->m_longitude, targetFleet->m_latitude, 0 );
                        Vector3<Fixed> offset( 0, 5, 0 );
                        offset.RotateAroundZ( targetFleet->m_fleetId * Fixed::Hundredths(30) );

                        Vector3<Fixed> ourPos( m_longitude, m_latitude, 0 );

                        if( IsValidFleetPosition( targetPos.x + offset.x, targetPos.y + offset.y ) &&
                            (ourPos - targetPos).MagSquared() > 5 * 5 )
                        {
                            // Try moving to an offset first
                            MoveFleet( targetPos.x + offset.x, targetPos.y + offset.y, false );
                            validPersueTarget = true;
                        }
                        else if( IsValidFleetPosition( targetPos.x, targetPos.y ) &&
                            (ourPos - targetPos).MagSquared() > 5 * 5 )
                        {
                            // That failed (maybe they are tucked in a bay?) so aim straight for them.
                            MoveFleet( targetPos.x, targetPos.y, false );
                            validPersueTarget = true;
                        }        
                    }
                    END_PROFILE( "Persue" );
                }
            }           
        }

        if( !validPersueTarget )
        {
            m_pursueTarget = false;
            m_targetTeam = -1;
            m_targetFleet = -1;
        }
    }



    if( m_targetLongitude != 0 && m_targetLatitude != 0 )
    {
        if( IsAtTarget() )
        {
            m_targetLongitude = 0;
            m_targetLatitude = 0;
            m_atTarget = true;
            return;
        }

        if( !IsOnSameSideOfSeam() )
        {
            m_crossingSeam = true;
            return;
        }

        m_pathCheckTimer -= SERVER_ADVANCE_PERIOD * g_app->GetWorld()->GetTimeScaleFactor();
        if( m_pathCheckTimer <= 0 )
        {
            START_PROFILE( "Path Check" );
            m_pathCheckTimer = 5 + syncfrand(10);
            
            m_pointIgnoreList.Empty();
            
            if( m_crossingSeam && IsOnSameSideOfSeam() )
            {
                // We were crossing the seam, but now we are fully over
                m_targetNodeId = -1;
                m_crossingSeam = false;
                if( m_targetLongitude < -180 )
                {
                    m_targetLongitude += 360;
                }
                else if( m_targetLongitude > 180 )
                {
                    m_targetLongitude -= 360;
                }

                bool basicSailableCheck = g_app->GetWorld()->IsSailable( m_longitude, m_latitude, m_targetLongitude, m_targetLatitude );
                if( !basicSailableCheck )
                {
                    int nodeId = g_app->GetWorld()->GetClosestNode( m_longitude, m_latitude );
                    if( nodeId != -1 )
                    {
                        Node *node = g_app->GetWorld()->m_nodes[nodeId];
                        AlterWaypoints( node->m_longitude, node->m_latitude, false );
                    }
                    nodeId = g_app->GetWorld()->GetClosestNode( m_targetLongitude, m_targetLatitude );
                    if( nodeId != -1 )
                    {
                        m_targetNodeId = nodeId;
                    }
                }
            }


            bool basicSailableCheck = false;
            if( m_targetNodeId == -1 ) basicSailableCheck = true;           
            else                       basicSailableCheck = g_app->GetWorld()->IsSailable( m_longitude, m_latitude, m_targetLongitude, m_targetLatitude );

            if( basicSailableCheck )
            {
                // We can now sail directly to our target
                AlterWaypoints( m_targetLongitude, m_targetLatitude, false );
                m_targetNodeId = -1;
                END_PROFILE( "Path Check" );
                return;
            }


            // Query all directly sailable nodes for the quickest route to targetNodeId;
            Fixed currentBestDistance = Fixed::MAX;
            Fixed newLong = 0;
            Fixed newLat = 0;

            for( int n = 0; n < g_app->GetWorld()->m_nodes.Size(); ++n )
            {
                Node *node = g_app->GetWorld()->m_nodes[n];
                int routeId = node->GetRouteId( m_targetNodeId );
                if( routeId != -1 )
                {
                    Fixed totalDistance = node->m_routeTable[routeId]->m_totalDistance;                    

                    if( totalDistance < currentBestDistance )
                    {
                        totalDistance += g_app->GetWorld()->GetDistance( m_longitude, m_latitude, node->m_longitude, node->m_latitude );
                        if( totalDistance < currentBestDistance )
                        {
                            // The check "totalDistance < currentBestDistance" is done twice to quickly exclude long routes
                            // without having to call the expensive IsSailable function

                            if( g_app->GetWorld()->IsSailable( m_longitude, m_latitude, node->m_longitude, node->m_latitude ) )
                            {
                                currentBestDistance = totalDistance;               
                                newLong = node->m_longitude;
                                newLat = node->m_latitude;
                            }
                        }
                    }
                }
            }
            if( newLong != 0 && newLat != 0 )
            {
                AlterWaypoints( newLong, newLat, false );
            }

            END_PROFILE( "Path Check" );
        }
    }
}

void Fleet::GetFormationPosition( int memberCount, int memberId, Fixed *longitude, Fixed *latitude )
{
    Fixed offset = Fixed::Hundredths(50);
    
    offset /= g_app->GetWorld()->GetGameScale();

    switch( memberCount )
    {
        case 2:
        {
            switch(memberId)
            {
                case 0  :   *longitude -= offset*3;                 break;
                case 1  :   *longitude += offset*3;                 break;
            }
            break;
        }
        case 3:
        {
            switch(memberId)
            {
                case 0  :   *longitude -= offset*3; *latitude -= offset*3;  break;
                case 1  :                           *latitude += offset;    break;
                case 2  :   *longitude += offset*3; *latitude -= offset*3;  break;
            }
            break;
        }
        case 4:
        {
            switch(memberId)
            {
                case 0  :   *longitude -= offset*4;                         break;
                case 1  :                           *latitude += offset*3;  break;
                case 2  :   *longitude += offset*4;                         break;
                case 3  :                           *latitude -= offset*3;  break;
            }
            break;
        }

        case 5:
        {
            switch(memberId)
            {
                case 0  :                           *latitude += offset*6;  break;
                case 1  :   *longitude -= offset*4; *latitude -= offset*3;  break;
                case 2  :   *longitude += offset*4; *latitude -= offset*3;  break;
                case 3  :   *longitude -= offset*6; *latitude += offset*3;  break;
                case 4  :   *longitude += offset*6; *latitude += offset*3;  break;
            }
            break;
        }


        case 6:
        {
            switch(memberId)
            {
                case 0  :                           *latitude += offset*6;  break;
                case 1  :                           *latitude -= offset*6;  break;
                case 2  :   *longitude += offset*6; *latitude -= offset*3;  break;
                case 3  :   *longitude -= offset*6; *latitude -= offset*3;  break;
                case 4  :   *longitude += offset*6; *latitude += offset*3;  break;
                case 5  :   *longitude -= offset*6; *latitude += offset*3;  break;
            }
            break;
        }
    }
}


void Fleet::AlterWaypoints( Fixed longitude, Fixed latitude, bool checkNewWaypoint )
{
    bool validWaypoint = !checkNewWaypoint;
    
    if( checkNewWaypoint )
    {
        validWaypoint = IsValidFleetPosition( longitude, latitude ) &&
                        g_app->GetWorld()->IsSailable( m_longitude, m_latitude, longitude, latitude );
    }

    if( validWaypoint )
    {
        int fleetSize = m_fleetMembers.Size();

        for( int i = 0; i < fleetSize; ++i )
        {
            if( m_fleetMembers.ValidIndex(i) )
            {
                MovingObject *obj = (MovingObject* )g_app->GetWorld()->GetWorldObject( m_fleetMembers[i] );
                if( obj && obj->IsMovingObject() )
                {
                    Fixed newLong = longitude;
                    Fixed newLat = latitude;
                    GetFormationPosition( fleetSize, i, &newLong, &newLat );
                    /*if( m_crossingSeam )
                    {
                        if( newLong > 180.0f &&
                            obj->m_longitude < 0.0f)
                        {
                            newLong -= 360.0f;
                        }
                        else if( newLong < -180.0f &&
                                 obj->m_longitude > 0.0f )
                        {
                            newLong += 360.0f;
                        }
                    }*/
                    obj->SetWaypoint( newLong, newLat );
                }
            }
        }
    }
}

void Fleet::MoveFleet( Fixed longitude, Fixed latitude, bool cancelPursuits )
{
    START_PROFILE( "MoveFleet" );

    m_atTarget = false;
    if( cancelPursuits )
    {
        m_pursueTarget = false;
        m_targetTeam = -1;
        m_targetFleet = -1;
    }

    //
    // If there are subs in the fleet currently nuking then put a stop to it

    if( IsInFleet( WorldObject::TypeSub ) )
    {
        for( int i = 0; i < m_fleetMembers.Size(); ++i )
        {
            WorldObject *obj = g_app->GetWorld()->GetWorldObject( m_fleetMembers[i] );
            if( obj &&
                obj->m_type == WorldObject::TypeSub &&
                obj->m_currentState == 2 )
            {
                obj->ClearActionQueue();
                obj->SetState(0);
            }
        }
    }

    if( !IsValidFleetPosition( longitude, latitude ) )
    {
        if( m_teamId == g_app->GetWorld()->m_myTeamId )
        {
            char msg[128];
            sprintf( msg, LANGUAGEPHRASE("message_fleet_move") );
            g_app->GetWorld()->AddWorldMessage( longitude, latitude, m_teamId, msg, WorldMessage::TypeInvalidAction );
        }
        END_PROFILE( "MoveFleet" );
        return;
    }


    if( IsOnSameSideOfSeam() )
    {
        Fixed directDistanceSqd = g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, longitude, latitude, true);

        // calculate the distance to the target if the unit cross a world join
        Fixed targetSeamLatitude;
        Fixed targetSeamLongitude;
        g_app->GetWorld()->GetSeamCrossLatitude( Vector3<Fixed>( longitude, latitude, 0 ), 
                                                 Vector3<Fixed>(m_longitude, m_latitude, 0), 
                                                 &targetSeamLongitude, &targetSeamLatitude);

        Fixed distanceAcrossSeamSqd = g_app->GetWorld()->GetDistanceAcrossSeamSqd( m_longitude, m_latitude, longitude, latitude);

        bool fasterAcrossSeam = false;
        if( distanceAcrossSeamSqd < directDistanceSqd )
        {
            if( targetSeamLongitude < 0 )
            {
                longitude -= 360;
            }
            else
            {
                longitude += 360;
            }
            fasterAcrossSeam = true;
        }
        m_targetNodeId = g_app->GetWorld()->GetClosestNode( longitude, latitude );
    
        m_targetLongitude = longitude;
        m_targetLatitude = latitude;

        Node *node = g_app->GetWorld()->m_nodes[ g_app->GetWorld()->GetClosestNode( m_longitude, m_latitude )];
        if( !node )
        {
            END_PROFILE( "MoveFleet" );
            return;
        }
        Fixed newLong = 0;
        Fixed newLat = 0;

        bool sailable = g_app->GetWorld()->IsSailable( m_longitude, m_latitude, m_targetLongitude, m_targetLatitude );
        if( sailable )
        {
            newLong = m_targetLongitude;
            newLat = m_targetLatitude;
        }
        else
        {
            newLong = node->m_longitude;
            newLat = node->m_latitude;
            sailable = g_app->GetWorld()->IsSailable( m_longitude, m_latitude, newLong, newLat );
        }

        if( sailable )
        {
            for( int i = 0; i < m_fleetMembers.Size(); ++i )
            {
                if( m_fleetMembers.ValidIndex(i) )
                {
                    MovingObject *obj = (MovingObject* )g_app->GetWorld()->GetWorldObject( m_fleetMembers[i] );
                    if( obj && obj->IsMovingObject() )
                    {
                        Fixed thisLong = newLong;
                        Fixed thisLat = newLat;
                        GetFormationPosition( m_fleetMembers.Size(), i, &thisLong, &thisLat );
                        obj->SetWaypoint( thisLong, thisLat );
                    }
                    else
                    {
                        m_fleetMembers.RemoveData(i);
                        --i;
                    }
                }
            }
        }
    }
    else
    {
        //
        // Fleet is split across a seam, so waypoints must be handled seperately for each ship
        m_targetLongitude = longitude;
        m_targetLatitude= latitude;

        if( g_app->GetWorld()->GetDistanceAcrossSeamSqd( m_longitude, m_latitude, m_targetLongitude, m_targetLatitude ) < 
            g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, m_targetLongitude, m_targetLatitude, true ) )
        {
            if( m_longitude < 0 )
            {
                m_targetLongitude -= 360;
            }
            else
            {
                m_targetLongitude += 360;
            }
        }
        for( int i = 0; i < m_fleetMembers.Size(); ++i )
        {
            if( m_fleetMembers.ValidIndex(i) )
            {
                MovingObject *obj = (MovingObject* )g_app->GetWorld()->GetWorldObject( m_fleetMembers[i] );
                if( obj && obj->IsMovingObject() )
                {
                    //m_targetNodeId = g_app->GetWorld()->GetClosestNode( longitude, latitude );
                    Fixed directDistanceSqd = g_app->GetWorld()->GetDistanceSqd( obj->m_longitude, obj->m_latitude, longitude, latitude, true);

                    // calculate the distance to the target if the unit cross a world join
                    Fixed targetSeamLatitude;
                    Fixed targetSeamLongitude;
                    g_app->GetWorld()->GetSeamCrossLatitude( Vector3<Fixed>( longitude, latitude, 0 ), Vector3<Fixed>(obj->m_longitude, obj->m_latitude, 0), &targetSeamLongitude, &targetSeamLatitude);

                    Fixed distanceAcrossSeamSqd = g_app->GetWorld()->GetDistanceAcrossSeamSqd( obj->m_longitude, obj->m_latitude, longitude, latitude);

                    if( distanceAcrossSeamSqd < directDistanceSqd )
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

                    Node *node = g_app->GetWorld()->m_nodes[ g_app->GetWorld()->GetClosestNode( obj->m_longitude, obj->m_latitude )];
                    if( !node )
                    {
                        END_PROFILE( "MoveFleet" );
                        return;
                    }
                    Fixed newLong = 0;
                    Fixed newLat = 0;
                    if( g_app->GetWorld()->IsSailable( obj->m_longitude, obj->m_latitude, longitude, latitude ) )
                    {
                        newLong = longitude;
                        newLat = latitude;
                    }
                    else
                    {
                        newLong = node->m_longitude;
                        newLat = node->m_latitude;
                    }
                    
                    GetFormationPosition( m_fleetMembers.Size(), i, &newLong, &newLat );
                    obj->SetWaypoint( newLong, newLat );
                }
                else
                {
                    m_fleetMembers.RemoveData(i);
                    --i;
                }
            }
        }
    }

    END_PROFILE( "MoveFleet" );
}

bool Fleet::IsValidFleetPosition( Fixed longitude, Fixed latitude )
{
    if( longitude > 180 ||
        longitude < -180 )
    {
        return true;
    }
    
    for( int i = 0; i < m_fleetMembers.Size(); ++i )
    {
        Fixed thisLong = longitude;
        Fixed thisLat = latitude;
        GetFormationPosition( m_fleetMembers.Size(), i, &thisLong, &thisLat );

	    if( !g_app->GetMapRenderer()->IsValidTerritory( -1, thisLong, thisLat, true ) )
        {
            return false;
        }
    }

    return true;
}

void Fleet::FleetAction( int targetObjectId )
{
    for( int i = 0; i < m_fleetMembers.Size(); ++i )
    {
        WorldObject *obj = g_app->GetWorld()->GetWorldObject( m_fleetMembers[i] );
        if( obj )
        {
            obj->FleetAction( targetObjectId );
        }
    }
}

bool Fleet::ValidFleetPlacement( Fixed longitude, Fixed latitude )
{	
    for( int i = 0; i < m_memberType.Size(); ++i )
    {
        Fixed thisLong = longitude;
        Fixed thisLat = latitude;
        GetFormationPosition( m_memberType.Size(), i, &thisLong, &thisLat );

	    if( !g_app->GetWorld()->IsValidPlacement( m_teamId, thisLong, thisLat, m_memberType[i] ) )
        {
            return false;
        }
    }
    return true;
}

void Fleet::PlaceFleet( Fixed longitude, Fixed latitude )
{
    for( int i = 0; i < m_memberType.Size(); ++i )
    {
        Fixed thisLong = longitude;
        Fixed thisLat = latitude;
        GetFormationPosition( m_memberType.Size(), i, &thisLong, &thisLat );
        if( thisLong > 180 )
        {
            thisLong -= 360;
        }
        else if( thisLong < -180 )
        {
            thisLong += 360;
        }
        WorldObject *newObject = WorldObject::CreateObject( m_memberType[i] );
        newObject->SetTeamId(m_teamId);
        g_app->GetWorld()->AddWorldObject( newObject );
        newObject->SetPosition( thisLong, thisLat );
        newObject->m_fleetId = m_fleetId;
        m_fleetMembers.PutData(newObject->m_objectId);
        g_app->GetWorld()->GetTeam( m_teamId )->m_unitCredits -= g_app->GetWorld()->GetUnitValue( newObject->m_type );
    }
    m_longitude = longitude;
    m_latitude = latitude;
    m_active = true;
}

bool Fleet::IsFleetMoving()
{
    for( int i = 0; i < m_fleetMembers.Size(); ++i )
    {
        MovingObject *obj = (MovingObject *)g_app->GetWorld()->GetWorldObject( m_fleetMembers[i] );
        if( obj &&
            obj->IsMovingObject() )
        {
            if( obj->m_targetLatitude != 0 &&
                obj->m_targetLongitude != 0 )
            {
                return true;
            }
        }
    }
    return false;
}

bool Fleet::IsFleetIdle()
{
    for( int i = 0; i < m_fleetMembers.Size(); ++i )
    {
        MovingObject *obj = (MovingObject *)g_app->GetWorld()->GetWorldObject( m_fleetMembers[i] );
        if( obj &&
            obj->IsMovingObject() )
        {
            if( !obj->IsIdle() )
            {
                return false;
            }
        }
    }
    return true;
}

void Fleet::RunAI()
{
    if( m_fleetMembers.Size() == 0 )
    {
        return;
    }

    Team *team = g_app->GetWorld()->GetTeam( m_teamId );
    if( team->m_currentState == Team::StatePanic )
    {
        if( CountNukesInFleet() > 0 )
        {
            m_aiState = AIStateSneaking;
        }
        else
        {
            if( m_aiState != AIStateIntercepting )
            {
                m_aiState = AIStateHunting;
            }
        }
    }
    if( m_aiState != AIStateIntercepting )
    {
        if( CountNukesInFleet() > 0 )
        {
            m_aiState = AIStateSneaking;
        }
        else
        {
            m_aiState = AIStateHunting;
        }
    }

    switch( m_aiState )
    {
        case AIStateHunting     :   RunAIHunting();     break;
        case AIStateSneaking    :   RunAISneaking();    break;
        case AIStateIntercepting:   RunAIIntercepting();break;
    }

    for( int i = 0; i < m_fleetMembers.Size(); ++i )
    {
        WorldObject *obj = g_app->GetWorld()->GetWorldObject( m_fleetMembers[i] );
        if( !obj )
        {
            m_fleetMembers.RemoveData(i);
            --i;
        }
    }
}

void Fleet::RunAIHunting()
{
    if( g_app->GetWorld()->GetDefcon() <= 4 )
    {
        if( m_targetLongitude == 0 && m_targetLatitude == 0 )
        {
            Fixed attackLongitude, attackLatitude;

            bool success = FindGoodAttackSpot( attackLongitude, attackLatitude );
            if( success )
            {
                MoveFleet( attackLongitude, attackLatitude );
            }
        }
    }
}

void Fleet::RunAISneaking()
{
    if( g_app->GetWorld()->GetDefcon() <= 4 )
    {
        if( CountNukesInFleet() == 0 )
        {
            m_aiState = AIStateHunting;
            return;
        }
        if( IsFleetIdle() && !IsInNukeRange() )
        {
            if( m_targetLongitude != 0 && m_targetLatitude != 0 )
            {
                MoveFleet( m_targetLongitude, m_targetLatitude );
            }
        }

        if( m_targetLongitude == 0 && m_targetLatitude == 0 )
        {
            if( !IsInNukeRange() )
            {   
                Fixed attackLongitude, attackLatitude;
                bool success = FindGoodAttackSpot( attackLongitude, attackLatitude );
                if( success )
                {
                    MoveFleet( attackLongitude, attackLatitude );
                }
            }
        }
    }
}


struct ValidTargetPoint
{
    Fixed m_longitude;
    Fixed m_latitude;
    Fixed m_sailRange;
};


bool Fleet::FindGoodAttackSpot( Fixed &_longitude, Fixed &_latitude )
{
    START_PROFILE( "FindAttackSpot" );

    LList<ValidTargetPoint> validTargetPoints;
    
    Fixed attackRange = 60;
    if( IsInFleet(WorldObject::TypeSub) &&
        CountNukesInFleet() > 0 )
    {
        attackRange = 45;
    }

    for( int i = 0; i < g_app->GetWorld()->m_aiTargetPoints.Size(); ++i )
    {
        if( !IsIgnoringPoint(i) )
        {
            Vector3<Fixed> *point = g_app->GetWorld()->m_aiTargetPoints[i];

            bool inRange = false;            
            for( int j = 0; j < World::NumTerritories; ++j )
            {
                int owner = g_app->GetWorld()->GetTerritoryOwner(j);
                if( owner != -1 &&
                    !g_app->GetWorld()->IsFriend( owner, m_teamId ) )
                {
                    Fixed popCenterLong = g_app->GetWorld()->m_populationCenter[j].x;
                    Fixed popCenterLat = g_app->GetWorld()->m_populationCenter[j].y;

                    if( g_app->GetWorld()->GetDistanceSqd( point->x, point->y, popCenterLong, popCenterLat ) < attackRange * attackRange )
                    {
                        inRange = true;
                        break;
                    }
                }
            }
            if( inRange )
            {
                Fixed sailRange = g_app->GetWorld()->GetSailDistance( m_longitude, m_latitude, point->x, point->y );
                
                ValidTargetPoint newPoint;
                newPoint.m_longitude = point->x;
                newPoint.m_latitude = point->y;
                newPoint.m_sailRange = sailRange;

                if( sailRange < 5 )
                {
                    m_pointIgnoreList.PutData(i);
                }
                else
                {
                    // Ordered insert

                    bool added = false;
                    for( int j = 0; j < validTargetPoints.Size(); ++j )
                    {
                        ValidTargetPoint point = *validTargetPoints.GetPointer(j);
                        if( sailRange < point.m_sailRange )
                        {
                            validTargetPoints.PutDataAtIndex( newPoint, j );
                            added = true;
                            break;
                        }
                    }
                    if( !added )
                    {
                        validTargetPoints.PutDataAtEnd(newPoint);
                    }
                }
            }
        }
    }


    while(validTargetPoints.Size())
    {
        int numValid = ( validTargetPoints.Size() * Fixed::Hundredths(20) ).IntValue();
        numValid = max( numValid, 1 );
        int index = syncrand() % numValid;
        ValidTargetPoint point = *validTargetPoints.GetPointer(index);

        validTargetPoints.RemoveData(index);

        Fixed longitude = point.m_longitude + syncsfrand( 10 );
        Fixed latitude = point.m_latitude + syncsfrand( 10 );

        if( IsValidFleetPosition(longitude, latitude) )
        {
            _longitude = longitude;
            _latitude = latitude;
            END_PROFILE( "FindAttackSpot" );
            return true;
        }
    }


    END_PROFILE( "FindAttackSpot" );
    return false;
}


bool Fleet::FindGoodAttackSpotSlow( Fixed &_longitude, Fixed &_latitude )
{
    START_PROFILE( "FindAttackSpot" );

    LList<ValidTargetPoint> validTargetPoints;

    Fixed attackRange = 60;
    if( IsInFleet(WorldObject::TypeSub) &&
        CountNukesInFleet() > 0 )
    {
        attackRange = 45;
    }

    for( int i = 0; i < g_app->GetWorld()->m_aiTargetPoints.Size(); ++i )
    {
        if( !IsIgnoringPoint(i) )
        {
            Vector3<Fixed> *point = g_app->GetWorld()->m_aiTargetPoints[i];

            bool inRange = false;            
            for( int j = 0; j < World::NumTerritories; ++j )
            {
                int owner = g_app->GetWorld()->GetTerritoryOwner(j);
                if( owner != -1 &&
                    !g_app->GetWorld()->IsFriend( owner, m_teamId ) )
                {
                    Fixed popCenterLong = g_app->GetWorld()->m_populationCenter[j].x;
                    Fixed popCenterLat = g_app->GetWorld()->m_populationCenter[j].y;

                    if( g_app->GetWorld()->GetDistanceSqd( point->x, point->y, popCenterLong, popCenterLat ) < attackRange * attackRange )
                    {
                        inRange = true;
                        break;
                    }
                }
            }
            if( inRange )
            {
                Fixed sailRange = g_app->GetWorld()->GetSailDistance( m_longitude, m_latitude, point->x, point->y );

                ValidTargetPoint newPoint;
                newPoint.m_longitude = point->x;
                newPoint.m_latitude = point->y;
                newPoint.m_sailRange = sailRange;

                if( sailRange < 5 )
                {
                    m_pointIgnoreList.PutData(i);
                }
                else
                {
                    // Ordered insert

                    bool added = false;
                    for( int j = 0; j < validTargetPoints.Size(); ++j )
                    {
                        ValidTargetPoint point = *validTargetPoints.GetPointer(j);
                        if( sailRange < point.m_sailRange )
                        {
                            validTargetPoints.PutDataAtIndex( newPoint, j );
                            added = true;
                            break;
                        }
                    }
                    if( !added )
                    {
                        validTargetPoints.PutDataAtEnd(newPoint);
                    }
                }
            }
        }
    }


    while(validTargetPoints.Size())
    {
        int numValid = ( validTargetPoints.Size() * Fixed::Hundredths(20) ).IntValue();
        numValid = max( numValid, 1 );
        int index = syncrand() % numValid;
        ValidTargetPoint point = *validTargetPoints.GetPointer(index);

        validTargetPoints.RemoveData(index);

        Fixed longitude = point.m_longitude + syncsfrand( 10 );
        Fixed latitude = point.m_latitude + syncsfrand( 10 );

        if( IsValidFleetPosition(longitude, latitude) )
        {
            _longitude = longitude;
            _latitude = latitude;
            END_PROFILE( "FindAttackSpot" );
            return true;
        }
    }


    END_PROFILE( "FindAttackSpot" );
    return false;
}


void Fleet::RunAIIntercepting()
{    
    WorldObject *obj = g_app->GetWorld()->GetWorldObject( m_targetObjectId );
    if( obj )
    {
        if( m_targetLongitude == 0 && m_targetLatitude == 0 )
        {
            if( IsValidFleetPosition( obj->m_lastKnownPosition[m_teamId].x, 
                                      obj->m_lastKnownPosition[m_teamId].y ) )
            {
                MoveFleet( obj->m_lastKnownPosition[m_teamId].x, 
                           obj->m_lastKnownPosition[m_teamId].y );
                if( CountNukesInFleet() > 0 )
                {
                    m_aiState = AIStateSneaking;
                }
                else
                {
                    m_aiState = AIStateHunting;
                }
                return;
            }
        }

        if( g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, m_targetLongitude, m_targetLatitude ) < 10 * 10 )
        {
            if( obj->m_visible[m_teamId] )
            {
                if( g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, obj->m_longitude, obj->m_latitude ) > 10 * 10 )
                {
                    MoveFleet( obj->m_longitude, obj->m_latitude );
                }
            }
            else
            {
                for( int i = 0; i < m_fleetMembers.Size(); ++i )
                {
                    if( m_fleetMembers.ValidIndex(i) )
                    {
                        WorldObject *obj = g_app->GetWorld()->GetWorldObject( m_fleetMembers[i] );
                        if( obj )
                        {
                            if( obj->m_type == WorldObject::TypeCarrier  )
                            {
                                obj->SetState(2);
                            }
                        }
                    }
                }
            }
        }
    }
    else
    {
        if( CountNukesInFleet() > 0 )
        {
            m_aiState = AIStateSneaking;
        }
        else
        {
            m_aiState = AIStateHunting;
        }
    }
}

void Fleet::GetFleetMembers( int fleetType, int *ships, int *subs, int *carriers )
{
    int ship = WorldObject::TypeBattleShip;
    int sub = WorldObject::TypeSub;
    int carrier = WorldObject::TypeCarrier;

    int fleetMembers[ NumFleetTypes ] [ 3 ] =     

        {   
            //ships     //subs      // carriers
            6,          0,          0,          // FleetTypeBattleShips
            3,          0,          3,          // FleetTypeScout
            0,          3,          3,          // FleetTypeNuke
            0,          6,          0,          // FleetTypeSub
            2,          2,          2,          // FleetTypeMixed
            2,          0,          4,          // FleetTypeAntiSub
            5,          0,          1,          // FleetTypeDefender
            0,          2,          4,          // FleetTypeDefender2
        };

    *ships      = fleetMembers[fleetType][0];
    *subs       = fleetMembers[fleetType][1];
    *carriers   = fleetMembers[fleetType][2];
}

int Fleet::CountSurvivingMembers()
{
    int numMembers = 0;

    for( int i = 0; i < m_fleetMembers.Size(); ++i )
    {
        WorldObject *obj = g_app->GetWorld()->GetWorldObject( m_fleetMembers[i] );
        if( obj && obj->m_life > 0 )
        {
            numMembers++;
        }
    }

    return numMembers;
}

void Fleet::GetFleetPosition( Fixed *longitude, Fixed *latitude )
{
    Fixed totalLong = 0;
    Fixed totalLat = 0;
    Fixed previousTotal = 0;
    bool onSeam = IsOnSameSideOfSeam();
    int numMembers = 0;
    for( int i = 0; i < m_fleetMembers.Size(); ++i )
    {
        WorldObject *obj = g_app->GetWorld()->GetWorldObject( m_fleetMembers[i] );
        if( obj )
        {
            numMembers++;
            totalLong += obj->m_longitude;
            totalLat += obj->m_latitude;

            if( !onSeam )
            {
                if( previousTotal != 0 )
                {
                    if( previousTotal < 0 &&
                        previousTotal < totalLong )
                    {
                        totalLong -= 360;
                    }
                    else if( previousTotal > 0 &&
                             previousTotal > totalLong )
                    {
                        totalLong += 360;
                    }
                }

                previousTotal = totalLong;
            }
        }
    }
    if( numMembers > 0 )
    {
        *longitude = totalLong / numMembers;
        *latitude = totalLat / numMembers;
    }
}

bool Fleet::IsAtTarget()
{
    if( g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, m_targetLongitude, m_targetLatitude ) <= 1 )
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool Fleet::IsInCombat()
{
    for( int i = 0; i < m_fleetMembers.Size(); ++i )
    {
        WorldObject *obj = g_app->GetWorld()->GetWorldObject( m_fleetMembers[i] );
        if( obj )
        {
            if( obj->m_targetObjectId != -1 ||
                obj->m_isRetaliating )
            {
                return true;
            }
        }
    }
    return false;
}

bool Fleet::IsInFleet( int objectType )
{
    for( int i = 0; i < m_fleetMembers.Size(); ++i )
    {
        WorldObject *obj = g_app->GetWorld()->GetWorldObject( m_fleetMembers[i] );
        if( obj &&
            obj->m_type == objectType )
        {
            return true;
        }
    }
    return false;
}

int Fleet::GetFleetMemberId( int objectId )
{
    for( int i = 0; i < m_fleetMembers.Size(); ++i )
    {
        if( m_fleetMembers[i] == objectId )
        {
            return i;
        }
    }
    return -1;
}

bool Fleet::IsOnSameSideOfSeam()
{
    Fixed previousLong = 0;
    for( int i = 0; i < m_fleetMembers.Size(); ++i )
    {
        WorldObject *obj = g_app->GetWorld()->GetWorldObject(m_fleetMembers[i]);
        if( obj )
        {
            if( previousLong != 0 )
            {
                if( previousLong < -160 &&
                    obj->m_longitude > 160 )
                {
                    return false;
                }
                else if( previousLong > 160 &&
                         obj->m_longitude < -160 )
                {
                    return false;
                }
            }
            previousLong = obj->m_longitude;
        }
    }
    return true;
}

void Fleet::StopFleet()
{
    m_targetLongitude = 0;
    m_targetLatitude = 0;
    m_pursueTarget = false;
    m_targetFleet = -1;
    m_targetTeam = -1;
    for( int i = 0; i < m_fleetMembers.Size(); ++i )
    {
        MovingObject *obj = (MovingObject *)g_app->GetWorld()->GetWorldObject( m_fleetMembers[i] );
        if( obj &&
            obj->IsMovingObject() )
        {
            obj->ClearWaypoints();
            obj->m_vel.Zero();
        }
    }
}

void Fleet::CheckFleetFormation()
{
    bool crossingSeam = IsOnSameSideOfSeam();
    if( !crossingSeam &&
        m_longitude > -180 && m_longitude < 180 )
    {
        for( int i = 0; i < m_fleetMembers.Size(); ++i )
        {
            WorldObject *obj = g_app->GetWorld()->GetWorldObject( m_fleetMembers[i] );
            if( obj )
            {
                if( g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, obj->m_longitude, obj->m_latitude ) > 10 * 10 )
                {
                    Fixed longitude = m_targetLongitude;
                    Fixed latitude = m_targetLatitude;
                    StopFleet();
                    m_targetLongitude = longitude;
                    m_targetLatitude = latitude;
                    AlterWaypoints( m_longitude, m_latitude, false );
                    return;
                }
            }
        }
    }
}

void Fleet::CounterSubs()
{
    int carrierId = -1;
    for( int i = 0; i < m_fleetMembers.Size(); ++i )
    {
        WorldObject *obj = g_app->GetWorld()->GetWorldObject( m_fleetMembers[i] );
        if( obj && obj->m_type == WorldObject::TypeCarrier )
        {
            if( obj->m_currentState == 2 )
            {
                return;
            }
            else
            {
                carrierId = obj->m_objectId;
            }
        }
    }
    if( carrierId != -1 )
    {
        WorldObject *obj = g_app->GetWorld()->GetWorldObject( carrierId );
        if( obj )
        {
            obj->SetState(2);
        }
    }
}

int Fleet::CountNukesInFleet()
{
    int nukesInFleet = 0;
    for( int i = 0; i < m_fleetMembers.Size(); ++i )
    {
        WorldObject *obj = g_app->GetWorld()->GetWorldObject( m_fleetMembers[i] );
        if( obj )
        {
            if( obj->m_type == WorldObject::TypeSub )
            {
                nukesInFleet += obj->m_states[2]->m_numTimesPermitted;
            }
            else if(obj->m_type == WorldObject::TypeCarrier )
            {
                nukesInFleet += obj->m_states[1]->m_numTimesPermitted;
            }
        }
    }
    return nukesInFleet;
}

void Fleet::CreateBlip()
{
    if( !FindBlip( Blip::BlipTypeSelect ) )
    {
        if( m_targetLongitude != 0 )
        {        
            CreateBlip( m_targetLongitude, m_targetLatitude, Blip::BlipTypeHighlight );            
        }
    }
}

void Fleet::CreateBlip( Fixed longitude, Fixed latitude, int type )
{
    if( !FindBlip( type ) )
    {
        if( m_blipTimer <= 0 )
        {
            if( g_app->GetWorld()->GetDistanceSqd( longitude, latitude, m_longitude, m_latitude ) > 6 * 6 )
            {
                Fixed directDistanceSqd = g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, longitude, latitude, true);

                Fixed targetSeamLatitude;
                Fixed targetSeamLongitude;
                g_app->GetWorld()->GetSeamCrossLatitude( Vector3<Fixed>( longitude, latitude, 0 ), Vector3<Fixed>(m_longitude, m_latitude, 0), &targetSeamLongitude, &targetSeamLatitude);
                Fixed distanceAcrossSeamSqd = g_app->GetWorld()->GetDistanceAcrossSeamSqd( m_longitude, m_latitude, longitude, latitude);

                if( distanceAcrossSeamSqd < directDistanceSqd )
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

                m_blipTimer = 5;
                Blip *blip = new Blip();
                blip->m_teamId = m_teamId;
                blip->m_longitude = m_longitude;
                blip->m_latitude = m_latitude;
                blip->m_targetNodeId = g_app->GetWorld()->GetClosestNode( longitude, latitude );
                blip->m_finalTargetLongitude = longitude;
                blip->m_finalTargetLatitude = latitude;
                blip->m_origin = m_fleetId;
                blip->m_type = type;
                blip->Update();
                m_movementBlips.PutData( blip );
            }
        }
        else
        {
            m_blipTimer -= SERVER_ADVANCE_PERIOD;
        }
    }
}

bool Fleet::FindBlip( int type )
{
    for( int i = 0; i < m_movementBlips.Size(); ++i )
    {
        if( m_movementBlips[i]->m_type == type )
        {
            return true;
        }
    }
    return false;
}

bool Fleet::IsInNukeRange()
{
    int targetTeam = g_app->GetWorld()->GetTeam( m_teamId )->m_targetTeam;

    if( g_app->GetGame()->m_victoryTimer > 0 )
    {        
        Fixed maxrange = 40;
        Fixed bestDistSqd = 40 * 40;

        for( int i = 0; i < World::NumTerritories; ++i )
        {
            int owner = g_app->GetWorld()->GetTerritoryOwner(i);
            if( owner != -1 &&
                !g_app->GetWorld()->IsFriend( owner, m_teamId ) )
            {
                Fixed popCenterLong = g_app->GetWorld()->m_populationCenter[i].x;
                Fixed popCenterLat = g_app->GetWorld()->m_populationCenter[i].y;
                Fixed distSqd = g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, popCenterLong, popCenterLat );

                if( distSqd < bestDistSqd )
                {                
                    bestDistSqd = distSqd;
                    targetTeam = g_app->GetWorld()->GetTerritoryOwner(i);
                }
            }
        }
        Fixed longitude = 0;
        Fixed latitude = 0;

        Nuke::FindTarget( m_teamId, targetTeam, m_fleetMembers[0], maxrange, &longitude, &latitude );

        if( longitude != 0 && latitude != 0 )
        {
            return true;
        }
        return false;
    }

    for( int i = 0; i < World::NumTerritories; ++i )
    {
        Fixed maxrangeSqd = 40 * 40;

        int owner = g_app->GetWorld()->GetTerritoryOwner(i);
        if( owner != -1 &&
            !g_app->GetWorld()->IsFriend( owner, m_teamId ) )
        {
            Fixed popCenterLong = g_app->GetWorld()->m_populationCenter[i].x;
            Fixed popCenterLat = g_app->GetWorld()->m_populationCenter[i].y;

            if( g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, popCenterLong, popCenterLat ) < maxrangeSqd )
            {
                return true;
            }
        }
    }
    return false;
}

bool Fleet::IsIgnoringPoint( int pointId )
{
    for( int i = 0; i < m_pointIgnoreList.Size(); ++i )
    {
        if( m_pointIgnoreList[i] == pointId )
        {
            return true;
        }
    }
    return false;
}

Fixed Fleet::GetSpeedDropoff()
{
    if( IgnoreSpeedDropoff() )
    {
        return 1;
    }

    Fixed minDistanceSqd = 10 * 10;
    for( int t = 0; t < g_app->GetWorld()->m_teams.Size(); ++t )
    {
        Team *team = g_app->GetWorld()->m_teams[t];
        if( team->m_teamId != m_teamId )
        {
            for( int f = 0; f < team->m_fleets.Size(); ++f )
            {
                Fleet *fleet = team->GetFleet(f);
                if( fleet->m_fleetMembers.Size() > 0 &&
                    !fleet->IgnoreSpeedDropoff() )
                {
                    Fixed thisDistanceSqd = g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, fleet->m_longitude, fleet->m_latitude );
                    if( thisDistanceSqd < minDistanceSqd )
                    {
                        minDistanceSqd = thisDistanceSqd;
                    }
                }
            }
        }
    }
    Fixed dropOff =  minDistanceSqd / (10 * 10);
    return ( dropOff < Fixed::Hundredths(20) ? Fixed::Hundredths(20) : dropOff );
}

bool Fleet::IgnoreSpeedDropoff()
{
    for( int i = 0; i < m_fleetMembers.Size(); ++i )
    {
        WorldObject *obj = g_app->GetWorld()->GetWorldObject( m_fleetMembers[i] );
        if( obj &&
            obj->m_type != WorldObject::TypeSub )
        {
            return false;
        }
    }

    return true;
}


char *Fleet::LogState()
{
    static char s_buffer[1024];
    snprintf( s_buffer, 1024, "id[%d] FLEET targetlong[%12.8f] targetLat[%12.8f] pathcheck[%2.2f] speedupdate[%2.2f] state[%d] aistate[%d] targetObj[%d] targetFleet[%d]",
                        m_fleetId,
                        m_targetLongitude.DoubleValue(),
                        m_targetLatitude.DoubleValue(),
                        m_pathCheckTimer.DoubleValue(),
                        m_speedUpdateTimer.DoubleValue(),
                        m_currentState,
                        m_aiState,
                        m_targetObjectId,
                        m_targetFleet );

    return s_buffer;
}