#include "lib/universal_include.h"

#include <stdio.h>
#include <math.h>

#include "lib/resource/resource.h"
#include "lib/resource/image.h"
#include "lib/language_table.h"
#include "lib/math/random_number.h"
#include "lib/hi_res_time.h"

#include "app/app.h"
#include "app/globals.h"
#include "app/game.h"

#include "world/world.h"
#include "world/worldobject.h"
#include "world/team.h"

#include "interface/interface.h"

#include "renderer/map_renderer.h"

#include "world/radarstation.h"
#include "world/silo.h"
#include "world/sub.h"
#include "world/battleship.h"
#include "world/airbase.h"
#include "world/carrier.h"
#include "world/bomber.h"
#include "world/fighter.h"
#include "world/city.h"
#include "world/nuke.h"
#include "world/tornado.h"
#include "world/saucer.h"
#include "world/fleet.h"


WorldObject::WorldObject()
:   m_teamId(-1),
    m_objectId(-1),
    m_longitude(0),
    m_latitude(0),
    m_type(TypeInvalid),
    m_radarRange(0),
    m_life(1),
	m_lastHitByTeamId( -1 ),
    m_selectable(false),
    m_currentState(0),
    m_previousState(0),
    m_stateTimer(0),
    m_ghostFadeTime(50),
    m_targetObjectId(-1),
    m_isRetaliating(false),
    m_fleetId(-1),
    m_nukeSupply(-1),
    m_previousRadarRange(0),
    m_offensive(false),
    m_aiTimer(0),
    m_aiSpeed(5),
    m_numNukesInFlight(0),
    m_numNukesInQueue(0),
    m_nukeCountTimer(0),
    m_forceAction(false),
    m_maxFighters(0),
    m_maxBombers(0),
    m_retargetTimer(0)
{
    m_visible.Initialise(MAX_TEAMS);
    m_seen.Initialise(MAX_TEAMS);
    m_lastKnownPosition.Initialise(MAX_TEAMS);
    m_lastKnownVelocity.Initialise(MAX_TEAMS);
    m_lastSeenTime.Initialise(MAX_TEAMS);
    m_lastSeenState.Initialise(MAX_TEAMS);

    if( g_app->m_gameRunning )
    {
        m_seen.SetAll( false );
        m_visible.SetAll( false );
        m_lastKnownPosition.SetAll( Vector3<Fixed>::ZeroVector() );
        m_lastKnownVelocity.SetAll( Vector3<Fixed>::ZeroVector() );
        m_lastSeenTime.SetAll( 0 );
        m_lastSeenState.SetAll( 0 );
    }
}

WorldObject::~WorldObject()
{
    if( g_app->m_gameRunning )
    {
        Team *team = g_app->GetWorld()->GetTeam( m_teamId );
        if( team )
        {
            team->m_unitsInPlay[m_type]--;
        }
    }
    m_actionQueue.EmptyAndDelete();
    m_states.EmptyAndDelete();
}

void WorldObject::InitialiseTimers()
{    
    if( m_type != TypeInvalid )
    {
        m_aiTimer = syncfrand(m_aiSpeed); // start the timer at a random number so that ai updates dont all occur simulatenously
        
        m_nukeCountTimer = frand(5.0f);
        m_nukeCountTimer += (double)GetHighResTime();

        m_radarRange /= World::GetGameScale();       
    }
}

void WorldObject::SetType( int type )
{
    m_type = type;
}

void WorldObject::SetTeamId( int teamId )
{
    m_teamId = teamId;
}

void WorldObject::SetPosition( Fixed longitude, Fixed latitude )
{
    m_longitude = longitude;
    m_latitude = latitude;
}

bool WorldObject::IsHiddenFrom()
{
    return false;
}

