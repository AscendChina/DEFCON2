#include "lib/universal_include.h"
   
#include <fstream>
#include <math.h>
#include <time.h>
#include <float.h>

#include "lib/hi_res_time.h"
#include "lib/gucci/input.h"
#include "lib/resource/resource.h"
#include "lib/resource/image.h"
#include "lib/resource/bitmap.h"
#include "lib/render/renderer.h"
#include "lib/profiler.h"
#include "lib/language_table.h"
#include "lib/math/random_number.h"
#include "lib/sound/soundsystem.h"
#include "lib/filesys/file_system.h"
#include "lib/filesys/text_stream_readers.h"
#include "lib/preferences.h"
#include "lib/string_utils.h"

#include "app/app.h"
#include "app/globals.h"
#include "app/game.h"
#include "app/statusicon.h"
#include "app/tutorial.h"

#include "network/Server.h"
#include "network/network_defines.h"

#include "world/world.h"
#include "world/explosion.h"
#include "world/silo.h"
#include "world/team.h"
#include "world/nuke.h"
#include "world/city.h"
#include "world/sub.h"
#include "world/radarstation.h"
#include "world/battleship.h"
#include "world/airbase.h"
#include "world/carrier.h"
#include "world/tornado.h"
#include "world/saucer.h"
#include "world/fleet.h"
#include "world/bomber.h"
#include "world/earthdata.h"

#include "interface/interface.h"

#include "renderer/map_renderer.h"


World::World()
:   m_myTeamId(-1),
    m_timeScaleFactor(20.0f),
    m_nextUniqueId(0),
    m_numNukesGivenToEachTeam(0)
{    
    m_populationCenter.Initialise(NumTerritories);
    m_firstLaunch.Initialise(MAX_TEAMS);

    for( int i = 0; i < MAX_TEAMS; ++i )
    {        
        m_firstLaunch[i] = false;
    }
    
    m_defconTime.Initialise(6);
    m_defconTime[5] = 0;
    m_defconTime[4] = 6;
    m_defconTime[3] = 12;
    m_defconTime[2] = 20;
    m_defconTime[1] = 30;
    m_defconTime[0] = -1;

    for( int i = 0; i < NumAchievementCities; ++i )
    {
        m_achievementCitiesNuked[i] = false;
    }

}

int World::GenerateUniqueId() 
{ 
    m_nextUniqueId++; 
    return m_nextUniqueId; 
}  

void World::Init()
{
    srand(time(NULL));
    AppSeedRandom( time(NULL) );
    syncrandseed();


    //
    // Initialise radar system

    m_radarGrid.Initialise( 1, MAX_TEAMS );
}


void World::LoadNodes()
{
    // create travel nodes

    Image *nodeImage = g_resource->GetImage( "earth/travel_nodes.bmp" );
    int pixelX = 0;
    int pixelY = 0;
    int numNodes = 0;
    for ( int x = 0; x < 800; ++x )
    {
        for( int y = 0; y < 400; ++y )
        {
            Colour col = nodeImage->GetColour( x, 400-y );
            if( col.m_r > 240 &&
                col.m_g > 240 &&
                col.m_b > 240 )
            {
                Node *node = new Node();

                Fixed width = 360;
                Fixed aspect = Fixed(600) / Fixed(800);
                Fixed height = (360 * aspect);

                Fixed longitude = width * (x-800) / 800 - width/2 + 360;
                Fixed latitude = height * (600-y) / 600 - 180;

                node->m_longitude = longitude;
                node->m_latitude = latitude;
                node->m_objectId = numNodes;
                ++numNodes;

                m_nodes.PutData( node );
            }
        }
    }

    // Go through all nodes in the world
    for( int i = 0; i < m_nodes.Size(); ++i)
    {
        for( int n = 0; n < m_nodes.Size(); ++n )
        {
            Node *startNode = m_nodes[n];
            for( int s = 0; s < m_nodes.Size(); ++s )
            {
                Node *targetNode = m_nodes[s];
                if( IsSailable( startNode->m_longitude, startNode->m_latitude, targetNode->m_longitude, targetNode->m_latitude ) )
                {
                    Fixed distance = GetDistance( startNode->m_longitude, startNode->m_latitude, targetNode->m_longitude, targetNode->m_latitude);
                    startNode->AddRoute( n, s, distance );

                    for( int e = 0; e < targetNode->m_routeTable.Size(); ++e )
                    {
                        Node *nextNode = m_nodes[ targetNode->m_routeTable[e]->m_targetNodeId ];
                        Fixed totalDistance = distance + targetNode->m_routeTable[e]->m_totalDistance;                                                           

                        startNode->AddRoute( nextNode->m_objectId, targetNode->m_objectId, totalDistance );
                    }
                }
            }
        }
    }

    for( int i = 0; i < m_nodes.Size(); ++i )
    {
        if( m_nodes[i]->m_routeTable.Size() != m_nodes.Size() )
        {
            AppReleaseAssert( false, "Node route size mismatch" );
        }
    }

    // load ai markers
    Image *aiMarkers = g_resource->GetImage( "earth/ai_markers.bmp" );
    for ( int x = 0; x < 512; ++x )
    {
        for( int y = 0; y < 285; ++y )
        {
            Colour col = aiMarkers->GetColour( x, y );
            if( col.m_r > 200 ||
                col.m_g > 200 )
            {
                Vector3<Fixed> *point = new Vector3<Fixed>;
                Fixed width = 360;
                Fixed height = 200;

                Fixed longitude = x * (width/aiMarkers->Width()) - 180;
                Fixed latitude = y * ( height/aiMarkers->Height()) - 100;

                point->x = longitude;
                point->y = latitude;

                if( col.m_r > 200 )
                {
                    m_aiTargetPoints.PutData( point );
                }
                else
                {
                    m_aiPlacementPoints.PutData( point );
                }
            }
        }
    }
}


Fixed World::GetGameScale()
{
    GameOption *option = g_app->GetGame()->GetOption( "WorldScale" );
    Fixed gameScale = Fixed::FromDouble(option->m_currentValue / option->m_default);
    return gameScale;
}



void World::AssignCities()
{
    //
    // Load city data from scratch

    g_app->GetEarthData()->LoadCities();

    //
    // Give territories to AI players
    SyncRandLog("Assign Cities numteams=%d", g_app->GetWorld()->m_teams.Size() );
    for( int i = 0; i < g_app->GetWorld()->m_teams.Size(); ++i )
    {
        Team *team = m_teams[i];

#ifdef NON_PLAYABLE
        if( team->m_type == Team::TypeLocalPlayer ||
            team->m_type == Team::TypeRemotePlayer )
        {
            team->m_type = Team::TypeAI;
            SyncRandLog( "Team %d set to type AI", i );
        }
        team->m_aiAssaultTimer = Fixed::FromDouble( 2400.0f ) + syncsfrand(900);
        SyncRandLog( "Team %d assault timer set to %2.4f", i, team->m_aiAssaultTimer.DoubleValue() );
        team->m_desiredGameSpeed = 20;
#endif

#ifdef TESTBED
        team->m_desiredGameSpeed = 20;
#endif

        team->m_unitCredits = 53 * g_app->GetGame()->GetOptionValue("TerritoriesPerTeam");

        if( !g_app->GetGame()->GetOptionValue("VariableUnitCounts") ) team->m_unitCredits = 999999;

        if( team->m_territories.Size() < g_app->GetGame()->GetOptionValue("TerritoriesPerTeam") )
        {
            if( !g_app->GetTutorial() )
            {
                SyncRandLog( "Team %d Assign territories", i );
                team->AssignAITerritory();
            }
        }
        if( team->m_aggression == 0 &&
            team->m_type == Team::TypeAI )
        {
            team->m_aggression = 6 + syncrand() % 5;
            SyncRandLog( "Team %d aggression set to %d", i, team->m_aggression );
        }
    }

    int territoriesPerTeam = g_app->GetGame()->GetOptionValue("TerritoriesPerTeam");
    int variableTeamUnits = g_app->GetGame()->GetOptionValue("VariableUnitCounts");
    Fixed worldScale = GetGameScale();

    if( variableTeamUnits == 0 )
    {
        for( int i = 0; i < WorldObject::NumObjectTypes; i++)
        {
            for( int j = 0; j < m_teams.Size(); ++j )
            {
                if( i == WorldObject::TypeAirBase )
                {
                    m_teams[j]->m_unitsAvailable[i] = (4 * territoriesPerTeam * worldScale).IntValue();
                }
                else if( i == WorldObject::TypeRadarStation )
                {
                    m_teams[j]->m_unitsAvailable[i] = (7 * territoriesPerTeam * worldScale).IntValue();
                }
                else if( i == WorldObject::TypeSilo )
                {
                    m_teams[j]->m_unitsAvailable[i] = (6 * territoriesPerTeam * worldScale).IntValue();
                }
                else if(i == WorldObject::TypeBattleShip ||
                        i == WorldObject::TypeCarrier ||
                        i == WorldObject::TypeSub )
                {
                    Fixed shipsAvailable = 12 * territoriesPerTeam * sqrt(worldScale*worldScale*worldScale);
                    // Round before truncating to int, to eliminate floating point error
                    m_teams[j]->m_unitsAvailable[i] = int( shipsAvailable.DoubleValue() + 0.5f );
                }

            }
        }
    }
    else
    {
        for( int i = 0; i < WorldObject::NumObjectTypes; i++)
        {
            for( int j = 0; j < m_teams.Size(); ++j )
            {
                m_teams[j]->m_unitCredits = 120 * g_app->GetGame()->GetOptionValue( "TerritoriesPerTeam" );
                if( i == WorldObject::TypeAirBase )
                {
                    m_teams[j]->m_unitsAvailable[i] = 8 * territoriesPerTeam;
                }
                else if( i == WorldObject::TypeRadarStation )
                {
                    m_teams[j]->m_unitsAvailable[i] = 12 * territoriesPerTeam;
                }
                else if( i == WorldObject::TypeSilo )
                {
                    m_teams[j]->m_unitsAvailable[i] = 10 * territoriesPerTeam;
                }
                else if(i == WorldObject::TypeBattleShip ||
                        i == WorldObject::TypeCarrier ||
                        i == WorldObject::TypeSub )
                {
                    m_teams[j]->m_unitsAvailable[i] = 18 * territoriesPerTeam;
                }
            }
        }
    }
    
    //
    // Work out how many nukes are given to each team in this game

    Team *team = m_teams[0];
    m_numNukesGivenToEachTeam = 0;
    m_numNukesGivenToEachTeam += team->m_unitsAvailable[WorldObject::TypeSilo] * 10;
    m_numNukesGivenToEachTeam += team->m_unitsAvailable[WorldObject::TypeAirBase] * 10;
    m_numNukesGivenToEachTeam += team->m_unitsAvailable[WorldObject::TypeCarrier] * 6;
    m_numNukesGivenToEachTeam += team->m_unitsAvailable[WorldObject::TypeSub] * 5;

    AppDebugOut( "Num nukes given to each team on start : %d\n", m_numNukesGivenToEachTeam );

    Fixed populations[MAX_TEAMS];
    int numCities[MAX_TEAMS];
    memset( numCities, 0, MAX_TEAMS * sizeof(int) );

    // Work out which territories the cities are in
    // Give population bias to capital cities
    for( int i = 0; i < g_app->GetEarthData()->m_cities.Size(); ++i )
    {
        City *city = g_app->GetEarthData()->m_cities[i];
        city->m_teamId = g_app->GetMapRenderer()->GetTerritory( city->m_longitude, city->m_latitude );        
    }

    // take the 20 most populated cities (or capitals) from each territory and discard the rest
    int cityLimit = g_app->GetGame()->GetOptionValue("CitiesPerTerritory");
    int density = g_app->GetGame()->GetOptionValue("PopulationPerTerritory") * 1000000;

    int lowestPop[World::NumTerritories];
    LList<City *> tempList[World::NumTerritories];
    for( int i = 0; i < World::NumTerritories; ++i )
    {
        lowestPop[i] = 999999999;
    }

    
    int randomCityPops = g_app->GetGame()->GetOptionValue( "CityPopulations" );    

    for( int i = 0; i < g_app->GetEarthData()->m_cities.Size(); ++i )
    {
        City *city = g_app->GetEarthData()->m_cities[i];
        if( city->m_teamId == -1 ) continue;

        int territoryId = g_app->GetMapRenderer()->GetTerritoryIdUnique( city->m_longitude, city->m_latitude );
        if( territoryId != -1 )
        {
            // Nearest city in this territory?
            bool tooNear = false;
            for( int j = 0; j < tempList[ territoryId ].Size(); ++j )
            {
                City *thisCity = tempList[territoryId][j];

                if( randomCityPops == 3 )
                {
                    Fixed population = thisCity->m_population;
                    population *= ( 1 + syncsfrand(1) );
                    thisCity->m_population = population.IntValue();                    
                    thisCity->m_capital = false;
                }

                Fixed distanceSqd = (Vector3<Fixed>(city->m_longitude, city->m_latitude,0) -
                                  Vector3<Fixed>(thisCity->m_longitude, thisCity->m_latitude,0)).MagSquared();

                if( distanceSqd < 2 * 2 )
                {
                    // Too near - discard lowest pop
                    if( (city->m_population > thisCity->m_population || city->m_capital) &&
                        !thisCity->m_capital )
                    {
                        tempList[territoryId].RemoveData(j);
                    }
                    else if( !city->m_capital )
                    {
                        tooNear = true;
                        break;
                    }
                }
            }
            
            if( tooNear ) continue;
            
            if( tempList[ territoryId ].Size() < cityLimit )
            {
                tempList[territoryId].PutData( city );
                if( city->m_population > lowestPop[ territoryId ] ||
                    city->m_capital )
                {
                    lowestPop[ territoryId ] = city->m_population;
                }
            }
            else
            {
                if( city->m_population > lowestPop[ territoryId ] ||
                    city->m_capital )
                {
                    for( int j = 0; j < tempList[ territoryId ].Size(); ++j )
                    {
                        if( tempList[ territoryId ][j]->m_population == lowestPop[ territoryId ] )
                        {
                            tempList[ territoryId ].RemoveData(j);
                            tempList[ territoryId ].PutData( city );
                            lowestPop[ territoryId ] = city->m_population;
                            break;
                        }
                    }
                }
            }

            // recalculate lowest population
            lowestPop[ territoryId ] = 999999999;
            for( int j = 0; j < tempList[ territoryId ].Size(); ++j )
            {
                if( tempList[territoryId][j]->m_population < lowestPop[territoryId] )
                {
                    lowestPop[territoryId] = tempList[territoryId][j]->m_population;
                }
            }

        }
    }

    m_cities.Empty();

    for( int i = 0; i < World::NumTerritories; ++i )
    {        
        Fixed totalLong = 0;
        Fixed totalLat = 0;
        for( int j = 0; j < tempList[i].Size(); ++j )
        {
            if( g_app->GetMapRenderer()->GetTerritory( tempList[i][j]->m_longitude, tempList[i][j]->m_latitude ) != -1 )
            {
                m_cities.PutData( tempList[i][j] );
                totalLong += tempList[i][j]->m_longitude;
                totalLat += tempList[i][j]->m_latitude;
            }
        }
        if( totalLong != 0 && totalLat != 0 )
        {
            Vector3<Fixed> center = Vector3<Fixed>( totalLong/tempList[i].Size(), totalLat/tempList[i].Size(), 0 );
            m_populationCenter[i] = center;
        }
    }

    // calculate the total population for each team
    // Add radar coverage
    // Multiply populations by a random factor if requested (to stop standard city distribution)

    Fixed targetPopulation = density * territoriesPerTeam;

    for( int i = 0; i < g_app->GetWorld()->m_teams.Size(); ++i )
    {
        Team *team = g_app->GetWorld()->m_teams[i];
        populations[i] = 0;

        for( int j = 0; j < m_cities.Size(); ++j )
        {
            City *city = m_cities[j];
            city->m_objectId = OBJECTID_CITYS + j;
            
            if( city->m_teamId == team->m_teamId )
            {
                if( randomCityPops )
                {
                    Fixed averageCityPop = targetPopulation / (cityLimit * territoriesPerTeam);
                    Fixed thisCityPop = averageCityPop;
                    if( randomCityPops == 2 || randomCityPops == 3 ) 
                    {
                        Fixed randomFactor = RandomNormalNumber( 0, 30 ).abs();  
                        thisCityPop = (thisCityPop * randomFactor).IntValue();                                   
                    }
                    city->m_population = thisCityPop.IntValue();   
                }

                populations[i] += city->m_population;
                m_radarGrid.AddCoverage( city->m_longitude, city->m_latitude, city->GetRadarRange(), city->m_teamId );
            }
        }
    }




    // scale populations

    for( int i = 0; i < g_app->GetWorld()->m_teams.Size(); ++i )
    {
        if( populations[i] != targetPopulation )
        {
            Team *team = g_app->GetWorld()->m_teams[i];
            Fixed scaleFactor = targetPopulation / populations[i];
            for( int j = 0; j < m_cities.Size(); ++j )
            {
                City *city = m_cities[j];
                if( city->m_teamId == team->m_teamId )
                {
                    Fixed population = city->m_population;
                    population *= scaleFactor;
                    city->m_population = population.IntValue();                    
                }
            }
        }
    }

    // recalc total populations for debug.log
    for( int i = 0; i < g_app->GetWorld()->m_teams.Size(); ++i )
    {
        populations[i] = 0;
        numCities[i] = 0;
        for( int j = 0; j < m_cities.Size(); ++j )
        {
            Team *team = g_app->GetWorld()->m_teams[i];
            City *city = m_cities[j];
            if( city->m_teamId == team->m_teamId )
            {
                populations[i] += city->m_population;
                numCities[i] ++;
            }
        }
    }

    for( int i = 0; i < m_cities.Size(); ++i )
    {
        m_populationTotals.PutData( m_cities[i]->m_population );
    }

    for( int i = 0; i < g_app->GetWorld()->m_teams.Size(); ++i )
    {
        Fixed population = populations[i] / Fixed(1000000);
        AppDebugOut( "Population of Team #%d : %.1f Million spread over %d cities\n", i, population.DoubleValue(), numCities[i] );
    }
 
    //if( IsSpectating( g_app->GetClientToServer()->m_clientId ) != -1 )
    //{
    //    g_app->GetMapRenderer()->m_renderEverything = true;
    //}
}


