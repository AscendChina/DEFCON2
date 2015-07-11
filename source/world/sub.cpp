#include "lib/universal_include.h"
#include "lib/resource/resource.h"
#include "lib/resource/image.h"
#include "lib/language_table.h"
#include "lib/math/random_number.h"

#include "app/app.h"
#include "app/globals.h"
#include "app/game.h"

#include "renderer/map_renderer.h"

#include "lib/profiler.h"

#include "interface/interface.h"

#include "world/world.h"
#include "world/sub.h"
#include "world/nuke.h"
#include "world/fleet.h"


Sub::Sub()
:   MovingObject(),
    m_hidden(true)
{
    SetType( TypeSub );

    strcpy( bmpImageFilename, "graphics/sub.bmp" );

    m_radarRange = 3;
    m_speed = Fixed::Hundredths(2);
    m_turnRate = Fixed::Hundredths(1);
    m_selectable = true;  
    m_maxHistorySize = 10;
    m_range = Fixed::MAX;
    m_movementType = MovementTypeSea;
    m_nukeSupply = 10;
    m_life = 8;
    
    m_ghostFadeTime = 100;
    
    AddState( LANGUAGEPHRASE("state_passivesonar"), 240, 20, 0, 5, true, -1, 3 );
    AddState( LANGUAGEPHRASE("state_activesonar"), 20, 15, 0, 8, false, -1, 3 );
    AddState( LANGUAGEPHRASE("state_subnuke"), 20, 10, 3, Fixed::MAX, true, 10, 1 );
	//AddState( LANGUAGEPHRASE("state_submirv"), 10, 10, 3, Fixed::MAX, true, 5, 1 );
	AddState( LANGUAGEPHRASE("state_submirv"), 10, 10, 3, Fixed::MAX, true, 10, 5 );
	m_states[2]->m_numTimesPermitted = m_nukeSupply;

    InitialiseTimers();
}

void Sub::Action( int targetObjectId, Fixed longitude, Fixed latitude )
{
    if( !CheckCurrentState() )
    {
        return;
    }

    if( m_currentState == 0 || m_currentState == 1 )
    {
        WorldObject *targetObject = g_app->GetWorld()->GetWorldObject(targetObjectId);
        if( targetObject )
        {
            if( targetObject->m_visible[m_teamId] && 
                g_app->GetWorld()->GetAttackOdds( m_type, targetObject->m_type ) > 0 )
            {
                m_targetObjectId = targetObjectId;
            }
        }            
    }
    else if( m_currentState == 2  || m_currentState == 3 )
    {
        if( m_stateTimer <= 0 )
        {
            g_app->GetWorld()->LaunchNuke( m_teamId, m_objectId, longitude, latitude, 90 );

            Fleet *fleet = g_app->GetWorld()->GetTeam( m_teamId )->GetFleet( m_fleetId );
            if( fleet ) fleet->m_subNukesLaunched++;

        }
        else 
        {
            return;
        }
    }

    WorldObject::Action( targetObjectId, longitude, latitude );
}

bool Sub::IsHiddenFrom()
{
    if( ( m_currentState <= 0 || m_currentState == 1 ) 
         && m_stateTimer <= 0 ) 
    {
        // Active or passive sonar
        // We are under water
        m_hidden = true;
        return true;
    }
    else if( ( m_currentState <= 0 || m_currentState == 1 ) && 
             m_stateTimer > 0 )
    {
        // Switching to active or passive sonar
        // Hidden status depends on the previous state
        return( m_hidden );
    }
    else
    {
        // We aren't in a hidden state
        m_hidden = false;
        return WorldObject::IsHiddenFrom();
    }
}