void WorldObject::AddState( char *stateName, Fixed prepareTime, Fixed reloadTime, Fixed radarRange, 
                            Fixed actionRange, bool isActionable, int numTimesPermitted, int defconPermitted )
{
    WorldObjectState *state = new WorldObjectState();
    state->m_stateName = strdup( stateName);
    state->m_timeToPrepare = prepareTime;
    state->m_timeToReload = reloadTime;
    state->m_radarRange = radarRange;
    state->m_actionRange = actionRange;
    state->m_isActionable = isActionable;
    state->m_numTimesPermitted = numTimesPermitted;
    state->m_defconPermitted = defconPermitted;

    Fixed gameScale = World::GetGameScale();
    state->m_radarRange /= gameScale;
    state->m_actionRange /= gameScale;

    m_states.PutData( state );
}

bool WorldObject::IsActionable()
{
    return( m_states[m_currentState]->m_isActionable );
}

Fixed WorldObject::GetActionRange ()
{
    Fixed result = m_states[m_currentState]->m_actionRange;
    return result;
}

Fixed WorldObject::GetActionRangeSqd()
{
    Fixed result = GetActionRange();
    return( result * result );
}

Fixed WorldObject::GetRadarRange ()
{
    Fixed result = m_states[m_currentState]->m_radarRange;
    return result;
}


void WorldObject::Action( int targetObjectId, Fixed longitude, Fixed latitude )
{
    m_stateTimer = m_states[m_currentState]->m_timeToReload;

    WorldObjectState *currentState = m_states[m_currentState];
    if( currentState->m_numTimesPermitted != -1 &&
        currentState->m_numTimesPermitted > 0 )
    {
        --currentState->m_numTimesPermitted;
    }

    if( UsingNukes() &&
        m_nukeSupply > 0 )
    {
        m_nukeSupply--;
    }

}


bool WorldObject::CanSetState( int state )
{
    if( !m_states.ValidIndex(state) )
    {
        return false;
    }

    if( m_currentState == state )
    {
        return false;
    }

    if( m_states[state]->m_numTimesPermitted == 0 )
    {
        return false;
    }

    if( m_states[state]->m_defconPermitted < g_app->GetWorld()->GetDefcon() )
    {
        return false;
    }

    return true;
}


void WorldObject::SetState( int state )
{
    if( CanSetState(state) )
    {
        WorldObjectState *theState = m_states[state];
        m_currentState = state;
        m_stateTimer = theState->m_timeToPrepare;
        m_actionQueue.Empty();
        m_targetObjectId = -1;
    }
}


