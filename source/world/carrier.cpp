#include "lib/universal_include.h"
#include "lib/resource/resource.h"
#include "lib/resource/image.h"
#include "lib/language_table.h"
#include "lib/math/random_number.h"
#include "lib/sound/soundsystem.h"

#include "app/app.h"
#include "app/globals.h"
#include "app/game.h"

#include "renderer/map_renderer.h"

#include "world/world.h"
#include "world/carrier.h"
#include "world/bomber.h"
#include "world/fighter.h"
#include "world/city.h"
#include "world/nuke.h"
#include "world/depthcharge.h"
#include "world/fleet.h"



Carrier::Carrier()
:   MovingObject()
{
    SetType( TypeCarrier );
    
    strcpy( bmpImageFilename, "graphics/carrier.bmp" );
    strcpy( bmpFighterMarkerFilename, "graphics/fighter.bmp" );
    strcpy( bmpBomberMarkerFilename, "graphics/bomber.bmp" );

    m_radarRange = 22;
    m_selectable = false;  
    m_speed = Fixed::Hundredths(3);
    m_turnRate = Fixed::Hundredths(1);
    m_movementType = MovementTypeSea;
    m_maxHistorySize = 10;
    m_range = Fixed::MAX;
    m_life = 3;
    
    m_nukeSupply = 6;

    m_maxFighters = 5;
    m_maxBombers = 2;
    
    m_ghostFadeTime = 150;
    
    AddState( LANGUAGEPHRASE("state_fighterlaunch"), 10, 20, 15, 45, true, 5, 3 );
    AddState( LANGUAGEPHRASE("state_bomberlaunch"), 120, 120, 15, 140, true, 2, 3 );
    AddState( LANGUAGEPHRASE("state_antisub"), 240, 60, 15, 5, false, -1, 3 );

    InitialiseTimers();
}


void Carrier::RequestAction(ActionOrder *_action)
{
    if( m_currentState < 2 )
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


void Carrier::Action( int targetObjectId, Fixed longitude, Fixed latitude )
{
    if( !CheckCurrentState() )
    {
        return;
    }

    WorldObject *obj = g_app->GetWorld()->GetWorldObject( targetObjectId );
    if( obj )
    {
        longitude = obj->m_longitude;
        latitude = obj->m_latitude;
    }

    if( m_stateTimer <= 0 )
    {
        if( m_currentState == 0 )
        {        
            if(!LaunchFighter( targetObjectId, longitude, latitude ))
            {
                return;
            }
        }
        else if( m_currentState == 1 )
        {
            if(!LaunchBomber( targetObjectId, longitude, latitude ))
            {
                return;
            }
        }
        MovingObject::Action( targetObjectId, longitude, latitude );
    }
}

bool Carrier::Update()
{
    AppDebugAssert( m_type == WorldObject::TypeCarrier );
    MoveToWaypoint(); 
    Fleet *fleet = g_app->GetWorld()->GetTeam( m_teamId )->GetFleet( m_fleetId );
    if( fleet )
    {
        bool attackPossible = false;

        if( m_stateTimer <= 0 && m_currentState == 2 )
        {
            Ping();
            m_stateTimer = m_states[2]->m_timeToReload;
            attackPossible = true;
        }

        if( attackPossible && fleet->m_currentState != Fleet::FleetStatePassive )
        {
            if( m_targetObjectId != -1 && m_currentState == 2 )
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
                            FireGun( GetActionRange() );
                        }
                    }
                }
                else
                {
                    m_targetObjectId = -1;
                }
            }
        }
    }

    if( m_currentState != 2 )
    {
        int altState = (m_currentState == 0 ? 1 : 0);
        if( m_states[m_currentState]->m_numTimesPermitted == 0 &&
            m_states[altState]->m_numTimesPermitted > 0 )
        {
            SetState( altState );
        }
        if( m_states[0]->m_numTimesPermitted == 0 &&
            m_states[1]->m_numTimesPermitted == 0 &&
            m_currentState != 2 )
        {
            SetState(2);
        }
    }

    return MovingObject::Update();
}


