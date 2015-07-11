#include "lib/universal_include.h"
#include "lib/filesys/text_stream_readers.h"
#include "lib/filesys/file_system.h"
#include "lib/debug_utils.h"
#include "lib/language_table.h"
#include "lib/math/random_number.h"
#include "lib/sound/soundsystem.h"
#include "lib/metaserver/authentication.h"

#include "interface/interface.h"
#include "interface/worldstatus_window.h"

#include "lib/eclipse/eclipse.h"

#include "app/app.h"
#include "app/globals.h"
#include "app/game.h"
#include "app/tutorial.h"
#include "app/statusicon.h"

#include "renderer/map_renderer.h"

#include "world/world.h"
#include "world/fleet.h"


Game::Game()
:   m_victoryTimer(-1),
    m_recalcTimer(0),
    m_winner(-1),
    m_maxGameTime(-1),
    m_gameTimeWarning(false),
    m_lockVictoryTimer(false),
    m_lastKnownDefcon(5),
    m_gameMode(-1)
{
    //
    // Load game options
    
    TextReader *in = g_fileSystem->GetTextReader( "data/gameoptions.txt" );
	AppAssert(in && in->IsOpen());

    while( in->ReadLine() )
    {
        if( !in->TokenAvailable() ) continue;

        char *param = in->GetNextToken();

        GameOption *option = new GameOption();
        m_options.PutData( option );

        strcpy( option->m_name,     param );
        option->m_min       = atof( in->GetNextToken() );
        option->m_max       = atof( in->GetNextToken() );
        option->m_default   = atof( in->GetNextToken() );
        option->m_change    = atoi( in->GetNextToken() );     
        
        if( option->m_change == 0 )
        {
            // Drop down menu - so load the sub options
            int numOptions = option->m_max - option->m_min;
            numOptions ++;
            for( int i = 0; i < numOptions; ++i )
            {
                in->ReadLine();
                char *subOption = strdup( in->GetRestOfLine() );
                
                // Strip trailing \n and \r
                int stringLength = strlen(subOption);
                if( subOption[stringLength-1] == '\n' ) subOption[stringLength-1] = '\x0';                
                if( subOption[stringLength-2] == '\r' ) subOption[stringLength-2] = '\x0';
                
                option->m_subOptions.PutData( subOption );
            }
        }

        if( option->m_change == -1 )
        {
            // String - load default
            in->ReadLine();
            strcpy( option->m_currentString, in->GetRestOfLine() );
        }
    }

    delete in;


    ResetOptions();

#ifdef TESTBED
    GetOption("MaxTeams")->m_default = 2;
    GetOption("MaxTeams")->m_currentValue = 2;
    GetOption("MaxGameRealTime")->m_default = 15;
    GetOption("MaxGameRealTime")->m_currentValue = 15;
#endif


    m_score.Initialise(MAX_TEAMS);
    m_nukeCount.Initialise(MAX_TEAMS);
    m_totalNukes.Initialise(MAX_TEAMS);

    for( int t = 0; t < MAX_TEAMS; ++t )
    {
        m_score[t] = 0;
        m_nukeCount[t] = 0;
        m_totalNukes[t] = 0;
    }
}


void Game::ResetOptions()
{
    for( int i = 0; i < m_options.Size(); ++i )
    {
        GameOption *option = m_options[i];
        option->m_currentValue = option->m_default;
    }
}


GameOption *Game::GetOption( char *_name )
{
    for( int i = 0; i < m_options.Size(); ++i )
    {
        GameOption *option = m_options[i];
        if( strcmp( option->m_name, _name ) == 0 )
        {
            return option;
        }
    }

    return NULL;
}


int Game::GetOptionIndex( char *_name )
{
    for( int i = 0; i < m_options.Size(); ++i )
    {
        GameOption *option = m_options[i];
        if( strcmp( option->m_name, _name ) == 0 )
        {
            return i;
        }
    }

    return -1;
}


int Game::GetOptionValue( char *_name )
{
    GameOption *option = GetOption(_name);
    if( option )
    {
        return option->m_currentValue;
    }

    return -1;
}


