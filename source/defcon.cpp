    
/*
 * ======
 * DEFCON
 * ======
 *
 * This file started on 2nd August 2003
 * Renamed to Defcon from Wargames on 24th November 2005
 *
 */

#include "lib/universal_include.h"

#include <unrar/rarbloat.h>
#include <unrar/sha1.h>
#include <float.h>
#include <time.h>

#include "lib/gucci/input.h"
#include "lib/hi_res_time.h"
#include "lib/debug_utils.h"
#include "lib/profiler.h"
#include "lib/language_table.h"
#include "lib/math/math_utils.h"
#include "lib/math/random_number.h"
#include "lib/sound/soundsystem.h"
#include "lib/eclipse/eclipse.h"
#include "lib/filesys/filesys_utils.h"
#include "lib/metaserver/authentication.h"
#include "lib/preferences.h"
#include "lib/netlib/net_mutex.h"

#include "app/globals.h"
#include "app/app.h"
#include "app/game.h"
#include "app/modsystem.h"
#include "app/version_manager.h"

#include "network/ClientToServer.h"
#include "network/Server.h"
#include "network/network_defines.h"

#include "world/world.h"
#include "world/team.h"
#include "world/fleet.h"

#include "interface/components/message_dialog.h"
#include "interface/lobby_window.h"
#include "interface/connecting_window.h"
#include "interface/interface.h"
#include "interface/badkey_window.h"

#ifdef TARGET_OS_MACOSX
#include "lib/netlib/net_mutex.h"
#include "lib/gucci/window_manager.h"
#include <OpenGL/OpenGL.h>
#endif

NetMutex g_renderLock;
double   g_startTime = FLT_MAX;
double   g_gameTime = 0.0f;
float    g_advanceTime = 0.0f;
double   g_lastServerAdvance = 0.0f;
float    g_predictionTime = 0.0f;
int      g_lastProcessedSequenceId = -2;                         // -2=not yet ready to begin. -1=ready for first update (id=0)


void UpdateAdvanceTime()
{
    double realTime = GetHighResTime();
	g_advanceTime = float(realTime - g_gameTime);
	if (g_advanceTime > 0.25f)
	{
		g_advanceTime = 0.25f;
	}
	g_gameTime = realTime;
    g_predictionTime = float(realTime - g_lastServerAdvance) - 0.1f;
    g_predictionTime = min( g_predictionTime, 1.0f );
}


bool TimeToRender(float _lastRenderTime)
{
#ifndef TESTBED
    if( !g_app->GetClientToServer() ||
        !g_app->GetClientToServer()->m_synchronising  )
    {
        return true;
    }
#else
    if( !g_app->m_gameRunning )
    {
        return true;
    }
#endif


    //
    // If we are connecting to a game
    // Limit the frame rate to 20fps
    // to ensure glFlip doesn't hog the CPU time

    double timeNow = GetHighResTime();
    double timeSinceLastRender = timeNow - _lastRenderTime;

    return( timeSinceLastRender > 0.05f );   
}


void DumpWorldToSyncLog( char *_filename )
{
    FILE *file = fopen( _filename, "a" );
    if( !file ) return;

    fprintf( file, "\n\nBegin Final Object Positions\n" );

    for( int i = 0; i < g_app->GetWorld()->m_teams.Size(); ++i )
    {
        Team *team = g_app->GetWorld()->m_teams[i];
        for( int j = 0; j < team->m_fleets.Size(); ++j )
        {
            Fleet *fleet = team->m_fleets[j];
            char *state = fleet->LogState();
            fprintf( file, "%s\n", state );
        }
    }

    for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
    {
        if( g_app->GetWorld()->m_objects.ValidIndex(i) )
        {
            WorldObject *obj = g_app->GetWorld()->m_objects[i];
            char *state = obj->LogState();
            fprintf( file, "%s\n", state );
        }
    }

    fclose(file);
}


