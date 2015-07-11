#include "lib/universal_include.h"
#include "lib/language_table.h"
#include "lib/math/random_number.h"
#include "lib/math/math_utils.h"
#include "lib/hi_res_time.h"
#include "lib/profiler.h"

#include "app/globals.h"
#include "app/app.h"
#include "app/game.h"

#include "network/ClientToServer.h"

#include "renderer/map_renderer.h"

#include "world/team.h"
#include "world/world.h"
#include "world/silo.h"
#include "world/nuke.h"
#include "world/sub.h"
#include "world/radarstation.h"
#include "world/battleship.h"
#include "world/airbase.h"
#include "world/carrier.h"
#include "world/fighter.h"
#include "world/bomber.h"
#include "world/city.h"
#include "world/fleet.h"

#include <ctype.h> // for isspace()



Team::Team()
:   m_teamId(-1),
    m_clientId(-1),
    m_allianceId(-1),
    m_type(TypeUnassigned),
    m_enemyKills(0),
    m_friendlyDeaths(0),
    m_collatoralDamage(0),
    m_aiActionTimer(0),
    m_aiActionSpeed(5),
    m_numTerritories(0),
    m_currentState(StatePlacement),
    m_previousState(-1),
    m_desiredGameSpeed(20),
    m_teamColourFader(1),
    m_readyToStart(false),
    m_targetTeam(-1),
    m_subState(SubStateExploration),
    m_aiStateTimer(0),
    m_siloSwitchTimer(10),
    m_aiAssaultTimer(0),
    m_maxTargetsSeen(0),
    m_targetsVisible(0),
    m_validTargetPopulation(300000),
    m_unitCredits(53),
    m_randSeed(0),
    m_nameSet(false),
    m_alwaysSolo(true)
{
    m_unitsAvailable.Initialise( WorldObject::NumObjectTypes );
    m_unitsInPlay.Initialise( WorldObject::NumObjectTypes );
    m_ceaseFire.Initialise( MAX_TEAMS );
    m_sharingRadar.Initialise( MAX_TEAMS );
    m_leftAllianceTimer.Initialise( MAX_TEAMS );

    //m_aggression = 5 + (syncrand() % 5);
    m_aggression = 0;
    
    m_unitsAvailable.SetAll(0);
    m_unitsInPlay.SetAll(0);
    m_ceaseFire.SetAll(false);
    m_sharingRadar.SetAll(false);
    m_leftAllianceTimer.SetAll(-1.0f);

    SetTeamName( "[name-not-set]" );
    
    //m_aiAssaultTimer = 600;
}

void Team::SetTeamType( int type )
{
    m_type = type;
}

void Team::SetTeamId( int teamId )
{
    m_teamId = teamId;
}

void Team::SetTeamName( char *name )
{
	strncpy( m_name, name, MAX_TEAM_NAME );
	m_name[ MAX_TEAM_NAME-1 ] = '\x0';
}

bool Team::IsValidName( char *name )
{
	int len = strlen(name);
	bool isBlank = true;
	
	for (int i = 0; i < len; i++)
	{
		if (!isspace(name[i]))
		{
			isBlank = false;
			break;
		}
	}
	
	return len <= MAX_TEAM_NAME && !isBlank;
}

Colour Team::GetTeamColour()
{
    if( m_allianceId == -1 ) return DarkGray;
    
    Colour allianceColour = g_app->GetWorld()->GetAllianceColour( m_allianceId );

    Team *myTeam = g_app->GetWorld()->GetMyTeam();
    if( myTeam && 
        myTeam->m_allianceId == m_allianceId &&
        myTeam->m_teamId != m_teamId &&
        g_app->m_gameRunning )
    {
        // This is a teammate, so dull his colours a bit
        allianceColour.m_r *= 0.5;
        allianceColour.m_g *= 0.5;
        allianceColour.m_b *= 0.5;
    }       
           
    return allianceColour;
    

//    int fadedColour = 100 + 100 * m_teamColourFader;
//    
//    switch( m_teamId )
//    {
//        case -1:    return Colour(100,100,100,100);
//        case 0:     return Colour(50,fadedColour,50,200);
//        case 1:     return Colour(fadedColour,50,50,200);
//        case 2:     return Colour(fadedColour,fadedColour,50,200);
//        case 3:     return Colour(fadedColour,fadedColour*0.5f,0,200);
//        case 4:     return Colour(fadedColour,50,fadedColour,200);
//        case 5:     return Colour(50,fadedColour,fadedColour,200);
//    }

//    switch( m_teamId )
//    {
//        case -1:    return Colour(100,100,100,100);
//        case 0:     return Colour(50,200,50,200);
//        case 1:     return Colour(200,50,50,200);
//        case 2:     return Colour(200,200,50,200);
//        case 3:     return Colour(255,120,0,200);
//        case 4:     return Colour(200,50,200,200);
//        case 5:     return Colour(50,200,200,200);
//    }

}

char *Team::GetTeamType()
{
    switch( m_type )
    {
        case TypeUnassigned :       return LANGUAGEPHRASE("team_unassigned");
        case TypeLocalPlayer :      return LANGUAGEPHRASE("team_local");
        case TypeRemotePlayer :     return LANGUAGEPHRASE("team_remote");
        case TypeAI :               return LANGUAGEPHRASE("team_ai");
    }

    return LANGUAGEPHRASE("unknown");
}

char *Team::GetTeamName()
{
	if( strcmp( m_name, "[name-not-set]" ) == 0 )
	{
		return LANGUAGEPHRASE("team_name_not_set");
	}

    return m_name;
}