Colour World::GetAllianceColour( int _allianceId )
{
    Colour s_alliances[] = { 
                                Colour(200, 50, 50  ),
                                Colour(50,  200,50  ),
                                Colour(50,  50, 255 ),
                                Colour(200, 200,50  ),
                                Colour(255, 150,0   ),
                                Colour(0,   200,200 )
                            };
        
    AppDebugAssert( _allianceId >= 0 && _allianceId < 6 );
    return s_alliances[_allianceId];
}


char *World::GetAllianceName( int _allianceId )
{
    AppDebugAssert( _allianceId >= 0 && _allianceId < 6 );

	switch( _allianceId )
	{
		case 0:
			return LANGUAGEPHRASE("dialog_alliance_0");
			break;
		case 1:
			return LANGUAGEPHRASE("dialog_alliance_1");
			break;
		case 2:
			return LANGUAGEPHRASE("dialog_alliance_2");
			break;
		case 3:
			return LANGUAGEPHRASE("dialog_alliance_3");
			break;
		case 4:
			return LANGUAGEPHRASE("dialog_alliance_4");
			break;
		case 5:
			return LANGUAGEPHRASE("dialog_alliance_5");
			break;
	}

    return LANGUAGEPHRASE("unknown");
}


int World::CountAllianceMembers ( int _allianceId )
{
    int count = 0;

    for( int t = 0; t < g_app->GetWorld()->m_teams.Size(); ++t )
    {
        Team *team = m_teams[t];
        if( team->m_allianceId == _allianceId )
        {
            ++count;
        }
    }

    return count;
}


int World::FindFreeAllianceId()
{
    for( int a = 0; a < 6; ++a )
    {
        if( CountAllianceMembers(a) == 0 )
        {
            return a;
        }
    }

    return -1;
}


void World::RequestAlliance( int teamId, int allianceId )
{
    Team *team = GetTeam(teamId);
    if( team )
    {
        team->m_allianceId = allianceId;
    }
}


bool World::IsFriend( int _teamId1, int _teamId2 )
{
    Team *team1 = GetTeam(_teamId1);
    Team *team2 = GetTeam(_teamId2);
    
    return( team1 && 
            team2 &&
            team1->m_allianceId == team2->m_allianceId );
}


void World::InitialiseTeam ( int teamId, int teamType, int clientId )
{
    //
    // If this client is a spectator and they now want to join,
    // remove their spectator status

    if( teamType != Team::TypeAI &&
        IsSpectating(clientId) != -1 )
    {
        RemoveSpectator(clientId);
    }


    Team *team = new Team();
    m_teams.PutData( team );

    team->SetTeamType( teamType );
    team->SetTeamId( teamId );
    team->m_allianceId = FindFreeAllianceId();
    team->m_clientId = clientId;
    team->m_readyToStart = false;
 
    if( teamType == Team::TypeLocalPlayer )
    {
        m_myTeamId = teamId;
    }    

    char teamName[256];
	strcpy( teamName, LANGUAGEPHRASE("dialog_c2s_default_name_player") );
	LPREPLACEINTEGERFLAG( 'T', teamId, teamName );
    team->SetTeamName( teamName );

    if( teamType == Team::TypeAI )
    {
        team->m_desiredGameSpeed = 20;
    }
    else
    {
        team->m_desiredGameSpeed = 5;
    }

    team->m_aiActionTimer = syncfrand( team->m_aiActionSpeed );
    team->m_aiAssaultTimer = int(5400 + ( 200 * team->m_aggression * ( syncrand() % 5 + 1)));
    
    if( g_app->GetGame()->GetOptionValue("GameMode") == GAMEMODE_SPEEDDEFCON )
    {
        team->m_aiAssaultTimer *= Fixed::Hundredths(50);
    }
}


void World::InitialiseSpectator( int _clientId )
{
    //
    // Kill all his teams first

    RemoveTeams( _clientId, Disconnect_ClientLeave );


    //
    // Register as spectator

    Spectator *spec = new Spectator();
    strcpy( spec->m_name, LANGUAGEPHRASE("dialog_c2s_default_name_spectator") );
    spec->m_clientId = _clientId;

    m_spectators.PutData( spec );

    if( _clientId == g_app->GetClientToServer()->m_clientId )
    {
        //g_app->GetMapRenderer()->m_renderEverything = true;
        m_myTeamId = -1;
    }

    char msg[256];
    sprintf( msg, LANGUAGEPHRASE("dialog_world_new_spec_connecting") );
    AddChatMessage( _clientId, 100, msg, -1, false );
}


bool World::AmISpectating()
{
    return( IsSpectating(g_app->GetClientToServer()->m_clientId) != -1 );
}


int World::IsSpectating( int _clientId )
{
    for( int i = 0; i < m_spectators.Size(); ++i )
    {
        Spectator *spec = m_spectators[i];
        if( spec->m_clientId == _clientId )
        {
            return i;
        }
    }

    return -1;
}


Spectator *World::GetSpectator( int _clientId )
{
    int specIndex = IsSpectating( _clientId );
    if( specIndex == -1 ) return NULL;
    
    if( !m_spectators.ValidIndex(specIndex) ) return NULL;

    return m_spectators[specIndex];
}


void World::ReassignTeams( int _clientId )
{
    // Looks for teams that are assigned to this client,
    // but have been taken over by AI control

    for( int i = 0; i < m_teams.Size(); ++i )
    {
        Team *team = m_teams[i];
        if( team->m_type == Team::TypeAI &&
            team->m_clientId == _clientId )
        {           
            if( _clientId == g_app->GetClientToServer()->m_clientId )
            {
                team->SetTeamType( Team::TypeLocalPlayer );
            }
            else
            {
                team->SetTeamType( Team::TypeRemotePlayer );
            }

            AppDebugOut( "Team %s rejoined\n", team->m_name ); 

            char msg[256];
            sprintf( msg, LANGUAGEPHRASE("dialog_world_rejoined") );
			LPREPLACESTRINGFLAG( 'T', team->m_name, msg );
            strupr(msg);
            g_app->GetInterface()->ShowMessage( 0, 0, -1, msg, true );
        }
    }
}


void World::RemoveTeams( int clientId, int reason )
{
    //
    // Remove all of his teams

    for( int i = 0; i < m_teams.Size(); ++i )
    {
        Team *team = m_teams[i];
        if( team->m_type != Team::TypeUnassigned &&
            team->m_type != Team::TypeAI &&
            team->m_clientId == clientId )
        {
            if( g_app->m_gameRunning )
            {
                char msg[256];
                
                switch( reason )
                {
                    case Disconnect_ClientLeave:
						sprintf( msg, LANGUAGEPHRASE("dialog_world_disconnected") );
						LPREPLACESTRINGFLAG( 'T', team->m_name, msg );
                        break;

                    case Disconnect_ClientDrop:
						sprintf( msg, LANGUAGEPHRASE("dialog_world_dropped") );
						LPREPLACESTRINGFLAG( 'T', team->m_name, msg );
                        break;

                    case Disconnect_InvalidKey:
                    case Disconnect_DuplicateKey:
                    case Disconnect_KeyAuthFailed:
                    case Disconnect_BadPassword:
                    case Disconnect_KickedFromGame:
						sprintf( msg, LANGUAGEPHRASE("dialog_world_kicked") );
						LPREPLACESTRINGFLAG( 'T', team->m_name, msg );
                        break;
                }

                strupr(msg);
                g_app->GetInterface()->ShowMessage( 0, 0, -1, msg, true );
            }

            char msg[256];
			sprintf( msg, LANGUAGEPHRASE("dialog_world_team_left_game") );
			LPREPLACESTRINGFLAG( 'T', team->m_name, msg );
            AddChatMessage( -1, 100, msg, -1, false );
            
            if( g_app->m_gameRunning )
            {
                // Game In Progress : Replace the team with AI   
                AppDebugOut( "Team %d replaced with AI\n", team->m_teamId );
                team->SetTeamType( Team::TypeAI );
            }
            else
            {
                // We're in the lobby, so remove the team
                AppDebugOut( "Team %d removed\n", team->m_teamId );
                RemoveTeam( team->m_teamId );
			}            
        }
		else
			m_teams[i]->m_readyToStart = false;
    }
}


