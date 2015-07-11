#include "lib/universal_include.h"
#include "lib/hi_res_time.h"
#include "lib/tosser/directory.h"
#include "lib/metaserver/metaserver.h"
#include "lib/metaserver/metaserver_defines.h"
#include "lib/render/renderer.h"
#include "lib/language_table.h"

#include "interface/serverbrowser_window.h"
#include "interface/components/message_dialog.h"
#include "interface/connecting_window.h"
#include "interface/badkey_window.h"

#include "network/ClientToServer.h"
#include "network/Server.h"

#include "app/app.h"
#include "app/globals.h"
#include "app/game.h"
#include "app/game_history.h"
#include "app/modsystem.h"

#include "world/world.h"

#include "serverdetails_window.h"

class ConnectToDetailsButton : public InterfaceButton
{
    void MouseUp()
    {
        ServerDetailsWindow *parent = (ServerDetailsWindow *)m_parent;
        Directory *server = MetaServer_GetServer( parent->m_serverIp, parent->m_serverPort, false );
		ServerBrowserWindow::ConnectToServer( server );
    }
};



ServerDetailsWindow::ServerDetailsWindow( char *_serverIp, int _serverPort )
:   InterfaceWindow( "Server Details" ),
    m_details(NULL),
    m_refreshTimer(0.0f),
    m_numRefreshes(0)
{
    char name[512];
	sprintf( name, LANGUAGEPHRASE("dialog_server_details") );

	LPREPLACESTRINGFLAG( 'I', _serverIp, name );
	LPREPLACEINTEGERFLAG( 'P', _serverPort, name );
    strupr( name );
    char *uniqueName = EclGenerateUniqueWindowName( name );


    SetName( uniqueName );
    SetTitle( name );

    strcpy( m_serverIp, _serverIp );
    m_serverPort = _serverPort;

    SetSize( 290, 360 );
}


void ServerDetailsWindow::Create()
{
    InterfaceWindow::Create();

    InvertedBox *box = new InvertedBox();
    box->SetProperties( "invert", 10, 30, m_w - 20, m_h - 95, " ", " ", false, false );
    RegisterButton( box );

    ConnectToDetailsButton *connect = new ConnectToDetailsButton();
    connect->SetProperties( "Connect", 40, m_h - 55, m_w - 80, 20, "dialog_connect", " ", true, false ) ;
    RegisterButton( connect );

    CloseButton *close = new CloseButton();
    close->SetProperties( "Close", 40, m_h - 30, m_w - 80, 20, "dialog_close", " ", true, false );
    RegisterButton( close );
}


