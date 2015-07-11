#include "lib/universal_include.h"
#include "lib/resource/resource.h"
#include "lib/resource/image.h"
#include "lib/render/renderer.h"
#include "lib/math/vector3.h"
#include "lib/math/random_number.h"
#include "lib/language_table.h"

#include "app/app.h"
#include "app/globals.h"

#include "renderer/map_renderer.h"

#include "interface/interface.h"

#include "world/world.h"
#include "world/fighter.h"
#include "world/team.h"
#include "world/city.h"


Fighter::Fighter()
:   MovingObject()
{
    SetType( TypeFighter );

    strcpy( bmpImageFilename, "graphics/fighter.bmp" );

    m_radarRange = 5;
    m_speed = Fixed::Hundredths(10);
    m_turnRate = Fixed::Hundredths(4);
    m_selectable = true;
    m_maxHistorySize = 10;
    m_range = 45;

    m_movementType = MovementTypeAir;

    AddState( LANGUAGEPHRASE("state_attack"), 40, 15, 6, 12, true ); //phuck

    InitialiseTimers();
}


void Fighter::Action( int targetObjectId, Fixed longitude, Fixed latitude )
{   
    m_targetObjectId = -1;

    if( m_currentState == 0 )
    {
        //SetWaypoint( longitude, latitude );
        WorldObject *target = g_app->GetWorld()->GetWorldObject( targetObjectId );
        if( target )
        {
            if( target->m_visible[m_teamId] &&
                g_app->GetWorld()->GetAttackOdds( m_type, target->m_type ) > 0)
            {
                m_targetObjectId = targetObjectId;
            }
        }
    }

    if( m_teamId == g_app->GetWorld()->m_myTeamId &&
        targetObjectId == -1 )
    {
        g_app->GetMapRenderer()->CreateAnimation( MapRenderer::AnimationTypeActionMarker, m_objectId,
												  longitude.DoubleValue(), latitude.DoubleValue() );
    }

    MovingObject::Action( targetObjectId, longitude, latitude );
}

bool Fighter::Update()
{   
    bool arrived = MoveToWaypoint();
    bool holdingPattern = false;

    if( m_targetObjectId != -1 )
    {
        WorldObject *targetObject = g_app->GetWorld()->GetWorldObject(m_targetObjectId);  
        if( targetObject )
        {
            if( targetObject->m_teamId == m_teamId )
            {
                if( targetObject->m_type == WorldObject::TypeCarrier ||
                    targetObject->m_type == WorldObject::TypeAirBase )
                {
                    Land( m_targetObjectId );
                }
                m_targetObjectId = -1;
            }
            else
            {
                if( !m_isRetaliating )
                {
                    SetWaypoint( targetObject->m_lastKnownPosition[m_teamId].x, 
                                 targetObject->m_lastKnownPosition[m_teamId].y );
                }
           
                if( arrived )
                {
                    if( !targetObject->m_visible[m_teamId] )
                    {
                        m_targetObjectId = -1;
                    }
                    holdingPattern = true;
                }

                if( m_stateTimer <= 0 )
                {
                    if( targetObject->m_visible[m_teamId] )
                    {
                        Fixed distanceSqd = g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, targetObject->m_longitude, targetObject->m_latitude);

                        if( distanceSqd <= GetActionRangeSqd() )
                        {
                            FireGun( GetActionRange() );
                            m_stateTimer = m_states[ m_currentState ]->m_timeToReload;
                        }
                    }
                    else
                    {
                        m_targetObjectId = -1;
                    }
                }
            }
        }
        else
        {
            m_targetObjectId = -1;
        }
    }

    if( m_targetLongitude == 0 && m_targetLatitude == 0 )
    {
        if( m_targetObjectId == -1 )
        {
            Fixed timePerUpdate = SERVER_ADVANCE_PERIOD * g_app->GetWorld()->GetTimeScaleFactor();
            m_longitude += m_vel.x * Fixed(timePerUpdate);
            m_latitude  += m_vel.y * Fixed(timePerUpdate);
            if( m_range <= 0 )
            {
                m_life = 0;
				m_lastHitByTeamId = m_teamId;
                g_app->GetWorld()->AddOutOfFueldMessage( m_objectId );
            }
        }
        else
        {
            holdingPattern = true;
        }
    }

    if( IsIdle() )
    {
        WorldObject *obj = g_app->GetWorld()->GetWorldObject( GetTarget( ( m_range < 40 ) ? m_range : 40 ) );
        if( obj )
        {
            SetWaypoint( obj->m_longitude, obj->m_latitude );
            m_targetObjectId = obj->m_objectId;
        }
        else
        {
            Land( GetClosestLandingPad() );
        }
    }

    if( holdingPattern )
    {
        Fixed timePerUpdate = SERVER_ADVANCE_PERIOD * g_app->GetWorld()->GetTimeScaleFactor();

        m_vel.RotateAroundZ( Fixed::Hundredths(2) * timePerUpdate );
        m_longitude += m_vel.x * Fixed(timePerUpdate);
        m_latitude += m_vel.y * Fixed(timePerUpdate);

        if( m_longitude <= -180 ||
            m_longitude >= 180 )
        {
            CrossSeam();
        }

        if( m_range <= 0 )
        {
            m_life = 0;
			m_lastHitByTeamId = m_teamId;
            g_app->GetWorld()->AddOutOfFueldMessage( m_objectId );            
        }
    }

    bool amIDead = MovingObject::Update();
    return amIDead;
}

void Fighter::Render()
{
    MovingObject::Render();
}

void Fighter::RunAI()
{
    if( IsIdle() )
    {
        Land( GetClosestLandingPad() );
    }
}

int Fighter::GetAttackState()
{
    return 0;
}


void Fighter::Land( int targetId )
{
    WorldObject *target = g_app->GetWorld()->GetWorldObject(targetId);
    if( target )
    {
        SetWaypoint( target->m_longitude, target->m_latitude );
        m_isLanding = targetId;
    }
}

bool Fighter::UsingGuns()
{
    return true;
}




int Fighter::CountTargettedFighters( int targetId )
{
    int counter = 0;
    for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
    {
        if( g_app->GetWorld()->m_objects.ValidIndex(i) )
        {
            WorldObject *obj = g_app->GetWorld()->m_objects[i];
            if( obj->m_type == WorldObject::TypeFighter &&
                obj->m_teamId == m_teamId &&
                obj->m_targetObjectId == targetId )
            {
                counter++;
            }
        }
    }
    return counter;
}


int Fighter::IsValidCombatTarget( int _objectId )
{
    WorldObject *obj = g_app->GetWorld()->GetWorldObject( _objectId );
    if( !obj ) return TargetTypeInvalid;

    if( obj->m_type == TypeCarrier || 
        obj->m_type == TypeAirBase )
    {
        if( obj->m_teamId == m_teamId )
        {
            return TargetTypeLand;
        }
    }

    return MovingObject::IsValidCombatTarget( _objectId );
}


bool Fighter::SetWaypointOnAction()
{
    return true;
}