void Game::SetOptionValue( char *_name, int _value )
{
    GameOption *option = GetOption(_name);
    if( option )
    {
        option->m_currentValue = _value;
    }
}


void Game::SetGameMode( int _mode )
{
    //
    // Reset all options to defaults

    for( int i = 0; i < m_options.Size(); ++i )
    {
        GameOption *option = m_options[i];
        if( strcmp(option->m_name, "GameMode") != 0 &&
            strcmp(option->m_name, "ServerName") != 0 &&
            strcmp(option->m_name, "ScoreMode") != 0 &&
            strcmp(option->m_name, "MaxTeams") != 0 &&
            strcmp(option->m_name, "MaxSpectators") )
        {
            option->m_currentValue = option->m_default;
        }
    }

    m_gameMode = _mode;


    //
    // Set specific options

    switch( _mode )
    {
        case GAMEMODE_STANDARD:            
            break;

        case GAMEMODE_OFFICEMODE:
            SetOptionValue( "GameSpeed", 1 );
            SetOptionValue( "SlowestSpeed", 1 );
            SetOptionValue( "MaxGameRealTime", 360 );
            SetOptionValue( "VictoryTimer", 60 );
            break;

        case GAMEMODE_SPEEDDEFCON:
            SetOptionValue( "GameSpeed", 4 );
            SetOptionValue( "MaxGameRealTime", 15 );
            SetOptionValue( "SlowestSpeed", 4 );
            break;

        case GAMEMODE_BIGWORLD:
            SetOptionValue( "WorldScale", 200 );
            SetOptionValue( "CitiesPerTerritory", 40 );
            break;

        case GAMEMODE_DIPLOMACY:
            SetOptionValue( "PermitDefection", 1 );
            SetOptionValue( "RadarSharing", 1 );
            SetOptionValue( "ScoreMode", 1 );
            break;

        case GAMEMODE_TOURNAMENT:
            SetOptionValue( "MaxSpectators", 10 );
            SetOptionValue( "SpectatorChatChannel", 0 );
            SetOptionValue( "RandomTerritories", 1 );
            break;
    }
}


bool Game::IsOptionEditable( int _optionId )
{
    //
    // If we are a demo, nothing is editable
    // Except the server name
    
    char authKey[256];
    Authentication_GetKey( authKey );
    if( Authentication_IsDemoKey(authKey) )
    {
        if( stricmp(m_options[_optionId]->m_name, "ServerName" ) == 0 )
        {
            return true;
        }

       // return false;
		return true;
    }



    int mode = GetOptionValue("GameMode");
    char *optionName = m_options[_optionId]->m_name;

    if( stricmp(optionName, "TeamSwitching" ) == 0 &&
        mode != GAMEMODE_CUSTOM )
    {
        return false;
    }


    switch( mode )
    {
        case GAMEMODE_STANDARD:
            return( stricmp(optionName, "GameSpeed") != 0 &&
                    stricmp(optionName, "RadarSharing" ) != 0 &&
                    stricmp(optionName, "VariableUnitCounts" ) != 0 &&
                    stricmp(optionName, "WorldScale" ) != 0 );

        case GAMEMODE_OFFICEMODE:
            return( stricmp(optionName, "GameSpeed" ) != 0 &&                
                    stricmp(optionName, "SlowestSpeed" ) != 0 &&
                    stricmp(optionName, "MaxGameRealTime" ) != 0 &&
                    stricmp(optionName, "VictoryTimer" ) != 0 );

        case GAMEMODE_SPEEDDEFCON:
            return( stricmp(optionName, "GameSpeed" ) != 0 &&
                    stricmp(optionName, "MaxGameRealTime" ) != 0 &&
                    stricmp(optionName, "SlowestSpeed" ) != 0  );

        case GAMEMODE_BIGWORLD:
            return( stricmp(optionName, "WorldScale" ) != 0 &&
                    stricmp(optionName, "CitiesPerTerritory" ) != 0 );

        case GAMEMODE_DIPLOMACY:
            return( stricmp(optionName, "PermitDefection" ) != 0 &&
                    stricmp(optionName, "RadarSharing" ) != 0 &&
                    stricmp(optionName, "ScoreMode" ) != 0 );

        case GAMEMODE_TOURNAMENT:
            return( stricmp(optionName, "GameMode" ) == 0 ||
                    stricmp(optionName, "TerritoriesPerTeam") == 0 ||
                    stricmp(optionName, "ServerName") == 0 );
            
    }

    return true;
}