void World::RemoveSpectator( int clientId )
{
    int specIndex = IsSpectating( clientId );
    if( specIndex != -1 )
    {
        Spectator *spec = m_spectators[specIndex];

        char msg[256];
		sprintf( msg, LANGUAGEPHRASE("dialog_world_spec_left_game") );
		LPREPLACESTRINGFLAG( 'T', spec->m_name, msg );
        AddChatMessage( -1, 100, msg, -1, false );

        m_spectators.RemoveData(specIndex);
        delete spec;
    }
}

void World::RemoveAITeam( int _teamId )
{
    Team *team = GetTeam( _teamId );
    if( team && team->m_type == Team::TypeAI )
    {
        RemoveTeam(_teamId);
    }

    
    if( g_app->GetServer() && 
        g_app->GetServer()->m_teams.ValidIndex(_teamId) )
    {
        ServerTeam *serverTeam = g_app->GetServer()->m_teams[_teamId];
        g_app->GetServer()->m_teams.RemoveData(_teamId);
        delete serverTeam;
    }
}

void World::RemoveTeam( int _teamId )
{
    for( int i = 0; i < m_teams.Size(); ++i )
    {
        Team *team = m_teams[i];
        if( team->m_teamId == _teamId )
        {
            m_teams.RemoveData(i);
            delete team;
        }
    }
}


void World::RandomObjects( int teamId )
{

    float startTime = GetHighResTime();
    Team *team = GetTeam( teamId );

    //
    // Gimme some objects
    if( team->m_type == Team::TypeAI )
    {
        for( int i = 0; i < 10; ++i )
        {
            Silo *silo1 = new Silo();
            silo1->SetTeamId(teamId);
            AddWorldObject( silo1 );

            while( true )
            {
                Fixed longitude = syncsfrand(360);
                Fixed latitude = syncsfrand(180);
                if( IsValidPlacement( teamId, longitude, latitude, WorldObject::TypeSilo ) )
                {
                    silo1->SetPosition( longitude, latitude );
                    break;
                }
            }
        }


        for( int i = 0; i < 10; ++i )
        {
            Sub *sub = new Sub();
            sub->SetTeamId(teamId);
            AddWorldObject( sub);

            while( true )
            {
                Fixed longitude = syncsfrand(360);
                Fixed latitude = syncsfrand(180);
                if( IsValidPlacement( teamId, longitude, latitude, WorldObject::TypeSub ) )
                {
                    sub->SetPosition( longitude, latitude );
                    break;
                }
            }
        }
    

        for( int i = 0; i < 10; ++i )
        {
            BattleShip *ship = new BattleShip();
            ship->SetTeamId(teamId);
            AddWorldObject(ship);

            while( true )
            {
                Fixed longitude = syncsfrand(360);
                Fixed latitude = syncsfrand(180);
                if( IsValidPlacement( teamId, longitude, latitude, WorldObject::TypeBattleShip ) )
                {
                    ship->SetPosition( longitude, latitude );
                    break;
                }
            }
        }

        for( int i = 0; i < 10; ++i )
        {
            Carrier *carrier = new Carrier();
            carrier->SetTeamId(teamId);
            AddWorldObject(carrier);

            while( true )
            {
                Fixed longitude = syncsfrand(360);
                Fixed latitude = syncsfrand(180);
                if( IsValidPlacement( teamId, longitude, latitude, WorldObject::TypeCarrier ) )
                {
                    carrier->SetPosition( longitude, latitude );
                    break;
                }
            }
        }


        for( int i = 0; i < 10; ++i )
        {
            RadarStation *station = new RadarStation();
            station->SetTeamId(teamId);
            AddWorldObject(station);

            while( true )
            {
                Fixed longitude = syncsfrand(360);
                Fixed latitude = syncsfrand(180);
                if( IsValidPlacement( teamId, longitude, latitude, WorldObject::TypeRadarStation ) )
                {
                    station->SetPosition( longitude, latitude );
                    break;
                }
            }
        }


        for( int i = 0; i < 10; ++i )
        {
            AirBase *airBase = new AirBase();
            airBase->SetTeamId(teamId);
            AddWorldObject(airBase);

            while( true )
            {
                Fixed longitude = syncsfrand(360);
                Fixed latitude = syncsfrand(180);
                if( IsValidPlacement( teamId, longitude, latitude, WorldObject::TypeAirBase ) )
                {
                    airBase->SetPosition( longitude, latitude );
                    break;
                }
            }
        }

    }
    float totalTime = GetHighResTime() - startTime;
    AppDebugOut( "Placed objects in %dms", int(totalTime * 1000.0f) );
}

void World::LaunchNuke( int teamId, int objId, Fixed longitude, Fixed latitude, Fixed range )
{
    WorldObject *from = GetWorldObject(objId);
    AppDebugAssert( from );

    Nuke *nuke = new Nuke();
    nuke->m_teamId = teamId;
    nuke->m_longitude = from->m_longitude;
    nuke->m_latitude = from->m_latitude;
    //nuke->m_range = range;
    if ((from->m_type == WorldObject::TypeSub) && (from->m_currentState == 3 ))
	nuke->SetMirv();

    nuke->SetWaypoint( longitude, latitude );
    nuke->LockTarget();
    
    AddWorldObject( nuke );

    Fixed fromLongitude = from->m_longitude;
    Fixed fromLatitude = from->m_latitude;
    if( from->m_fleetId != -1 )
    {
        Fleet *fleet = GetTeam( from->m_teamId )->GetFleet( from->m_fleetId );
        if( fleet )
        {
            fromLongitude = fleet->m_longitude;
            fromLatitude = fleet->m_latitude;
        }
    }

   // bool silentNukeLaunch = ( from->m_type == WorldObject::TypeBomber );
	bool silentNukeLaunch = ( from->m_type == WorldObject::TypeBomber || from->m_type == WorldObject::TypeNuke );

    if( !silentNukeLaunch )
    {
        if( teamId != m_myTeamId )
        {
            bool messageFound = false;
            for( int i = 0; i < g_app->GetWorld()->m_messages.Size(); ++i )
            {
                WorldMessage *wm = g_app->GetWorld()->m_messages[i];
                if( wm->m_messageType == WorldMessage::TypeLaunch &&
                    GetDistanceSqd( wm->m_longitude, wm->m_latitude, fromLongitude, fromLatitude ) < 10 * 10 )
                {                    
                    wm->SetMessage(LANGUAGEPHRASE("message_multilaunch"));
                    
                    wm->m_timer += 3;
                    if( wm->m_timer > 20 )
                    {
                        wm->m_timer = 20;
                    }
                    wm->m_renderFull = true;
                    messageFound = true;
                    break;
                }
            }
            if( !messageFound )
            {
                char msg[64];
                sprintf( msg, "%s", LANGUAGEPHRASE("message_nukelaunch") );
                AddWorldMessage( fromLongitude, fromLatitude, teamId, msg, WorldMessage::TypeLaunch );
                int id = g_app->GetMapRenderer()->CreateAnimation( MapRenderer::AnimationTypeNukePointer, objId,
																   from->m_longitude.DoubleValue(), from->m_latitude.DoubleValue() );
                if( g_app->GetMapRenderer()->m_animations.ValidIndex( id ) )
                {
                    NukePointer *pointer = (NukePointer *)g_app->GetMapRenderer()->m_animations[id];
                    if( pointer->m_animationType == MapRenderer::AnimationTypeNukePointer )
                    {
                        pointer->m_targetId = from->m_objectId;
                        pointer->Merge();
                    }
                }
            }

            if( g_app->m_hidden )
            {
                g_app->GetStatusIcon()->SetSubIcon( STATUS_ICON_NUKE );
                g_app->GetStatusIcon()->SetCaption( LANGUAGEPHRASE("tray_icon_nuke_launches_detected") );
            }

            if( m_firstLaunch[ teamId ] == false )
            {
                m_firstLaunch[ teamId ] = true;
                char msg[128];
                sprintf( msg, "%s", LANGUAGEPHRASE("message_firstlaunch") );
                LPREPLACESTRINGFLAG('T', GetTeam(teamId)->GetTeamName(), msg );
                strupr(msg);
                g_app->GetInterface()->ShowMessage( 0, 0, teamId, msg, true );
                g_soundSystem->TriggerEvent( "Interface", "FirstLaunch" );

            }
        }

        for( int i = 0; i < g_app->GetWorld()->m_teams.Size(); ++i )
        {
            if( m_teams[i]->m_type == Team::TypeAI )
            {
                m_teams[i]->AddEvent( Event::TypeNukeLaunch, objId, from->m_teamId, -1, from->m_longitude, from->m_latitude );
            }
        }
    }
}

int World::GetNearestObject( int teamId, Fixed longitude, Fixed latitude, int objectType, bool enemyTeam )
{
    int result = -1;
    Fixed nearestSqd = Fixed::MAX;

    for( int i = 0; i < m_objects.Size(); ++i )
    {
        if( m_objects.ValidIndex(i) )
        {
            WorldObject *obj = m_objects[i];
            bool isFriend = IsFriend( obj->m_teamId, teamId );

            if( ( (isFriend && !enemyTeam) || (!isFriend && enemyTeam )) &&
                ( objectType == -1 || obj->m_type == objectType) &&
                ( (enemyTeam && obj->m_visible[teamId] ) || !enemyTeam ) &&
                !GetTeam( teamId )->m_ceaseFire[obj->m_teamId])
            {
                Fixed distanceSqd = GetDistanceSqd( longitude, latitude, obj->m_longitude, obj->m_latitude);
                if( distanceSqd < nearestSqd )
                {
                    result = obj->m_objectId;
                    nearestSqd = distanceSqd;
                }
            }
        }
    }

    return result;
}

bool World::IsValidPlacement( int teamId, Fixed longitude, Fixed latitude, int objectType )
{
    if( teamId == -1 ) return false;

    switch( objectType )
    {
        case WorldObject::TypeSilo:      
        case WorldObject::TypeRadarStation:
        case WorldObject::TypeAirBase:
        {
            if( g_app->GetMapRenderer()->IsValidTerritory( teamId, longitude, latitude, false ) )
            {
                int nearestIndex = GetNearestObject( teamId, longitude, latitude );
                if( nearestIndex == -1 ) return true;
                else
                {
                    WorldObject *obj = GetWorldObject(nearestIndex);
                    Fixed distance = GetDistance( longitude, latitude, obj->m_longitude, obj->m_latitude);
                    Fixed maxDistance = 5 / GetGameScale();
                    return ( distance > maxDistance );                    
                }
            }
            break;
        }

        case WorldObject::TypeSub:
        case WorldObject::TypeBattleShip:
        case WorldObject::TypeCarrier:
        {
            if( g_app->GetMapRenderer()->IsValidTerritory( teamId, longitude, latitude, true ) )
            {
                int nearestIndex = GetNearestObject( teamId, longitude, latitude );
                if( nearestIndex == -1 ) return true;
                else
                {
                    WorldObject *obj = GetWorldObject(nearestIndex);
                    Fixed distance = GetDistance( longitude, latitude, obj->m_longitude, obj->m_latitude);
                    Fixed maxDistance = 1 / GetGameScale();
                    return ( distance > maxDistance );
                }
            }
            break;
        }
        case WorldObject::TypeFighter:
        case WorldObject::TypeBomber:
        {
            return true;
        }
        break;
    }


    return false;
}

Team *World::GetTeam( int teamId )
{
    for( int i = 0; i < m_teams.Size(); ++i )
    {
        Team *team = m_teams[i];
        if( team->m_teamId == teamId )
        {
            return team;
        }
    }

    return NULL;
}


Team *World::GetMyTeam()
{
    return GetTeam( m_myTeamId );
}


int World::AddWorldObject( WorldObject *obj )
{
    Team *team = GetTeam( obj->m_teamId );
    if( team )
    {
        team->m_unitsInPlay[ obj->m_type ]++;
    }
    m_objects.PutData( obj);
    obj->m_objectId = GenerateUniqueId();
    return obj->m_objectId;
}

WorldObject *World::GetWorldObject( int _uniqueId )
{
    if( _uniqueId == -1 )
    {
        return NULL;
    }


    if( _uniqueId >= OBJECTID_CITYS )
    {
        return m_cities[_uniqueId - OBJECTID_CITYS];
    }
    
    
    for( int i = 0; i < m_objects.Size(); ++i )
    {
        if( m_objects.ValidIndex(i) )
        {
            WorldObject *obj = m_objects[i];
            if( obj->m_objectId == _uniqueId )
            {
                return obj;
            }
        }
    }
    return NULL;
}