int Team::GetRemainingPopulation()
{
    int pop = 0;
    for( int i = 0; i < g_app->GetWorld()->m_cities.Size(); ++i )
    {
        if( g_app->GetWorld()->m_cities.ValidIndex(i) )
        {
            City *city = g_app->GetWorld()->m_cities[i];
            if( city->m_teamId == m_teamId )
            {
                pop += city->m_population;
            }
        }
    }
    return pop;
}   

// ### AI Functions ### //

int Team::GetPlacementPriority()
{
    Fixed gameScale = World::GetGameScale();

    int territoriesPerTeam = g_app->GetGame()->GetOptionValue("TerritoriesPerTeam");

    int numRadar = (7 * gameScale * territoriesPerTeam).IntValue();
    if( m_unitsAvailable[WorldObject::TypeRadarStation] > 0 &&
        m_unitsInPlay[WorldObject::TypeRadarStation] < numRadar )
    {
        return WorldObject::TypeRadarStation;
    }

    int numSilos = (6 * gameScale).IntValue();
    if( g_app->GetGame()->GetOptionValue("VariableUnitCounts") == 1 )
    {
        numSilos = numSilos / float(5.0f / m_aggression);
    }
    numSilos *= territoriesPerTeam;

    if( m_unitsAvailable[WorldObject::TypeSilo] > 0 &&
        m_unitsInPlay[WorldObject::TypeSilo] < numSilos )
        return WorldObject::TypeSilo;

    int numAirbases = (4 * territoriesPerTeam * gameScale).IntValue();
    if( m_unitsAvailable[WorldObject::TypeAirBase] > 0 &&
        m_unitsInPlay[WorldObject::TypeAirBase] < numAirbases )
        return WorldObject::TypeAirBase;

    if( m_unitsAvailable[WorldObject::TypeSub] > 0 )
        return WorldObject::TypeSub;

    if( m_unitsAvailable[WorldObject::TypeCarrier] > 0 )
        return WorldObject::TypeCarrier;

    if( m_unitsAvailable[WorldObject::TypeBattleShip] > 0 )
        return WorldObject::TypeBattleShip;

    if( m_unitCredits > 0 &&
        g_app->GetGame()->GetOptionValue("VariableUnitCounts") == 1)
    {
        if( m_unitCredits >= g_app->GetWorld()->GetUnitValue( WorldObject::TypeSilo ) )
            return WorldObject::TypeSilo;

        if( m_unitCredits >= g_app->GetWorld()->GetUnitValue( WorldObject::TypeAirBase ) )
            return WorldObject::TypeAirBase;

        if( m_unitCredits >= g_app->GetWorld()->GetUnitValue( WorldObject::TypeRadarStation) )
            return WorldObject::TypeRadarStation;
    }

    return -1;
}

void Team::CheckPanicState()
{
    if( m_currentState < StateStrike )
    {
        if( g_app->GetWorld()->GetDefcon() == 1 )
        {
            int territoriesPerTeam = g_app->GetGame()->GetOptionValue("TerritoriesPerTeam");
            float totalFriendly = 100000000 * territoriesPerTeam;
            float fractionAlive = 1.0f - m_friendlyDeaths / totalFriendly;
            fractionAlive = min( fractionAlive, 1.0f );
            fractionAlive = max( fractionAlive, 0.0f );

            if( m_unitsInPlay[WorldObject::TypeSilo] < 3 ||
                fractionAlive < 0.3f )
            {
                if( m_currentState != StatePanic )
                {
                    // panic!
                    m_currentState = StatePanic;
                }
            }
        }
    }
}