void Game::CountNukes()
{
    for( int t = 0; t < g_app->GetWorld()->m_teams.Size(); ++t )
    {
        Team *team = g_app->GetWorld()->m_teams[t];

        m_nukeCount[team->m_teamId] = 0;
        m_nukeCount[team->m_teamId] += team->m_unitsAvailable[WorldObject::TypeSilo] * 10;
        m_nukeCount[team->m_teamId] += team->m_unitsAvailable[WorldObject::TypeAirBase] * 5;
        m_nukeCount[team->m_teamId] += team->m_unitsAvailable[WorldObject::TypeCarrier] * 2;
        m_nukeCount[team->m_teamId] += team->m_unitsAvailable[WorldObject::TypeSub] * 5;
    }

    for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
    {
        if( g_app->GetWorld()->m_objects.ValidIndex(i) )
        {
            WorldObject *obj = g_app->GetWorld()->m_objects[i];
            switch( obj->m_type )
            {
                case WorldObject::TypeNuke:         
                    m_nukeCount[obj->m_teamId]++;
                    break;

                case WorldObject::TypeSilo:
                    m_nukeCount[obj->m_teamId] += obj->m_states[0]->m_numTimesPermitted;           
                    break;

                case WorldObject::TypeSub:          
                    m_nukeCount[obj->m_teamId] += obj->m_states[2]->m_numTimesPermitted;           
                    break;

                case WorldObject::TypeAirBase:      
                case WorldObject::TypeCarrier:
                    // count only nukes currently loaded onto bombers
                    m_nukeCount[obj->m_teamId] += std::min(obj->m_nukeSupply, obj->m_states[1]->m_numTimesPermitted);
                    break;
					
                case WorldObject::TypeBomber:       
                    m_nukeCount[obj->m_teamId] += obj->m_states[1]->m_numTimesPermitted;           
                    break;
            }
        }
    }
}


void Game::CalculateScores()
{
    //
    // Work out the score weightings

    int scoreMode = GetOptionValue( "ScoreMode" );
    switch( scoreMode )
    {
        case 0:                                             // Standard
            m_pointsPerSurvivor = 0;
            m_pointsPerDeath = -1;
            m_pointsPerKill = 2;
            m_pointsPerNuke = 0;
            m_pointsPerCollatoral = -2;
            break;

        case 1:                                             // Survivor
            m_pointsPerSurvivor = 1;
            m_pointsPerDeath = 0;
            m_pointsPerKill = 0;
            m_pointsPerNuke = 0;
            m_pointsPerCollatoral = -1;
            break;

        case 2:                                             // Genocide
            m_pointsPerSurvivor = 0;
            m_pointsPerDeath = 0;
            m_pointsPerKill = 1;
            m_pointsPerNuke = 0;
            m_pointsPerCollatoral = -1;
            break;
    }

    
    //
    // Work out the scores based on the weightings

    int startingPopulation = GetOptionValue( "PopulationPerTerritory" ) * 1000000.0f;
    startingPopulation *= GetOptionValue( "TerritoriesPerTeam" );

    for( int t = 0; t < g_app->GetWorld()->m_teams.Size(); ++t )
    {
        Team *team = g_app->GetWorld()->m_teams[t];

        m_score[team->m_teamId] = (startingPopulation - team->m_friendlyDeaths) * m_pointsPerSurvivor +
                                     team->m_friendlyDeaths * m_pointsPerDeath +
                                     team->m_enemyKills * m_pointsPerKill +
                                     m_nukeCount[team->m_teamId] * m_pointsPerNuke * 1000000.0f +
                                     team->m_collatoralDamage * m_pointsPerCollatoral;

        m_score[team->m_teamId] /= 1000000.0f;
    }
}