void World::Shutdown()
{
    // TODO : destruct the world object
}

void World::CreateExplosion ( int teamId, Fixed longitude, Fixed latitude, Fixed intensity, int targetTeamId )
{
    Explosion *explosion = new Explosion();
    explosion->SetTeamId( teamId );
    explosion->SetPosition( longitude, latitude );
    explosion->m_intensity = intensity;
    explosion->m_targetTeamId = targetTeamId;

    int expId = m_explosions.PutData( explosion );
    explosion->m_objectId = expId;
    

    //
    // Destroy any military objects

    if( intensity > 10 )
    {
        for( int i = 0; i < m_objects.Size(); ++i )
        {
            if( m_objects.ValidIndex(i) )
            {
                WorldObject *wobj = m_objects[i];

                if( wobj->m_type != WorldObject::TypeNuke &&
                    wobj->m_type != WorldObject::TypeExplosion &&
                    wobj->m_life > 0 &&
                    GetDistance( longitude, latitude, wobj->m_longitude, wobj->m_latitude) <= intensity/50 )
                {
                    if( wobj->IsMovingObject() ) 
                    {
                        wobj->m_life = 0;

						wobj->m_lastHitByTeamId = teamId;
                    }
                    else
                    {
                        int damageDone = 10;
                        wobj->m_life -= damageDone;
                        wobj->m_life = max( wobj->m_life, 0 );
						wobj->m_lastHitByTeamId = teamId;
                        wobj->NukeStrike();
                    }

                    if( !wobj->IsMovingObject() &&
                        wobj->m_life <= 0 )
                    {
                        AddDestroyedMessage( expId, wobj->m_objectId, true );

                    }
                }
            }
        }

        if( intensity > 50 )
        {
            Vector3<Fixed> *pos = new Vector3<Fixed>();
            pos->x = longitude;
            pos->y = latitude;
            m_radiation.PutData(pos);
        }
    
        //
        // Kill people living in cities

        bool directHitPossible = true;
        for( int i = 0; i < m_cities.Size(); ++i )
        {
            City *city = m_cities[i];
            Fixed range = GetDistance( longitude, latitude, city->m_longitude, city->m_latitude);
        
            if( range <= intensity/50 )
            {
                bool directHit = city->NuclearStrike( teamId, intensity, range, directHitPossible );
                if( directHit ) directHitPossible = false;
            }
        }
    }
}


void World::ObjectPlacement( int teamId, int unitType, Fixed longitude, Fixed latitude, int fleetId)
{
	if( g_app->GetWorld()->IsValidPlacement( teamId, longitude, latitude, unitType ) )    
	{  		    
        Team *team = g_app->GetWorld()->GetTeam(teamId);
        if( team && team->m_unitsAvailable[unitType] )
        {
		    team->m_unitsAvailable[unitType]--;
            team->m_unitCredits -= GetUnitValue( unitType );

	        WorldObject *newUnit = WorldObject::CreateObject( unitType );
		    newUnit->SetTeamId(teamId);
		    AddWorldObject( newUnit ); 
		    newUnit->SetPosition( longitude, latitude );
            if( fleetId != 255 )
            {
                if( team->m_fleets.ValidIndex( fleetId ) )
                {
                    team->m_fleets[fleetId]->m_fleetMembers.PutData( newUnit->m_objectId );
                    newUnit->m_fleetId = fleetId;
                }
            }
        }
	}    
}


void World::ObjectStateChange( int objectId, int newState )
{
    WorldObject *wobj = GetWorldObject(objectId);
    if( wobj)
    {
        wobj->SetState( newState );
    }
}

void World::ObjectAction( int objectId, int targetObjectId, Fixed longitude, Fixed latitude, bool pursue )
{
    if( latitude > 100 || latitude < -100 ) 
    {
        return;
    }

    WorldObject *wobj = GetWorldObject(objectId);
    if( wobj )
    {
        WorldObject *targetObject = GetWorldObject(targetObjectId);
        if( !targetObject )
        {
            targetObjectId = -1;
        }
        //wobj->Action( targetObjectId, longitude, latitude );
        ActionOrder *action = new ActionOrder();
        action->m_longitude = longitude;
        action->m_latitude = latitude;
        action->m_targetObjectId = targetObjectId;
        action->m_pursueTarget = pursue;

        if( targetObject &&
            IsFriend(wobj->m_teamId, targetObject->m_teamId) )
        {
            action->m_pursueTarget = false;
        }

        wobj->RequestAction( action );

        //
        // If that was the last possible action deselect this unit now
        // (assuming we have it selected)

        if( g_app->GetMapRenderer()->GetCurrentSelectionId() == objectId )
        {
            int numTimesRemaining = wobj->m_states[wobj->m_currentState]->m_numTimesPermitted;
            if( numTimesRemaining > -1 ) numTimesRemaining -= wobj->m_actionQueue.Size();

            if( numTimesRemaining <= 0)
            {
                g_app->GetMapRenderer()->SetCurrentSelectionId(-1);
            }
        }
    }
}

void World::ObjectSetWaypoint  ( int objectId, Fixed longitude, Fixed latitude )
{
    WorldObject *wobj = GetWorldObject(objectId);
    if( wobj && wobj->IsMovingObject() )
    {
        MovingObject *mobj = (MovingObject *) wobj;
        mobj->SetWaypoint( longitude, latitude );
    }
}


void World::ObjectClearActionQueue( int objectId )
{
    WorldObject *wobj = GetWorldObject(objectId);
    if( wobj ) wobj->ClearActionQueue();
}

void World::ObjectClearLastAction( int objectId )
{
    WorldObject *wobj = GetWorldObject(objectId);
    if( wobj ) wobj->ClearLastAction();
}


void World::ObjectSpecialAction( int objectId, int targetObjectId, int specialActionType )
{
    WorldObject *wobj = GetWorldObject(objectId);

    if( wobj )
    {
        switch( specialActionType )
        {
            case SpecialActionLandingAircraft:
                if( wobj->IsMovingObject() )
                {
                    MovingObject *mobj = (MovingObject *) wobj;
                    mobj->Land( targetObjectId );
                }
                break;

            /*case SpecialActionFormingSquad:
                if( wobj->IsMovingObject() )
                {
                    MovingObject *mobj = (MovingObject *) wobj;
                    mobj->AddToSquad( targetObjectId );
                }
                break;*/
        }
    }
}

void World::FleetSetWaypoint( int teamId, int fleetId, Fixed longitude, Fixed latitude )
{    
    Team *team = GetTeam(teamId);
    if( team )
    {
        Fleet *fleet = team->GetFleet( fleetId );
        if( fleet )
        {
            fleet->MoveFleet( longitude, latitude );
        }
    }
}

void World::TeamCeaseFire( int teamId, int targetTeam )
{
    bool currentState = GetTeam(teamId)->m_ceaseFire[targetTeam];
    if( !currentState )
    {
        for( int i = 0; i < m_objects.Size(); ++i )
        {
            if( m_objects.ValidIndex(i) )
            {
                WorldObject *obj = m_objects[i];
                if( obj->m_teamId == teamId )
                {
                    obj->CeaseFire( targetTeam );
                }
            }
        }
    }
    GetTeam( teamId )->m_ceaseFire[ targetTeam ] = !currentState;
}

void World::TeamShareRadar( int teamId, int targetTeam )
{
    if( GetTeam( teamId ) && GetTeam( targetTeam ) )
    {
        GetTeam( teamId )->m_sharingRadar[targetTeam] = !GetTeam( teamId )->m_sharingRadar[targetTeam ];
    }
}

void World::AssignTerritory( int territoryId, int teamId, int addOrRemove )
{
    if( GetTeam( teamId ) )
    {
        int owner = GetTerritoryOwner( territoryId );

        if( owner == -1 )
        {
            if( addOrRemove == 0 || addOrRemove == 1 )
            {
                GetTeam(teamId)->AddTerritory(territoryId);
            }
        }
        else if( owner == teamId )
        {
            if( addOrRemove == 0 || addOrRemove == -1 )
            {
                GetTeam(teamId)->RemoveTerritory( territoryId );
            }
        }
    }
}

void World::WhiteBoardAction( int teamId, int action, int pointId, Fixed longitude, Fixed latitude, Fixed longitude2, Fixed latitude2 )
{
    if( GetTeam( teamId ) )
	{
		m_whiteBoards[ teamId ].ReceivedAction( (WhiteBoard::Action) action, 
                                                 pointId, 
                                                 longitude.DoubleValue(), 
                                                 latitude.DoubleValue(), 
                                                 longitude2.DoubleValue(), 
                                                 latitude2.DoubleValue() );
	}
}

void World::AddWorldMessage( Fixed longitude, Fixed latitude, int teamId, char *msg, int msgType, bool showOnToolbar )
{
    if( showOnToolbar )
    {
        g_app->GetInterface()->ShowMessage( longitude, latitude, teamId, msg );
    }

    WorldMessage *wm = new WorldMessage();
    wm->m_longitude = longitude;
    wm->m_latitude = latitude;
    wm->m_teamId = teamId;
    wm->SetMessage( msg );
    wm->m_timer = 3;
    wm->m_messageType = msgType;
    m_messages.PutData( wm );
}

void World::AddWorldMessage( int objectId, int teamId, char *msg, int msgType, bool showOnToolbar )
{
    if( showOnToolbar )
    {
        if( GetWorldObject(objectId) )
        {
            WorldObject *obj = GetWorldObject(objectId);            
            g_app->GetInterface()->ShowMessage( obj->m_longitude, obj->m_latitude, teamId, msg );
        }
    }

    WorldObject *obj = GetWorldObject(objectId);
    if( obj )
    {
        WorldMessage *wm = new WorldMessage();
        wm->m_objectId = objectId;
        wm->m_longitude = obj->m_longitude;
        wm->m_latitude = obj->m_latitude;
        wm->m_teamId = teamId;
        wm->SetMessage( msg );
        wm->m_timer = 3;
        wm->m_messageType = msgType;
        m_messages.PutData( wm );
    }
}

void World::AddDestroyedMessage( int attackerId, int defenderId, bool explosion )
{    
    WorldObject *attacker = NULL;
    WorldObject *defender = GetWorldObject( defenderId );

    if( explosion )
    {
        attacker = m_explosions[attackerId];
    }
    else
    {
        attacker = GetWorldObject( attackerId );
    }

    if( attacker && defender )
    {
        char msg[1024];
        if( defender->m_teamId != TEAMID_SPECIALOBJECTS &&
            attacker->m_teamId != TEAMID_SPECIALOBJECTS )
        {
            sprintf( msg, "%s", LANGUAGEPHRASE("message_destroyed") );

			if( g_languageTable->DoesFlagExist( 'V', msg ) )
			{
				LPREPLACESTRINGFLAG( 'V', GetTeam( defender->m_teamId )->GetTeamName(), msg );
			}
			if( g_languageTable->DoesFlagExist( 'T', msg ) )
			{
				LPREPLACESTRINGFLAG( 'T', GetTeam( attacker->m_teamId )->GetTeamName(), msg );
			}
			if( g_languageTable->DoesFlagExist( 'U', msg ) )
			{
				LPREPLACESTRINGFLAG( 'U', WorldObject::GetName(defender->m_type), msg );
			}
			if( g_languageTable->DoesFlagExist( 'A', msg ) )
			{
				LPREPLACESTRINGFLAG( 'A', WorldObject::GetName(attacker->m_type), msg );
			}
        }

        AddWorldMessage( defender->m_longitude, defender->m_latitude, attacker->m_teamId, msg, WorldMessage::TypeDestroyed );
    }
}

void World::AddOutOfFueldMessage( int objectId )
{
    return;
    
    WorldObject *object = GetWorldObject( objectId );
    if( object )
    {
        if( object->m_teamId != TEAMID_SPECIALOBJECTS )
        {
            if( object->m_teamId == m_myTeamId )
            {
                char msg[256];
                sprintf( msg, "%s %s %s", 
                              GetTeam( object->m_teamId )->GetTeamName(), 
                              WorldObject::GetName(object->m_type),
                              LANGUAGEPHRASE("message_fuel"));
                AddWorldMessage( object->m_longitude, object->m_latitude, object->m_teamId, msg, WorldMessage::TypeFuel );
            }
        }
        else
        {
            if( object->m_type == WorldObject::TypeTornado )
            {
                char msg[256];
                sprintf( msg, "%s", LANGUAGEPHRASE("message_tornadodeath" ) );
                AddWorldMessage( object->m_longitude, object->m_latitude, object->m_teamId, msg, WorldMessage::TypeFuel );
            }
        }
    }
}