void Team::RunAI()
{
    START_PROFILE("TeamAI");
    if( g_app->GetWorld()->GetTimeScaleFactor() == 0 )
    {
        END_PROFILE("TeamAI");
        return;
    }

    //
    // Choose a target team

    if( m_targetTeam == -1 )
    {
        LList<int> validTargetTeams;
        for( int i = 0; i < g_app->GetWorld()->m_teams.Size(); ++i )
        {
            int teamId = g_app->GetWorld()->m_teams[i]->m_teamId;
            if( !g_app->GetWorld()->IsFriend( m_teamId, teamId ) )
            {
                validTargetTeams.PutData(teamId);
            }
        }

        if( validTargetTeams.Size() )
        {
            int randomIndex = syncrand() % validTargetTeams.Size();
            m_targetTeam = validTargetTeams[randomIndex];
        }
    }


    Team *targetTeam = g_app->GetWorld()->GetTeam( m_targetTeam );
    if( targetTeam )
    {
        int populationThreshold = g_app->GetGame()->GetOptionValue( "PopulationPerTerritory" ) * 
                                  g_app->GetGame()->GetOptionValue( "TerritoriesPerTeam" ) * 1000000 * 0.25f;
        if( targetTeam->GetRemainingPopulation() < populationThreshold )
        {
            if( g_app->GetWorld()->m_teams.Size() > 2 )
            {
                int oldTeam = m_targetTeam;
                m_targetTeam = -1;
                int pop = 0.0f;
                for( int i = 0; i < g_app->GetWorld()->m_teams.Size(); ++i )
                {
                    Team *team = g_app->GetWorld()->m_teams[i];
                    if( !g_app->GetWorld()->IsFriend( m_teamId, team->m_teamId ) )
                    {
                        int teamPop = g_app->GetWorld()->GetTeam(team->m_teamId)->GetRemainingPopulation();
                        if( teamPop > pop )
                        {
                            pop = teamPop;
                            m_targetTeam = team->m_teamId;
                        }
                    }
                }
                if( m_targetTeam != oldTeam )
                {
                    for( int i = 0; i < m_fleets.Size(); ++i )
                    {
                        if( !m_fleets[i]->IsInCombat() )
                        {
                            // stop all fleets so they can set new target points for new enemy
                            m_fleets[i]->StopFleet();
                        }
                    }
                }
            }
            else
            {
                m_validTargetPopulation *= 0.75f;
            }
        }
    }
    if( g_app->GetGame()->m_victoryTimer > 0 )
    {
        m_validTargetPopulation = 0;
    }
    m_aiAssaultTimer -= SERVER_ADVANCE_PERIOD * g_app->GetWorld()->GetTimeScaleFactor();
    if( m_currentState >= StateStrike )
    {
        m_siloSwitchTimer -= SERVER_ADVANCE_PERIOD * g_app->GetWorld()->GetTimeScaleFactor();
    }

    m_aiActionTimer -= SERVER_ADVANCE_PERIOD * g_app->GetWorld()->GetTimeScaleFactor();

    if( m_aiActionTimer <= 0 )
    {
        m_aiActionTimer = m_aiActionSpeed;

        m_targetsVisible = GetNumTargets();
        if( m_targetsVisible > m_maxTargetsSeen )
        {
            m_maxTargetsSeen = m_targetsVisible;
        }

        CheckPanicState();

        switch( m_currentState )
        {
            case StatePlacement:    PlacementAI();  break;
            case StateScouting:     ScoutingAI();   break;
            case StateAssault:      AssaultAI();    break;
            case StateStrike:       StrikeAI();     break;            
        }
        HandleAIEvents();

        //
        // Fleet AI
        
        for( int i = 0; i < m_fleets.Size(); ++i )
        {
            Fleet *fleet = m_fleets[i];
            fleet->RunAI();
        }


        //
        // Object AI 
        
        for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
        {
            if( g_app->GetWorld()->m_objects.ValidIndex(i) )
            {
                WorldObject *obj = g_app->GetWorld()->m_objects[i];
                if( obj->m_teamId == m_teamId )
                {
                    obj->RunAI();
                }
            }
        }
    }

    END_PROFILE("TeamAI");
}