void Carrier::RunAI()
{    
    if( m_aiTimer <= 0 )
    {
        if( g_app->GetWorld()->GetDefcon() <= 3 )
        {
            m_aiTimer = m_aiSpeed;
            Team *team = g_app->GetWorld()->GetTeam( m_teamId );
            Fleet *fleet = team->GetFleet( m_fleetId );
            if( m_states[m_currentState]->m_numTimesPermitted - m_actionQueue.Size() <= 0 &&
                m_currentState != 2 )
            {
                return;
            }
        
            if( m_currentState == 2 )
            {
                if( fleet->m_currentState != Fleet::AIStateIntercepting &&
                    m_targetObjectId == -1 )
                {
                    for( int i = 0; i < fleet->m_fleetMembers.Size(); ++i )
                    {
                        WorldObject *obj = g_app->GetWorld()->GetWorldObject( fleet->m_fleetMembers[i] );
                        if( obj )
                        {
                            if( obj->m_type == WorldObject::TypeCarrier && 
                                obj->m_currentState == 2 && 
                                obj->m_objectId != m_objectId )
                            {
                                if( m_states[0]->m_numTimesPermitted > 0 )
                                {
                                    SetState(0);
                                }
                                else if( m_states[1]->m_numTimesPermitted > 0 )
                                {
                                    SetState(1);
                                }
                                break;
                            }
                        }
                    }
                }
            }

            if( team->m_targetsVisible >= 4 ||
                team->m_currentState >= Team::StateAssault )
            {
                if( m_states[1]->m_numTimesPermitted > 0 &&
                    g_app->GetWorld()->GetDefcon() == 1 )
                {
                    if( m_currentState != 1 )
                    {
                        SetState(1);
                    }
                    Fixed longitude = 0;
                    Fixed latitude = 0;
                    int objectId = -1;
                    Fixed maxDistance = 90;

                    Nuke::FindTarget( m_teamId, team->m_targetTeam, m_objectId, maxDistance, &longitude, &latitude, &objectId );
                    if( ( longitude != 0 && latitude != 0 ) &&
                        ( objectId != -1 || team->m_currentState >= Team::StateAssault ) )
                    {
                        ActionOrder *action = new ActionOrder();
                        action->m_longitude = longitude;
                        action->m_latitude = latitude;
                        action->m_targetObjectId = objectId;
                        RequestAction( action );
                    }
                }
                else
                {
                    LaunchScout();
                }
            }
            else
            {
                LList<int> targets;
                int scanRange = 15.0f;
                for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
                {
                    if( g_app->GetWorld()->m_objects.ValidIndex(i) )
                    {
                        WorldObject *obj = g_app->GetWorld()->m_objects[i];
                        bool visible = obj->m_visible[m_teamId];
                        if( obj->m_type == WorldObject::TypeSub && m_currentState == 1 ) visible = true;                // TODO CHRIS : This looks like a bug to me.  surely current state should be 2?

                        if( !g_app->GetWorld()->IsFriend( m_teamId, obj->m_teamId) &&                            
                            visible &&
                            obj->IsMovingObject() &&
                            team->CountTargettedUnits(i) < 3 &&
                            g_app->GetWorld()->GetDistance( m_longitude, m_latitude, obj->m_longitude, obj->m_latitude ) < scanRange )
                        {
                            targets.PutData(obj->m_objectId );
                        }
                    }
                }
                int targetIndex = -1;
                if( targets.Size() > 0 )
                {
                    targetIndex = targets[syncrand() % targets.Size()];
                }

                WorldObject *obj = g_app->GetWorld()->GetWorldObject( targetIndex );
                if( obj )
                {                
                    if( obj->m_type == WorldObject::TypeFighter ||
                        obj->m_type == WorldObject::TypeBomber )
                    {
                        if( m_stateTimer <= 0 ) 
                        {
                            SetState(0);

                        }
                        ActionOrder *action = new ActionOrder();
                        action->m_targetObjectId = obj->m_objectId;
                        action->m_longitude = obj->m_longitude;
                        action->m_latitude = obj->m_latitude;
                        RequestAction(action);
                    }
                    else if( obj->m_type == WorldObject::TypeSub )
                    {
                        if( m_stateTimer <= 0 ) 
                        {
                            if( obj->m_currentState != 2 )
                            {
                                SetState(2);
                                Fleet *fleet = g_app->GetWorld()->GetTeam( m_teamId )->GetFleet( m_fleetId );
                                if( fleet )
                                {
                                    fleet->AlterWaypoints( obj->m_longitude, obj->m_latitude, true );
                                }
                            }
                            else
                            {
                                SetState(1);
                                ActionOrder *action = new ActionOrder();
                                action->m_targetObjectId = obj->m_objectId;
                                action->m_longitude = obj->m_longitude;
                                action->m_latitude = obj->m_latitude;
                                RequestAction(action);
                            }
                        }
                    }
                    else
                    {
                        if( m_stateTimer <= 0 ) 
                        {
                            SetState(1);
                        }
                        ActionOrder *action = new ActionOrder();
                        action->m_targetObjectId = obj->m_objectId;
                        action->m_longitude = obj->m_longitude;
                        action->m_latitude = obj->m_latitude;
                        RequestAction(action);
                    }
                }
                else
                {
                    bool redirectingBombers = false;
                    if( m_nukeSupply == 0 &&
                        m_states[1]->m_numTimesPermitted > 0 )
                    {
                        for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
                        {
                            if( g_app->GetWorld()->m_objects.ValidIndex(i) )
                            {
                                WorldObject *obj = g_app->GetWorld()->m_objects[i];
                                if( obj->m_teamId == m_teamId &&
                                    ( obj->m_type == WorldObject::TypeCarrier ||
                                      obj->m_type == WorldObject::TypeAirBase ) )
                                {
                                    if( obj->m_nukeSupply - obj->m_states[1]->m_numTimesPermitted > 0 )
                                    {
                                        if( m_currentState != 1 )
                                        {
                                            SetState(1);
                                        }

                                        int nukesSpare = obj->m_nukeSupply - obj->m_states[1]->m_numTimesPermitted;
                                        for( int j = 0; j < nukesSpare; ++j )
                                        {
                                            if( m_states[1]->m_numTimesPermitted - m_actionQueue.Size() > 0 )
                                            {
                                                ActionOrder *action = new ActionOrder();
                                                action->m_targetObjectId = obj->m_objectId;
                                                action->m_longitude = obj->m_longitude;
                                                action->m_latitude = obj->m_latitude;
                                                RequestAction(action);
                                            }
                                        }
                                        redirectingBombers = true;
                                    }
                                }
                            }
                        }
                    }
                    if( m_states[0]->m_numTimesPermitted > 2 &&
                        !redirectingBombers )
                    {
                        LaunchScout();
                    }
                }
            }
        }
    }
}