void World::Update()
{
    START_PROFILE( "World Update" );

    int trackSyncRand = g_preferences->GetInt( PREFS_NETWORKTRACKSYNCRAND );


    //
    // Update the time

    int speedDesired = 999;

    int speedOption = g_app->GetGame()->GetOptionValue("GameSpeed");

    switch( speedOption )
    {
        case 0:
            for( int i = 0; i < m_teams.Size(); ++i )
            {
                if( m_teams[i]->m_type != Team::TypeAI &&
                    m_teams[i]->m_desiredGameSpeed < speedDesired )
                {
                    speedDesired = m_teams[i]->m_desiredGameSpeed;
                }
            }
            break;
    
        case 1:     speedDesired = 1;           break;
        case 2:     speedDesired = 5;           break;
        case 3:     speedDesired = 10;          break;
       // case 4:     speedDesired = 20;          break; phuck
		case 4:     speedDesired = 30;          break;
    }
        
    if( speedDesired == 999 )
    {
        speedDesired = 20;
    }


    //
    // Its defcon 4 or 5, and the server has requested 20x speed
    // but thats too fast to place stuff

    if( speedOption == 4 && GetDefcon() > 3 )
    {            
        speedDesired = 10;
    }

    

#ifdef NON_PLAYABLE
    speedDesired = 20;
#endif

#ifdef TESTBED
    speedDesired = 20;
#endif

    SetTimeScaleFactor( speedDesired );
    

    m_theDate.AdvanceTime( GetTimeScaleFactor() * SERVER_ADVANCE_PERIOD );


    //
    // Update everyones radar sharing

    int shareAllianceRadar = g_app->GetGame()->GetOptionValue( "RadarSharing" );
    for(int i = 0; i < m_teams.Size(); ++i )
    {
        if( shareAllianceRadar == 0 )
        {
            // Always off
            m_teams[i]->m_sharingRadar.SetAll( false );
        }
        else if( shareAllianceRadar == 1 )
        {
            // Always on for alliance
            for(int j = 0; j < m_teams.Size(); ++j )
            {
                int ourTeamId = m_teams[i]->m_teamId;
                int theirTeamid = m_teams[j]->m_teamId;

                m_teams[i]->m_sharingRadar[ theirTeamid ] = ( i != j && IsFriend(ourTeamId, theirTeamid) );
            }
        }
        else if( shareAllianceRadar == 2 )
        {
            // Up to the players
            // AI shares only with alliance
            if( m_teams[i]->m_type == Team::TypeAI )
            {
                for(int j = 0; j < m_teams.Size(); ++j )
                {
                    int ourTeamId = m_teams[i]->m_teamId;
                    int theirTeamid = m_teams[j]->m_teamId;
                    m_teams[i]->m_sharingRadar[ theirTeamid ] = ( i != j && IsFriend(ourTeamId, theirTeamid) );
                }
            }
        }
        else if( shareAllianceRadar == 3 )
        {
            // Always on
            m_teams[i]->m_sharingRadar.SetAll( true );
        }
    }
    
    START_PROFILE( "Radar Coverage" );
    UpdateRadar();
    END_PROFILE( "Radar Coverage" );


    //
    // Update everyones ceasefire status

    int permitDefection = g_app->GetGame()->GetOptionValue("PermitDefection");

    if( permitDefection == 0 )
    {
        for(int i = 0; i < m_teams.Size(); ++i )
        {
            for(int j = 0; j < m_teams.Size(); ++j )
            {
                int ourTeamId = m_teams[i]->m_teamId;
                int theirTeamid = m_teams[j]->m_teamId;

                m_teams[i]->m_ceaseFire[ theirTeamid ] = (i != j && IsFriend(ourTeamId, theirTeamid) );
            }
        }
    }



    //
    // Update all cities

    START_PROFILE( "Cities" );
    for( int i = 0; i < m_cities.Size(); ++i )
    {
        City *city = m_cities[i];
        city->Update();
    }
    END_PROFILE( "Cities" );


    //
    // Update all objects

    START_PROFILE( "Objects" );
    for( int i = 0; i < m_objects.Size(); ++i )
    {
        if( m_objects.ValidIndex(i) )
        {
            WorldObject *wobj = m_objects[i];

            char *name = WorldObject::GetName(wobj->m_type);
            START_PROFILE( name );

            Fixed oldLongitude = wobj->m_longitude;
            Fixed oldLatitude = wobj->m_latitude;
            Fixed oldRadarSize = wobj->m_previousRadarRange;

            if( trackSyncRand ) SyncRandLog( wobj->LogState() );

            bool amIdead = wobj->Update();

            if( trackSyncRand ) SyncRandLog( wobj->LogState() );

            if( wobj->m_teamId != TEAMID_SPECIALOBJECTS )
            {
                if( amIdead )
                {
                    m_radarGrid.RemoveCoverage( oldLongitude, oldLatitude, oldRadarSize, wobj->m_teamId );
                    m_objects.RemoveData(i);
                    delete wobj;
                }
                else
                {
                    Fixed newRadarRange = wobj->GetRadarRange();
                    m_radarGrid.UpdateCoverage( oldLongitude, oldLatitude, oldRadarSize, 
                        wobj->m_longitude, wobj->m_latitude, newRadarRange, wobj->m_teamId );
                    wobj->m_previousRadarRange = newRadarRange;
                }
            }
            else
            {
                if( amIdead )
                {
                    m_objects.RemoveData(i);
                    delete wobj;
                }
            }

            END_PROFILE( name );
        }
    }
    END_PROFILE( "Objects" );


    //
    // Update Fleets
    
    START_PROFILE("Fleets");
    for( int i = 0; i < g_app->GetWorld()->m_teams.Size(); ++i )
    {
        Team *team = m_teams[i];
        for( int j = 0; j < team->m_fleets.Size(); ++j )
        {
            Fleet *fleet = team->m_fleets[j];
            if( fleet->m_fleetMembers.Size() > 0 )
            {
                if( trackSyncRand ) SyncRandLog( fleet->LogState() );
                fleet->Update();                
                if( trackSyncRand ) SyncRandLog( fleet->LogState() );
            }
        }
    }
    END_PROFILE("Fleets");

    //
    // Update explosions

    START_PROFILE("Explosions");
    for( int i = 0; i < m_explosions.Size(); ++i )
    {
        if( m_explosions.ValidIndex(i) )
        {
            WorldObject *explosion = m_explosions[i];
            if( explosion->Update() )
            {
                m_explosions.RemoveData(i);
                delete explosion;
            }
        }
    }
    END_PROFILE("Explosions");
    

    // update all gunfire objects

    START_PROFILE( "GunFire" );
    for( int i = 0; i < m_gunfire.Size(); ++i )
    {
        if( m_gunfire.ValidIndex(i) )
        {
            GunFire *bullet = m_gunfire[i];
            if( bullet->Update() ||
                bullet->m_range <= 0)
            {
                if( bullet->m_range > 0 )
                {
                    bullet->Impact();
                }
                m_gunfire.RemoveData(i);
                delete bullet;
            }
        }
    }
    END_PROFILE( "GunFire" );

    
    m_votingSystem.Update();

    //
    // Update Messages

    START_PROFILE( "Messages" );
    for( int i = 0; i < m_messages.Size(); ++i )
    {
        WorldMessage *msg = m_messages[i];
        msg->m_timer -= SERVER_ADVANCE_PERIOD;
        if( ( msg->m_timer <= 0 ) ||
            ( msg->m_objectId != -1 &&
              !g_app->GetWorld()->GetWorldObject( msg->m_objectId ) )
          )
        {
            m_messages.RemoveData(i);
            --i;
            delete msg;
        }  
    }
    END_PROFILE( "Messages" );


    //
    // Run AI for computer teams

    START_PROFILE( "AI" );
    for( int i = 0; i < m_teams.Size(); ++i )
    {
        if( m_teams[i]->m_type == Team::TypeAI )
        {
            m_teams[i]->RunAI();
        }
    }
    END_PROFILE( "AI" );

    //
    // Update the map render colour, based on population
    
    START_PROFILE("WorldColour");
    int territoriesPerTeam = g_app->GetGame()->GetOptionValue("TerritoriesPerTeam");
    int populationPerTerritory = g_app->GetGame()->GetOptionValue("PopulationPerTerritory");
    int totalStartingPop = territoriesPerTeam * populationPerTerritory * 1000000;

    //if( g_keys[KEY_O] ) g_app->GetWorld()->GetMyTeam()->m_friendlyDeaths += 3000000;
    //if( g_keys[KEY_P] ) g_app->GetWorld()->GetMyTeam()->m_friendlyDeaths -= 3000000;
    
    for( int i = 0; i < m_teams.Size(); ++i )
    {
        float numFriendlyDead = m_teams[i]->m_friendlyDeaths;
        float fractionAlive = 1.0f - numFriendlyDead / totalStartingPop;
        fractionAlive = min( fractionAlive, 1.0f );
        fractionAlive = max( fractionAlive, 0.0f );
        float fraction1 = SERVER_ADVANCE_PERIOD.DoubleValue() * 0.1f;
        float fraction2 = 1.0f - fraction1;
        m_teams[i]->m_teamColourFader = m_teams[i]->m_teamColourFader * fraction2 + 
										fractionAlive * fraction1;
    }
    END_PROFILE("WorldColour");


    //
    // Generate a world event
    int worldEvents = g_app->GetGame()->GetOptionValue("EnableWorldEvents");
    if( worldEvents == 1 )
    {
        if( GetDefcon() == 1 )
        {
            if( syncfrand( 100000 ) <= 100 )
            {
                GenerateWorldEvent();
            }
        }
    }
        
    if( !g_app->GetInterface()->UsingChatWindow() )
    {
        //
        // If team switching is enabled, num keys select team ID
        // Otherwise it sets game speed

        if( g_app->GetGame()->GetOptionValue("TeamSwitching") == 1)
        {
            if( g_keys[KEY_1] )    m_myTeamId = 0;
            if( g_keys[KEY_2] )    m_myTeamId = 1;
            if( g_keys[KEY_3] )    m_myTeamId = 2; 
            if( g_keys[KEY_4] )    m_myTeamId = 3;
            if( g_keys[KEY_5] )    m_myTeamId = 4;
            if( g_keys[KEY_6] )    m_myTeamId = 5;
            if( g_keys[KEY_7] )    m_myTeamId = 6;
            if( g_keys[KEY_8] )    g_app->GetMapRenderer()->m_renderEverything = !g_app->GetMapRenderer()->m_renderEverything;

            if( m_myTeamId >= m_teams.Size() )
            {
                m_myTeamId = -1;
            }
        }
        else
        {
            if( m_myTeamId != -1 )
            {
                int requestedSpeed = -1;
                if( g_keys[KEY_0] )     requestedSpeed = GAMESPEED_PAUSED;    
                if( g_keys[KEY_1] )     requestedSpeed = GAMESPEED_REALTIME;    
                if( g_keys[KEY_2] )     requestedSpeed = GAMESPEED_SLOW;    
                if( g_keys[KEY_3] )     requestedSpeed = GAMESPEED_MEDIUM;    
                if( g_keys[KEY_4] )     requestedSpeed = GAMESPEED_FAST;    

                if( requestedSpeed != -1 )
                {
                    if( CanSetTimeFactor( requestedSpeed ) )
                    {
                        g_app->GetClientToServer()->RequestGameSpeed( m_myTeamId, requestedSpeed );            
                    }
                }
            }
        }
    }


    //
    // Update the Game scores / victory conditions etc

    START_PROFILE( "Game" );
    g_app->GetGame()->Update();
    END_PROFILE( "Game" );



    END_PROFILE( "World Update" );
}


bool World::CanSetTimeFactor( Fixed factor )
{
    if( m_myTeamId == -1 ||
        g_app->GetGame()->m_winner != -1 ) 
    {
        return false;
    }

    int gameSpeedOption = g_app->GetGame()->GetOptionValue("GameSpeed");
    int minSpeedSetting = g_app->GetGame()->GetOptionValue("SlowestSpeed");
    int minSpeed = (    minSpeedSetting == 0 ? 0 :
                        minSpeedSetting == 1 ? 1 :
                        minSpeedSetting == 2 ? 5 :
                        minSpeedSetting == 3 ? 10 :
                        minSpeedSetting == 4 ? 20 : 20 );

    if( factor < minSpeed ) 
    {
        return false;
    }


    bool speedSettable = (gameSpeedOption == 0 && 
                          factor >= minSpeed);

    return speedSettable;
}



bool World::IsVisible( Fixed longitude, Fixed latitude, int teamId )
{       
    if( teamId == -1 ) return false;

    //
    // Check our own radar first

    if( m_radarGrid.GetCoverage( longitude, latitude, teamId ) > 0 )
    {
        return true;
    }


    //
    // Not on our radar - but maybe its on our allies radar
    // And maybe our allies are nice enough to share their radar (the fools)

    for( int t = 0; t < m_teams.Size(); ++t )
    {
        Team *team = m_teams[t];
        if( teamId != team->m_teamId &&
            teamId != -1 &&
            team->m_sharingRadar[teamId] &&
            m_radarGrid.GetCoverage( longitude, latitude, team->m_teamId ) )
        {
            return true;
        }
    }


    //
    // Nope, can't see it

    return false;
}


