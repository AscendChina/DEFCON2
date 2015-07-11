#include "lib/universal_include.h"
#include "lib/resource/resource.h"
#include "lib/resource/image.h"
#include "lib/render/renderer.h"
#include "lib/math/vector3.h"
#include "lib/math/random_number.h"
#include "lib/math/math_utils.h"
#include "lib/language_table.h"
 
#include "app/app.h"
#include "app/globals.h"
#include "app/game.h"

#include "network/ClientToServer.h"

#include "renderer/map_renderer.h"

#include "interface/interface.h"

#include "world/world.h"
#include "world/bomber.h"
#include "world/team.h"


Bomber::Bomber()
:   MovingObject(),
    m_nukeTargetLongitude(0),
    m_nukeTargetLatitude(0),
    m_bombingRun(false)
{
    SetType( TypeBomber );

    strcpy( bmpImageFilename, "graphics/bomber.bmp" );

    m_radarRange = 5;
    m_speed = Fixed::Hundredths(5);
    m_turnRate = Fixed::Hundredths(1);
    m_selectable = true;
    m_maxHistorySize = 10;
    m_range = 150;

    m_nukeSupply = 1;

    m_movementType = MovementTypeAir;

    AddState( LANGUAGEPHRASE("state_airtoseamissile"), 60, 30, 10, 20, true, -1, 3 );
    AddState( LANGUAGEPHRASE("state_bombernuke"), 240, 120, 5, 25, true, 1, 1 );

    InitialiseTimers();
}


void Bomber::Action( int targetObjectId, Fixed longitude, Fixed latitude )
{
    if( !CheckCurrentState() ||
        m_stateTimer > 0 )
    {
        return;
    }

    m_nukeTargetLongitude = 0;
    m_nukeTargetLatitude = 0;

    WorldObject *target = g_app->GetWorld()->GetWorldObject( targetObjectId );
    if( target )
    {
        if( target->m_type == WorldObject::TypeBattleShip ||
            target->m_type == WorldObject::TypeCarrier ||
            target->m_type == WorldObject::TypeSub )
        {
            if( m_currentState != 0 )
            {
                SetState(0);
            }
        }

        if( target->m_type == WorldObject::TypeCity ||
            target->m_type == WorldObject::TypeSilo ||
            target->m_type == WorldObject::TypeRadarStation ||
            target->m_type == WorldObject::TypeAirBase )
        {
            if( m_currentState != 1 &&
                m_states[1]->m_numTimesPermitted > 0 )
            {
                m_bombingRun = true;
                SetNukeTarget( longitude, latitude );
            }
        }
    }


    if( m_currentState == 0 )
    {    
        //if( m_teamId == g_app->GetWorld()->m_myTeamId )
        //{
        //    g_app->GetMapRenderer()->CreateAnimation( MapRenderer::AnimationTypeActionMarker, m_objectId, longitude, latitude );
        //}
        //SetWaypoint( longitude, latitude );
        WorldObject *target = g_app->GetWorld()->GetWorldObject( targetObjectId );
        if( target &&
            target->m_visible[m_teamId] &&
            g_app->GetWorld()->GetAttackOdds( m_type, target->m_type ) > 0)
        {
            m_targetObjectId = targetObjectId;
        }
    }
    else if( m_currentState == 1 )
    {
        if( m_states[1]->m_numTimesPermitted > 0 )
        {
            Fixed nukeRange = GetActionRange();
            if( m_stateTimer <= 0 &&
                g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, longitude, latitude ) <= nukeRange * nukeRange )
            {                
                g_app->GetWorld()->LaunchNuke( m_teamId, m_objectId, longitude, latitude, nukeRange );
                m_nukeTargetLongitude = 0;
                m_nukeTargetLatitude = 0;
                m_bombingRun = true;
            }
            else
            {
                m_bombingRun = true;
                SetNukeTarget( longitude, latitude );
                //SetWaypoint( longitude, latitude );
                return;
            }
        }
    }

    MovingObject::Action( targetObjectId, longitude, latitude );
}