bool WorldObject::Update()
{    
    if( m_stateTimer > 0 )
    {
        m_stateTimer -= SERVER_ADVANCE_PERIOD * g_app->GetWorld()->GetTimeScaleFactor();
        if( m_stateTimer <= 0 )
        {
            m_stateTimer = 0;
            
            //if( m_teamId == g_app->GetWorld()->m_myTeamId &&
            //    m_type != TypeNuke )
            {
                //g_app->GetWorld()->AddWorldMessage( m_objectId, m_teamId,
                //    m_states[m_currentState]->GetReadyName(), WorldMessage::TypeObjectState,false );                
            }
        }
    }

    if( m_stateTimer <= 0 )
    {
        if( m_actionQueue.Size() > 0 )
        {
            ActionOrder *action = m_actionQueue[0];
            Action( action->m_targetObjectId, action->m_longitude, action->m_latitude );

            if( action->m_targetObjectId == -1 )
            {
                //Fleet *fleet = g_app->GetWorld()->GetTeam( m_teamId )->GetFleet( m_fleetId );
                //if( fleet )
                {
                    //fleet->StopFleet();
                }
            }
            else if( action->m_pursueTarget )
            {
                Fleet *fleet = g_app->GetWorld()->GetTeam( m_teamId )->GetFleet( m_fleetId );
                if( fleet )
                {
                    MovingObject *obj = (MovingObject *)g_app->GetWorld()->GetWorldObject( action->m_targetObjectId );
                    if( obj &&
                        obj->IsMovingObject() )
                    {
                        if( obj->m_movementType == MovingObject::MovementTypeSea )
                        {                        
                            fleet->m_pursueTarget = true;
                            fleet->m_targetFleet = obj->m_fleetId;
                            fleet->m_targetTeam = obj->m_teamId;
                        }
                    }
                }
            }
            m_actionQueue.RemoveData(0);
            delete action;
        }
    }

    if( m_currentState != m_previousState )
    {
        m_previousState = m_currentState;
        if( m_teamId == g_app->GetWorld()->m_myTeamId &&
            m_stateTimer > 0 )
        {
            //g_app->GetWorld()->AddWorldMessage( m_objectId, m_teamId,
            //    m_states[m_currentState]->GetPreparingName(), WorldMessage::TypeObjectState, false );                                   
        }
    }
        
    for( int i = 0; i < g_app->GetWorld()->m_teams.Size(); ++i ) 
    {
        Team *team = g_app->GetWorld()->m_teams[i];
        if(m_visible[team->m_teamId])
        {
            m_lastKnownPosition[team->m_teamId] = Vector3<Fixed>( m_longitude, m_latitude, 0 );
            m_lastKnownVelocity[team->m_teamId] = m_vel;
            m_lastSeenTime[team->m_teamId] = m_ghostFadeTime;
            m_lastSeenState[team->m_teamId] = m_currentState;
        }
    }

    if( !IsMovingObject() )
    {
        float realTimeNow = GetHighResTime();
        if( realTimeNow > m_nukeCountTimer )
        {
            g_app->GetWorld()->GetNumNukers( m_objectId, &m_numNukesInFlight, &m_numNukesInQueue );
            m_nukeCountTimer = realTimeNow + 2.0f;
        }
    }

    m_aiTimer -= SERVER_ADVANCE_PERIOD * g_app->GetWorld()->GetTimeScaleFactor();
    if( m_retargetTimer > 0 )
    {
        m_retargetTimer -= SERVER_ADVANCE_PERIOD * g_app->GetWorld()->GetTimeScaleFactor();
        m_retargetTimer = max( m_retargetTimer, 0 );
    }

    if( m_life <= 0 )
    {
        if( m_fleetId != -1 )
        {
            Fleet *fleet = g_app->GetWorld()->GetTeam( m_teamId )->GetFleet( m_fleetId );
            if( fleet )
            {
                fleet->m_lastHitByTeamId.PutData( m_lastHitByTeamId );
            }
        }
    }
                
    return( m_life <= 0 );
}


char *WorldObject::GetBmpBlurFilename()
{
    static char blurFilename[256];
    sprintf( blurFilename, bmpImageFilename );
    char *dot = strrchr( blurFilename, '.' );
    sprintf( dot, "_blur.bmp" );
    return blurFilename;
}


void WorldObject::Render ()
{
    Fixed predictionTime = Fixed::FromDouble(g_predictionTime) * g_app->GetWorld()->GetTimeScaleFactor();
    float predictedLongitude = (m_longitude + m_vel.x * predictionTime).DoubleValue();
    float predictedLatitude = (m_latitude + m_vel.y * predictionTime).DoubleValue(); 
    float size = GetSize().DoubleValue();

    float x = predictedLongitude-size;
    float y = predictedLatitude+size;
    float thisSize = size*2;

    Team *team = g_app->GetWorld()->GetTeam(m_teamId);
    if( team->m_territories[0] >= 3 )
    {
        x = predictedLongitude+size;
        thisSize = size*-2;
    }       

    Colour colour       = team->GetTeamColour();            
    colour.m_a = 255;

    Image *bmpImage = g_resource->GetImage( bmpImageFilename );
    if( bmpImage )
    {
        g_renderer->Blit( bmpImage, x, y, thisSize, size*-2, colour );        
    }   



    //
    // Current selection?

    colour.Set(255,255,255,255);
    int selectionId = g_app->GetMapRenderer()->GetCurrentSelectionId();
    for( int i = 0; i < 2; ++i )
    {
        if( i == 1 )
        {
            int highlightId = g_app->GetMapRenderer()->GetCurrentHighlightId();
            if( highlightId == selectionId ) break;
            selectionId = highlightId;
        }
        WorldObject *selection = g_app->GetWorld()->GetWorldObject(selectionId);

        if( selection )
        {
            bool selected = selection == this;
            bool sameFleet = i == 0 &&
                             selection->m_teamId == m_teamId &&
                             selection->m_fleetId != -1 &&
                             selection->m_fleetId == m_fleetId;

            if( selected || sameFleet )
            {
                bmpImage = g_resource->GetImage( GetBmpBlurFilename() );
                g_renderer->Blit( bmpImage, x, y, thisSize, size*-2, colour );        
            }
        }
        colour.m_a /= 2;
    }
}