void DumpSyncLogs( char *syncId )
{
    static float s_lastDump = 0.0f;
    if( GetHighResTime() > s_lastDump + 10.0f )
    {
        s_lastDump = GetHighResTime();

        char filename[256];
        sprintf( filename, "synclog %s c%d.txt", 
                            syncId, 
                            g_app->GetClientToServer()->m_clientId );
        
        if( !DoesFileExist( filename ) )
        {
            DumpSyncRandLog( filename );
            DumpWorldToSyncLog( filename );

            AppDebugOut( "SYNCERROR log dumped to '%s'\n", filename );
        }
    }
}


void TestBedSetupMods()
{
    g_modSystem->DeActivateAllMods();


    if( AppRandom() % 10 > 3 && 
        g_modSystem->m_mods.Size() > 0 )
    {
        // Pick a random Mod to run with

        int modIndex = AppRandom() % g_modSystem->m_mods.Size();
        InstalledMod *mod = g_modSystem->m_mods[modIndex];
        g_modSystem->ActivateMod( mod->m_name, mod->m_version );
    }


    g_modSystem->Commit();
	g_app->RestartAmbienceMusic();
}


void RestartTestBed( int result, char *notes )
{
    if( g_app->GetServer() )
    {
        //
        // Log result

        FILE *file = fopen("testbed-results.txt", "a");
        if( file )
        {
            char *resultString = NULL;
            switch( result )
            {
                case -1:    resultString = "FAILED";            break;
                case 1:     resultString = "SUCCESS";           break;
            }

            time_t theTimeT = time(NULL);
            tm *theTime = localtime(&theTimeT);

            char *modPath = g_preferences->GetString( "ModPath" );

            fprintf( file, "%02d-%02d-%02d    %02d:%02d.%02d        %15s        %s        %s\n", 
                            1900+theTime->tm_year, 
                            theTime->tm_mon+1,
                            theTime->tm_mday,
                            theTime->tm_hour,
                            theTime->tm_min,
                            theTime->tm_sec,
                            modPath,
                            resultString,
                            notes ); 
            fclose( file );
        }



        //
        // Shut down game

        g_app->ShutdownCurrentGame();
        EclRemoveAllWindows();
        g_app->GetInterface()->OpenSetupWindows();

        //
        // Start some random Mods

        TestBedSetupMods();

        //
        // New game

        LobbyWindow *lobby = new LobbyWindow();
        lobby->StartNewServer();
        EclRegisterWindow( lobby );
    }
    else
    {
        //
        // We are a client 
        // Shut down game
        // Remember the server we were connected to

        char serverIp[256];
        int serverPort=0;
        int numTeams = 0;
        g_app->GetClientToServer()->GetServerDetails( serverIp, &serverPort );

        g_app->ShutdownCurrentGame();
        EclRemoveAllWindows();
        g_app->GetInterface()->OpenSetupWindows();

        Sleep(3000);


        //
        // Parse the settings file

        FILE *file = fopen( "testbed.txt", "rt" );
        if( file )
        {
            fscanf( file, "IP      %s\n", serverIp );
            fscanf( file, "PORT    %d\n", &serverPort );
            fscanf( file, "TEAMS   %d\n", &numTeams );
            fclose(file);
        }


        //
        // Attempt rejoin
        // Connect to the new server
        // Open up game windows

        g_app->GetClientToServer()->ClientJoin( serverIp, serverPort );
        g_app->InitWorld();

        ConnectingWindow *connectWindow = new ConnectingWindow();
        connectWindow->m_popupLobbyAtEnd = true;
        EclRegisterWindow( connectWindow );
    }
}