/////////////////////
// Placement AI
// Places all units
/////////////////////
void Team::PlacementAI()
{
    int objectPriority = GetPlacementPriority();
    if( objectPriority != -1 )
    {
        bool createFleet = false;
        Fixed minDistance = 0;
        switch( objectPriority )
        {            
            case WorldObject::TypeRadarStation : minDistance = 20;   break;
            case WorldObject::TypeSilo         : minDistance = 10;   break;
            case WorldObject::TypeAirBase      : minDistance = 10;   break;

            default: 
                createFleet = true;   
                break;
        }

        Fixed worldScale = World::GetGameScale();
        minDistance /= worldScale;

        int x = 0;
        if( createFleet )
        {
            // set up a new fleet    
            int fleetType = -1;
            bool defensive = false;
            LList<int> fleetTypeList;
            
            //if( CanAssembleFleet(Fleet::FleetTypeBattleships)) fleetTypeList.PutData( Fleet::FleetTypeBattleships );
            if( CanAssembleFleet(Fleet::FleetTypeScout)) fleetTypeList.PutData( Fleet::FleetTypeScout );
            if( CanAssembleFleet(Fleet::FleetTypeNuke)) fleetTypeList.PutData( Fleet::FleetTypeNuke );
            if( CanAssembleFleet(Fleet::FleetTypeSubs)) fleetTypeList.PutData( Fleet::FleetTypeSubs );
            if( CanAssembleFleet(Fleet::FleetTypeMixed)) fleetTypeList.PutData( Fleet::FleetTypeMixed );

            if( m_unitsAvailable[WorldObject::TypeBattleShip] +
                m_unitsAvailable[WorldObject::TypeSub] +
                m_unitsAvailable[WorldObject::TypeCarrier] <= 6 )
            {
                fleetType = Fleet::FleetTypeMixed;
            }
            else
            {
                if( fleetTypeList.Size() > 0 )
                {
                    int r = syncrand() % fleetTypeList.Size();
                    fleetType = fleetTypeList[r];
                }
                else
                {
                    fleetType = Fleet::FleetTypeRandom;
                }
            }

            if(AssembleFleet( fleetType ))
            {
                int id = m_fleets.Size() -1;
                switch( fleetType )
                {
                    case Fleet::FleetTypeScout :    
                    case Fleet::FleetTypeNuke :     
                    case Fleet::FleetTypeSubs :     m_fleets[id]->m_aiState = Fleet::AIStateSneaking; break;

                    default: m_fleets[id]->m_aiState = Fleet::AIStateHunting; break;

                }

                LList<int> validPointsList;
                for( int i = 0; i < g_app->GetWorld()->m_aiPlacementPoints.Size(); ++i )
                {
                    Vector3<Fixed> *point = g_app->GetWorld()->m_aiPlacementPoints[i];
                    if( g_app->GetMapRenderer()->GetTerritory( point->x, point->y, true ) == m_teamId )
                    {
                        //Fixed distance = Fixed::MAX;
                        //for( int j = 0; j < m_fleets.Size(); ++j )
                        //{
                        //    Fleet *fleet = GetFleet(j);
                        //    if( g_app->GetWorld()->GetDistance( fleet->m_longitude, fleet->m_latitude, point->x, point->y ) < distance )
                        //    {
                        //        distance = g_app->GetWorld()->GetDistance( fleet->m_longitude, fleet->m_latitude, point->x, point->y );
                        //    }
                        //}
                        //Fixed maxDistance = 2 / worldScale;
                        //if( distance > maxDistance )
                        {
                            validPointsList.PutData(i);
                        }
                    }
                }

                int attempt = 20;
                bool success = false;
                while( attempt > 0 && validPointsList.Size() )
                {
                    int pointId = syncrand() % validPointsList.Size();
                    Vector3<Fixed> *point = g_app->GetWorld()->m_aiPlacementPoints[ validPointsList[pointId]];
                    Fixed longitude = point->x + syncsfrand(10);
                    Fixed latitude = point->y + syncsfrand(10);
                    if( m_fleets[id]->ValidFleetPlacement(longitude, latitude) &&
                        g_app->GetWorld()->GetClosestNode( longitude, latitude ) != -1 )
                    {
                        m_fleets[id]->PlaceFleet(longitude, latitude );    
                        success = true;
                        break;
                    }
                    --attempt;
                }

                if( !success && validPointsList.Size() )
                {
                    int pointId = syncrand() % validPointsList.Size();
                    Vector3<Fixed> *point = g_app->GetWorld()->m_aiPlacementPoints[ validPointsList[pointId]];
                    m_fleets[id]->PlaceFleet( point->x, point->y );
                    AppDebugOut( "Failed to find good spot, fell back on %2.2f %2.2f\n", point->x.DoubleValue(), point->y.DoubleValue() );
                }
            }
        }
        else
        {
            Fixed longitude = 0;
            Fixed latitude = 0;
            bool randomPlacement = false;

            if( objectPriority == WorldObject::TypeRadarStation ||
                objectPriority == WorldObject::TypeSilo )
            {
                if( CheckCityCoverage( objectPriority, &longitude, &latitude ) )
                {
                    if( GetUncoveredArea( objectPriority == WorldObject::TypeRadarStation, &longitude, &latitude) )
                    {
                        g_app->GetWorld()->ObjectPlacement( m_teamId, objectPriority, longitude, latitude, 255 );
                    }
                    else
                    {
                        randomPlacement = true;
                    }
                }
                else
                {
                    int x = 0;
                    while (x<1000)
                    {
                        x++;
                        if( x >= 1000 )
                        {
                            randomPlacement = true;
                        }
                        Fixed newLong = longitude + syncsfrand(10);
                        Fixed newLat = latitude + syncsfrand(10);
                        if( g_app->GetWorld()->IsValidPlacement( m_teamId, newLong, newLat, objectPriority ) &&
                            IsSafeLocation( newLong, newLat) )
                        {
                            g_app->GetWorld()->ObjectPlacement( m_teamId, objectPriority, newLong, newLat, 255 );
                            break;
                        }
                    }
                }
            }
            else if( objectPriority == WorldObject::TypeAirBase )
            {
                int defensiveBases = 4 * g_app->GetGame()->GetOptionValue("TerritoriesPerTeam");
                defensiveBases *= (10.0f - m_aggression) / 10.0f;

                if( m_unitsAvailable[objectPriority] <= defensiveBases )
                {
                    // place city-defensive airbases
                    if( CheckCityCoverage( objectPriority, &longitude, &latitude ) )
                    {
                        randomPlacement = true;
                    }
                    else
                    {
                        int x = 0;
                        while (x<1000)
                        {
                            x++;
                            if( x >= 1000 )
                            {
                                randomPlacement = true;
                            }
                            Fixed newLong = longitude + syncsfrand(10);
                            Fixed newLat = latitude + syncsfrand(10);
                            if( g_app->GetWorld()->IsValidPlacement( m_teamId, newLong, newLat, objectPriority ) &&
                                IsSafeLocation( newLong, newLat) )
                            {
                                g_app->GetWorld()->ObjectPlacement( m_teamId, objectPriority, newLong, newLat, 255 );
                                break;
                            }
                        }
                    }
                }
                else
                {
                    // place coastal airbases
                    int x = 0;
                    bool success = false;
                    while(true)
                    {
                        x++;
                        if( x > 1000 )
                        {
                            randomPlacement = true;
                            break;
                        }
                        longitude = syncsfrand(360);
                        latitude = syncsfrand(180);
                        
                        int randTerritory = syncrand() % World::NumTerritories;
                        int x = 0;
                        while((g_app->GetWorld()->GetTerritoryOwner(randTerritory) == m_teamId ||
                               g_app->GetWorld()->GetTerritoryOwner(randTerritory) == -1 ) &&
                               x < 20 )
                        {
                            randTerritory = syncrand() % World::NumTerritories;
                            x++;
                        }
                        
                        if( g_app->GetWorld()->IsValidPlacement( m_teamId, longitude, latitude, objectPriority ) &&
                            g_app->GetMapRenderer()->IsCoastline( longitude, latitude ) &&
                            g_app->GetMapRenderer()->IsValidTerritory( m_teamId, longitude, latitude, false ))
                        {
                            if( g_app->GetWorld()->GetDistanceSqd( longitude, latitude, 
                                                                    g_app->GetWorld()->m_populationCenter[randTerritory].x, 
                                                                    g_app->GetWorld()->m_populationCenter[randTerritory].y) < 90 * 90 )
                            {
                                int index = g_app->GetWorld()->GetNearestObject( m_teamId, longitude, latitude, objectPriority );
                                WorldObject *obj = g_app->GetWorld()->GetWorldObject( index );
                                if( obj )
                                {
                                    if( g_app->GetWorld()->GetDistanceSqd( longitude, latitude, obj->m_longitude, obj->m_latitude ) > minDistance * minDistance )
                                    {
                                        success = true;
                                        break;
                                    }
                                }
                                else
                                {
                                    success = true;
                                    break;
                                }
                            }
                        }
                    }
                    // place unit 
                    if( success )
                    {
                        m_unitsAvailable[objectPriority]--;

	                    WorldObject *newUnit = WorldObject::CreateObject( objectPriority );
		                newUnit->SetTeamId(m_teamId);
		                g_app->GetWorld()->AddWorldObject( newUnit ); 
		                newUnit->SetPosition( longitude, latitude );
                        newUnit->m_offensive = true;
                    }
                }
            }

            if( randomPlacement )
            {
                int x = 0;
                while( x < 5000 )
                {
                    x++;
                    if( x >= 5000 )
                    {
                        m_unitsAvailable[objectPriority]--;
                    }
                    longitude = syncsfrand(360);
                    latitude = syncsfrand(180);
                    int index = g_app->GetWorld()->GetNearestObject( m_teamId, longitude, latitude, objectPriority );
                    WorldObject *obj = g_app->GetWorld()->GetWorldObject( index );
                                        
                    if( g_app->GetWorld()->IsValidPlacement( m_teamId, longitude, latitude, objectPriority ) )
                    {
                        if (!obj || 
                             g_app->GetWorld()->GetDistanceSqd( longitude, latitude, obj->m_longitude, obj->m_latitude ) > minDistance * minDistance )
                        {
                            g_app->GetWorld()->ObjectPlacement( m_teamId, objectPriority, longitude, latitude, 255 );
                            break;
                        }
                    }
                }
            }

        }
    }
    else
    {
        ChangeState( StateScouting );
    }
}