void WorldObject::RenderCounter( int counter )
{

    char count[8];
    sprintf( count, "%u", counter );

    float size = GetSize().DoubleValue();
    Fixed predictionTime = Fixed::FromDouble(g_predictionTime) * g_app->GetWorld()->GetTimeScaleFactor();
    float predictedLongitude = ( m_longitude + m_vel.x * predictionTime ).DoubleValue();
    float predictedLatitude = ( m_latitude + m_vel.y * predictionTime ).DoubleValue();

    g_renderer->SetFont( "kremlin", true );

    g_renderer->Text( predictedLongitude + ( size / 5 ), predictedLatitude + ( size / 5 ), 
                        White, size, count);


    if( m_nukeSupply > -1 &&
        CanLaunchBomber() )
    {
        char num[8];
        Image *bmp = g_resource->GetImage( "graphics/nukesymbol.bmp");
        sprintf( num, "%d", m_nukeSupply );
        float textWidth = g_renderer->TextWidth( num, size );
        float xModifier = textWidth/2;
        if( m_nukeSupply < 10 ) xModifier = 0;
        g_renderer->Text( predictedLongitude + ( size / 5 ) - xModifier, predictedLatitude + ( size ), 
                White, size, num );

        g_renderer->Blit( bmp, predictedLongitude - textWidth, predictedLatitude + ( size * 1.75f), size * 0.75f, size * -0.75f, White );
    }

   // g_app->GetRenderer()->Text( m_longitude + ( size / 5 ) +  + g_app->GetRenderer()->GetMapRenderer()->GetLongitudeMod(), m_latitude + ( size / 5 ), 
      //  count, Colour(255, 255, 255, 255), size, "fonts/bitlow.ttf", true);

    g_renderer->SetFont();
}

Fixed WorldObject::GetSize()
{
	Fixed size = 2;

    if( m_type == TypeFighter || 
        m_type == TypeBomber )
    {
        size *= 1;
    }

    if( g_app->GetMapRenderer()->GetZoomFactor() <= 0.25f )
    {
        size *= Fixed::FromDouble(g_app->GetMapRenderer()->GetZoomFactor()) * 4;
    }

    size /= g_app->GetWorld()->GetGameScale();

    return size;
}

bool WorldObject::CheckCurrentState()
{
    WorldObjectState *currentState = m_states[m_currentState];
    if( currentState->m_numTimesPermitted == 0 )
    {
        return false;
    }

    if( currentState->m_defconPermitted < g_app->GetWorld()->GetDefcon() )
    {
        if(m_teamId == g_app->GetWorld()->m_myTeamId)
        {
            char msg[128];
            sprintf( msg, LANGUAGEPHRASE("message_invalid_action") );
            LPREPLACEINTEGERFLAG( 'D', currentState->m_defconPermitted, msg );
            g_app->GetWorld()->AddWorldMessage( m_objectId, m_teamId, msg, WorldMessage::TypeInvalidAction, false );
        }
        return false;
    }

    /*if( m_stateTimer > 0.0f )
    {
        return false;
    }*/
    return true;
}