bool ProcessServerLetters( Directory *letter )
{
    if( strcmp( letter->m_name, NET_DEFCON_MESSAGE ) != 0 ||
        !letter->HasData( NET_DEFCON_COMMAND ) )
    { 
        AppDebugOut( "Client received bogus message, discarded (4)\n" );
        return true;
    }


    char *cmd = letter->GetDataString( NET_DEFCON_COMMAND );


    if( strcmp( cmd, NET_DEFCON_CLIENTHELLO ) == 0 )
    {
        int clientId = letter->GetDataInt(NET_DEFCON_CLIENTID);
        if( clientId == g_app->GetClientToServer()->m_clientId )
        {
            g_app->GetClientToServer()->m_connectionState = ClientToServer::StateConnected;
            AppDebugOut( "CLIENT : Received HelloClient from Server\n" );
            if( !g_app->GetTutorial() )
            {
                g_app->GetClientToServer()->RequestTeam( Team::TypeLocalPlayer );
            }
        }

        // If this is a Demo client, make a note of that
        if( letter->HasData( NET_DEFCON_CLIENTISDEMO ) )
        {
            g_app->GetClientToServer()->SetClientDemo( clientId );
        }

        // This might be a client rejoining the game
        // Need to give him his teams back
        g_app->GetWorld()->ReassignTeams( clientId );        

        return true;
    }
    else if( strcmp( cmd, NET_DEFCON_CLIENTGOODBYE ) == 0 )
    {
        int clientId = letter->GetDataInt(NET_DEFCON_CLIENTID);
        int reason = letter->GetDataInt(NET_DEFCON_DISCONNECT);

        AppDebugOut( "CLIENT : Client %d left the game\n", clientId );
        g_app->GetWorld()->RemoveTeams( clientId, reason );
        g_app->GetWorld()->RemoveSpectator( clientId );
        g_app->GetClientToServer()->SetSyncState( clientId, true );
        return true;
    }
    else if( strcmp( cmd, NET_DEFCON_CLIENTID ) == 0 )
    {
        int clientId = letter->GetDataInt(NET_DEFCON_CLIENTID);        
        AppDebugOut( "CLIENT : Received ClientID of %d\n", clientId );        

        if( g_app->GetClientToServer()->m_clientId != -1 )
        {
            AppAssert( g_app->GetClientToServer()->m_clientId == clientId );
            return true;
        }

        g_app->GetClientToServer()->m_clientId = clientId;
        g_app->GetClientToServer()->m_connectionState = ClientToServer::StateHandshaking;
        g_lastProcessedSequenceId = -1;

        if( letter->HasData( NET_DEFCON_VERSION, DIRECTORY_TYPE_STRING ) )
        {
            char *serverVersion = letter->GetDataString( NET_DEFCON_VERSION );
            strcpy( g_app->GetClientToServer()->m_serverVersion, serverVersion );
            AppDebugOut( "CLIENT : Server version is %s\n", serverVersion );
        }

        if( !VersionManager::DoesSupportModSystem( g_app->GetClientToServer()->m_serverVersion ) )
        {
            // This server is too old to support Mods, so make sure we de-activate any critical ones
            g_modSystem->DeActivateAllCriticalMods();
            if( g_modSystem->CommitRequired() )
            {
                g_modSystem->Commit();
				g_app->RestartAmbienceMusic();
            }
        }

        return true;
    }
    else if( strcmp( cmd, NET_DEFCON_TEAMASSIGN ) == 0 )
    {
        int clientId = letter->GetDataInt(NET_DEFCON_CLIENTID);        
        int teamId   = letter->GetDataInt(NET_DEFCON_TEAMID);
        int teamType = letter->GetDataInt(NET_DEFCON_TEAMTYPE);

        if( teamType != Team::TypeAI &&
            clientId != g_app->GetClientToServer()->m_clientId )
        {
            teamType = Team::TypeRemotePlayer;
        }

        g_app->GetWorld()->InitialiseTeam(teamId, teamType, clientId );
        return true;
    }
    else if( strcmp( cmd, NET_DEFCON_SPECTATORASSIGN ) == 0 )
    {
        int clientId = letter->GetDataInt(NET_DEFCON_CLIENTID);        
        g_app->GetWorld()->InitialiseSpectator(clientId);

        if( clientId == g_app->GetClientToServer()->m_clientId )
        {            
            AppDebugOut( "CLIENT: I am a spectator\n" );
        }
				
        return true;
    }
    else if( strcmp( cmd, NET_DEFCON_NETSYNCERROR ) == 0 )
    {
        int clientId = letter->GetDataInt(NET_DEFCON_CLIENTID);        
        AppDebugOut( "SYNCERROR Server informed us that Client %d is out of Sync\n", clientId );

        g_app->GetClientToServer()->SetSyncState( clientId, false );

        char *syncId = letter->GetDataString( NET_DEFCON_SYNCERRORID );
        DumpSyncLogs(syncId);

        return true;
    }
    else if( strcmp( cmd, NET_DEFCON_NETSYNCFIXED ) == 0 )
    {
        int clientId = letter->GetDataInt(NET_DEFCON_CLIENTID);        
        AppDebugOut( "SYNCFIXED Server informed us that Client %d has repaired his Sync Error\n", clientId );
        g_app->GetClientToServer()->SetSyncState( clientId, true );
        return true;
    }
    else if( strcmp( cmd, NET_DEFCON_DISCONNECT ) == 0 )
    {
        if( g_app->GetClientToServer()->m_connectionState > ClientToServer::StateDisconnected )
        {
            g_app->ShutdownCurrentGame();
            if ( EclGetWindow("LOBBY") )
				EclRemoveWindow( "LOBBY" );
			if ( EclGetWindow("Comms Window") )
				EclRemoveWindow( "Comms Window" );
			if ( EclGetWindow("Preparing Game...") )
				EclRemoveWindow("Preparing Game..." );

            g_app->GetInterface()->OpenSetupWindows();

            char *reason;
			char *reasonLanguagePhrase;
            int reasonDisconnectInt = letter->GetDataInt(NET_DEFCON_DISCONNECT);
            switch( reasonDisconnectInt )
            {
                case Disconnect_ClientLeave:            reason = "You have left the game";
					                                    reasonLanguagePhrase = "dialog_disconnect_client_leave";         break;
                case Disconnect_ServerShutdown:         reason = "The server has shutdown";
					                                    reasonLanguagePhrase = "dialog_disconnect_server_shutdown";      break;
                case Disconnect_InvalidKey:             reason = "You are using an invalid key";
					                                    reasonLanguagePhrase = "dialog_disconnect_invalid_key";          break;
                case Disconnect_DuplicateKey:           reason = "You are using a duplicate key";
					                                    reasonLanguagePhrase = "dialog_disconnect_duplicate_key";        break;
                case Disconnect_KeyAuthFailed:          reason = "Key authentication failed";
					                                    reasonLanguagePhrase = "dialog_disconnect_key_auth_failed";      break;
                case Disconnect_BadPassword:            reason = "Invalid Password Entered";
					                                    reasonLanguagePhrase = "dialog_disconnect_bad_password";         break;
                case Disconnect_GameFull:               reason = "Game is already full";
					                                    reasonLanguagePhrase = "dialog_disconnect_game_full";            break;
                case Disconnect_KickedFromGame:         reason = "Kicked by the Server";
					                                    reasonLanguagePhrase = "dialog_disconnect_kicked_from_game";     break;
                case Disconnect_DemoFull:               reason = "Too many Demo Players already";
					                                    reasonLanguagePhrase = "dialog_disconnect_demo_full";            break;
				default:                                reason = "Unknown";
					                                    reasonLanguagePhrase = "dialog_disconnect_unknown";              break;
            }

            if( reasonDisconnectInt == Disconnect_KeyAuthFailed ||
                reasonDisconnectInt == Disconnect_InvalidKey ||
                reasonDisconnectInt == Disconnect_DuplicateKey )
            {
                char authKey[256];
				if( Authentication_IsKeyFound() )
				{
					Authentication_GetKey( authKey );
				}
				else
				{
					sprintf( authKey, LANGUAGEPHRASE("dialog_authkey_not_found") );
				}

                BadKeyWindow *badKey = new BadKeyWindow();

				sprintf( badKey->m_extraMessage, LANGUAGEPHRASE("dialog_auth_error") );
				LPREPLACESTRINGFLAG( 'E', LANGUAGEPHRASE(reasonLanguagePhrase), badKey->m_extraMessage );
				LPREPLACESTRINGFLAG( 'K', authKey, badKey->m_extraMessage );

				EclRegisterWindow(badKey);
            }
            else if( reasonDisconnectInt == Disconnect_DemoFull )
            {
                int maxGameSize;
                int maxDemoPlayers;
                bool allowDemoServers;
                g_app->GetClientToServer()->GetDemoLimitations( maxGameSize, maxDemoPlayers, allowDemoServers );
                BadKeyWindow *badKey = new BadKeyWindow();

                sprintf( badKey->m_extraMessage, LANGUAGEPHRASE("dialog_server_demo_restricted") );
                LPREPLACEINTEGERFLAG( 'N', maxDemoPlayers, badKey->m_extraMessage );

                badKey->m_offerDemo = false;
                EclRegisterWindow(badKey);                
            }
            else
            {
                MessageDialog *dialog = new MessageDialog( "Disconnected", reasonLanguagePhrase, true, "dialog_disconnected", true );
                EclRegisterWindow( dialog );
            }

            AppDebugOut( "CLIENT : Received Disconnect from server : %s\n", reason );

#ifdef TESTBED
            if( letter->GetDataInt(NET_DEFCON_DISCONNECT) == Disconnect_ServerShutdown )
            {
                RestartTestBed(1, "Client Disconnect");
            }
#endif

        }
        return true;            
    }
    else if( strcmp( cmd, NET_DEFCON_SETMODPATH ) == 0 )
    {
        char *modPath = letter->GetDataString( NET_DEFCON_SETMODPATH );
        AppDebugOut( "Server has set the MOD path: '%s'\n", modPath );
        
        if( !g_modSystem->IsCriticalModPathSet( modPath ) )
        {
            if( g_modSystem->CanSetModPath( modPath ) )
            {
                g_modSystem->DeActivateAllCriticalMods();
                g_modSystem->SetModPath( modPath );
            }
            else
            {
                char reason[8192];
                sprintf( reason, LANGUAGEPHRASE("dialog_error_mod_path_caption") );

                char modPathCopy[4096];
                strcpy( modPathCopy, modPath );
                LList<char *> *tokens = g_modSystem->ParseModPath( modPathCopy );

                for( int i = 0; i < tokens->Size(); i+=2 )
                {
                    char *modName = tokens->GetData(i);
                    char *version = tokens->GetData(i+1);

                    strcat( reason, modName );
                    strcat( reason, " " );
                    strcat( reason, version );

                    if( !g_modSystem->IsModInstalled( modName, version ) )
                    {
                        strcat( reason, "    " );
                        strcat( reason, LANGUAGEPHRASE("dialog_mod_not_installed") );
                    }
                    
                    strcat( reason, "\n" );
                }

                delete tokens;
                
                MessageDialog *dialog = new MessageDialog( "Error setting MOD path", reason, false, "dialog_error_mod_path_title", true );
                EclRegisterWindow( dialog );
            }
        }

        return true;
    }
    else
    {
        return false;
    }
}