void World::UpdateRadar()
{
    if( GetDefcon() == 5 )
    {
        // show ONLY allies during defcon 5
        for( int i = 0; i < m_objects.Size(); ++i ) 
        {
            if( m_objects.ValidIndex(i) )
            {
                WorldObject *wobj = m_objects[i];
        
                for( int t = 0; t < m_teams.Size(); ++t )
                {
                    int thisTeamId = m_teams[t]->m_teamId;
                 
                    wobj->m_visible[thisTeamId] = false;

                    bool showObject = ( wobj->m_teamId == thisTeamId );

                    if( GetTeam( wobj->m_teamId )->m_sharingRadar[ thisTeamId ] &&
                        wobj->m_type != WorldObject::TypeSub )
                    {
                        showObject = true;
                    }
                    
                    if( showObject )
                    {
                        wobj->m_visible[thisTeamId] = true;
                    }
                }
            }
        }   

        return;
    }

    
    //
    // Update gunfire visibility

    for( int j = 0; j < m_gunfire.Size(); ++j )
    {
        if( m_gunfire.ValidIndex(j) )
        {
            WorldObject *potential = m_gunfire[j];
            for( int k = 0; k < m_teams.Size(); ++k )
            {
                Team *team = m_teams[k];
                potential->m_visible[team->m_teamId] = IsVisible( potential->m_longitude, potential->m_latitude, team->m_teamId );
            }
        }
    }

    //
    // Update animation visibility
    
    for( int j = 0; j < g_app->GetMapRenderer()->m_animations.Size(); ++j )
    {
        if( g_app->GetMapRenderer()->m_animations.ValidIndex(j) )
        {
            SonarPing *ping = (SonarPing *)g_app->GetMapRenderer()->m_animations[j];
            if( ping->m_animationType == MapRenderer::AnimationTypeSonarPing )
            {
                for( int k = 0; k < m_teams.Size(); ++k )
                {
                    Team *team = m_teams[k];
                    ping->m_visible[team->m_teamId] = (team->m_teamId == ping->m_teamId) ||
                                                       IsVisible( Fixed::FromDouble(ping->m_longitude),
																  Fixed::FromDouble(ping->m_latitude), team->m_teamId );
                }
            }
        }
    }


    //
    // Update explosion visibility

    for( int j = 0; j < m_explosions.Size(); ++j )
    {
        if( m_explosions.ValidIndex(j) )
        {
            Explosion *explosion = m_explosions[j];
            
            for( int k = 0; k < g_app->GetWorld()->m_teams.Size(); ++k )
            {
                Team *team = g_app->GetWorld()->m_teams[k];
                explosion->m_visible[team->m_teamId] = explosion->m_targetTeamId == team->m_teamId ||
                                                       explosion->m_teamId == team->m_teamId ||
                                                       explosion->m_initialIntensity > 30 ||
                                                       IsVisible( explosion->m_longitude, explosion->m_latitude, team->m_teamId );
            }
        }
    }


    //
    // Update object visibility
    
    for( int i = 0; i < m_objects.Size(); ++i ) 
    {
        if( m_objects.ValidIndex(i) )
        {
            WorldObject *wobj = m_objects[i];
            
            for( int t = 0; t < m_teams.Size(); ++t )
            {
                Team *team = m_teams[t];
                wobj->m_visible[team->m_teamId] = false;

                Team *owner = GetTeam(wobj->m_teamId);
                
                if( owner->m_teamId != TEAMID_SPECIALOBJECTS )
                {
                    wobj->m_visible[owner->m_teamId] = true;
                }

                if( wobj->m_teamId == TEAMID_SPECIALOBJECTS ||
                    IsVisible( wobj->m_longitude, wobj->m_latitude, team->m_teamId ) )
                {
                    int permitDefection = g_app->GetGame()->GetOptionValue("PermitDefection");
                    if( !wobj->IsHiddenFrom() || 
                        (IsFriend( wobj->m_teamId, team->m_teamId ) && permitDefection == 0) ||
                        wobj->m_teamId == team->m_teamId)
                    {
                        wobj->m_visible[team->m_teamId] = true;

                        if( team->m_teamId == m_myTeamId &&
                            !wobj->m_seen[team->m_teamId] &&
                            wobj->m_type == WorldObject::TypeSub &&
                            g_app->m_hidden )
                        {
                            g_app->GetStatusIcon()->SetSubIcon( STATUS_ICON_SUB );
                            g_app->GetStatusIcon()->SetCaption( LANGUAGEPHRASE("tray_icon_enemy_subs_detected") );
                        }

                        // handle AI events
                        if( GetDefcon() <= 3 )
                        {
                            if( !wobj->m_seen[team->m_teamId] &&
                                team->m_type == Team::TypeAI &&
                                wobj->m_teamId != team->m_teamId )
                            {
                                if( wobj->m_type == WorldObject::TypeSub )
                                {
                                    if( g_app->GetMapRenderer()->IsValidTerritory( team->m_teamId, wobj->m_longitude, wobj->m_latitude, true ) )
                                    {
                                        team->AddEvent(Event::TypeEnemyIncursion, wobj->m_objectId, wobj->m_teamId, wobj->m_fleetId, wobj->m_longitude, wobj->m_latitude );
                                    }
                                    else
                                    {
                                        team->AddEvent( Event::TypeSubDetected, wobj->m_objectId, wobj->m_teamId, wobj->m_fleetId, wobj->m_longitude, wobj->m_latitude );
                                    }
                                }
                                else if (wobj->m_type == WorldObject::TypeFighter ||
                                         wobj->m_type == WorldObject::TypeBomber )
                                {
                                    if( g_app->GetMapRenderer()->IsValidTerritory( team->m_teamId, wobj->m_longitude, wobj->m_latitude, false ) )
                                    {
                                        team->AddEvent( Event::TypeIncomingAircraft, wobj->m_objectId, wobj->m_teamId, wobj->m_fleetId, wobj->m_longitude, wobj->m_latitude );                                         
                                    }
                                }
                                else if ( wobj->m_type == WorldObject::TypeBattleShip ||
                                          wobj->m_type == WorldObject::TypeSub ||
                                          wobj->m_type == WorldObject::TypeCarrier )
                                {
                                    if( g_app->GetMapRenderer()->IsValidTerritory( team->m_teamId, wobj->m_longitude, wobj->m_latitude, true ) )
                                    {
                                        team->AddEvent(Event::TypeEnemyIncursion, wobj->m_objectId, wobj->m_teamId, wobj->m_fleetId, wobj->m_longitude, wobj->m_latitude );
                                    }
                                }
                            }
                        }

                        wobj->m_seen[team->m_teamId] = true;
                    }
                }
            }
        }
    }
}


int World::GetDefcon()
{    
    //return 1;

    if ( m_theDate.GetDays() > 0 ||
         m_theDate.GetHours() > 0 || 
         m_theDate.GetMinutes() >= 30 )
    {
        return 1;
    }
    else if(m_theDate.GetDays() > 0 ||
            m_theDate.GetHours() > 0 || 
            m_theDate.GetMinutes() > 12 )
    {
        return 4 - m_theDate.GetMinutes() / 10;
    }
    else
    {
        return 5 - m_theDate.GetMinutes() / 6;
    }   
}

void World::SetTimeScaleFactor( Fixed factor )
{
    m_timeScaleFactor = factor.IntValue();
}

Fixed World::GetTimeScaleFactor()
{
    if( g_app->GetGame()->m_winner != -1 )
    {
        return 0;
    }

    return m_timeScaleFactor;
}


void World::GenerateWorldEvent()
{
    //return;

    int num = 1;// syncrand() % 2;
    char msg[128];
    switch(num)
    {
        case 0:
            {
                int numTornados = 1 + syncrand() % 2;
                for( int i = 0; i < numTornados; i++ )
                {
                    Fixed longitude = syncsfrand(360);
                    Fixed latitude = syncsfrand(180);
                    Tornado *tornado = new Tornado();
                    tornado->SetSize( 15 + syncsfrand(5) );
                    tornado->SetTeamId( TEAMID_SPECIALOBJECTS );
                    tornado->SetPosition( longitude, latitude );  
                    tornado->GetNewTarget();
                    g_app->GetWorld()->AddWorldObject( tornado );
                    sprintf( msg, LANGUAGEPHRASE("tornado_warning"));
                }
            }
            break;
        case 1:
            {
                Fixed longitude = syncsfrand(360);
                Fixed latitude = syncsfrand(180);
                Saucer *saucer = new Saucer();
                saucer->SetTeamId( TEAMID_SPECIALOBJECTS );
                saucer->SetPosition( longitude, latitude );
                saucer->GetNewTarget();
                g_app->GetWorld()->AddWorldObject( saucer );
                sprintf( msg, LANGUAGEPHRASE("alien_invasion"));
            }
            break;
    }
    g_app->GetInterface()->ShowMessage( 0, 0, -1, msg, true );
}


bool World::IsSailable( Fixed const &fromLongitude, Fixed const &fromLatitude, Fixed const &toLongitude, Fixed const &toLatitude )
{
    Fixed timeScaleFactor = g_app->GetWorld()->GetTimeScaleFactor();
    if( timeScaleFactor == 0 ) return false;

    Fixed longitude = fromLongitude;
    Fixed latitude = fromLatitude;
    Fixed actualToLongitude = toLongitude;
    Fixed actualToLatitude = toLatitude;

    if( actualToLongitude < -180 )
    {
        actualToLongitude = -180;
    }
    else if( actualToLongitude > 180 )
    {
        actualToLongitude = 180;
    }
   

    Vector3<Fixed> vel = (Vector3<Fixed>( actualToLongitude, actualToLatitude, 0 ) -
                          Vector3<Fixed>( longitude, latitude, 0 ));
    Fixed velMag = vel.Mag();
    int nbIterations = 0;

    if( timeScaleFactor == 20 )
    {
        // Stepsize = 1
        nbIterations = velMag.IntValue();
        vel /= velMag;
    }
    else
    {
        // Stepsize = 0.5
        Fixed zeroPointFive = Fixed::FromDouble(0.5f);
        nbIterations = ( velMag / zeroPointFive ).IntValue();
        vel /= velMag;
        vel *= zeroPointFive;
    }
    
    for( int i = 0; i < nbIterations; ++i )
    {
        longitude += vel.x;
        latitude += vel.y;

        // For debugging purposes
        //glVertex2f( longitude.DoubleValue(), latitude.DoubleValue() );

        if( !g_app->GetMapRenderer()->IsValidTerritory( -1, longitude, latitude, false ) )
        {
            return false;
        }
    }

    return true;
}


bool World::IsSailableSlow( Fixed const &fromLongitude, Fixed const &fromLatitude, Fixed const &toLongitude, Fixed const &toLatitude )
{
    START_PROFILE( "IsSailable" );

    Fixed longitude = fromLongitude;
    Fixed latitude = fromLatitude;
    Fixed actualToLongitude = toLongitude;
    Fixed actualToLatitude = toLatitude;

    if( actualToLongitude < -180 )
    {
        actualToLongitude = -180;
    }
    else if( actualToLongitude > 180 )
    {
        actualToLongitude = 180;
    }


    Vector3<Fixed> vel;
    while(true)
    {
        Fixed timePerUpdate = SERVER_ADVANCE_PERIOD * g_app->GetWorld()->GetTimeScaleFactor();
        if( timePerUpdate == 0 )
        {
            END_PROFILE( "IsSailable" );
            return false;
        }
        Fixed factor1 = Fixed::Hundredths(1) * timePerUpdate / 10;
        Fixed factor2 = 1 - factor1;

        Vector3<Fixed> targetDir = (Vector3<Fixed>( actualToLongitude, actualToLatitude, 0 ) -
            Vector3<Fixed>( longitude, latitude, 0 )).Normalise();  

        vel = ( targetDir * factor1 ) + ( vel * factor2 );
        vel.Normalise();
        vel *= Fixed::Hundredths(50);


        longitude = longitude + vel.x * timePerUpdate;
        latitude = latitude + vel.y * timePerUpdate;

        glVertex2f( longitude.DoubleValue(), latitude.DoubleValue() );

        if(!g_app->GetMapRenderer()->IsValidTerritory( -1, longitude, latitude, false ) )
        {
            END_PROFILE( "IsSailable" );
            return false;
        }

        Fixed distanceSqd = GetDistanceSqd( longitude, latitude, actualToLongitude, actualToLatitude);               
        if( distanceSqd < 2 * 2 ) 
        {
            END_PROFILE( "IsSailable" );
            return true;
        }
    }

    END_PROFILE( "IsSailable" );
    return true;
}


struct nodeInfoStruct
{
    Node *node;
    int index;
    Fixed distanceSqd;
};