char *WorldObject::GetName (int _type)
{
    switch( _type )
    {
        case TypeSilo:          return LANGUAGEPHRASE("unit_silo");
        case TypeNuke:          return LANGUAGEPHRASE("unit_nuke");
        case TypeCity:          return LANGUAGEPHRASE("unit_city");
        case TypeExplosion:     return LANGUAGEPHRASE("unit_explosion");
        case TypeSub:           return LANGUAGEPHRASE("unit_sub");
        case TypeRadarStation:  return LANGUAGEPHRASE("unit_radar");
        case TypeBattleShip:    return LANGUAGEPHRASE("unit_battleship");
        case TypeAirBase:       return LANGUAGEPHRASE("unit_airbase");
        case TypeFighter:       return LANGUAGEPHRASE("unit_fighter");
        case TypeBomber:        return LANGUAGEPHRASE("unit_bomber");
        case TypeCarrier:       return LANGUAGEPHRASE("unit_carrier");
		case TypeTornado:       return LANGUAGEPHRASE("unit_tornado");
        case TypeSaucer:        return LANGUAGEPHRASE("unit_saucer");
    }

    return "?";
}


int WorldObject::GetType( char *_name )
{
    for( int i = 0; i < NumObjectTypes; ++i )
    {
        if( stricmp( GetName(i), _name ) == 0 )
        {
            return i;
        }
    }

    return -1;
}


char *WorldObject::GetTypeName (int _type)
{
    switch( _type )
    {
        case TypeSilo:          return "silo";
        case TypeNuke:          return "nuke";
        case TypeCity:          return "city";
        case TypeExplosion:     return "explosion";
        case TypeSub:           return "sub";
        case TypeRadarStation:  return "radar";
        case TypeBattleShip:    return "battleship";
        case TypeAirBase:       return "airbase";
        case TypeFighter:       return "fighter";
        case TypeBomber:        return "bomber";
        case TypeCarrier:       return "carrier";
		case TypeTornado:       return "tornado";
        case TypeSaucer:        return "saucer";
    }

    return "?";
}


void WorldObject::SetRadarRange ( Fixed radarRange )
{
    m_radarRange = radarRange;
}

void WorldObject::RunAI()
{

}

void WorldObject::RenderGhost( int teamId )
{
    if( m_lastSeenTime[teamId] != 0)
    {
        float size = GetSize().DoubleValue();       

        int transparency = (255 * ( m_lastSeenTime[teamId] / m_ghostFadeTime ) ).IntValue();
        Colour col = Colour(150, 150, 150, transparency);
        Image *img = GetBmpImage( m_lastSeenState[ teamId ] );

        float x = m_lastKnownPosition[teamId].x.DoubleValue() - size;
        float y = m_lastKnownPosition[teamId].y.DoubleValue() + size;
        float thisSize = size*2;

        Team *team = g_app->GetWorld()->GetTeam(m_teamId);

        if( team->m_territories[0] >= 3 )
        {
            x = m_lastKnownPosition[teamId].x.DoubleValue() + size;
            thisSize = size*-2;
        }       

        g_renderer->Blit( img, x, y, thisSize, size*-2, col );
    }
}

Image *WorldObject::GetBmpImage( int state )
{
    Image *bmpImage = g_resource->GetImage( bmpImageFilename );
    return bmpImage;
}


bool WorldObject::IsMovingObject()
{
    if( m_type == TypeSub ||
        m_type == TypeBattleShip ||        
        m_type == TypeFighter ||
        m_type == TypeBomber ||
        m_type == TypeCarrier ||
        m_type == TypeNuke )
    {
        return true;
    }
    else return false;
}