inline
void Hash( hash_context &c, unsigned i )
{
       unsigned char syncBuffer[4] = {
               i & 0xFF,
               (i >> 8) & 0xFF,
               (i >> 16) & 0xFF,
               (i >> 24) & 0xFF
       };

       hash_process( &c, (unsigned char *)syncBuffer, 4 );
}

inline
void Hash( hash_context &c, int i )
{
       Hash( c, (unsigned) i );
}

inline 
void Hash( hash_context &c, float f )
{
	union 
	{
		float floatPart;
		unsigned intPart;
	} value;

	value.floatPart = f;	
	Hash( c, value.intPart );
}

inline
void Hash( hash_context &c, double d )
{
	union
	{
		double doublePart;
		unsigned long long intPart;
	} value;
	
	value.doublePart = d;
	const unsigned long long i = value.intPart;
	
	unsigned char syncBuffer[8] = {
		i & 0xFF,
		(i >> 8) & 0xFF,
		(i >> 16) & 0xFF,
		(i >> 24) & 0xFF,
		(i >> 32) & 0xFF,
		(i >> 40) & 0xFF,
		(i >> 48) & 0xFF,
		(i >> 56) & 0xFF,
	};
	
	hash_process( &c, (unsigned char *)syncBuffer, 8 );
}