bool Sub::Update()
{        
    if( m_currentState == 2 || m_currentState == 3 )
    {
        strcpy( bmpImageFilename, "graphics/sub_surfaced.bmp" );
    }
    else
    {
        strcpy( bmpImageFilename, "graphics/sub.bmp" );
    }

    Fleet *fleet = g_app->GetWorld()->GetTeam( m_teamId )->GetFleet( m_fleetId );
    if( fleet )
    {
        bool canAttack = false;
        if( m_stateTimer <= 0 && m_currentState == 1 )
        {
            Ping();
            m_stateTimer = m_states[1]->m_timeToReload;
            canAttack = true;
        }

        // Are we shooting?
        bool haveValidTarget = false;

        if( m_targetObjectId != -1 )
        {
            WorldObject *targetObject = g_app->GetWorld()->GetWorldObject(m_targetObjectId);
            if( targetObject )
            {
                if( targetObject->m_visible[m_teamId] ||
                    targetObject->m_seen[m_teamId] )
                {                
                    Fixed distanceSqd = g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, targetObject->m_longitude, targetObject->m_latitude);

                    if( distanceSqd <= GetActionRangeSqd() )
                    {
                        haveValidTarget = true;
                        if( m_stateTimer <= 0 || canAttack )
                        {
                            FireGun( GetActionRange() );
                            fleet->FleetAction( m_targetObjectId );
                            if( m_currentState == 0 )
                            {
                                m_stateTimer = m_states[0]->m_timeToReload;
                            }
                        }
                    }
                }
            }
        }

        if( !haveValidTarget && m_retargetTimer <= 0 )
        {
            START_PROFILE("FindTarget");
            m_targetObjectId = -1;
            if( g_app->GetWorld()->GetDefcon() < 4 )
            {
                m_retargetTimer = 4 + syncfrand(11);
                WorldObject *obj = FindTarget();
                if( obj )
                {
                    m_targetObjectId = obj->m_objectId;
                }                
            }
            END_PROFILE("FindTarget");
        }
    }

    if( m_currentState != 2 )
    {
        bool arrived = MoveToWaypoint();
    }
    else if( m_states[m_currentState]->m_numTimesPermitted == 0 &&
             m_currentState != 0 )
    {
        SetState(0);
    }
    
    return MovingObject::Update();
}

void Sub::Render()
{
    MovingObject::Render();



    //
    // Render nuke count

    if( m_teamId == g_app->GetWorld()->m_myTeamId ||
        g_app->GetWorld()->m_myTeamId == -1 ||
        g_app->GetGame()->m_winner != -1 )
    {   
        float predictionTime = g_predictionTime * g_app->GetWorld()->GetTimeScaleFactor().DoubleValue();
        float predictedLongitude = m_longitude.DoubleValue() + m_vel.x.DoubleValue() * predictionTime;
        float predictedLatitude = m_latitude.DoubleValue() + m_vel.y.DoubleValue() * predictionTime; 

        int numNukesInStore = m_states[2]->m_numTimesPermitted;
        int numNukesInQueue = m_actionQueue.Size();

        Team *team = g_app->GetWorld()->GetTeam(m_teamId);
        Colour colour = team->GetTeamColour();            
        colour.m_a = 150;

        Image *bmpImage = g_resource->GetImage("graphics/smallnuke.bmp");
        if( bmpImage )
        {
            float x = predictedLongitude - GetSize().DoubleValue() * 0.2f;
            float y = predictedLatitude;       
            float nukeSize = GetSize().DoubleValue() * 0.35f;
            float dx = nukeSize * 0.5f;
            
            if( team->m_territories[0] >= 3 )
            {
                dx *= -1;
            }       

            for( int i = 0; i <  ( numNukesInStore * 0.5f ); ++i ) //phuck
            {
                if( i >= (numNukesInStore-numNukesInQueue) )
                {
                    colour.Set( 128,128,128,100 );
                }
                
                g_renderer->Blit( bmpImage, x, y, nukeSize, -nukeSize, colour );
                x -= dx;
            }
        }
    }
}

