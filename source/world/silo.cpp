#include "lib/universal_include.h"
#include "lib/resource/resource.h"
#include "lib/resource/image.h"
#include "lib/language_table.h"
#include "lib/math/random_number.h"
#include "lib/gucci/input.h"

#include "app/app.h"
#include "app/globals.h"
#include "app/game.h"

#include "lib/profiler.h"

#include "renderer/map_renderer.h"

#include "interface/interface.h"

#include "world/world.h"
#include "world/silo.h"
#include "world/city.h"
#include "world/nuke.h"


Silo::Silo()
:   WorldObject(),
    m_numNukesLaunched(0)
{
    SetType( TypeSilo );

    strcpy( bmpImageFilename, "graphics/sam.bmp" );

    m_radarRange = 20;
    m_selectable = true;

    m_currentState = 1;
    m_life = 45;

    m_nukeSupply = 20;

    AddState( LANGUAGEPHRASE("state_silonuke"), 35, 30, 10, Fixed::MAX, true, 20, 1 );         
    AddState( LANGUAGEPHRASE("state_airdefense"), 340, 60, 10, 30, true );
	//AddState( LANGUAGEPHRASE("state_doomsday"), 1, 1, 10, Fixed::MAX, true, 10, 1 ); 
	AddState( LANGUAGEPHRASE("state_doomsday"), 1, 1, 10, Fixed::MAX, true, 10, 5 ); 

	m_states[0]->m_numTimesPermitted = m_nukeSupply;

    InitialiseTimers();
}

void Silo::Action( int targetObjectId, Fixed longitude, Fixed latitude )
{
    if( !CheckCurrentState() )
    {
        return;
    }

    if( m_stateTimer <= 0 )
    {
        if( m_currentState == 0 || m_currentState == 2)
        {
            g_app->GetWorld()->LaunchNuke( m_teamId, m_objectId, longitude, latitude, 360 );
            m_numNukesLaunched++;
            for( int i = 0; i < g_app->GetWorld()->m_teams.Size(); ++i )
            {
                Team *team = g_app->GetWorld()->m_teams[i];
                m_visible[team->m_teamId] = true;
                m_lastSeenTime[team->m_teamId] = m_ghostFadeTime;
                m_seen[team->m_teamId] = true;
            }
    
        }
        else if( m_currentState == 1 )
        {
            WorldObject *targetObject = g_app->GetWorld()->GetWorldObject(targetObjectId);
            if( targetObject )
            {
                if( targetObject->m_visible[m_teamId] &&                     
                    g_app->GetWorld()->GetAttackOdds( m_type, targetObject->m_type ) > 0 &&
                    g_app->GetWorld()->GetDistance( m_longitude, m_latitude, targetObject->m_longitude, targetObject->m_latitude) <= GetActionRange() )
                {
                    m_targetObjectId = targetObjectId;
                }
                else
                {
                    return;
                }
            }
            else
            {
                return;
            }
        }

        WorldObject::Action( targetObjectId, longitude, latitude );
    }
}

void Silo::SetState( int state )
{
    WorldObject::SetState( state );
    if( m_currentState == 0 )
    {
        strcpy( bmpImageFilename, "graphics/silo.bmp" );
    }
    else
    {
        strcpy( bmpImageFilename, "graphics/sam.bmp" );
    }
}
        