inline
void Hash( hash_context &c, const Fixed &f )
{
       Hash(c, f.DoubleValue() );
}

unsigned char GenerateSyncValue()
{
    START_PROFILE( "GenerateSyncValue" );

    //
    // Generate a number between 0 and 255 that represents every unit in game
    // So if a single one is different, we will know immediately
    
    unsigned char result = 0;


    //
    // Objects

    hash_context c;
    hash_initial(&c);

#ifdef TRACK_SYNC_RAND
    for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
    {
        if( g_app->GetWorld()->m_objects.ValidIndex(i) )
        {
            WorldObject *obj = g_app->GetWorld()->m_objects[i];
			
			Hash( c, obj->m_longitude );
			Hash( c, obj->m_latitude ); 
			Hash( c, obj->m_vel.x ); 
			Hash( c, obj->m_vel.y ); 
			Hash( c, obj->m_currentState );
        }
    }
#endif

    //
    // Random value
    
	Hash( c, syncfrand(255) );
	
	uint32 hashResult[5];
	hash_final(&c, hashResult);
	result = hashResult[0] & 0xFF;
		
    END_PROFILE( "GenerateSyncValue" );

    return result;
}


void HandleGameStart()
{
    //
    // Start the game if everyone is ready

    if( !g_app->m_gameRunning )
    {
#ifdef TESTBED
        int maxTeams = g_app->GetGame()->GetOptionValue("MaxTeams");
        int numTeams = g_app->GetWorld()->m_teams.Size();
        if( numTeams == maxTeams ) 
        {
            Team *myTeam = g_app->GetWorld()->GetMyTeam();
            if( myTeam && !myTeam->m_readyToStart )
            {
                g_app->GetClientToServer()->RequestStartGame( myTeam->m_teamId, (unsigned char) time(NULL) );
            }
        }
#endif

        bool ready = true;

        for( int i = 0; i < g_app->GetWorld()->m_teams.Size(); ++i )
        {
            Team *team = g_app->GetWorld()->m_teams[i];
            if( team->m_type == Team::TypeUnassigned || 
                !team->m_readyToStart )
            {
                ready = false;
                break;
            }
        }

        if( g_app->GetWorld()->m_teams.Size() == 0 )
        {
            ready = false;
        }

        if( ready )
        {
            if( g_app->m_gameStartTimer < 0 )
            {
                g_app->m_gameStartTimer = 4;                
            }
            else if( g_app->m_gameStartTimer > 0 )
            {
                g_app->m_gameStartTimer -= 0.5f;
                if( g_app->m_gameStartTimer <= 0 )
                {
                    g_app->StartGame();     
                }
            }
        }
        else
        {
            g_app->m_gameStartTimer = -1;
        }
    }
}