bool Bomber::Update()
{
    Fixed gameScale = World::GetGameScale();

    m_speed = Fixed::Hundredths(5) / gameScale;         
    m_turnRate = Fixed::Hundredths(1) / gameScale;

    bool arrived = MoveToWaypoint();


    //
    // Do we move ?
    
    Vector3<Fixed> oldVel = m_vel;

    bool hasTarget = false;
    if( m_targetObjectId != -1 )
    {
        if( m_currentState == 0 )
        {
            WorldObject *targetObject = g_app->GetWorld()->GetWorldObject(m_targetObjectId);
            if( targetObject )
            {
                if( targetObject->m_teamId == m_teamId )
                {
                    if( targetObject->m_type == WorldObject::TypeCarrier ||
                        targetObject->m_type == WorldObject::TypeAirBase )
                    {
                        SetWaypoint( targetObject->m_longitude, targetObject->m_latitude );
                        Land( m_targetObjectId );
                    }
                    m_targetObjectId = -1;
                }
                else
                {
                    if( m_targetLongitude == 0 && m_targetLatitude == 0 )
                    {
                        SetWaypoint( targetObject->m_longitude, targetObject->m_latitude );
                    }

                    if( targetObject->m_visible[ m_teamId ] )
                    {
                        hasTarget = true;
                        if(m_stateTimer <= 0)
                        {
                            Fixed distanceSqd = g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, targetObject->m_longitude, targetObject->m_latitude);
                            if( distanceSqd <= GetActionRangeSqd() )
                            {
                                FireGun( GetActionRange() );
                                m_stateTimer = m_states[0]->m_timeToReload;
                            }
                        }
                    }
                }
            }
            else
            {
                m_targetObjectId = -1;
            }
        }            
    }

    if( m_currentState == 0 &&
        !hasTarget &&
        m_retargetTimer <= 0 &&
        !m_bombingRun &&
        m_isLanding == -1 )
    {
        m_retargetTimer = 10;
        m_targetObjectId = -1;
        WorldObject *obj = g_app->GetWorld()->GetWorldObject( GetTarget(25) );
        if( obj )
        {
            m_targetObjectId = obj->m_objectId;
        }
    }

    if( arrived )
    {
        m_vel = oldVel;
    }
    
    if( m_targetLongitude == 0 && m_targetLatitude == 0 )
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
    
    if( m_currentState == 1 )
    {
        if( m_nukeTargetLongitude != 0 && m_nukeTargetLatitude != 0 )
        {
            if( m_stateTimer == 0 )
            {
                Fixed distanceSqd = g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, m_nukeTargetLongitude, m_nukeTargetLatitude );
                if( distanceSqd <= GetActionRangeSqd() )
                {
                    Action( -1, m_nukeTargetLongitude, m_nukeTargetLatitude );
                    m_bombingRun = true;
                }
            }
        }
    }

    if( m_bombingRun && m_states[1]->m_numTimesPermitted > 0 &&
        g_app->GetWorld()->GetDefcon() == 1 &&
        m_currentState != 1 )
    {
        SetState(1);
    }

    if( (m_bombingRun && m_states[1]->m_numTimesPermitted ==  0) ||
        IsIdle() )
    {
        Land( GetClosestLandingPad() );
    }

    if( m_currentState == 1 )
    {
        if( m_states[1]->m_numTimesPermitted == 0 )
        {
            SetState(0);
        }
    }

    //
    // Maybe we're in a holding pattern

    /*if( holdingPattern  )
    {
        float timePerUpdate = SERVER_ADVANCE_PERIOD * g_app->GetWorld()->GetTimeScaleFactor();

        m_vel.RotateAroundZ( 0.02f * timePerUpdate );
        m_longitude += m_vel.x * timePerUpdate;
        m_latitude += m_vel.y * timePerUpdate;

        if( m_longitude <= -180.0f ||
            m_longitude >= 180.0f )
        {
            CrossSeam();
        }

        if( m_range <= 0 )
        {
            m_life = 0;
            g_app->GetWorld()->AddOutOfFueldMessage( m_objectId );
        }

    }*/

    return MovingObject::Update();   
}

Fixed Bomber::GetActionRange()
{
    //if( m_currentState == 0 )
    //{
    //    return m_range;
    //}
    //else
    {
        return MovingObject::GetActionRange();
    }
}

void Bomber::RunAI()
{
    if( IsIdle() )
    {
        for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
        {
            if( g_app->GetWorld()->m_objects.ValidIndex(i) )
            {
                WorldObject *obj = g_app->GetWorld()->m_objects[i];
                if( obj )
                {
                    if( g_app->GetWorld()->GetAttackOdds( m_type, obj->m_type ) > 0 &&
                        !g_app->GetWorld()->IsFriend( m_teamId, obj->m_teamId ) &&                        
                        obj->m_visible[m_teamId] &&
                        g_app->GetWorld()->GetDistance( m_longitude, m_latitude, obj->m_longitude, obj->m_latitude ) < m_range - 15 )
                    {
                        SetState(0);
                        Action( obj->m_targetObjectId, obj->m_longitude, obj->m_latitude );
                        return;
                    }
                }
            }
        }
        Land( GetClosestLandingPad() );
    }
}



void Bomber::Land( int targetId )
{
    WorldObject *target = g_app->GetWorld()->GetWorldObject(targetId);
    if( target )
    {
        SetWaypoint( target->m_longitude, target->m_latitude );
        m_isLanding = targetId;
    }
}

bool Bomber::UsingNukes()
{
    if( m_currentState == 1 )
    {
        return true;
    }
    else
    {
        return false;
    }
}