bool Silo::Update()
{    
//#ifdef _DEBUG
    //if( g_keys[KEY_U] )
    //{
    //    static bool doneIt = false;
    //    //if( !doneIt )
    //    {
    //        // Pretend sync error
    //        Fixed addMe( 10 );
    //        m_longitude = m_longitude + addMe;
    //        doneIt=true;
    //    }
    //}
//#endif

    if( m_stateTimer <= 0 )
    {
        if( m_retargetTimer <= 0 )
        {
            m_retargetTimer = 10;
            AirDefense();
        }
        if( m_targetObjectId != -1  )
        {
            WorldObject *targetObject = g_app->GetWorld()->GetWorldObject(m_targetObjectId);
            if( targetObject )
            {
                if( targetObject->m_visible[m_teamId] &&                     
                    g_app->GetWorld()->GetAttackOdds( m_type, targetObject->m_type ) > 0 &&
                    g_app->GetWorld()->GetDistance( m_longitude, m_latitude, targetObject->m_longitude, targetObject->m_latitude) <= GetActionRange() )
                {
                    FireGun( GetActionRange() );
                    m_stateTimer = m_states[1]->m_timeToReload;
                }
                else
                {
                    m_targetObjectId = -1;
                }
            }
            else
            {
                m_targetObjectId = -1;
            }
        }
    }

    if( m_currentState == 0 &&
        m_states[0]->m_numTimesPermitted == 0 )
    {
        SetState(1);
    }

    return WorldObject::Update();
}

void Silo::Render()
{
    WorldObject::Render();


    //
    // Render nuke count under the silo

    if( m_teamId == g_app->GetWorld()->m_myTeamId ||
        g_app->GetWorld()->m_myTeamId == -1 ||
        g_app->GetGame()->m_winner != -1 )
    {   
        int numNukesInStore = m_states[0]->m_numTimesPermitted;
        int numNukesInQueue = m_actionQueue.Size();

        Team *team = g_app->GetWorld()->GetTeam(m_teamId);
        Colour colour = team->GetTeamColour();            
        colour.m_a = 150;

        Image *bmpImage = g_resource->GetImage("graphics/smallnuke.bmp");
        if( bmpImage )
        {
            float x = m_longitude.DoubleValue();
            float y = m_latitude.DoubleValue() - GetSize().DoubleValue() * 0.9f;       
            float nukeSize = GetSize().DoubleValue() * 0.35f;
            x -= GetSize().DoubleValue()*0.95f;

            for( int i = 0; i < ( numNukesInStore * 0.5f ); ++i ) //phuck
            {
                if( i >= (numNukesInStore-numNukesInQueue) )
                {
                    colour.Set( 128,128,128,100 );
                }
                
                g_renderer->Blit( bmpImage, x+i*nukeSize*0.5f, y, nukeSize, -nukeSize, colour );
            }
        }
    }
}

void Silo::RunAI()
{
    START_PROFILE("SiloAI");
    Team *team = g_app->GetWorld()->GetTeam( m_teamId );
    if( (team->m_currentState >= Team::StateStrike &&
         team->m_subState >= Team::SubStateLaunchNukes) ||
        team->m_currentState == Team::StatePanic )
    {
        if( m_currentState != 0 &&
            m_states[0]->m_numTimesPermitted > 0 )
        {
            if( team->m_siloSwitchTimer <= 0 )
            {
                SetState(0);
                team->m_siloSwitchTimer = 30;
            }
        }

        if( m_currentState == 0 &&
            m_states[0]->m_numTimesPermitted > 0 &&
            m_stateTimer <= 0 )
        {
            Fixed longitude = 0;
            Fixed latitude  = 0;

            Nuke::FindTarget( m_teamId, team->m_targetTeam, m_objectId, Fixed::MAX, &longitude, &latitude );

            if( longitude != 0 && latitude != 0 )
            {
                ActionOrder *action = new ActionOrder();
                action->m_longitude = longitude;
                action->m_latitude = latitude;
                RequestAction( action );
            }
        }
    }
    END_PROFILE("SiloAI");
}

int Silo::GetTargetObjectId()
{
    return m_targetObjectId;
}