void Team::ScoutingAI()
{
    if( g_app->GetWorld()->GetDefcon() == 1 )
    {
        int territoriesPerTeam = g_app->GetGame()->GetOptionValue( "TerritoriesPerTeam" );
        
        if( m_targetsVisible >= 5 * territoriesPerTeam ||
            m_aiAssaultTimer <= 0 ||
            g_app->GetGame()->m_victoryTimer > 0 )
        {
            m_currentState = StateAssault;
        }    
    }
}


void Team::AssaultAI()
{
    if( g_app->GetWorld()->GetDefcon() == 1 )
    {
        int territoriesPerTeam = g_app->GetGame()->GetOptionValue( "TerritoriesPerTeam" );
        int targetsDestroyed = m_maxTargetsSeen - m_targetsVisible;

        if( targetsDestroyed >= 5 * territoriesPerTeam ||
            m_aiAssaultTimer <= 0 ||
            g_app->GetGame()->m_victoryTimer > 0 )
        {
            m_currentState = StateStrike;
        }    
    }
}


///////////////////////
// Strike AI
// Full strike against cities. Enemy defensives should be low at this point
///////////////////////
void Team::StrikeAI()
{
    if( m_subState < SubStateLaunchBombers )
    {
        m_subState = SubStateLaunchBombers;
    }

    m_aiStateTimer -= SERVER_ADVANCE_PERIOD * g_app->GetWorld()->GetTimeScaleFactor();
    if( m_aiStateTimer <= 0 )
    {
        switch( m_subState )
        {
            case SubStateLaunchBombers: m_subState = SubStateLaunchNukes;   break;
            case SubStateLaunchNukes  : m_subState = SubStateActivateSubs;  break;
            case SubStateActivateSubs : m_currentState = StateFinal;        break;
        }
        int territoryId = -1;
        int myTerritory = -1;
        for( int i = 0; i < World::NumTerritories; ++i )
        {
            if( g_app->GetWorld()->GetTerritoryOwner(i) == m_targetTeam )
            {
                territoryId = i;
            }
            if( g_app->GetWorld()->GetTerritoryOwner(i) == m_teamId )
            {
                myTerritory = i;
            }
        }
        if( territoryId != -1 && myTerritory != -1 )
        {
            Fixed distance = g_app->GetWorld()->GetDistance(    g_app->GetWorld()->m_populationCenter[myTerritory].x,
                                                                g_app->GetWorld()->m_populationCenter[myTerritory].y,
                                                                g_app->GetWorld()->m_populationCenter[territoryId].x,
                                                                g_app->GetWorld()->m_populationCenter[territoryId].y );
            m_aiStateTimer = (distance / Fixed::Hundredths(15)) / 2; // time it takes for nukes to get from friendly to enemy territory                                    
        }
    }
}