void WorldObject::RequestAction( ActionOrder *_action )
{
    m_actionQueue.PutData( _action);
}

bool WorldObject::IsActionQueueable()
{
    return false;
}

bool WorldObject::UsingGuns()
{
    return false;
}

void WorldObject::NukeStrike()
{
}

bool WorldObject::UsingNukes()
{
    return false;
}

void WorldObject::Retaliate( int attackerId )
{
}

bool WorldObject::IsPinging()
{
    return false;
}

void WorldObject::FireGun( Fixed range )
{
    WorldObject *targetObject = g_app->GetWorld()->GetWorldObject(m_targetObjectId);
    AppAssert( targetObject );
    GunFire *bullet = new GunFire( range );
    bullet->SetPosition( m_longitude, m_latitude );
    bullet->SetWaypoint( targetObject->m_longitude, targetObject->m_latitude );
    bullet->SetTargetObjectId( targetObject->m_objectId );
    bullet->SetTeamId( m_teamId );
    bullet->m_origin = m_objectId;
    bullet->m_distanceToTarget = g_app->GetWorld()->GetDistance( m_longitude, m_latitude, targetObject->m_longitude, targetObject->m_latitude );
    bullet->m_attackOdds = g_app->GetWorld()->GetAttackOdds( m_type, targetObject->m_type, m_objectId );
    g_app->GetWorld()->m_gunfire.PutData( bullet );
}

int WorldObject::GetAttackOdds( int _defenderType )
{
    return g_app->GetWorld()->GetAttackOdds( m_type, _defenderType );
}

WorldObject *WorldObject::CreateObject( int _type )
{
    switch( _type )
	{
        case TypeCity:                  return new City();
		case TypeSilo:                  return new Silo();			
		case TypeRadarStation:          return new RadarStation();
        case TypeNuke:                  return new Nuke();
        case TypeExplosion:             return new Explosion();
		case TypeSub:                   return new Sub();
		case TypeBattleShip:            return new BattleShip();
		case TypeAirBase:               return new AirBase();
        case TypeFighter:               return new Fighter();
        case TypeBomber:                return new Bomber();
		case TypeCarrier:               return new Carrier();
        case TypeTornado:               return new Tornado();
        //case TypeSaucer:                return new Saucer();
	}

	return NULL;
}

void WorldObject::ClearActionQueue()
{
    m_actionQueue.EmptyAndDelete();
}

void WorldObject::ClearLastAction()
{
    if( m_actionQueue.Size() > 0 )
    {
        ActionOrder *action = m_actionQueue[ m_actionQueue.Size() -1 ];
        m_actionQueue.RemoveDataAtEnd();
        delete action;
    }
}

void WorldObject::SetTargetObjectId( int targetObjectId )
{
	m_targetObjectId = targetObjectId;
}

int WorldObject::GetTargetObjectId()
{
    return m_targetObjectId;
}

void WorldObject::FleetAction( int targetObjectId )
{
}

