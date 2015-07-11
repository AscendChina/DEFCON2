#include "lib/universal_include.h"
#include "lib/resource/resource.h"
#include "lib/resource/image.h"
#include "lib/language_table.h"
#include "lib/math/random_number.h"
#include "lib/hi_res_time.h"

#include "app/app.h"
#include "app/globals.h"

#include "renderer/map_renderer.h"

#include "interface/interface.h"

#include "world/world.h"
#include "world/battleship.h"
#include "world/gunfire.h"
#include "world/fleet.h"


BattleShip::BattleShip()
:   MovingObject()
{
    SetType( TypeBattleShip );

    strcpy( bmpImageFilename, "graphics/battleship.bmp" );
    
    m_radarRange = 10;
    m_speed = Fixed::Hundredths(3);
    m_turnRate = Fixed::Hundredths(1);
    m_selectable = true;  
    m_maxHistorySize = 10;
    m_range = Fixed::MAX;
    m_movementType = MovementTypeSea;    
    m_life = 3;

    m_ghostFadeTime = 150;

    AddState( LANGUAGEPHRASE("state_attack"), 60, 20, 10, 10, true, -1, 3 );

    InitialiseTimers();
}

void BattleShip::Action( int targetObjectId, Fixed longitude, Fixed latitude )
{
    if( !CheckCurrentState() )
    {
        return;
    }

    m_targetObjectId = -1;
        
    WorldObject *target = g_app->GetWorld()->GetWorldObject( targetObjectId );
    if( target )
    {
        if( target->m_visible[m_teamId] &&
            g_app->GetWorld()->GetAttackOdds( m_type, target->m_type, m_objectId ) > 0 )
        {
            m_targetObjectId = targetObjectId;            
        }
    }

    MovingObject::Action( targetObjectId, longitude, latitude );
}

bool BattleShip::Update()
{
    AppDebugAssert( m_type == WorldObject::TypeBattleShip );
    MoveToWaypoint();
    bool hasTarget = false;


    Fleet *fleet = g_app->GetWorld()->GetTeam( m_teamId )->GetFleet( m_fleetId );
    if( fleet )
    {
        if( m_targetObjectId != -1 )
        {
            WorldObject *targetObject = g_app->GetWorld()->GetWorldObject(m_targetObjectId);                  
            if( targetObject )
            {
                if( targetObject->m_visible[m_teamId] )
                {
                    Fixed distance = g_app->GetWorld()->GetDistance( m_longitude, m_latitude, targetObject->m_longitude, targetObject->m_latitude);

                    if( distance <= GetActionRange() )
                    {
                        hasTarget = true;
                        if( m_stateTimer <= 0 )
                        {
                            FireGun( GetActionRange() );
                            m_stateTimer = m_states[ m_currentState ]->m_timeToReload;
                            fleet->FleetAction( m_targetObjectId );
                        }
                    }
                }
            }
        }

        if( !hasTarget && m_retargetTimer <= 0)
        {
            m_targetObjectId = -1;
            if( g_app->GetWorld()->GetDefcon() < 4 )
            {
                m_retargetTimer = 10;
                WorldObject *obj = g_app->GetWorld()->GetWorldObject( GetTarget( 10 ) );
                if( obj )
                {
                    m_targetObjectId = obj->m_objectId;
                }                
            }
        }
    }
    bool amIDead = MovingObject::Update();
    return amIDead;
}

void BattleShip::Render()
{
    MovingObject::Render();
}

void BattleShip::RunAI()
{
    /*if( IsIdle() )
    {
        Fleet *fleet = g_app->GetWorld()->GetTeam( m_teamId )->GetFleet( m_fleetId );
        if( fleet )
        {
            if( fleet->m_targetLongitude && fleet->m_targetLatitude )
            {
                int id = -1;
                for( int i = 0; i < fleet->m_fleetMembers.Size(); ++i )
                {
                    if(fleet->m_fleetMembers[i] == m_objectId )
                    {
                        id = i;
                        break;
                    }
                }
                float longitude = fleet->m_targetLongitude;
                float latitude = fleet->m_targetLatitude;
                fleet->GetFormationPosition( fleet->m_fleetMembers.Size(), id, &longitude, &latitude );
                if( m_longitude != longitude && m_latitude != latitude )
                {
                    SetWaypoint( fleet->m_targetLongitude, fleet->m_targetLatitude );
                }
            }
        }
    }*/
}

int BattleShip::GetAttackState()
{
    return 0;
}

bool BattleShip::UsingGuns()
{
    return true;
}

int BattleShip::GetTarget( Fixed range )
{
    LList<int> targets;
    Team *team = g_app->GetWorld()->GetTeam( m_teamId );
    Fleet *fleet = team->GetFleet( m_fleetId );
    WorldObject *currentTarget = NULL;

    int objectsSize = g_app->GetWorld()->m_objects.Size();
    for( int i = 0; i < objectsSize; ++i )
    {
        if( g_app->GetWorld()->m_objects.ValidIndex(i) )
        {
            WorldObject *obj = g_app->GetWorld()->m_objects[i];
            if( obj->m_teamId != TEAMID_SPECIALOBJECTS )
            {
                bool validNewTarget = !currentTarget ||
                                        GetAttackPriority( currentTarget->m_type ) > GetAttackPriority( obj->m_type );

                if( validNewTarget &&
                    obj->m_visible[m_teamId] &&
                    !team->m_ceaseFire[ obj->m_teamId ] &&
                    !g_app->GetWorld()->IsFriend( obj->m_teamId, m_teamId ) &&                    
                    g_app->GetWorld()->GetAttackOdds( m_type, obj->m_type, m_objectId ) > 0 &&
                    g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, obj->m_longitude, obj->m_latitude ) < range * range )
                {
                    currentTarget = obj;
                }
            }
        }
    }
    if( currentTarget )
    {
        return currentTarget->m_objectId;
    }
    else
    {
        return -1;
    }
}

void BattleShip::FleetAction( int targetObjectId )
{
    if( m_targetObjectId == -1 )
    {
        WorldObject *obj = g_app->GetWorld()->GetWorldObject( targetObjectId );
        if( obj && obj->m_visible[ m_teamId ])
        {
            if( g_app->GetWorld()->GetAttackOdds( m_type, obj->m_type ) > 0 )
            {
                m_targetObjectId = targetObjectId;                
            }
        }
    }
}


int BattleShip::GetAttackPriority( int _type )
{
    switch( _type )
    {
        case WorldObject::TypeSub:          return 1;   
        case WorldObject::TypeCarrier:      return 2;   
        case WorldObject::TypeBomber:       return 3;   
        case WorldObject::TypeFighter:      return 4;   
        case WorldObject::TypeBattleShip:   return 5;  
        default:                            return 6;
    }
}