void Team::HandleAIEvents()
{
    for( int i = 0; i < m_eventLog.Size(); ++i )
    {
        Event *event = m_eventLog[i];
        bool eventHandled = false;

        switch( event->m_type )
        {
            case Event::TypeSubDetected:
            {
                WorldObject *obj = g_app->GetWorld()->GetWorldObject( event->m_objectId );
                if( !obj )
                {
                    eventHandled = true;
                }

                if( obj && obj->m_seen[m_teamId] )
                {
                    Fleet *bestFleet = NULL;
                    Fixed distanceSqd = Fixed::MAX;

                    for( int j = 0; j < m_fleets.Size(); ++j )
                    {
                        Fleet *fleet = GetFleet(j);
                        if( fleet &&                            
                            !fleet->IsInCombat() &&
                            fleet->IsInFleet( WorldObject::TypeCarrier ) &&
                            fleet->m_aiState == Fleet::AIStateHunting &&
                            !fleet->m_crossingSeam )
                        {
                            Fixed newDistanceSqd = g_app->GetWorld()->GetDistanceSqd( fleet->m_longitude, 
                                                                                      fleet->m_latitude, 
                                                                                      event->m_longitude, 
                                                                                      event->m_latitude );

                            if( newDistanceSqd < distanceSqd )
                            {
                                distanceSqd = newDistanceSqd;
                                bestFleet = fleet;
                            }
                        }
                    }
                    
                    if( bestFleet )
                    {
                        bestFleet->m_aiState = Fleet::AIStateIntercepting;
                        bestFleet->m_targetObjectId = event->m_objectId;
                        bestFleet->MoveFleet( obj->m_lastKnownPosition[m_teamId].x, 
                                              obj->m_lastKnownPosition[m_teamId].y );
                        eventHandled = true;
                    }
                }
                break;
            }
        
            case Event::TypeEnemyIncursion:
            {
                WorldObject *obj = g_app->GetWorld()->GetWorldObject( event->m_objectId );
                if( !obj )
                {
                    eventHandled = true;
                }

                if( obj )
                {
                    Fleet *bestFleet = NULL;
                    Fixed distanceSqd = Fixed::MAX;

                    for( int j = 0; j < m_fleets.Size(); ++j )
                    {
                        Fleet *fleet = m_fleets[j];
                        if( fleet &&                        
                            !fleet->IsInCombat() &&
                            fleet->m_currentState == Fleet::AIStateHunting &&
                            !fleet->m_crossingSeam )
                        {
                            Fixed thisDistanceSqd = g_app->GetWorld()->GetDistanceSqd( fleet->m_longitude, fleet->m_latitude, event->m_longitude, event->m_latitude );

                            if( thisDistanceSqd < distanceSqd )
                            {
                                distanceSqd = thisDistanceSqd;
                                bestFleet = fleet;
                            }
                        }
                    }
                    
                    if( bestFleet )
                    {
                        bestFleet->MoveFleet( obj->m_lastKnownPosition[m_teamId].x, 
                                              obj->m_lastKnownPosition[m_teamId].y );
                        event->m_actionTaken++;
                        if( event->m_actionTaken >= 2 )
                        {
                            eventHandled = true;
                        }
                    }
                }
                break;
            }

            case Event::TypeNukeLaunch:
                // Nobody bothers with this event, so delete it straight away
                eventHandled = true;
                break;

            case Event::TypeIncomingAircraft:
                break;
        }

        if( eventHandled )
        {
            DeleteEvent(i);
            --i;
        }
    }
}

Fixed Team::GetClosestObject( int cityId, int objectType )
{
    Fixed dist = Fixed::MAX;
    City *city = g_app->GetWorld()->m_cities[cityId];
    int index = g_app->GetWorld()->GetNearestObject( m_teamId, city->m_longitude, city->m_latitude, objectType );
    WorldObject *obj = g_app->GetWorld()->GetWorldObject( index );
    if( obj )
    {
        dist = g_app->GetWorld()->GetDistance( city->m_longitude, city->m_latitude, obj->m_longitude, obj->m_latitude );
    }
    return ( dist == Fixed::MAX ? -1 : dist );
}

bool Team::CheckCityCoverage( int objectType, Fixed *longitude, Fixed *latitude )
{
    Fixed checkDistance = 20; // radar and silo both have range of 30.0f, make sure cities are reasonably covered
    if( objectType == WorldObject::TypeAirBase ) 
    {
        checkDistance = 40;
    }
    else if( objectType == WorldObject::TypeSilo )
    {
        checkDistance = Fixed::FromDouble(7.5);
    }

    for( int i = 0; i < g_app->GetWorld()->m_cities.Size(); ++i )
    {
        if( g_app->GetWorld()->m_cities.ValidIndex(i) )
        {
            City *city = g_app->GetWorld()->m_cities[i];
            if( city->m_teamId == m_teamId )
            {
                Fixed dist = GetClosestObject( i, objectType );
                if( dist > checkDistance ||
                    dist == -1 )
                {
                    *longitude = city->m_longitude;
                    *latitude = city->m_latitude;
                    return false;
                }
            }
        }
    }
    return true;
}


bool Team::IsSafeLocation(Fixed longitude, Fixed latitude)
{
    for( int i = 0; i < g_app->GetWorld()->m_cities.Size(); ++i )
    {
        if( g_app->GetWorld()->m_cities.ValidIndex(i) )
        {
            City *city = g_app->GetWorld()->m_cities[i];
            if( city->m_teamId == m_teamId )
            {
                if( g_app->GetWorld()->GetDistanceSqd( longitude, latitude, city->m_longitude, city->m_latitude ) < Fixed::FromDouble(2.5f * 2.5f) )
                {
                    return false;
                }
            }
        }
    }
    return true;
}

bool Team::GetUncoveredArea( bool radar, Fixed *longitude, Fixed *latitude )
{
    int x = 0;

    int type = -1;
    Fixed distance = 30;
    if( radar )
    {
        type = WorldObject::TypeRadarStation;
    }
    else
    {
        type = WorldObject::TypeSilo;
        distance = Fixed::FromDouble(7.5f);
    }

    while( x < 1000 )
    {
        x++;
        Fixed randLong = syncsfrand(360);
        Fixed randLat = syncsfrand(180);
        int index = g_app->GetWorld()->GetNearestObject( m_teamId, randLong, randLat, type );;
        WorldObject *obj = g_app->GetWorld()->GetWorldObject(index);

        if( obj )
        {
            if( g_app->GetMapRenderer()->IsValidTerritory( m_teamId, randLong, randLat, false ) &&
                g_app->GetWorld()->GetDistance( randLong, randLat, obj->m_longitude, obj->m_latitude ) > distance )
            {
                *longitude = randLong;
                *latitude = randLat;
                return true;
            }
        }
    }
    return false;
}