void Carrier::LaunchScout()
{
    if( m_states[0]->m_numTimesPermitted > 0 )
    {
        if( m_currentState != 0 )
        {
            SetState(0);
        }
        Team *team = g_app->GetWorld()->GetTeam( m_teamId );
        
        int territoryId = -1;
        for( int i = 0; i < World::NumTerritories; ++i )
        {
            int owner = g_app->GetWorld()->GetTerritoryOwner( i );
            if( owner != -1 &&
                !g_app->GetWorld()->IsFriend( owner, m_teamId ) )
            {
                Fixed distance = g_app->GetWorld()->GetDistance( m_longitude, m_latitude, 
                                                                g_app->GetWorld()->m_populationCenter[i].x,
                                                                g_app->GetWorld()->m_populationCenter[i].y );
                Fixed maxRange = 45 / g_app->GetWorld()->GetGameScale();
                if( distance < maxRange )
                {
                    territoryId = i;
                    break;
                }
            }
        }

        if( territoryId != -1 )
        {
            Fixed longitude = longitude = g_app->GetWorld()->m_populationCenter[territoryId].x + syncsfrand( 60 );
            Fixed latitude = latitude = g_app->GetWorld()->m_populationCenter[territoryId].y + syncsfrand( 60 );
                                        
            ActionOrder *action = new ActionOrder();
            action->m_longitude = longitude;
            action->m_latitude = latitude;
            RequestAction( action );
        }
    }
}

bool Carrier::IsActionQueueable()
{
    if( m_currentState != 2 )
    {
        return true;
    }
    else
    {
        return false;
    }
}

int Carrier::FindTarget()
{
    for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
    {
        if( g_app->GetWorld()->m_objects.ValidIndex(i) )
        {
            WorldObject *obj = g_app->GetWorld()->m_objects[i];

            if( obj->m_teamId != m_teamId &&                
                g_app->GetWorld()->GetAttackOdds( WorldObject::TypeFighter, obj->m_type ) > 0 &&
                obj->m_visible[m_teamId] == true )
            {
                Fixed distance = g_app->GetWorld()->GetDistance( m_longitude, m_latitude, obj->m_longitude, obj->m_latitude);

                if( distance <= 30 )
                {
                    return obj->m_objectId;
                }
            }
        }
    }
    return -1;
}

int Carrier::GetAttackState()
{
    return -1;
}