bool WorldObject::LaunchBomber( int targetObjectId, Fixed longitude, Fixed latitude )
{
    Bomber *bomber = new Bomber();
    bomber->SetTeamId( m_teamId );
    bomber->SetPosition( m_longitude, m_latitude );
    bomber->SetWaypoint( longitude, latitude );

    WorldObject *target = g_app->GetWorld()->GetWorldObject( targetObjectId );
    if( target )
    {
        if( g_app->GetWorld()->GetAttackOdds( WorldObject::TypeBomber, target->m_type ) > 0 )
        {
            bomber->SetState(0);
            bomber->m_targetObjectId = target->m_objectId;
        }
        else
        {
            if( !target->IsMovingObject() )
            {
                bomber->SetState(1);
                bomber->SetNukeTarget( longitude, latitude );
                bomber->m_bombingRun = true;
            }
        }
    }
    else if( targetObjectId != -1 )
    {
        // target has already been destroyed, launch ready for attack
        bomber->SetState(0);
    }

    if( bomber->m_currentState == 0 )
    {
        for( int i = 0; i < g_app->GetWorld()->m_cities.Size(); ++i )
        {
            if( g_app->GetWorld()->m_cities.ValidIndex(i) )
            {
                City *city = g_app->GetWorld()->m_cities[i];
                if( city->m_longitude == longitude &&
                    city->m_latitude == latitude )
                {
                    bomber->SetState(1);
                    bomber->SetNukeTarget( longitude, latitude );
                    bomber->m_bombingRun = true;
                    break;
                }
            }
        }
    }

    g_app->GetWorld()->AddWorldObject( bomber );

    if( m_nukeSupply <= 0 )
    {
        bomber->m_bombingRun = false;
        bomber->m_nukeTargetLongitude = 0;
        bomber->m_nukeTargetLatitude = 0;
        bomber->m_states[1]->m_numTimesPermitted = 0;
    }

    return true;
}

bool WorldObject::LaunchFighter( int targetObjectId, Fixed longitude, Fixed latitude )
{
    if( targetObjectId >= OBJECTID_CITYS )
    {
        targetObjectId = -1;
    }

    Fighter *fighter = new Fighter();
    if( fighter->IsValidPosition( longitude, latitude ) )
    {
        fighter->SetTeamId( m_teamId );
        fighter->SetPosition( m_longitude, m_latitude );
	    WorldObject *target = g_app->GetWorld()->GetWorldObject(targetObjectId);
        if( target && target->IsMovingObject() )
        {
            fighter->SetTargetObjectId(targetObjectId);
		    fighter->SetWaypoint(target->m_longitude, target->m_latitude);
        }
	    else
	    {
		    fighter->SetWaypoint( longitude, latitude );
            fighter->m_playerSetWaypoint = true;
	    }
        g_app->GetWorld()->AddWorldObject( fighter );
        return true;
    }
    else
    {
        delete fighter;
        return false;
    }
}


int WorldObject::IsValidCombatTarget( int _objectId )
{
    WorldObject *obj = g_app->GetWorld()->GetWorldObject( _objectId );
    if( !obj ) return TargetTypeInvalid;

    bool isFriend = g_app->GetWorld()->IsFriend( m_teamId, obj->m_teamId );
    int attackOdds = g_app->GetWorld()->GetAttackOdds( m_type, obj->m_type, m_objectId );

    int currentDefcon = g_app->GetWorld()->GetDefcon();
    if( !isFriend && 
        attackOdds > 0 && 
        m_states[m_currentState]->m_defconPermitted < currentDefcon )
    {
        return TargetTypeDefconRequired;
    }


    if( !IsMovingObject() )
    {
        Fixed distance = g_app->GetWorld()->GetDistance( m_longitude, m_latitude, 
                                                         obj->m_longitude, obj->m_latitude );
        
        if( distance > m_states[m_currentState]->m_actionRange )         
        {
            return TargetTypeOutOfRange;
        }
    }

    if( m_states[m_currentState]->m_numTimesPermitted > -1 &&
        m_states[m_currentState]->m_numTimesPermitted - m_actionQueue.Size() <= 0 )
    {
        return TargetTypeOutOfStock;
    }
    
    if( !isFriend && attackOdds > 0 )
    {
        return TargetTypeValid;
    }

    return TargetTypeInvalid;
}

void WorldObject::CeaseFire( int teamId )
{
    if( m_targetObjectId != -1 )
    {
        WorldObject *obj = g_app->GetWorld()->GetWorldObject( m_targetObjectId );
        if( obj )
        {
            if( obj->m_teamId == teamId )
            {
                m_targetObjectId = -1;
            }
        }
    }

    for( int i = 0; i < m_actionQueue.Size(); ++i )
    {
        ActionOrder *action = m_actionQueue[i];
        WorldObject *obj = g_app->GetWorld()->GetWorldObject( action->m_targetObjectId );
        if( obj )
        {
            if( obj->m_teamId == teamId )
            {
                m_actionQueue.RemoveData(i);
                delete action;
                --i;
            }
        }
    }
}