bool Team::AssembleFleet( int fleetType )
{
    int ships = 0;
    int subs = 0;
    int carriers =0;
    Fleet::GetFleetMembers( fleetType, &ships, &subs, &carriers );

    if( ( m_unitsAvailable[WorldObject::TypeBattleShip] >= ships &&
          m_unitsAvailable[WorldObject::TypeSub] >= subs &&
          m_unitsAvailable[WorldObject::TypeCarrier] >= carriers ) ||
        ( fleetType == Fleet::FleetTypeMixed &&
          m_unitsAvailable[WorldObject::TypeBattleShip] +
          m_unitsAvailable[WorldObject::TypeSub] +
          m_unitsAvailable[WorldObject::TypeCarrier] <= 6 ) ||
          fleetType == Fleet::FleetTypeRandom )
    {
        CreateFleet();
        if( (m_unitsAvailable[WorldObject::TypeBattleShip] +
            m_unitsAvailable[WorldObject::TypeSub] +
            m_unitsAvailable[WorldObject::TypeCarrier] <= 6 &&
            fleetType == Fleet::FleetTypeMixed ) ||
            !CanAssembleFleet())
        {
            fleetType = Fleet::FleetTypeMixed;
            ships = m_unitsAvailable[WorldObject::TypeBattleShip];
            subs = m_unitsAvailable[WorldObject::TypeSub];
            carriers = m_unitsAvailable[WorldObject::TypeCarrier];
        }
        else if( fleetType == Fleet::FleetTypeRandom )
        {
            ships = 0;
            subs = 0;
            carriers = 0;
            
            int credits = m_unitCredits;
            if( g_app->GetGame()->GetOptionValue("VariableUnitCounts") == 0 ) credits = 9999999;

            bool outOfResources = false;
            while( ships + subs + carriers < 6 &&
                   !outOfResources)
            {                
                int r = syncfrand(3).IntValue();
                switch(r)
                {
                    case 0 : 
                    {
                        if( m_unitsAvailable[WorldObject::TypeBattleShip] - ships > 0 &&
                            credits - g_app->GetWorld()->GetUnitValue(WorldObject::TypeBattleShip) >= 0) 
                        {
                            ships++; 
                            credits -= g_app->GetWorld()->GetUnitValue( WorldObject::TypeBattleShip );
                            break;
                        }
                    }
                    case 1 : 
                    {
                        if( m_unitsAvailable[WorldObject::TypeSub] - subs > 0  &&
                            credits - g_app->GetWorld()->GetUnitValue(WorldObject::TypeSub) >= 0) 
                        {
                            subs++;
                            credits -= g_app->GetWorld()->GetUnitValue( WorldObject::TypeSub);
                            break;
                        }
                    }
                    case 2 :
                    {
                        if( m_unitsAvailable[WorldObject::TypeCarrier] - carriers > 0  &&
                            credits - g_app->GetWorld()->GetUnitValue(WorldObject::TypeCarrier) >= 0) 
                        {
                            carriers++;
                            credits -= g_app->GetWorld()->GetUnitValue( WorldObject::TypeCarrier );
                            break;
                        }
                    }
                    default: outOfResources = true;
                }
            }

        }

        int id = m_fleets.Size() - 1;

        m_fleets[id]->m_fleetType = fleetType;

        for( int i = 0; i < ships; ++i )
        {
            if( m_unitsAvailable[WorldObject::TypeBattleShip] > 0 )
            {
                m_fleets[id]->m_memberType.PutData( WorldObject::TypeBattleShip );
                m_unitsAvailable[WorldObject::TypeBattleShip]--;
            }
        }

        for( int i = 0; i < subs; ++i )
        {
            if( m_unitsAvailable[WorldObject::TypeSub] > 0 )
            {
                m_fleets[id]->m_memberType.PutData( WorldObject::TypeSub );
                m_unitsAvailable[WorldObject::TypeSub]--;
            }
        }

        for( int i = 0; i < carriers; ++i )
        {
            if( m_unitsAvailable[WorldObject::TypeCarrier] > 0 )
            {
                m_fleets[id]->m_memberType.PutData( WorldObject::TypeCarrier );
                m_unitsAvailable[WorldObject::TypeCarrier]--;
            }
        }
        return true;
    }
    else
    {
        return false;
    }
}

bool Team::CanAssembleFleet()
{
    for( int i = 0; i < Fleet::NumFleetTypes; ++i )
    {
        int ships, subs, carriers;
        Fleet::GetFleetMembers( i, &ships, &subs, &carriers );

        int creditCost = g_app->GetWorld()->GetUnitValue( WorldObject::TypeBattleShip ) * ships;
        creditCost += g_app->GetWorld()->GetUnitValue( WorldObject::TypeSub ) * subs;
        creditCost += g_app->GetWorld()->GetUnitValue( WorldObject::TypeCarrier ) * carriers;

        int variableTeamUnits = g_app->GetGame()->GetOptionValue("VariableUnitCounts");
        bool enoughCredits = m_unitCredits >= creditCost;
        if( !variableTeamUnits ) enoughCredits = true;
        
        if( m_unitsAvailable[WorldObject::TypeBattleShip] >= ships &&
            m_unitsAvailable[WorldObject::TypeSub] >= subs &&
            m_unitsAvailable[WorldObject::TypeCarrier] >= carriers &&
            enoughCredits )
        {
            return true;
        }    
    }
    return false;
}