void Sub::RunAI()
{
    START_PROFILE("SubAI");
    AppDebugAssert( m_type == WorldObject::TypeSub );
    if( m_aiTimer <= 0 )
    {
        m_aiTimer = m_aiSpeed;
        Team *team = g_app->GetWorld()->GetTeam( m_teamId );
        Fleet *fleet = team->GetFleet( m_fleetId );

        for( int i = 0; i < m_actionQueue.Size(); ++i )
        {
            ActionOrder *action = m_actionQueue[i];
            if( action->m_targetObjectId != -1 )
            {
                WorldObject *obj = g_app->GetWorld()->GetWorldObject( action->m_targetObjectId );
                if( !obj )
                {
                    m_actionQueue.RemoveData(i);
                    delete action;
                    --i;
                }
            }
        }

        if( g_app->GetWorld()->GetDefcon() == 1 )
        {
            if( fleet->IsInNukeRange() )
            {
                if( team->m_maxTargetsSeen >= 4 ||
                    team->m_currentState >= Team::StateAssault &&
                    m_stateTimer == 0 )
                {            
                    if( m_states[2]->m_numTimesPermitted - m_actionQueue.Size() <= 0 )
                    {
                        END_PROFILE("SubAI");
                        return;
                    }
                    int targetsInRange = 0;
                    Fixed maxRange = 40;
                    Fixed maxRangeSqd = 40 * 40;
                    for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
                    {
                        if( g_app->GetWorld()->m_objects.ValidIndex(i) )
                        {
                            WorldObject *obj = g_app->GetWorld()->m_objects[i];
                            if( !obj->IsMovingObject() &&
                                obj->m_seen[m_teamId] &&
                                !g_app->GetWorld()->IsFriend( obj->m_teamId, m_teamId ) &&
                                g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, obj->m_longitude, obj->m_latitude ) < maxRangeSqd )
                            {
                                targetsInRange ++;
                            }
                        }
                    }

                    Fixed longitude = 0;
                    Fixed latitude = 0;
                    int objectId = -1;
                    int targetTeam = team->m_targetTeam;
                    if( g_app->GetGame()->m_victoryTimer > 0 )
                    {
                        for( int i = 0; i < World::NumTerritories; ++i )
                        {
                            int owner = g_app->GetWorld()->GetTerritoryOwner(i);
                            if( owner != -1 &&
                                !g_app->GetWorld()->IsFriend(owner, m_teamId ) )
                            {
                                Fixed popCenterLong = g_app->GetWorld()->m_populationCenter[i].x;
                                Fixed popCenterLat = g_app->GetWorld()->m_populationCenter[i].y;

                                if( g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, popCenterLong, popCenterLat ) < maxRangeSqd )
                                {                                
                                    targetTeam = g_app->GetWorld()->GetTerritoryOwner(i);
                                    break;
                                }
                            }
                        }
                    }

                    Nuke::FindTarget( m_teamId, targetTeam, m_objectId, maxRange, &longitude, &latitude, &objectId );
                    if( (longitude != 0 && latitude != 0) &&
                        (targetsInRange >= 4  || (team->m_currentState >= Team::StateAssault ) ))
                    {
                        if( m_states[2]->m_numTimesPermitted - m_actionQueue.Size() > 0 )
                        {
                            if( m_currentState != 2 )
                            {
                                SetState(2);
                            }
                            ActionOrder *action = new ActionOrder();
                            action->m_longitude = longitude;
                            action->m_latitude = latitude;
                            action->m_targetObjectId = objectId;
                            RequestAction( action );
                        }
                    }
                    else
                    {
                        if( m_actionQueue.Size() == 0 )
                        {
                            SetState(0);
                        }
                    }
                }
            }
            else
            {
                if( m_actionQueue.Size() == 0 )
                {
                    if( m_currentState != 0 )
                    {
                        SetState(0);
                    }
                }
            }
        }
    }
    END_PROFILE("SubAI");
}

WorldObject *Sub::FindTarget()
{
    Team *team = g_app->GetWorld()->GetTeam( m_teamId );
    Fleet *fleet = team->GetFleet( m_fleetId );

    int objectsSize = g_app->GetWorld()->m_objects.Size();
    for( int i = 0; i < objectsSize; ++i )
    {
        if( g_app->GetWorld()->m_objects.ValidIndex(i) )
        {
            WorldObject *obj = g_app->GetWorld()->m_objects[i];
            Fleet *fleetTarget = g_app->GetWorld()->GetTeam( obj->m_teamId )->GetFleet( obj->m_fleetId );

            if( fleetTarget &&
                obj->m_visible[m_teamId] &&
                !team->m_ceaseFire[ obj->m_teamId ] &&
                !g_app->GetWorld()->IsFriend( obj->m_teamId, m_teamId ) &&
                g_app->GetWorld()->GetAttackOdds( WorldObject::TypeSub, obj->m_type ) > 0 )
            {
                bool safeTarget = ( m_states[2]->m_numTimesPermitted == 0 || IsSafeTarget( fleetTarget ) );
                if( safeTarget &&
                    g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, obj->m_longitude, obj->m_latitude) <= 5 * 5 )
                {
                    if( fleet->m_pursueTarget )
                    {
                        if( obj->m_teamId == fleet->m_targetTeam &&
                            obj->m_fleetId == fleet->m_targetFleet )
                        {
                            return obj;
                        }
                    }
                    else
                    {
                        if( m_currentState == 1 )
                        {
                            return obj;
                        }
                    }
                }
            }
        }
    }
    return NULL;
}

int Sub::GetAttackState()
{
    return 0;
}

bool Sub::IsIdle()
{
    if( m_targetObjectId == -1 &&
        m_targetLongitude == 0 &&
        m_targetLatitude == 0 )
        return true;
    else return false;
}