void Game::Update()
{
    //
    // If there is a real-world timer, update it

    if( m_winner == -1 && m_maxGameTime > 0 )
    {
        m_maxGameTime -= SERVER_ADVANCE_PERIOD;
        m_maxGameTime = max( m_maxGameTime, 0 );

        if( !m_gameTimeWarning && 
            m_maxGameTime <= (10 * 60) &&
            g_app->GetGame()->GetOptionValue("MaxGameRealTime") >= 10 )
        {
            g_app->GetInterface()->ShowMessage( 0, 0, -1, LANGUAGEPHRASE("message_ten_minute_warning"), true );
            m_gameTimeWarning = true;

            if( g_app->m_hidden )
            {
                g_app->GetStatusIcon()->SetSubIcon( STATUS_ICON_TIMER );
                g_app->GetStatusIcon()->SetCaption( LANGUAGEPHRASE("tray_icon_ten_minute_warning") );
            }
        }
    }


    // Remove all remaining units and calculate total nukes when Defcon 3 hits

    int defcon = g_app->GetWorld()->GetDefcon();
    if( defcon != m_lastKnownDefcon )
    {
        if( !g_app->GetTutorial() ||
            g_app->GetTutorial()->GetCurrentLevel() == 7 )
        {
            if( defcon == 3 )
            {
                for( int i = 0; i < g_app->GetWorld()->m_teams.Size(); ++i )
                {
                    Team *team = g_app->GetWorld()->m_teams[i];
                    team->m_unitCredits = 0;
                    for( int j = 0; j < WorldObject::NumObjectTypes; ++j )
                    {
                        team->m_unitsAvailable[j] = 0;
                    }
                
                }

                CountNukes();
                for( int i = 0; i < g_app->GetWorld()->m_teams.Size(); ++i )
                {
                    Team *team = g_app->GetWorld()->m_teams[i];
                    m_totalNukes[team->m_teamId] = m_nukeCount[team->m_teamId];
                }

                EclRemoveWindow( "Side Panel" );
                EclRemoveWindow( "Placement" );                
            }
        }
        m_lastKnownDefcon = defcon;
    }


    //
    // Has somebody won?

    if( m_winner == -1 )
    {
        //
        // If the game is counting down...

        if( m_victoryTimer > 0 )
        {
            m_victoryTimer -= SERVER_ADVANCE_PERIOD * g_app->GetWorld()->GetTimeScaleFactor();
            m_victoryTimer = max( m_victoryTimer, 0 );
        }

        m_recalcTimer -= SERVER_ADVANCE_PERIOD;
        if( m_recalcTimer <= 0 )
        {
            m_recalcTimer = 3;

            //
            // Recalculate the scores

            CountNukes();
            CalculateScores();
    
    
            //
            // Look at nukes remaining
            // If there are few enough nukes, start the timer
            if( !m_lockVictoryTimer &&
                 m_victoryTimer < 0 )
            {            
                int totalNukeCount = 0;
                int totalMaxNukeCount = 0;
                int numTeams = g_app->GetWorld()->m_teams.Size();
                for( int t = 0; t < numTeams; ++t )
                {
                    Team *team = g_app->GetWorld()->m_teams[t];
                    totalNukeCount += m_nukeCount[team->m_teamId];
                    totalMaxNukeCount += m_totalNukes[team->m_teamId];
                }

                float averageNukeCount = totalNukeCount / (float) numTeams;
                float averageTotalCount = totalMaxNukeCount / (float) numTeams;
                float victoryNukeCount = averageTotalCount * GetOptionValue("VictoryTrigger") / 100.0f;

                if( averageNukeCount <= victoryNukeCount )
                {
                    m_victoryTimer = GetOptionValue("VictoryTimer");
                    m_victoryTimer *= 60;       //to get it into minutes

                    if( m_victoryTimer > 0 )
                    {
                        g_app->GetInterface()->ShowMessage( 0, 0, -1, LANGUAGEPHRASE("message_victory_timer"), true );
                        g_soundSystem->TriggerEvent( "Interface", "DefconChange" );

                        if( g_app->m_hidden )
                        {
                            g_app->GetStatusIcon()->SetSubIcon( STATUS_ICON_TIMER );
                            g_app->GetStatusIcon()->SetCaption( LANGUAGEPHRASE("tray_icon_victory_timer") );
                        }
                    }
                }
            }

            //
            // If the countdown has finished
            // Declare the winner now!

            if( m_victoryTimer == 0 || m_maxGameTime == 0 )
            {
                CalculateScores();
    
                m_winner = -1;
                int winningScore = 0;
                for( int t = 0; t < g_app->GetWorld()->m_teams.Size(); ++t )
                {
                    Team *team = g_app->GetWorld()->m_teams[t];
                    int score = GetScore(team->m_teamId);
                    g_app->GetClientToServer()->SendTeamScore( team->m_teamId, score );
                    if( score > winningScore )
                    {
                        winningScore = score;
                        m_winner = team->m_teamId;
                    }
                }

                int numPlayers = g_app->GetWorld()->m_teams.Size();

                char msg[128];
                if( m_winner != -1 )
                {
                    strcpy(msg, LANGUAGEPHRASE("message_victory"));
                    LPREPLACESTRINGFLAG( 'T', g_app->GetWorld()->GetTeam(m_winner)->GetTeamName(), msg );
                    
                    if( m_winner == g_app->GetWorld()->m_myTeamId &&
                        numPlayers > 1 )
                    {
                    }
                }
                else
                {
                    strcpy(msg, LANGUAGEPHRASE("message_stalemate"));
                    m_winner = 999;
                }
                strupr(msg);
                g_app->GetInterface()->ShowMessage( 0, 0, m_winner, msg, true );

                g_app->GetMapRenderer()->m_renderEverything = true;
                for( int i = 0; i < g_app->GetWorld()->m_teams.Size(); ++i )
                {
                    g_app->GetWorld()->m_teams[i]->m_desiredGameSpeed = 0;
                }

                if( !EclGetWindow( "Stats" ) )
                {
                    EclRegisterWindow( new StatsWindow()  );
                }

                g_soundSystem->StopAllSounds( SoundObjectId(), "StartMusic StartMusic" );
                g_soundSystem->TriggerEvent( "Interface", "GameOver" );

                int specVisible = g_app->GetGame()->GetOptionValue("SpectatorChatChannel");
                if( specVisible == 0 &&
                    g_app->GetWorld()->m_spectators.Size() )
                {
                    g_app->GetWorld()->AddChatMessage( -1, CHATCHANNEL_PUBLIC, LANGUAGEPHRASE("message_spectators_chat_players"), -1, false );
                }
            }
        }
    }
}