void Bomber::SetNukeTarget( Fixed longitude, Fixed latitude )
{
    m_nukeTargetLongitude = longitude;
    m_nukeTargetLatitude = latitude;
}

void Bomber::Retaliate( int attackerId )
{
    if( g_app->GetWorld()->GetTeam( m_teamId )->m_type == Team::TypeAI )
    {
        if( m_currentState == 1 &&
            m_states[1]->m_numTimesPermitted > 0 &&
            m_stateTimer == 0)
        {
            WorldObject *obj = g_app->GetWorld()->GetWorldObject( attackerId );
            if( obj )
            {
                if( obj->m_type == WorldObject::TypeSilo &&
                    obj->m_seen[ m_teamId ] )
                {
                    if( g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, obj->m_longitude, obj->m_latitude ) <= GetActionRangeSqd() )
                    {
                        Action( -1, obj->m_longitude, obj->m_latitude );
                        m_bombingRun = true;
                    }
                }
            }
        }
    }
}

void Bomber::SetState( int state )
{
    MovingObject::SetState( state );
    if( m_currentState != 1 )
    {
        m_bombingRun = false;
    }
}


int Bomber::GetAttackOdds( int _defenderType )
{
    if( m_states[1]->m_numTimesPermitted > 0 )
    {
        if( _defenderType == TypeCity ||
            _defenderType == TypeSilo ||
            _defenderType == TypeAirBase ||
            _defenderType == TypeRadarStation )
        {
            return g_app->GetWorld()->GetAttackOdds( TypeNuke, _defenderType );
        }
    }

    return MovingObject::GetAttackOdds( _defenderType );
}

int Bomber::IsValidCombatTarget( int _objectId )
{
    WorldObject *obj = g_app->GetWorld()->GetWorldObject( _objectId );
    if( !obj ) return TargetTypeInvalid;

    int basicResult = MovingObject::IsValidCombatTarget( _objectId );
    if( basicResult < TargetTypeInvalid )
    {
        return basicResult;
    }

    if( obj->m_type == TypeCarrier || 
        obj->m_type == TypeAirBase )
    {
        if( obj->m_teamId == m_teamId )
        {
            return TargetTypeLand;
        }
    }


    bool isFriend = g_app->GetWorld()->IsFriend( m_teamId, obj->m_teamId );    
    if( !isFriend )
    {       
        if( !obj->IsMovingObject() &&
            m_states[1]->m_numTimesPermitted > 0 )
        {
            return TargetTypeLaunchNuke;
        }

        if( obj->IsMovingObject() )
        {
            MovingObject *mobj = (MovingObject *) obj;
            if( mobj->m_movementType == MovementTypeSea )
            {
                return TargetTypeValid;
            }
        }
    }

    return TargetTypeInvalid;
}


void Bomber::CeaseFire( int teamId )
{
    if( m_nukeTargetLongitude != 0 && m_nukeTargetLatitude != 0)
    {
        if( g_app->GetMapRenderer()->IsValidTerritory( teamId, m_targetLongitude, m_targetLatitude, false ) )
        {
            m_nukeTargetLongitude = 0;
            m_nukeTargetLatitude = 0;
            m_bombingRun = false;
            Land( GetClosestLandingPad() );
        }
    }
    WorldObject::CeaseFire( teamId );
}


void Bomber::Render()
{
    MovingObject::Render();


    //
    // Render our Nuke

    if( (m_teamId == g_app->GetWorld()->m_myTeamId ||
        g_app->GetWorld()->m_myTeamId == -1 ||
        g_app->GetGame()->m_winner != -1 ) &&
        m_states[1]->m_numTimesPermitted > 0 )
    {
        Fixed predictionTime = Fixed::FromDouble(g_predictionTime) * g_app->GetWorld()->GetTimeScaleFactor();
        float predictedLongitude = (m_longitude + m_vel.x * predictionTime).DoubleValue();
        float predictedLatitude = (m_latitude + m_vel.y * predictionTime).DoubleValue(); 
    
        float angle = atan( -m_vel.x.DoubleValue() / m_vel.y.DoubleValue() );
        if( m_vel.y < 0 ) angle += M_PI;
    
        Team *team = g_app->GetWorld()->GetTeam(m_teamId);
        Colour colour = team->GetTeamColour();            
        colour.m_a = 150;

        Image *bmpImage = g_resource->GetImage( "graphics/smallnuke.bmp" );

        float size = GetSize().DoubleValue() * 0.4f;
        
        g_renderer->Blit( bmpImage, predictedLongitude + m_vel.x.DoubleValue() * 4, predictedLatitude + m_vel.y.DoubleValue() * 4,
						  size/2, size/2, colour, angle );
    }
}

bool Bomber::SetWaypointOnAction()
{
    return true;
}