void ServerDetailsWindow::Render( bool _hasFocus )
{
    InterfaceWindow::Render( _hasFocus );


    if( m_details )
    {
        //
        // Get all the server details

        char *serverName = m_details->GetDataString    ( NET_METASERVER_SERVERNAME );
        char *gameName = m_details->GetDataString      ( NET_METASERVER_GAMENAME );
        char *gameVer = m_details->GetDataString       ( NET_METASERVER_GAMEVERSION );

        int numTeams = m_details->GetDataUChar         ( NET_METASERVER_NUMTEAMS );
        int maxTeams = m_details->GetDataUChar         ( NET_METASERVER_MAXTEAMS );
        int numSpectators = m_details->GetDataUChar    ( NET_METASERVER_NUMSPECTATORS );
        int maxSpectators = m_details->GetDataUChar    ( NET_METASERVER_MAXSPECTATORS );
        int inProgress = m_details->GetDataUChar       ( NET_METASERVER_GAMEINPROGRESS );
        bool isDemoServer = m_details->HasData         ( NET_METASERVER_DEMOSERVER );
        bool demoRestricted = m_details->HasData       ( NET_METASERVER_DEMORESTRICTED );
    
        float x = m_x + 20;
        float y = m_y + 25;
        float w = 100;
        float h = 12;
        float g = 4;

        Colour titleColour(200,200,255,200);
        Colour dataColour(255,255,255,200);


        //
        // Render the basic server details

        g_renderer->TextSimple( x, y+=h+g, titleColour, h, LANGUAGEPHRASE("dialog_server_name") );
        g_renderer->TextSimple( x+w, y, dataColour, h, serverName );

        g_renderer->TextSimple( x, y+=h+g, titleColour, h,  LANGUAGEPHRASE("dialog_server_identity_address") );
        g_renderer->Text( x+w, y, dataColour, h, "%s : %d", m_serverIp, m_serverPort );

        g_renderer->TextSimple( x, y+=h+g, titleColour, h,  LANGUAGEPHRASE("dialog_server_game_name") );
        g_renderer->Text( x+w, y, dataColour, h, "%s %s", gameName, gameVer );


        if( m_details->HasData( NET_METASERVER_GAMEMODE, DIRECTORY_TYPE_CHAR ) )
        {
            int gameMode = m_details->GetDataUChar( NET_METASERVER_GAMEMODE );

            char gameModeName[256];
            sprintf( gameModeName, LANGUAGEPHRASE("unknown") );
            GameOption *option = g_app->GetGame()->GetOption("GameMode");        
            if( option->m_subOptions.ValidIndex(gameMode) )
            {
                sprintf( gameModeName, GameOption::TranslateValue( option->m_subOptions[gameMode] ) );
            }

            g_renderer->TextSimple( x, y+=h+g, titleColour, h, LANGUAGEPHRASE("dialog_server_game_mode") );
            g_renderer->TextSimple( x+w, y, dataColour, h, gameModeName );
        }


        if( m_details->HasData( NET_METASERVER_SCOREMODE, DIRECTORY_TYPE_CHAR ) )
        {
            int gameMode = m_details->GetDataUChar( NET_METASERVER_SCOREMODE );

            char scoreModeName[256];
            sprintf( scoreModeName, LANGUAGEPHRASE("unknown") );
            GameOption *option = g_app->GetGame()->GetOption("ScoreMode");
            if( option->m_subOptions.ValidIndex(gameMode) )
            {
                sprintf( scoreModeName, GameOption::TranslateValue( option->m_subOptions[gameMode] ) );
            }

            g_renderer->TextSimple( x, y+=h+g, titleColour, h, LANGUAGEPHRASE("dialog_server_score_mode") );
            g_renderer->TextSimple( x+w, y, dataColour, h, scoreModeName );
        }


        char *inProgressString = ( inProgress == 1 ? LANGUAGEPHRASE("dialog_server_started") : inProgress == 2 ? LANGUAGEPHRASE("dialog_server_ended") : LANGUAGEPHRASE("dialog_server_not_yet_started") );
        g_renderer->TextSimple( x, y+=h+g, titleColour, h, LANGUAGEPHRASE("dialog_server_browser_teams") );
        g_renderer->Text( x+w, y, dataColour, h, "%d/%d  %s", numTeams, maxTeams, inProgressString );

        g_renderer->TextSimple( x, y+=h+g, titleColour, h, LANGUAGEPHRASE("dialog_spectators") );
        g_renderer->Text( x+w, y, dataColour, h, "%d/%d", numSpectators, maxSpectators );
        

        if( m_details->HasData( NET_METASERVER_GAMETIME, DIRECTORY_TYPE_INT ) )
        {
            Date date;
            date.AdvanceTime( m_details->GetDataInt(NET_METASERVER_GAMETIME) );

            g_renderer->TextSimple( x, y+=h+g, titleColour, h, LANGUAGEPHRASE("dialog_server_game_time") );
            g_renderer->TextSimple( x+w, y, dataColour, h, date.GetTheDate() );
        }


        //
        // Render the player names and scores

        Directory *playerNames = m_details->GetDirectory( NET_METASERVER_PLAYERNAME );
        if( playerNames )
        {            
            y+=h;
            g_renderer->TextSimple( x, y+=h+g, titleColour, h, LANGUAGEPHRASE("dialog_server_scores") );

            for( int i = 0; i < playerNames->m_subDirectories.Size(); ++i )
            {
                if( playerNames->m_subDirectories.ValidIndex(i) )
                {
                    y+=h+g;
                    Directory *thisPlayer = playerNames->m_subDirectories[i];

                    char *playerName = thisPlayer->GetDataString( NET_METASERVER_PLAYERNAME );
                    int score = thisPlayer->GetDataInt( NET_METASERVER_PLAYERSCORE );
                    int alliance = thisPlayer->GetDataInt( NET_METASERVER_PLAYERALLIANCE );

                    Colour allianceCol = World::GetAllianceColour( alliance );

                    g_renderer->TextSimple( x+10, y, allianceCol, h, playerName );                    
                    g_renderer->Text( x + m_w - 60, y, allianceCol, h, "%d", score );
                }
            }
        }
        else
        {
            y+=h;
            Colour col = titleColour;
            if( fmodf(GetHighResTime()*2, 2) < 1.0f ) col.m_a = 100;
            g_renderer->TextSimple( x, y+=h+g, col, h, LANGUAGEPHRASE("dialog_retrieving_scores") );
        }
    }
}


void ServerDetailsWindow::Update()
{
    //
    // Request new data every few seconds

    double timeNow = GetHighResTime();

    if( timeNow > m_refreshTimer )
    {
        if( m_numRefreshes < 5 )    m_refreshTimer = timeNow + 1.0f;
        else                        m_refreshTimer = timeNow + 10.0f;
        ++m_numRefreshes;

        Directory *newDetails = MetaServer_GetServer( m_serverIp, m_serverPort, false );

        if( !newDetails )
        {
            EclRemoveWindow( m_name );
            return;
        }
        else
        {
            // Only accept server details that include player names
            // Unless our existing details doesnt include player names

            bool alreadyHavePlayerNames = m_details &&
                                          m_details->GetDirectory( NET_METASERVER_PLAYERNAME );

            if( newDetails->GetDirectory( NET_METASERVER_PLAYERNAME ) ||
                !alreadyHavePlayerNames )
            {
                if( m_details ) delete m_details;
                m_details = newDetails;
                m_numRefreshes += 10;
            }
            else
            {
                delete newDetails;
            }
        }

        MetaServer_RequestServerDetails( m_serverIp, m_serverPort );
    }
}