int WorldObject::IsValidMovementTarget( Fixed longitude, Fixed latitude )
{
    return TargetTypeInvalid;
}


bool WorldObject::CanLaunchBomber()
{
    return false;
}

bool WorldObject::CanLaunchFighter()
{
    return false;
}


int WorldObject::CountIncomingFighters()
{
    return 0;
}

bool WorldObject::SetWaypointOnAction()
{
    return false;
}

static char tempStateName[256];

char *WorldObjectState::GetStateName()
{
    sprintf( tempStateName, "%s", m_stateName );
    return tempStateName;
}

char *WorldObjectState::GetPrepareName()
{
    sprintf( tempStateName, "%s", m_stateName );
    return tempStateName;
}

char *WorldObjectState::GetPreparingName()
{
    sprintf( tempStateName, "%s", m_stateName );
    return tempStateName;
}

char *WorldObjectState::GetReadyName()
{
    sprintf( tempStateName, "%s", m_stateName );
    return tempStateName;
}


char *HashDouble( double value, char *buffer )
{
	union
	{
		double doublePart;
		unsigned long long intPart;
	} v;
	
	v.doublePart = value;
	const unsigned long long i = v.intPart;
	
    for( int j = 0; j < 16; ++j )
    {
		sprintf(buffer + j, "%01x", (unsigned int) ((i >> (j*4)) & 0xF) );
    }

    return buffer;
};


char *WorldObject::LogState()
{
    char buf1[17], buf2[17], buf3[17], buf4[17], buf5[17], buf6[17], buf7[17];

    static char s_result[10240];
    snprintf( s_result, 10240, "obj[%d] [%10s] team[%d] fleet[%d] long[%s] lat[%s] velX[%s] velY[%s] state[%d] target[%d] life[%d] timer[%s] retarget[%s] ai[%s]",
                m_objectId,
                GetName(m_type),
                m_teamId,
                m_fleetId,
                HashDouble( m_longitude.DoubleValue(), buf1 ),
                HashDouble( m_latitude.DoubleValue(), buf2 ),
                HashDouble( m_vel.x.DoubleValue(), buf3 ),
                HashDouble( m_vel.y.DoubleValue(), buf4 ),
                m_currentState,
                m_targetObjectId,
                m_life,
                HashDouble( m_stateTimer.DoubleValue(), buf5 ),
                HashDouble( m_retargetTimer.DoubleValue(), buf6 ),
                HashDouble( m_aiTimer.DoubleValue(), buf7 ) );

    for( int i = 0; i < g_app->GetWorld()->m_teams.Size(); ++i )
    {
        Team *team = g_app->GetWorld()->m_teams[i];
        char thisTeam[512];
        sprintf( thisTeam, "\n\tTeam %d visible[%d] seen[%d] pos[%s %s] vel[%s %s] seen[%s] state[%d]",
                            team->m_teamId,
                            m_visible[team->m_teamId],
                            m_seen[team->m_teamId],
                            HashDouble( m_lastKnownPosition[team->m_teamId].x.DoubleValue(), buf1 ),
                            HashDouble( m_lastKnownPosition[team->m_teamId].y.DoubleValue(), buf2 ),
                            HashDouble( m_lastKnownVelocity[team->m_teamId].x.DoubleValue(), buf3 ),
                            HashDouble( m_lastKnownVelocity[team->m_teamId].y.DoubleValue(), buf4 ),
                            HashDouble( m_lastSeenTime[team->m_teamId].DoubleValue(), buf5 ),
                            m_lastSeenState[team->m_teamId] );

        strcat( s_result, thisTeam );
    }

    return s_result;
}