int Game::GetScore( int _teamId )
{
    return m_score[_teamId];
}

void Game::ResetGame()
{
    m_gameTimeWarning = false;
    m_lockVictoryTimer = false;
    m_winner = -1;
    m_victoryTimer = -1;
    for( int i = 0; i < MAX_TEAMS; ++i )
    {
        m_score[i] = 0;
        m_nukeCount[i] = 0;
    }
}

// ============================================================================


void GameOption::Copy( GameOption *_option )
{
    memcpy( m_name, _option->m_name, sizeof(m_name) );
    m_min = _option->m_min;
    m_max = _option->m_max;
    m_default = _option->m_default;
    m_change = _option->m_change;
    m_currentValue = _option->m_currentValue;
    memcpy( m_currentString, _option->m_currentString, sizeof(m_currentString) );

    m_subOptions.Empty();
    for( int i = 0; i < _option->m_subOptions.Size(); ++i )
    {
        char *subOption = _option->m_subOptions[i];
        m_subOptions.PutData( strdup(subOption) );
    }
}


char* GameOption::TranslateValue( char *_value )
{
	char optionName[256];
	snprintf( optionName, sizeof(optionName), "gameoption_%s", _value );
	optionName[ sizeof(optionName) - 1 ] = '\0';
	for( char *cur = optionName; *cur; cur++ )
	{
		if( *cur == ' ' )
		{
			*cur = '_';
		}
	}
	if( g_languageTable->DoesTranslationExist( optionName ) || g_languageTable->DoesPhraseExist( optionName ) )
	{
		return LANGUAGEPHRASE(optionName);
	}
	return _value;
}