void Carrier::Retaliate( int attackerId )
{
    Fleet *fleet = g_app->GetWorld()->GetTeam( m_teamId )->GetFleet( m_fleetId );
    if( fleet )
    {
        WorldObject *obj = g_app->GetWorld()->GetWorldObject( attackerId );
        if( obj &&
            obj->m_visible[ m_teamId ] && 
            !g_app->GetWorld()->GetTeam( m_teamId )->m_ceaseFire[ obj->m_teamId ])
        {
            if( m_states[m_currentState]->m_numTimesPermitted > 0 )
            {
                int launchType = -1;
                if( m_currentState == 0 ) 
                {
                    launchType = WorldObject::TypeFighter;
                }
                else 
                {
                    launchType = WorldObject::TypeBomber;
                }
                if( g_app->GetWorld()->GetAttackOdds( launchType, obj->m_type ) > 0 )
                {
                    Action( attackerId, -1, -1 );

                    if( m_fleetId != -1 )
                    {
                        fleet->FleetAction( m_targetObjectId );
                    }
                }
            }
        }
    }
}

bool Carrier::UsingNukes()
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


void Carrier::FireGun( Fixed range)
{
    WorldObject *targetObject = g_app->GetWorld()->GetWorldObject(m_targetObjectId);
    AppAssert( targetObject );
    DepthCharge *bullet = new DepthCharge( range );
    bullet->SetPosition( m_longitude, m_latitude );
    bullet->SetTeamId( m_teamId );
    bullet->m_origin = m_objectId;
    bullet->m_attackOdds = g_app->GetWorld()->GetAttackOdds( m_type, targetObject->m_type );
    bullet->m_distanceToTarget = g_app->GetWorld()->GetDistance( m_longitude, m_latitude, targetObject->m_longitude, targetObject->m_latitude );
    g_app->GetWorld()->m_gunfire.PutData( bullet );
}

void Carrier::FleetAction( int targetObjectId )
{
    if( m_targetObjectId == -1 )
    {
        WorldObject *obj = g_app->GetWorld()->GetWorldObject( targetObjectId );
        if( obj )
        {
            if( m_states[ m_currentState ]->m_numTimesPermitted != 0 )
            {
                if( obj->m_type == WorldObject::TypeSub )
                {
                    SetState(2);
                }
                else if( m_stateTimer <= 0 )
                {
                    if( m_currentState == 0 )
                    {
                        if( g_app->GetWorld()->GetAttackOdds( WorldObject::TypeFighter, obj->m_type ) > 0 )
                        {
                            ActionOrder *action = new ActionOrder();
                            action->m_targetObjectId = obj->m_objectId;
                            action->m_longitude = obj->m_longitude;
                            action->m_latitude = obj->m_latitude;
                            RequestAction( action );
                        }
                    }
                    else if( m_currentState == 1 )
                    {
                        if( g_app->GetWorld()->GetAttackOdds( WorldObject::TypeBomber, obj->m_type ) > 0 )
                        {
                            Action( targetObjectId, obj->m_longitude, obj->m_latitude );
                            ActionOrder *action = new ActionOrder();
                            action->m_targetObjectId = targetObjectId;
                            action->m_longitude = obj->m_longitude;
                            action->m_latitude = obj->m_latitude;
                            RequestAction( action );
                        }
                    }
                }
            }
            MovingObject::Retaliate( targetObjectId );
        }
    }
}

bool Carrier::CanLaunchBomber()
{
    if( m_currentState == 1 &&
        m_states[1]->m_numTimesPermitted > 0 )
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool Carrier::CanLaunchFighter()
{
    if( m_currentState == 0 &&
        m_states[0]->m_numTimesPermitted > 0 )
    {
        return true;
    }
    else
    {
        return false;
    }
}


int Carrier::GetAttackOdds( int _defenderType )
{
    if( CanLaunchFighter() )
    {
        return g_app->GetWorld()->GetAttackOdds( TypeFighter, _defenderType );
    }

    if( CanLaunchBomber() )
    {
        if( _defenderType == TypeCity ||
            _defenderType == TypeSilo ||
            _defenderType == TypeAirBase ||
            _defenderType == TypeRadarStation )
        {
            return g_app->GetWorld()->GetAttackOdds( TypeNuke, _defenderType );
        }
        else
        {
            return g_app->GetWorld()->GetAttackOdds( TypeBomber, _defenderType );
        }
    }

    return MovingObject::GetAttackOdds( _defenderType );
}


int Carrier::IsValidCombatTarget( int _objectId )
{
    WorldObject *obj = g_app->GetWorld()->GetWorldObject( _objectId );
    if( !obj ) return TargetTypeInvalid;

    int basicCheck = WorldObject::IsValidCombatTarget( _objectId );
    if( basicCheck < TargetTypeInvalid ) return basicCheck;

    bool isFriend = g_app->GetWorld()->IsFriend( m_teamId, obj->m_teamId );    
    if( !isFriend )
    {       
        Fixed distanceSqd = g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, obj->m_longitude, obj->m_latitude );

        if( distanceSqd < GetActionRangeSqd() )
        {
            if( m_currentState == 0  )
            {
                int attackOdds = g_app->GetWorld()->GetAttackOdds( TypeFighter, obj->m_type );
                if( attackOdds > 0)
                {
                    return TargetTypeLaunchFighter;
                }
            }

            if( m_currentState == 1 )
            {
                int attackOdds = g_app->GetWorld()->GetAttackOdds( TypeBomber, obj->m_type );
                if( attackOdds > 0 || !obj->IsMovingObject() )
                {
                    return TargetTypeLaunchBomber;
                }
            }
        }
        else
        {
            return TargetTypeOutOfRange;
        }
    }

    return TargetTypeInvalid;
}