static int NodeInfoStructCompare( const void *elem1, const void *elem2 )
{
    const struct nodeInfoStruct *id1 = (const struct nodeInfoStruct *) elem1;
    const struct nodeInfoStruct *id2 = (const struct nodeInfoStruct *) elem2;

    if      ( id1->distanceSqd > id2->distanceSqd )     return +1;
    else if ( id1->distanceSqd < id2->distanceSqd )     return -1;
    else                                                return 0;
}


int World::GetClosestNode( Fixed const &longitude, Fixed const &latitude )
{
    START_PROFILE( "GetClosestNode" );
    int nodeId = -1;

    int nodesSize = m_nodes.Size();
    struct nodeInfoStruct *nodeInfo = new struct nodeInfoStruct[nodesSize];

    for( int i = 0; i < nodesSize; ++i )
    {
        Node *node = m_nodes[i];
        nodeInfo[i].node = node;
        nodeInfo[i].index = i;
        nodeInfo[i].distanceSqd = GetDistanceSqd( longitude, latitude, node->m_longitude, node->m_latitude );
    }

    qsort( nodeInfo, nodesSize, sizeof(struct nodeInfoStruct), NodeInfoStructCompare );

    for( int j = 0; j < nodesSize; j++ )
    {
        Node *node = nodeInfo[j].node;
        if( IsSailable( node->m_longitude, node->m_latitude, longitude, latitude ) )
        {
            nodeId = nodeInfo[j].index;
            break;
        }
    }

	delete [] nodeInfo;

    END_PROFILE( "GetClosestNode" );
    return nodeId;
}


int World::GetClosestNodeSlow( Fixed const &longitude, Fixed const &latitude )
{
    Fixed currentDistanceSqd = Fixed::MAX;
    int nodeId = -1;

    for( int i = 0; i < m_nodes.Size(); ++i )
    {
        Node *node = m_nodes[i];
        Fixed distanceSqd = GetDistanceSqd( longitude, latitude, node->m_longitude, node->m_latitude);

        if( distanceSqd < currentDistanceSqd &&
            IsSailable(node->m_longitude, node->m_latitude, longitude, latitude) )
        {
            currentDistanceSqd = distanceSqd;
            nodeId = i;
        }
    }
    return nodeId;
}


Fixed World::GetDistanceAcrossSeamSqd( Fixed const &fromLongitude, Fixed const &fromLatitude, Fixed const &toLongitude, Fixed const &toLatitude )
{
    Fixed targetSeamLatitude;
    Fixed targetSeamLongitude;
    GetSeamCrossLatitude( Vector3<Fixed>(toLongitude, toLatitude, 0), Vector3<Fixed>(fromLongitude, fromLatitude,0), &targetSeamLongitude, &targetSeamLatitude);
    

    Fixed distanceAcrossSeam = ( Vector3<Fixed>(targetSeamLongitude, targetSeamLatitude,0) -
                                 Vector3<Fixed>(fromLongitude, fromLatitude, 0) ).MagSquared();

    distanceAcrossSeam += ( Vector3<Fixed>(targetSeamLongitude * -1, targetSeamLatitude,0) -
                            Vector3<Fixed>(toLongitude, toLatitude, 0) ).MagSquared();

    return distanceAcrossSeam;
}


Fixed World::GetDistanceAcrossSeam( Fixed const &fromLongitude, Fixed const &fromLatitude, Fixed const &toLongitude, Fixed const &toLatitude )
{
    Fixed distSqd = GetDistanceAcrossSeamSqd( fromLongitude, fromLatitude, toLongitude, toLatitude );
    return sqrt( distSqd );
}


Fixed World::GetDistanceSqd( Fixed const &fromLongitude, Fixed const &fromLatitude, Fixed const &toLongitude, Fixed const &toLatitude, bool ignoreSeam )
{
    Vector3<Fixed>from(fromLongitude, fromLatitude, 0);
    Vector3<Fixed>to(toLongitude, toLatitude, 0);
    Vector3<Fixed> theVector = from - to;
    Fixed dist = theVector.MagSquared();
    if( ignoreSeam )
    {
        return dist;
    }
    else
    {
        Fixed distAcrossSeam = GetDistanceAcrossSeamSqd( fromLongitude, fromLatitude, toLongitude, toLatitude );
        return ( dist < distAcrossSeam ? dist : distAcrossSeam );
    }
}


Fixed World::GetDistance( Fixed const &fromLongitude, Fixed const &fromLatitude, Fixed const &toLongitude, Fixed const &toLatitude, bool ignoreSeam )
{
    Fixed distSqd = GetDistanceSqd( fromLongitude, fromLatitude, toLongitude, toLatitude, ignoreSeam );
    return sqrt( distSqd );
}


Fixed World::GetSailDistanceSlow( Fixed const &fromLongitude, Fixed const &fromLatitude, Fixed const &toLongitude, Fixed const &toLatitude )
{
    Fixed totalDistanceSqd = 0;
    Fixed currentLong = fromLongitude;
    Fixed currentLat = fromLatitude;
    int targetNodeId = GetClosestNode( toLongitude, toLatitude );
    int currentNodeId = -1;

    BoundedArray<bool> usedNodes;
    usedNodes.Initialise( g_app->GetWorld()->m_nodes.Size() );
    usedNodes.SetAll( false );

    // NOTE by chris:
    // This function no longer works correctly, because it relies on the totally erronious principle
    // of adding up squared distances
    // (ie it wrongly assumes  sqrt(a^2 + b^2) == a + b )


    while( true )
    {
        Fixed wrappedLong = toLongitude;

        if( fromLongitude > 90 && wrappedLong < -90 )       wrappedLong += 360;       
        if( fromLongitude < -90 && wrappedLong > 90 )       wrappedLong -= 360;        

        if( g_app->GetWorld()->IsSailable( currentLong, currentLat, wrappedLong, toLatitude ) )
        {
            // We can now sail directly to our target
            totalDistanceSqd += GetDistanceSqd(currentLong, currentLat, wrappedLong, toLatitude, false);
            break;
        }
        else
        {
            // Query all directly sailable nodes for the quickest route to targetNodeId;
            Fixed currentBestDistanceSqd = Fixed::MAX;
            Fixed newLong = 0;
            Fixed newLat = 0;
            int nodeId = -1;

            for( int n = 0; n < g_app->GetWorld()->m_nodes.Size(); ++n )
            {                
                Node *node = g_app->GetWorld()->m_nodes[n];
                if( !usedNodes[n] &&
                    (currentNodeId != -1 ||
                     g_app->GetWorld()->IsSailable( currentLong, currentLat, node->m_longitude, node->m_latitude )) )
                {
                    int routeId = node->GetRouteId( targetNodeId );
                    if( routeId != -1 )
                    {
                        Fixed distanceSqd = node->m_routeTable[routeId]->m_totalDistance;
                        distanceSqd *= distanceSqd;

                        distanceSqd += g_app->GetWorld()->GetDistanceSqd( currentLong, currentLat, node->m_longitude, node->m_latitude, false );

                        if( distanceSqd < currentBestDistanceSqd )
                        {
                            currentBestDistanceSqd = distanceSqd;               
                            newLong = node->m_longitude;
                            newLat = node->m_latitude;
                            nodeId = n;
                        }
                    }
                }
            }

            if( newLong != 0 && newLat != 0 )
            {
                totalDistanceSqd += GetDistanceSqd( currentLong, currentLat, newLong, newLat, false );
                currentLong = newLong;
                currentLat = newLat;
                usedNodes[nodeId] = true;
                currentNodeId = nodeId;
            }
            else
            {
                // We couldn't find a node that gets us any nearer
                return Fixed::MAX;
            }
        }
    }

    if( totalDistanceSqd == 0 ) return Fixed::MAX;
    else return sqrt(totalDistanceSqd);
}


Fixed World::GetSailDistance( Fixed const &fromLongitude, Fixed const &fromLatitude, Fixed const &toLongitude, Fixed const &toLatitude )
{
    Fixed totalDistance = 0;


    //
    // If its possible to sail in a straight line, do so

    Fixed wrappedLong = toLongitude;
    if( fromLongitude > 90 && toLongitude < -90 )       wrappedLong += 360;       
    if( fromLongitude < -90 && toLongitude > 90 )       wrappedLong -= 360;        

    if( IsSailable( fromLongitude, fromLatitude, wrappedLong, toLatitude ) )
    {
        totalDistance = GetDistance( fromLongitude, fromLatitude, wrappedLong, toLatitude, true );
        return totalDistance;
    }


    //
    // Find node nearest the target and add that distance

    int targetNodeId = GetClosestNode( toLongitude, toLatitude );
    if( targetNodeId == -1 )
    {
        return Fixed::MAX;
    }

    Node *targetNode = m_nodes[ targetNodeId ];
    totalDistance += GetDistanceSqd( targetNode->m_longitude, targetNode->m_latitude, toLongitude, toLatitude );


    //
    // Find the node nearest us that gets us the shortest route to the target

    Fixed bestDistance = Fixed::MAX;
    int bestNodeId = -1;

    for( int i = 0; i < m_nodes.Size(); ++i )
    {
        Node *node = m_nodes[i];
        int routeId = node->GetRouteId( targetNodeId );
        if( routeId != -1 )
        {
            Route *route = node->m_routeTable[routeId];

            Fixed thisDistance = route->m_totalDistance;

            if( thisDistance < bestDistance )
            {
                thisDistance += GetDistance( fromLongitude, fromLatitude, node->m_longitude, node->m_latitude );
                if( thisDistance < bestDistance )
                {
                    // The check "thisDistance < bestDistance" is done twice to quickly exclude long routes
                    // without having to call the expensive IsSailable function
                    bool sailable = IsSailable( fromLongitude, fromLatitude, node->m_longitude, node->m_latitude );
                    if( sailable )
                    {
                        bestDistance = thisDistance;
                        bestNodeId = i;
                    }
                }
            }
        }
    }


    // 
    // Calculate our final distance and return the result

    totalDistance += bestDistance;

    return totalDistance;
}

void World::GetSeamCrossLatitude( Vector3<Fixed> _to, Vector3<Fixed> _from, Fixed *longitude, Fixed *latitude )
{
//    y = mx + c
//    c = y - mx
//    x = (y - c) / m

    
    Fixed left = -180;
    Fixed right = 180;
    Fixed bottom = -90;
    Fixed top = 90;

    if( _to.x > 0 ) _to.x -= 360;
    else _to.x += 360;

    Fixed m = (_to.y - _from.y) / (_to.x - _from.x);
    Fixed c = _from.y - m * _from.x;

    if( _to.x < 0 )
    {
        // Intersect with left view plane 
        Fixed y = m * left + c;
        //if( y >= bottom && y <= top ) 
        {
            *longitude = left;
            *latitude = y;
            return;
        }
    }
    else
    {
        // Intersect with right view plane
        Fixed y = m * right + c;
        //if( y >= bottom && y <= top ) 
        {
            *longitude = right;
            *latitude = y;
            return;
        }        
    }

    // We should never ever get here
    AppAssert( false );
}


int World::GetTerritoryOwner( int territoryId )
{
    for( int i = 0; i < m_teams.Size(); ++i )
    {
        Team *team = m_teams[i];
        if(team->OwnsTerritory( territoryId ) )
        {
            return team->m_teamId;
        }
    }
    return -1;
}


void World::GetNumNukers( int objectId, int *inFlight, int *queued )
{
    *inFlight = 0;
    *queued = 0;
    
    WorldObject *wobj = GetWorldObject(objectId);
    if( !wobj ) return;

    for( int i = 0; i < m_objects.Size(); ++i )
    {
        if( m_objects.ValidIndex(i) )
        {
            WorldObject *obj = m_objects[i];
            if( obj->m_teamId == m_myTeamId )
            {
                if( obj->UsingNukes() )
                {
                    int nbQueued = 0;
                    for( int j = 0; j < obj->m_actionQueue.Size(); ++j )
                    {
                        ActionOrder *action = obj->m_actionQueue[j];
                        Fixed longitude = action->m_longitude;
                        if( longitude > 180 )
                        {
                            longitude -= 360;
                        }
                        else if( longitude < -180 )
                        {
                            longitude += 360;
                        }
                        if( longitude == wobj->m_longitude &&
                            action->m_latitude == wobj->m_latitude )
                        {
                            nbQueued++;
                        }
                    }
                    if( nbQueued > 0 )
                    {
                        if( obj->m_type == WorldObject::TypeAirBase || obj->m_type == WorldObject::TypeCarrier )
                        {
                            *queued += ( nbQueued > obj->m_nukeSupply )? obj->m_nukeSupply : nbQueued;
                        }
                        else
                        {
                            *queued += nbQueued;
                        }
                    }

                    if( obj->m_type == WorldObject::TypeBomber &&
                        obj->m_actionQueue.Size() == 0 )
                    {
                        Bomber *bomber = (Bomber *)obj;
                        Fixed longitude = bomber->m_nukeTargetLongitude;
                        if( longitude > 180 )
                        {
                            longitude -= 360;
                        }
                        else if( longitude < -180 )
                        {
                            longitude += 360;
                        }
                        if( longitude == wobj->m_longitude &&
                            bomber->m_nukeTargetLatitude == wobj->m_latitude &&
                            bomber->m_teamId != wobj->m_teamId )
                        {
                            (*queued)++;
                        }
                    }
                }

                if( obj->m_type == WorldObject::TypeNuke )
                {
                    MovingObject *nuke = (MovingObject *)obj;
                    Fixed longitude = nuke->m_targetLongitude;
                    if( longitude > 180 )
                    {
                        longitude -= 360;
                    }
                    else if( longitude < -180 )
                    {
                        longitude += 360;
                    }
                    if( longitude == wobj->m_longitude &&
                        nuke->m_targetLatitude == wobj->m_latitude )
                    {
                        (*inFlight)++;
                    }
                }
            }
        }
    }
}