bool Sub::UsingGuns()
{
    if( m_currentState == 0 ||
        m_currentState == 1 )
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool Sub::UsingNukes()
{
    if( m_currentState == 2 || m_currentState == 3)
    {
        return true;
    }
    else
    {
        return false;
    }
}

void Sub::SetState( int state )
{
    MovingObject::SetState( state );
    if( m_currentState == 2 || m_currentState == 3)
    {
        g_app->GetWorld()->GetTeam( m_teamId )->GetFleet( m_fleetId )->StopFleet();
    }
}

void Sub::ChangePosition()
{
    /*if( m_currentState != 0)
    {
        SetState ( 0 );
    }*/
    while(true)
    {
        Fixed longitude = syncsfrand(360);
        Fixed latitude = syncsfrand(180);
        if( !g_app->GetMapRenderer()->IsValidTerritory( m_teamId, longitude, latitude, true ) &&
            g_app->GetMapRenderer()->IsValidTerritory( -1, longitude, latitude, false ) &&
            g_app->GetMapRenderer()->GetTerritory( longitude, latitude, true ) != -1 )
        {
            SetWaypoint( longitude, latitude );
            break;
        }
    }
}

bool Sub::IsActionQueueable()
{
    if( m_currentState == 2 || m_currentState == 3)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool Sub::IsPinging()
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

int Sub::IsValidMovementTarget( Fixed longitude, Fixed latitude )
{
    if( m_currentState == 2 || m_currentState == 3)
    {
        return TypeInvalid;
    }

    return MovingObject::IsValidMovementTarget( longitude, latitude );
}


int Sub::GetAttackOdds( int _defenderType )
{
    if( (m_currentState == 2 || m_currentState == 3)&&
        m_states[2]->m_numTimesPermitted > 0 )
    {
        if( _defenderType == TypeCity ||
            _defenderType == TypeSilo ||
            _defenderType == TypeAirBase ||
            _defenderType == TypeRadarStation )
        {
            return g_app->GetWorld()->GetAttackOdds( TypeNuke, _defenderType );
        }
    }

    if( m_currentState == 2 || m_currentState == 3)
    {
        // We are surfaced, so can't attack anything other than nuke attacks
        return 0;
    }

    return MovingObject::GetAttackOdds( _defenderType );
}


int Sub::IsValidCombatTarget( int _objectId )
{
    WorldObject *obj = g_app->GetWorld()->GetWorldObject( _objectId );
    if( !obj ) return TargetTypeInvalid;

    int basicResult = MovingObject::IsValidCombatTarget( _objectId );
    if( basicResult < TargetTypeInvalid )
    {
        return basicResult;
    }

    bool isFriend = g_app->GetWorld()->IsFriend( m_teamId, obj->m_teamId );    
    if( !isFriend )
    {       
        if(( m_currentState == 2 || m_currentState == 3)&& !obj->IsMovingObject() )
        {
            Fixed distanceSqd = g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, 
                                                                obj->m_longitude, obj->m_latitude );

            Fixed actionRangeSqd = m_states[2]->m_actionRange;
            actionRangeSqd *= actionRangeSqd;

            if( distanceSqd < actionRangeSqd )
            {
                return TargetTypeLaunchNuke;
            }
            else
            {
                return TargetTypeOutOfRange;
            }
        }

        if( m_currentState == 0 || m_currentState == 1 )
        {
            if( obj->IsMovingObject() )
            {
                MovingObject *mobj = (MovingObject *) obj;
                if( mobj->m_movementType == MovementTypeSea )
                {
                    return TargetTypeValid;
                }
            }
        }
    }

    return TargetTypeInvalid;    
}


void Sub::FleetAction( int targetObjectId )
{
    if( m_targetObjectId == -1 )
    {
        WorldObject *obj = g_app->GetWorld()->GetWorldObject( targetObjectId );
        if( obj &&
            obj->m_visible[ m_teamId ] )
        {
            if( m_currentState == 0 || m_currentState == 1 )
            {
                if( g_app->GetWorld()->GetAttackOdds( m_type, obj->m_type ) > 0 )
                {
                    m_targetObjectId = targetObjectId;
                }
            }
        }
    }
}

void Sub::RequestAction(ActionOrder *_action)
{
    if( m_currentState == 2 || m_currentState == 3)
    {
        if(g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, _action->m_longitude, _action->m_latitude ) < GetActionRangeSqd() )
        {
            WorldObject::RequestAction(_action);
        }
    }
    else
    {
        WorldObject::RequestAction(_action);
    }
}


bool Sub::IsSafeTarget( Fleet *_fleet )
{
	int fleetMembersSize = _fleet->m_fleetMembers.Size();
    for( int i = 0; i < fleetMembersSize; ++i )
    {
        WorldObject *ship = g_app->GetWorld()->GetWorldObject( _fleet->m_fleetMembers[i] );
        if( ship )
        {
            if( ship->m_type == WorldObject::TypeCarrier &&
                ship->m_visible[ m_teamId ] )
            {
                return false;
            }
        }
    }
    return true;
}