bool Team::CanAssembleFleet( int fleetType )
{
    int ships, subs, carriers;
    Fleet::GetFleetMembers( fleetType, &ships, &subs, &carriers );
    int creditCost = g_app->GetWorld()->GetUnitValue( WorldObject::TypeBattleShip ) * ships;
    creditCost += g_app->GetWorld()->GetUnitValue( WorldObject::TypeSub ) * subs;
    creditCost += g_app->GetWorld()->GetUnitValue( WorldObject::TypeCarrier ) * carriers;

    int variableTeamUnits = g_app->GetGame()->GetOptionValue("VariableUnitCounts");
    bool enoughCredits = m_unitCredits >= creditCost;
    if( !variableTeamUnits ) enoughCredits = true;

    if( m_unitsAvailable[WorldObject::TypeBattleShip] >= ships &&
        m_unitsAvailable[WorldObject::TypeSub] >= subs &&
        m_unitsAvailable[WorldObject::TypeCarrier] >= carriers &&
        enoughCredits )
    {
        return true;
    }
    else 
    {
        return false;
    }
}

void Team::AddEvent( int type, int objectId, int teamId, int fleetId, Fixed longitude, Fixed latitude )
{
    if( teamId == m_teamId ) 
    {
        return;
    }
    for( int i = 0; i < m_eventLog.Size(); ++i )
    {
        Event *log = m_eventLog[i];
        if( log->m_type == type &&
            fleetId != -1 &&
            log->m_fleetId == fleetId &&
            log->m_teamId == teamId )
        {
            return;
        }

        if( log->m_objectId == objectId &&
            log->m_type == type )
        {
            return;
        }
    }

    Event *event = new Event();
    event->m_type = type;
    event->m_objectId = objectId;
    event->m_teamId = teamId;
    event->m_fleetId = fleetId;
    event->m_longitude = longitude;
    event->m_latitude = latitude;

    m_eventLog.PutData( event );
}

void Team::DeleteEvent( int id )
{
    Event *event = m_eventLog[id];
    m_eventLog.RemoveData(id);
    delete event;
}

Event *Team::GetEvent( int id )
{
    if( m_eventLog.ValidIndex(id) )
    {
        return m_eventLog[id];
    }
    else
    {
        return NULL;
    }
}

int Team::CountTargettedUnits( int targetId )
{
    int num = 0;
    for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
    {
        if( g_app->GetWorld()->m_objects.ValidIndex(i) )
        {
            WorldObject *obj = g_app->GetWorld()->m_objects[i];
            if( obj->m_teamId == m_teamId )
            {
                if( obj->m_targetObjectId == targetId )
                {
                    num++;
                }
                for( int j = 0; j < obj->m_actionQueue.Size(); ++j )
                {
                    if( obj->m_actionQueue[j]->m_targetObjectId == targetId )
                    {
                        num++;
                    }
                }
            }
        }
    }
    return num;
}

int Team::GetNumTargets()
{
    int num = 0;
    for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
    {
        if( g_app->GetWorld()->m_objects.ValidIndex(i) )
        {
            WorldObject *obj = g_app->GetWorld()->m_objects[i];
            if( !g_app->GetWorld()->IsFriend(obj->m_teamId, m_teamId ) &&
                ( obj->m_visible[m_teamId] || obj->m_seen[m_teamId] ) &&
                ( obj->m_type == WorldObject::TypeSilo ||
                  obj->m_type == WorldObject::TypeRadarStation ||
                  obj->m_type == WorldObject::TypeAirBase ) )           
            {
                num++;
            }
        }
    }

    return num;
}

void Team::ChangeState( int newState )
{
    m_previousState = m_currentState;
    m_currentState = newState;
}

// ### Territory Functions ### //

bool Team::OwnsTerritory( int territoryId )
{
    for( int i = 0; i < m_territories.Size(); ++i )
    {
        if( m_territories[i] == territoryId )
        {
            return true;
        }
    }
    return false;
}

void Team::RemoveTerritory( int territoryId )
{
    for( int i = 0; i < m_territories.Size(); ++i )
    {
        if( m_territories[i] == territoryId )
        {
            m_territories.RemoveData(i);
            m_numTerritories--;
            break;
        }
    }
}

void Team::AddTerritory( int territoryId )
{
    int territoriesPerTeam = g_app->GetGame()->GetOptionValue("TerritoriesPerTeam");

    if( m_numTerritories < territoriesPerTeam )
    {
        m_territories.PutData( territoryId );
        m_numTerritories++;
    }
}

void Team::AssignAITerritory()
{
    int territoriesPerTeam = g_app->GetGame()->GetOptionValue("TerritoriesPerTeam");

    for( int i = m_territories.Size(); i < territoriesPerTeam; ++i )
    {
        while(true)
        {
            int randTerritory = syncrand() % World::NumTerritories;
            if( g_app->GetWorld()->GetTerritoryOwner(randTerritory) == -1 )
            {
                AddTerritory(randTerritory);
                break;
            }
        }
    }
}

void Team::CreateFleet()
{
    Fleet *fleet = new Fleet();
    int index = m_fleets.Size();
    m_fleets.PutData(fleet);
    fleet->m_fleetId = index;
    fleet->m_teamId = m_teamId;
}


Fleet *Team::GetFleet( int fleetId )
{
    if( m_fleets.ValidIndex(fleetId) )
    {
        return m_fleets[fleetId];
    }
    else
    {
        return NULL;
    }
}