int Carrier::IsValidMovementTarget( Fixed longitude, Fixed latitude )
{
    Fixed distanceSqd = g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, 
                                                            longitude, latitude );

    if( distanceSqd < GetActionRangeSqd() )
    {
        if( m_currentState == 0 )
        {
            return TargetTypeLaunchFighter;
        }

        if( m_currentState == 1 )
        {
            return TargetTypeLaunchBomber;
        }
    }

    if( m_currentState == 2 )
    {
        return g_app->GetWorld()->GetTeam( m_teamId )->GetFleet( m_fleetId )->IsValidFleetPosition( longitude, latitude );
    }

    if( m_states[m_currentState]->m_numTimesPermitted - m_actionQueue.Size() <= 0 )
    {
        return TargetTypeOutOfStock;
    }

    if( m_states[m_currentState]->m_defconPermitted < g_app->GetWorld()->GetDefcon() )
    {
        return TargetTypeDefconRequired;
    }

    return TargetTypeInvalid;
}


int Carrier::CountIncomingFighters()
{
    int num = 0;
    for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
    {
        if( g_app->GetWorld()->m_objects.ValidIndex(i) )
        {
            MovingObject *obj = (MovingObject *)g_app->GetWorld()->m_objects[i];
            if( obj->m_type == WorldObject::TypeFighter &&
                obj->m_teamId == m_teamId &&
                obj->m_isLanding == m_objectId )
            {
                num++;
            }
        }
    }
    return num;
}


void Carrier::Render()
{
    MovingObject::Render();


    //
    // Render count

    if( m_teamId == g_app->GetWorld()->m_myTeamId ||
        g_app->GetWorld()->m_myTeamId == -1 ||
        g_app->GetGame()->m_winner != -1 )
    {   
        int numInStore = m_states[m_currentState]->m_numTimesPermitted;
        int numInQueue = m_actionQueue.Size();

        Team *team = g_app->GetWorld()->GetTeam(m_teamId);
        Colour colour = team->GetTeamColour();            
        colour.m_a = 150;

        Image *bmpImage = g_resource->GetImage("graphics/smallbomber.bmp");
        if( m_currentState == 0 ) bmpImage = g_resource->GetImage("graphics/smallfighter.bmp" );

        Fixed predictionTime = Fixed::FromDouble(g_predictionTime) * g_app->GetWorld()->GetTimeScaleFactor();
        float predictedLongitude = (m_longitude + m_vel.x * predictionTime).DoubleValue();
        float predictedLatitude = (m_latitude + m_vel.y * predictionTime).DoubleValue(); 

        if( bmpImage )
        {
            float x = predictedLongitude - GetSize().DoubleValue() * 0.85f;
            float y = predictedLatitude - GetSize().DoubleValue() * 0.25f; 
            float size = GetSize().DoubleValue() * 0.35f;
            
            float dx = size*0.9f;

            if( team->m_territories[0] >= 3 )
            {
                x += GetSize().DoubleValue()*1.4f;                                
                dx *= -1;
            }       

            for( int i = 0; i < numInStore; ++i )
            {
                if( i >= (numInStore-numInQueue) )
                {
                    colour.Set( 128,128,128,100 );
                }
                
                g_renderer->Blit( bmpImage, x, y, size*0.9f, -size, colour );
                x += dx;
            }
        }
    }        
}