void World::CreatePopList( LList<int> *popList )
{
    for( int i = 0; i < m_cities.Size(); ++i )
    {
        popList->PutData( m_cities[i]->m_population );
    }
}


bool World::IsChatMessageVisible( int teamId, int msgChannel, bool spectator )
{
    // teamId       : Id of team that posted the message
    // msgChannel   : Channel the message is in
    // spectator    : If the viewer is a spectator


    //
    // public channel, readable by all?

    if( msgChannel == CHATCHANNEL_PUBLIC )
    {
        return true;
    }

    //
    // Spectator message?
    // Can be blocked by SpectatorChatChannel variable
    // However specs can always speak after the game

    if( msgChannel == CHATCHANNEL_SPECTATORS )
    {
        int specVisible = g_app->GetGame()->GetOptionValue("SpectatorChatChannel");

        if( specVisible == 1 || 
            g_app->GetGame()->m_winner != -1 )
        {
            return true;
        }
    }


    //
    // System message?

    if( teamId == -1 )
    {
        return true;
    }


    //
    // Alliance channel : only if member of alliance

    if( msgChannel == CHATCHANNEL_ALLIANCE &&
        IsFriend( teamId, m_myTeamId ) )
    {
        return true;
    }


    //
    // Spectators can see everything except private chat

    if( spectator &&
        msgChannel >= CHATCHANNEL_PUBLIC ) 
    {
        return true;
    }


    // 
    // Private chat between 2 players

    if( msgChannel != CHATCHANNEL_SPECTATORS )
    {
        if( msgChannel == m_myTeamId ||                     // I am the receiving team
            teamId == m_myTeamId ||                         // I am the sending team
            teamId == msgChannel )
        {
            return true;
        }
    }


    //
    // Can't read it

    return false;
}


void World::AddChatMessage( int teamId, int channel, char *_msg, int msgId, bool spectator )
{
    //
    // Put the chat message into the list

    ChatMessage *msg = new ChatMessage();
    msg->m_fromTeamId = teamId;
    msg->m_channel = channel;
    msg->m_message = strdup( _msg );
    msg->m_spectator = spectator;
    msg->m_messageId = msgId;

    Spectator *amISpectating = GetSpectator(g_app->GetClientToServer()->m_clientId);

    msg->m_visible = IsChatMessageVisible( teamId, channel, amISpectating != NULL );

    if( spectator )
    {
        Spectator *spec = GetSpectator(teamId);
        strcpy( msg->m_playerName, spec->m_name );
    }
    else
    {
        if( msgId != -1 )
        {
            Team *team = GetTeam( teamId );
            if( team )  strcpy( msg->m_playerName, team->m_name );
            else        strcpy( msg->m_playerName, LANGUAGEPHRASE("unknown") );
        }
        else
        {
            strcpy( msg->m_playerName, " " );
        }
    }

    m_chat.PutDataAtStart( msg );


    //
    // Play sound

    if( msg->m_visible )
    {
        if( teamId == m_myTeamId )
        {
            g_soundSystem->TriggerEvent( "Interface", "SendChatMessage" );            
        }
        else
        {
            g_soundSystem->TriggerEvent( "Interface", "ReceiveChatMessage" );
        }
    }
}



int World::GetUnitValue( int _type )
{
    int variableTeamUnits = g_app->GetGame()->GetOptionValue("VariableUnitCounts");
    if( variableTeamUnits == 0 )
    {
        return 1;
    }
    else
    {
        switch( _type )
        {
            case WorldObject::TypeRadarStation: return 1;
            case WorldObject::TypeAirBase:
            case WorldObject::TypeCarrier:
            case WorldObject::TypeBattleShip:   return 2;
            case WorldObject::TypeSub :
            case WorldObject::TypeSilo:     return 3;
            default : return 2;
        }
    }
}


// ============================================================================

struct ParsedCity
{
    int     m_id;
    char    m_name[512];
    char    m_country[512];
    int     m_capital;
    int     m_population;
    float   m_longitude;
    float   m_latitude;
};


void World::ParseCityDataFile()
{
    //
    // Set up a data structure to store our parsed city data
    
    LList<ParsedCity *> cities;


    //
    // load positions from cities.positions
    // Example entry:
    //             3         0 6.1666999E+00 3.5566700E+01
    // 6.1666999E+00 3.5566700E+01 6.1666999E+00 3.5566700E+01


    std::ifstream locations( "cities.positions" );

    while( !locations.eof() )
    {
        char line1[512];
        char line2[512];
        locations.getline( line1, sizeof(line1) );
        locations.getline( line2, sizeof(line2) );

        char sIdNumber[64];
        char sLongitude[64];
        char sLatitude[64];

        strncpy( sIdNumber, line1, 19 );
        sIdNumber[19] = '\x0';

        strncpy( sLongitude, line1+20, 14 );
        sLongitude[14] = '\x0';

        strncpy( sLatitude, line1+34, 14 );
        sLatitude[14] = '\x0';

        ParsedCity *city = new ParsedCity();
        city->m_id = atoi(sIdNumber);
        city->m_longitude = atof( sLongitude );
        city->m_latitude = atof( sLatitude );
        strcpy( city->m_name, "Unknown" );
        strcpy( city->m_country, "Unknown" );
        city->m_population = -1;
        city->m_capital = -1;
        cities.PutData( city );
    }

    locations.close();
    

    //
    // Load names and populations from cities.names
    // Example entry :
    //     0.0000000E+00 0.0000000E+00          1          3          3Banta
    //           Algeria                             102756    102756197710


    std::ifstream names( "cities.names" );

    while( !names.eof() )
    {
        char line1[512];
        char line2[512];
        names.getline( line1, sizeof(line1) );
        names.getline( line2, sizeof(line2) );

        char sIdNumber[64];
        char sName[64];
        char sNamePart2[64];                        // Name can be split over 2 lines
        char sCountry[64];
        char sPopulation[64];
        char sCapital[64];

        strncpy( sIdNumber, line1+43, 12 );
        sIdNumber[12] = '\x0';

        strncpy( sName, line1+61, 64 );
        strncpy( sNamePart2, line2, 11 );
        sNamePart2[11] = '\x0';
        strcat( sName, sNamePart2 );

        strncpy( sCountry, line2+11, 30 );
        sCountry[30] = '\x0';

        strncpy( sPopulation, line2+41, 12 );
        sPopulation[12] = '\x0';

        strncpy( sCapital, line2+68, 1 );
        sCapital[1] = '\x0';
        
        int idNumber = atoi( sIdNumber );

        bool found = false;

        for( int i = 0; i < cities.Size(); ++i )
        {
            ParsedCity *city = cities[i];
            if( city->m_id == idNumber )
            {
                strcpy( city->m_name, sName );
                strcpy( city->m_country, sCountry );
                city->m_population = atoi( sPopulation );
                city->m_capital = atoi( sCapital );
                found = true;
                break;
            }
        }

        if( !found )
        {
            AppDebugOut( "City %s not found", sName );
        }
    }

    names.close();
    

    //
    // Save data into output file
    // Example entry:
    //    Cambridge urban area                     UK - England and Wales                   6.166700      35.566700     106673        0

    FILE *output = fopen( "cities.dat", "wt" );
    
    for( int i = 0; i < cities.Size(); ++i )
    {
        ParsedCity *city = cities[i];
        
        if( city->m_population != -1 )
        {
            fprintf( output, "%-40s %-40s %-13f %-13f %-13d %d\n",
                                        city->m_name,
                                        city->m_country,
                                        city->m_longitude,
                                        city->m_latitude,
                                        city->m_population,
                                        city->m_capital );
        }
    }

    fclose( output );
}


int World::GetAttackOdds( int attackerType, int defenderType, int attackerId )
{
    WorldObject *attacker = GetWorldObject( attackerId );
    if( attacker )
    {
        return attacker->GetAttackOdds( defenderType );
    }
    else
    {
        return GetAttackOdds( attackerType, defenderType );
    }
}

int World::GetAttackOdds( int attackerType, int defenderType )
{
    
    AppDebugAssert( attackerType >= 0 && attackerType < WorldObject::NumObjectTypes &&
                    defenderType >= 0 && defenderType < WorldObject::NumObjectTypes );

    static int id = 10;               // odds of identical units against each other

    static int s_attackOdds[ WorldObject::NumObjectTypes ] [ WorldObject::NumObjectTypes ] = 

                                                /* ATTACKER */

                                    /* INV CTY SIL RDR NUK EXP SUB SHP AIR FTR BMR CRR TOR SAU*/

                                    {   
                                        0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  // Invalid
                                        0,  0,  0,  0, 99,  0,  0,  0,  0,  0,  0,  0,  0,  0,  // City
                                        0,  0,  0,  0, 99,  0,  0,  0,  0,  0,  0,  0,  0, 50,  // Silo
                                        0,  0,  0,  0, 99,  0,  0,  0,  0,  0,  0,  0,  0, 50,  // Radar
                                        0,  0, 25,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  // Nuke            DEFENDER
                                        0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  // Explosion
                                        0,  0,  0,  0, 99,  0, id, 30,  0,  3, 25, 30,  0, 50,  // Sub
                                        0,  0,  0,  0, 99,  0, 20, id,  0, 10, 25,  0,  0, 50,  // BattleShip
                                        0,  0,  0,  0, 99,  0,  0,  0,  0,  0,  0,  0,  0, 50,  // Airbase
                                        0,  0, 10,  0,  0,  0,  0, 30,  0, id,  0,  0,  0, 50,  // Fighter
                                        0,  0, 10,  0,  0,  0,  0, 20,  0, 30,  0,  0,  0, 50,  // Bomber
                                        0,  0,  0,  0, 99,  0, 20, 20,  0, 10, 25,  0,  0, 50,  // Carrier
                                        0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  // Tornado
                                        0,  0, 10,  0,  0,  0,  0, 10,  0, 10,  0,  0,  0,  0,  // Saucer
                                    };


    return s_attackOdds[ defenderType ][ attackerType ];
}

void World::WriteNodeCoverageFile()
{
    Bitmap *bmp = new Bitmap(360,180);
    for( int i = -180; i < 180; ++i )
    {
        for( int j = -90; j < 90; ++j )
        {
            int nodeId = GetClosestNode( Fixed(i), Fixed(j) );
            if( nodeId != -1 )
            {
                bmp->GetPixel( i+180, j+90 ).m_b = 255;
                bmp->GetPixel( i+180, j+90 ).m_r = 255;
                bmp->GetPixel( i+180, j+90 ).m_g = 255;
                bmp->GetPixel( i+180, j+90 ).m_a = 255;
            }
            else
            {
                bmp->GetPixel( i+180, j+90 ).m_b = 0;
                bmp->GetPixel( i+180, j+90 ).m_r = 0;
                bmp->GetPixel( i+180, j+90 ).m_g = 0;
                bmp->GetPixel( i+180, j+90 ).m_a = 255;
            }
        }
    }
    char filename[256];
    sprintf( filename, "nodecoverage.bmp" );
    bmp->SaveBmp( filename );
}


void World::ClearWorld()
{
    for( int i = 0; i < m_objects.Size(); ++i )
    {
        if( m_objects.ValidIndex(i) )
        {
            m_objects[i]->m_life = 0;
        }
    }
    Update();
    m_objects.EmptyAndDelete();
    m_gunfire.EmptyAndDelete();
    m_explosions.EmptyAndDelete();
    m_radiation.EmptyAndDelete();
    for( int i = 0; i < m_teams.Size(); ++i )
    {
        Team *team = m_teams[i];
        team->m_fleets.EmptyAndDelete();
        team->m_enemyKills = 0;
        team->m_friendlyDeaths = 0;
        team->m_territories.Empty();

        team->m_unitsAvailable.SetAll(0);
        team->m_unitsInPlay.SetAll(0);        
        
        g_app->GetGame()->m_score[team->m_teamId] = 0;
    }

    for( int i = 0; i < m_cities.Size(); ++i )
    {
        m_cities[i]->m_population = m_populationTotals[i];
        m_cities[i]->m_dead = 0;
    }

    UpdateRadar();
    m_theDate.ResetClock();
}

void WorldMessage::SetMessage( char *_message )
{
    strcpy( m_message, _message );
    strupr( m_message );
}