void DefconMain()
{
    g_app = new App();
    g_app->MinimalInit();

	g_app->FinishInit();

    double nextServerAdvanceTime = GetHighResTime();
    double serverAdvanceStartTime = -1;
    double lastRenderTime = GetHighResTime();

    bool serverRunning = false;

#ifdef TESTBED
    FILE *file = fopen("testbed-results.txt", "w" );
    if( file ) fclose( file );
#endif

    //
    // Main loops

	while(true)
    {
        //
        // Get the time
        double timeNow = GetHighResTime();

        //
        // Advance the server
        if( g_app->GetServer() && g_app->GetServer()->IsRunning() )
        {
            if( !serverRunning )
            {
                nextServerAdvanceTime = GetHighResTime();
                serverAdvanceStartTime = -1;
                serverRunning = true;
            }


#ifdef TESTBED
            if( g_app->m_gameRunning )
            {
                if( g_app->GetServer()->TestBedReadyToContinue() ||
                    timeNow > nextServerAdvanceTime )
                {
                    g_app->GetServer()->Advance();
                    nextServerAdvanceTime = GetHighResTime() + 0.3f;
                }
            }
            else
            {
                if( timeNow > nextServerAdvanceTime )            
                {
                    g_app->GetServer()->Advance();
                    nextServerAdvanceTime = GetHighResTime() + 0.5f;
                }
            }            
#else
            if( timeNow > nextServerAdvanceTime )            
            {
                g_app->GetServer()->Advance();
                float timeToAdd = SERVER_ADVANCE_PERIOD.DoubleValue();
                if( !g_app->m_gameRunning ) timeToAdd *= 5.0f;
                nextServerAdvanceTime += timeToAdd;
                if (timeNow > nextServerAdvanceTime)
                {
                    nextServerAdvanceTime = timeNow + timeToAdd;
                }
            }        
#endif
        }        

        if( g_app->GetClientToServer() )        
        {            
            START_PROFILE("Client Main Loop");

			g_app->GetClientToServer()->Advance();

#ifdef TESTBED
            if( g_app->m_gameRunning &&
                g_app->GetGame()->m_winner != -1 )
            {
                RestartTestBed(1, "Game Over");
            }

            if( g_app->GetClientToServer()->m_outOfSyncClients.Size() )
            {                
                char notes[256];
                sprintf( notes, "SYNC ERROR at %s", g_app->GetWorld()->m_theDate.GetTheDate() );
                RestartTestBed(-1, notes );
            }

            if( g_app->m_gameRunning &&
                g_app->GetWorld()->m_theDate.m_theDate > 10 )
            {
                float estimatedLatency = g_app->GetClientToServer()->GetEstimatedLatency();
                if( estimatedLatency > 60.0f )
                {
                    RestartTestBed(-1, "Connection lost" );
                }

                if( g_app->GetServer() &&
                    g_app->GetServer()->m_clients.NumUsed() < g_app->GetGame()->GetOptionValue("MaxTeams") )
                {
                    RestartTestBed(-1, "Client lost connection" );
                }
            }
#endif


            //
            // Read latest update from Server
            Directory *letter = g_app->GetClientToServer()->GetNextLetter();

            if( letter )
            {
                if( !strcmp(letter->m_name, NET_DEFCON_MESSAGE) == 0 ||
                    !letter->HasData(NET_DEFCON_SEQID, DIRECTORY_TYPE_INT) )
                {
                    AppDebugOut( "Client received bogus message, discarded (5)\n" );
                    delete letter;
                }
                else
                {
                    int seqId = letter->GetDataInt( NET_DEFCON_SEQID );                
                    if( seqId == -1 )
                    {
                        // This letter is just for us
                        ProcessServerLetters( letter );
                        delete letter;
                    }
                    else
                    {
                        AppReleaseAssert( seqId == g_lastProcessedSequenceId + 1, "Networking broken!" );

                        bool handled = ProcessServerLetters(letter);
                        if(!handled)
                        {
                            g_app->GetClientToServer()->ProcessServerUpdates( letter );
                        }
                
                        if( g_app->m_gameRunning )
                        {
                            g_app->GetWorld()->Update();
                        }
                        else
                        {
                            HandleGameStart();
                        }

                        g_lastServerAdvance = (float)seqId * SERVER_ADVANCE_PERIOD.DoubleValue() + g_startTime;
                        g_lastProcessedSequenceId = seqId;       

                        delete letter;            
                               
                        if( seqId == 0 && g_app->GetClientToServer()->m_resynchronising > 0.0f )
                        {
                            AppDebugOut( "Client successfully began Resynchronising\n" );
                            g_app->GetClientToServer()->m_resynchronising = -1.0f;
                        }

                        unsigned char sync = GenerateSyncValue();
                        SyncRandLog( "Sync %d = %d", g_lastProcessedSequenceId, sync );
                        g_app->GetClientToServer()->SendSyncronisation( g_lastProcessedSequenceId, sync );
                    }
                }
            }

            END_PROFILE("Client Main Loop");
        }

        //
        // Render

        UpdateAdvanceTime();

        if( TimeToRender(lastRenderTime) )
        {
            lastRenderTime = GetHighResTime();
            g_app->Update();
            g_app->Render();
            g_soundSystem->Advance();
            if( g_profiler ) g_profiler->Advance();
        }
    }
	delete g_app;
	g_app = NULL;
}


void AppMain()
{
    DefconMain();
}

void AppShutdown()
{
	if( g_app && g_app->m_inited )
	{
		g_app->Shutdown();
	}
	else
	{
		exit(0);
	}
}