bool Silo::IsActionQueueable()
{
    if( m_currentState == 0 )
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool Silo::UsingGuns()
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

bool Silo::UsingNukes()
{
    if( m_currentState == 0 )
    {
        return true;
    }
    else
    {
        return false;
    }
}

void Silo::NukeStrike()
{
    // Hit by nuke:
    // reduce remaining nukes

    Fixed lossFraction = Fixed(1) / 2;
    m_states[0]->m_numTimesPermitted = (m_states[0]->m_numTimesPermitted * lossFraction).IntValue();
    m_nukeSupply = m_states[0]->m_numTimesPermitted;
}

void Silo::AirDefense()
{
    if( g_app->GetWorld()->GetDefcon() > 3 )
    {
        return;
    }
    if( m_currentState == 1 )
    {
        Fixed actionRangeSqd = GetActionRangeSqd();

        int nukeId = g_app->GetWorld()->GetNearestObject( m_teamId, m_longitude, m_latitude, WorldObject::TypeNuke, true );
        WorldObject *nuke = g_app->GetWorld()->GetWorldObject( nukeId );
        if( nuke )
        {
            if( g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, nuke->m_longitude, nuke->m_latitude ) <= actionRangeSqd )
            {
                Action( nuke->m_objectId, -1, -1 );
                return;
            }
        }
        
        int bomberId = g_app->GetWorld()->GetNearestObject( m_teamId, m_longitude, m_latitude, WorldObject::TypeBomber, true );
        WorldObject *bomber = g_app->GetWorld()->GetWorldObject( bomberId );
        if( bomber )
        {
            if( g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, bomber->m_longitude, bomber->m_latitude ) <= actionRangeSqd )
            {
                Action( bomber->m_objectId, -1, -1 );
                return;
            }
        }
        
        
        int fighterId = g_app->GetWorld()->GetNearestObject( m_teamId, m_longitude, m_latitude, WorldObject::TypeFighter, true );
        WorldObject *fighter = g_app->GetWorld()->GetWorldObject( fighterId );
        if( fighter )
        {
            if( g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, fighter->m_longitude, fighter->m_latitude ) <= actionRangeSqd )
            {
                Action( fighter->m_objectId, -1, -1 );
                return;
            }
        }
    }
}


int Silo::GetAttackOdds( int _defenderType )
{
    if( m_currentState == 0 &&
        m_states[0]->m_numTimesPermitted > 0 )
    {
        return g_app->GetWorld()->GetAttackOdds( TypeNuke, _defenderType );
    }

    if( m_currentState == 1 )
    {
        return WorldObject::GetAttackOdds( _defenderType );
    }

    return 0;
}


int Silo::IsValidCombatTarget( int _objectId )
{
    WorldObject *obj = g_app->GetWorld()->GetWorldObject( _objectId );
    if( !obj ) return TargetTypeInvalid;

    int basicResult = WorldObject::IsValidCombatTarget( _objectId );
    if( basicResult < TargetTypeInvalid ) return basicResult;

    bool isFriend = g_app->GetWorld()->IsFriend( m_teamId, obj->m_teamId );
    if( !isFriend )
    {
        int attackOdds = g_app->GetWorld()->GetAttackOdds( TypeNuke, obj->m_type );
        if( m_currentState == 0 && 
            m_states[0]->m_numTimesPermitted > 0 &&
            attackOdds > 0 )
        {
            return TargetTypeLaunchNuke;
        }

        attackOdds = g_app->GetWorld()->GetAttackOdds( TypeSilo, obj->m_type );
        if( m_currentState == 1 &&
            attackOdds > 0 )
        {
            return TargetTypeValid;
        }
    }

    return TargetTypeInvalid;
}


Image *Silo::GetBmpImage( int state )
{
    if( state == 0 )
    {
        return g_resource->GetImage( "graphics/silo.bmp" );
    }
    else
    {
        return g_resource->GetImage( "graphics/sam.bmp" );
    }
}


void Silo::CeaseFire( int teamId )
{
    for( int i = 0; i < m_actionQueue.Size(); ++i )
    {
        ActionOrder *action = m_actionQueue[i];
        if(g_app->GetMapRenderer()->IsValidTerritory( teamId, action->m_longitude, action->m_latitude, false ) )
        {
            m_actionQueue.RemoveData(i);
            delete action;
            --i;
        }
    }
    WorldObject::CeaseFire( teamId );
}
