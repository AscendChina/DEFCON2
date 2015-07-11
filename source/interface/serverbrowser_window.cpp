#include "lib/universal_include.h"
#include "lib/render/renderer.h"
#include "lib/metaserver/metaserver.h"
#include "lib/metaserver/authentication.h"
#include "lib/metaserver/metaserver_defines.h"
#include "lib/metaserver/matchmaker.h"
#include "lib/hi_res_time.h"
#include "lib/tosser/directory.h"
#include "lib/language_table.h"
#include "lib/resource/resource.h"
#include "lib/gucci/window_manager.h"
#include "lib/gucci/input.h"
#include "lib/netlib/net_lib.h"
#include "lib/string_utils.h"
#include "lib/math/random_number.h"
#include "lib/profiler.h"

#include "interface/components/inputfield.h"
#include "interface/components/drop_down_menu.h"
#include "interface/components/message_dialog.h"
#include "interface/components/scrollbar.h"
#include "interface/components/checkbox.h"
#include "interface/serverbrowser_window.h"
#include "interface/serverdetails_window.h"
#include "interface/lobby_window.h"
#include "interface/chat_window.h"
#include "interface/connecting_window.h"
#include "interface/demo_window.h"
#include "interface/badkey_window.h"

#include "network/ClientToServer.h"
#include "network/Server.h"

#include "app/globals.h"
#include "app/app.h"
#include "app/game.h"
#include "app/game_history.h"
#include "app/modsystem.h"



class RequestServerListButton : public InterfaceButton
{
public:
    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        ServerBrowserWindow *sbw = (ServerBrowserWindow *) m_parent;        

        if( !MetaServer_HasReceivedListWAN() &&
            !sbw->m_serverList )
        {
            SetCaption( "dialog_requesting", true );
        }
        else
        {
            int numFound = 0;
            if( sbw->m_serverList ) numFound = sbw->m_serverList->Size();

            char caption[256];
			sprintf( caption, LANGUAGEPHRASE("dialog_refresh_number_server") );
			LPREPLACEINTEGERFLAG( 'S', numFound, caption );
			SetCaption( caption, false );
        }

        InterfaceButton::Render( realX, realY, highlighted, clicked );
    }   
 
    void MouseUp()
    {
        MetaServer_ClearServerList();

        ServerBrowserWindow *sbw = (ServerBrowserWindow *) m_parent;                                
        sbw->m_requestTimer = 0;
        sbw->ClearServerList();
    }
};

class ApplyServerPasswordButton : public InterfaceButton
{
public:
    void MouseUp()
    {
        ServerPasswordWindow *passwindow = (ServerPasswordWindow *)EclGetWindow( "ServerPass");
        Directory server;
        server.CreateData( NET_METASERVER_IP, passwindow->m_ip );
        server.CreateData( NET_METASERVER_PORT, passwindow->m_port );
        server.CreateData( NET_METASERVER_SERVERNAME, passwindow->m_serverName );
        ServerBrowserWindow::ConnectToServer( &server, passwindow->m_password );

        EclRemoveWindow( "ServerPass" );
    }
};


// ============================================================================


ServerPasswordWindow::ServerPasswordWindow()
:   InterfaceWindow("ServerPass", "dialog_server_password", true),
    m_port(-1)
{
    SetSize( 300, 120 );
    Centralise();
    strcpy( m_password, "");
    strcpy( m_ip, "");
    strcpy( m_serverName, "unknown" );
}

void ServerPasswordWindow::Create()
{
    CreateValueControl("Password", 20, 50, m_w-40, 20, InputField::TypeString, &m_password, -1, 0, 20, NULL, " ", false );

    ApplyServerPasswordButton *apb = new ApplyServerPasswordButton();
    apb->SetProperties( "Apply", 10, m_h-25, 100, 19, "dialog_apply", " ", true, false );
    RegisterButton( apb );

    CloseButton *close = new CloseButton();
    close->SetProperties( "Cancel", m_w - 110, m_h-25, 100, 19, "dialog_cancel", " ", true, false );
    RegisterButton( close );

    EclSetCurrentFocus( m_name );
    strcpy( m_currentTextEdit, "Password" );
}

void ServerPasswordWindow::Render( bool hasFocus )
{
    InterfaceWindow::Render( hasFocus );
    g_renderer->TextCentreSimple( m_x + m_w/2, m_y+30, White, 15.0f, LANGUAGEPHRASE("dialog_enterpassword"));
}


// ============================================================================


class JoinGameButton : public InterfaceButton
{
public:
    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        ServerBrowserWindow *sbw = (ServerBrowserWindow *) m_parent;
        InterfaceButton::Render( realX, realY, highlighted, clicked );
    }

    void MouseUp()
    {
        ServerBrowserWindow *sbw = (ServerBrowserWindow *) m_parent;

        if( sbw->m_serverList &&
            sbw->m_serverList->ValidIndex( sbw->m_selection ) )
        {
            Directory *server = sbw->m_serverList->GetData(sbw->m_selection);
            ServerBrowserWindow::ConnectToServer( server );
        }
    }
};


void RenderPlayerSlots( int _x, int _y, int _w, int _h, int _numTeams, int _numHumanTeams, int _maxTeams, int _teamLimit, int _inProgress )
{
    float slotW = _w / (float) _teamLimit;
    float gap = 1;
    slotW -= gap;

    for( int i = 0; i < _teamLimit; ++i )
    {
        float x = _x + ( slotW + gap ) * i;
        float y = _y;
        float h = _h;

        if( _numHumanTeams > i )
        {            
            g_renderer->RectFill( x, y, slotW, h, Colour(0,255,0,255) );
        }
        else if( _numTeams > i )
        {
            g_renderer->RectFill( x, y, slotW, h, Colour(0,200,0,100) );
        }
        else if( _maxTeams > i && _inProgress == 0 )
        {
            g_renderer->RectFill( x, y, slotW, h, Colour(255,255,255,60) );
        }
        else
        {
            //g_renderer->RectFill( x, y, slotW, h, Colour(255,255,255,10) );
        }
    }


}


class GameServerButton : public EclButton
{
public:
    int m_index;

    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        ServerBrowserWindow *sbw = (ServerBrowserWindow *) m_parent;

        int realIndex = sbw->m_scrollBar->m_currentValue + m_index;

        if( sbw->m_serverList &&
            realIndex == sbw->m_serverList->Size()+1 )
        {
            char caption[128];
            sprintf( caption, LANGUAGEPHRASE("dialog_number_server_found") );
            LPREPLACEINTEGERFLAG( 'N', realIndex-1, caption );
            g_renderer->TextCentreSimple( realX + m_w/2, realY-10, DarkGray, 12, caption );
        }

        if( !sbw->m_serverList && m_index == 0 )
        {
            g_renderer->TextCentreSimple( realX + m_w/2, realY + 10, DarkGray, 12, LANGUAGEPHRASE("dialog_retrieving_server_list") );
        }

        if( sbw->m_serverList &&
            sbw->m_serverList->ValidIndex( realIndex ) )
        {
            START_PROFILE( "GetDetails" );

            if( sbw->m_selection == realIndex )
            {
                g_renderer->RectFill( realX, realY, m_w, m_h, Colour(50,50,80,80), Colour(150,150,200,80), true );
            }


            //g_renderer->RectFill( realX, realY, m_w, m_h, Colour(200,200,255,5) );

            Directory *server = sbw->m_serverList->GetData(realIndex);

            char *serverIp  = server->GetDataString     ( NET_METASERVER_IP );
            int serverPort = server->GetDataInt         ( NET_METASERVER_PORT );
            char *serverName = server->GetDataString    ( NET_METASERVER_SERVERNAME );
            char *gameName = server->GetDataString      ( NET_METASERVER_GAMENAME );
            char *gameVer = server->GetDataString       ( NET_METASERVER_GAMEVERSION );
            int numTeams = server->GetDataUChar         ( NET_METASERVER_NUMTEAMS );
            int maxTeams = server->GetDataUChar         ( NET_METASERVER_MAXTEAMS );
            int numDemoTeams = server->GetDataUChar     ( NET_METASERVER_NUMDEMOTEAMS );
            int numSpectators = server->GetDataUChar    ( NET_METASERVER_NUMSPECTATORS );
            int maxSpectators = server->GetDataUChar    ( NET_METASERVER_MAXSPECTATORS );
            int inProgress = server->GetDataUChar       ( NET_METASERVER_GAMEINPROGRESS );
            bool isDemoServer = server->HasData         ( NET_METASERVER_DEMOSERVER );
            bool demoRestricted = server->HasData       ( NET_METASERVER_DEMORESTRICTED );

            SafetyString( serverIp );
            SafetyString( serverName );
            SafetyString( gameName );
            SafetyString( gameVer );


            int hasJoinedGame = g_gameHistory.HasJoinedGame( serverIp, serverPort );
            if( hasJoinedGame > 0 && sbw->m_listType != ServerBrowserWindow::ListTypeRecent )
            {
                int alpha = 30;
                g_renderer->RectFill( realX, realY, m_w, m_h, Colour(155,155,255,alpha) );                
            }

            bool password = false;
            if( server->HasData( NET_METASERVER_PASSWORD, DIRECTORY_TYPE_STRING) )
            {
                password = true;
            }
            
            int protocolMatch = true;
            if( server->HasData( NET_METASERVER_PROTOCOLMATCH, DIRECTORY_TYPE_INT ) )
            {
                protocolMatch = server->GetDataInt( NET_METASERVER_PROTOCOLMATCH );
            }

            char netLocation[256];
            sprintf( netLocation, "%s:%d", serverIp, serverPort );

            char localIp[256];
            int localPort;
            bool lanGame = false;
            g_app->GetClientToServer()->GetIdentity( localIp, &localPort );
            if( stricmp( serverIp, localIp ) == 0 )
            {
				sprintf( netLocation, LANGUAGEPHRASE("dialog_net_location_lan") );
				LPREPLACESTRINGFLAG( 'I', serverIp, netLocation );
				LPREPLACEINTEGERFLAG( 'P', serverPort, netLocation );
                lanGame = true;
            }

            char teamCount[32];
            sprintf( teamCount, "%d/%d", numTeams, maxTeams );

            if( inProgress > 0 )
            {
                sprintf( teamCount, "%d", numTeams );
            }

            char inProgressString[256];

            if( inProgress == 0 )
            {
                sprintf( inProgressString, " " );
            }
            else 
            {
                if( server->HasData( NET_METASERVER_GAMETIME, DIRECTORY_TYPE_INT ) )
                {
                    Date date;
                    date.AdvanceTime( server->GetDataInt(NET_METASERVER_GAMETIME) );
                    sprintf( inProgressString, "%s", date.GetTheDate() );
                }

                if( inProgress == 2 )
                {
					strcat( inProgressString, " " );
                    strcat( inProgressString, LANGUAGEPHRASE("dialog_game_ended") );
                }
            }
            

            char gameNameVer[256];
            sprintf( gameNameVer, "%s", gameVer );

            char serverNameFull[512];
            strcpy( serverNameFull, serverName );

            char spectatorsFull[256];
            sprintf( spectatorsFull, "%d/%d", numSpectators, maxSpectators );
            if( maxSpectators == 0 ) sprintf( spectatorsFull, "-" );

            char gameModeString[256];
            sprintf( gameModeString, " " );

            if( server->HasData( NET_METASERVER_GAMEMODE, DIRECTORY_TYPE_CHAR ) )
            {
                int gameMode = server->GetDataUChar( NET_METASERVER_GAMEMODE );
                if( gameMode > 0 )
                {
                    GameOption *option = g_app->GetGame()->GetOption("GameMode");                
                    if( option->m_subOptions.ValidIndex(gameMode) )
                    {
                        sprintf( gameModeString, GameOption::TranslateValue( option->m_subOptions[gameMode] ) );
                    }
                }
            }

            if( isDemoServer )
            {
                sprintf( gameModeString, LANGUAGEPHRASE("dialog_demo_game_mode") );
            }


            bool modPathOk = true;
            char modPath[8192];
            modPath[0] = '\x0';

            if( server->HasData( NET_METASERVER_MODPATH ) )
            {
                char tempModPath[4096];
                strcpy( tempModPath, server->GetDataString( NET_METASERVER_MODPATH ) );
                LList<char *> *tokens = g_modSystem->ParseModPath( tempModPath );

                if( tokens->ValidIndex(0) )
                {
                    char *baseMod = tokens->GetData(0);
                    int numRemaining = (tokens->Size() - 2) / 2;

                    if( numRemaining > 0 )
                    {
						sprintf( modPath, LANGUAGEPHRASE("dialog_mod_base_number_more") );
						LPREPLACESTRINGFLAG( 'M', baseMod, modPath );
						LPREPLACEINTEGERFLAG( 'N', numRemaining, modPath );
                    }
                    else
                    {
                        sprintf( modPath, "%s", baseMod );
                    }
                }      


                for( int i = 0; i < tokens->Size(); i+=2 )
                {
                    char *modName = tokens->GetData(i);
                    char *modVer = tokens->GetData(i+1);

                    if( !g_modSystem->IsModInstalled( modName, modVer ) )
                    {
                        modPathOk = false;
                        break;
                    }
                }

                delete tokens;
            }



            bool ourServer = sbw->IsOurServer( serverIp, serverPort );

            Colour col = White;

            if( !protocolMatch )
            {
				sprintf( serverNameFull, LANGUAGEPHRASE("dialog_server_name_incompatible") );
				LPREPLACESTRINGFLAG( 'S', serverName, serverNameFull );
                col.Set(255,30,30,155);
            }
            else if( !modPathOk )
            {
				sprintf( serverNameFull, LANGUAGEPHRASE("dialog_server_name_mods_required") );
				LPREPLACESTRINGFLAG( 'S', serverName, serverNameFull );
                col.Set(255,255,50,155);
            }
            else if( ourServer )
            {
				sprintf( serverNameFull, LANGUAGEPHRASE("dialog_server_name_your_server") );
				LPREPLACESTRINGFLAG( 'S', serverName, serverNameFull );
                col.Set(0,255,0,155);
            }
            else if( demoRestricted )
            {
				sprintf( serverNameFull, LANGUAGEPHRASE("dialog_server_name_demo_restricted") );
				LPREPLACESTRINGFLAG( 'S', serverName, serverNameFull );
                col.Set(100,100,100,150);
            }
            else
            {
                if( (numTeams >= maxTeams || inProgress) &&
                    maxSpectators > 0 &&
                    numSpectators < maxSpectators )
                {
                    sprintf( serverNameFull, "%s", serverName );               
                    col.Set(100,100,255,255);                    
                }

                if( (numTeams >= maxTeams || inProgress) &&
                    numSpectators >= maxSpectators )
                {
					sprintf( serverNameFull, LANGUAGEPHRASE("dialog_server_name_full") );
					LPREPLACESTRINGFLAG( 'S', serverName, serverNameFull );
                    col.Set( 100,100,100,150);
                }
            }            

            if( hasJoinedGame > 0 && sbw->m_listType != ServerBrowserWindow::ListTypeRecent )
            {
				strcat( serverNameFull, "     " );
                strcat( serverNameFull, LANGUAGEPHRASE("dialog_connected_recently") );
            }

            if( lanGame )
            {
                col.Set(0,255,0,155);
            }
            
            END_PROFILE( "GetDetails" );

            START_PROFILE( "RenderText" );

            float fontSize = 11;
            //g_renderer->TextSimple( realX + 5, realY+2, col, fontSize, netLocation );
            g_renderer->TextSimple( realX + 15, realY+2, col, fontSize, serverNameFull );            
            //g_renderer->TextSimple( realX + m_w - 330, realY+2, col, fontSize, gameNameVer );
            g_renderer->TextSimple( realX + m_w - 410, realY+2, col, fontSize, gameModeString );
            g_renderer->TextSimple( realX + m_w - 300, realY+2, col, fontSize, modPath );
            g_renderer->TextSimple( realX + m_w - 180, realY+2, col, fontSize, inProgressString );
            //g_renderer->TextSimple( realX + m_w - 120, realY+2, col, fontSize, spectatorsFull );


            if( password )
            {
                g_renderer->SetBlendMode( Renderer::BlendModeAdditive );
                Image *lock = g_resource->GetImage( "gui/locked.bmp");
                g_renderer->Blit( lock, realX, realY, 14, 14, White );
                g_renderer->SetBlendMode( Renderer::BlendModeNormal );
            }

            if( highlighted || clicked )
            {
                g_renderer->Rect( realX, realY, m_w, m_h, White );
            }
    
            END_PROFILE( "RenderText" );


            int numHumanTeams = 0;
            if( server->HasData( NET_METASERVER_NUMHUMANTEAMS, DIRECTORY_TYPE_CHAR ) )
            {
                numHumanTeams = server->GetDataUChar( NET_METASERVER_NUMHUMANTEAMS );
            }

            int spectatorClamp = maxSpectators;
            Clamp( spectatorClamp, 6, 15 );

            RenderPlayerSlots( realX + m_w - 70, realY, 50, 15, numTeams, numHumanTeams, maxTeams, 6, inProgress );
            //RenderPlayerSlots( realX + m_w - 120, realY, 30, 15, numSpectators, 0, maxSpectators, spectatorClamp, 0 );


            //
            // Update our tooltip

            START_PROFILE( "UpdateTooltip" );

            char tooltip[2048];
            tooltip[0] = '\x0';

            if( !demoRestricted )
            {
                strcat( tooltip, netLocation );
                strcat( tooltip, "\n" );
            }

            if( inProgress != 0 )
            {
                if( server->HasData( NET_METASERVER_GAMETIME, DIRECTORY_TYPE_INT ) )
                {
                    Date date;
                    date.AdvanceTime( server->GetDataInt(NET_METASERVER_GAMETIME) );
                    strcat( tooltip, LANGUAGEPHRASE("tooltip_server_browser_game_time") );
					strcat( tooltip, " " );
                    strcat( tooltip, date.GetTheDate() );

                    if( inProgress == -1 )
                    {
                        strcat( tooltip, LANGUAGEPHRASE("tooltip_server_browser_game_has_ended") );
                    }
                }
                else 
                {
                    strcat( tooltip, LANGUAGEPHRASE("tooltip_server_browser_game_started") );
                }
            }
            else                
            {
                strcat( tooltip, LANGUAGEPHRASE("tooltip_server_browser_not_yet_started") );
            }

            strcat( tooltip, LANGUAGEPHRASE("tooltip_server_browser_teams") );
            strcat( tooltip, " " );
            strcat( tooltip, teamCount );
            strcat( tooltip, LANGUAGEPHRASE("tooltip_server_browser_spectator") );
            strcat( tooltip, " " );
            strcat( tooltip, spectatorsFull );
            
            if( server->HasData( NET_METASERVER_GAMEMODE, DIRECTORY_TYPE_CHAR ) )
            {
                int gameMode = server->GetDataUChar( NET_METASERVER_GAMEMODE );
                GameOption *option = g_app->GetGame()->GetOption("GameMode");
                strcat( tooltip, LANGUAGEPHRASE("tooltip_server_browser_game_mode") );
                strcat( tooltip, " " );
                if( option->m_subOptions.ValidIndex(gameMode) )
                {
                    strcat( tooltip, GameOption::TranslateValue( option->m_subOptions[gameMode] ) );
                }
            }

            if( server->HasData( NET_METASERVER_SCOREMODE, DIRECTORY_TYPE_CHAR ) )
            {
                int gameMode = server->GetDataUChar( NET_METASERVER_SCOREMODE );
                GameOption *option = g_app->GetGame()->GetOption("ScoreMode");
                strcat( tooltip, LANGUAGEPHRASE("tooltip_server_browser_score_mode") );
                strcat( tooltip, " " );
                if( option->m_subOptions.ValidIndex(gameMode) )
                {
                    strcat( tooltip, GameOption::TranslateValue( option->m_subOptions[gameMode] ) );
                }
            }
             
            if( server->HasData( NET_METASERVER_MODPATH ) )
            {
                char modPath[4096];
                strcpy( modPath, server->GetDataString( NET_METASERVER_MODPATH ) );
                LList<char *> *tokens = g_modSystem->ParseModPath( modPath );

                for( int i = 0; i < tokens->Size(); i += 2 )
                {
                    char *mod = tokens->GetData(i);
                    char *version = tokens->GetData(i+1);

                    strcat( tooltip, LANGUAGEPHRASE("tooltip_server_browser_mod") );
                    strcat( tooltip, " " );
                    strcat( tooltip, mod );
                    strcat( tooltip, " " );
                    strcat( tooltip, version );
                }      

                delete tokens;
            }



            strcat( tooltip, LANGUAGEPHRASE("tooltip_server_browser_right_click_details") );

            SetTooltip( tooltip, false );
            
            END_PROFILE( "UpdateTooltip" );

            // 
            // If someone right clicks on us
            // Pop up extended info on this server

            if( g_inputManager->m_mouseX >= realX &&
                g_inputManager->m_mouseX <= realX + m_w &&
                g_inputManager->m_mouseY >= realY &&
                g_inputManager->m_mouseY <= realY + m_h &&
                g_inputManager->m_rmbClicked )
            {                
                ServerDetailsWindow *window = new ServerDetailsWindow( serverIp, serverPort );
                EclRegisterWindow(window, m_parent);
            }
        }              
    };

    void MouseUp()
    {
        ServerBrowserWindow *sbw = (ServerBrowserWindow *) m_parent;

        int realIndex = sbw->m_scrollBar->m_currentValue + m_index;

        if( sbw->m_serverList &&
            sbw->m_serverList->ValidIndex( realIndex ) )
        {
            int oldSelection = sbw->m_selection;
            sbw->m_selection = realIndex;       

            double timeNow = GetHighResTime();
            if( oldSelection == realIndex )
            {
                double timeSinceClick = timeNow - sbw->m_doubleClickTimer;
                if( timeSinceClick < 0.4f )
                {
                    EclButton *joinGame = sbw->GetButton("Join");
                    if( joinGame ) joinGame->MouseUp();
                    timeNow = 0.0f;
                }
            }
            sbw->m_doubleClickTimer = timeNow;
        }
    }
};


class EnterIPManuallyButton : public InterfaceButton
{
    void MouseUp()
    {
        EclRegisterWindow( new EnterIPManuallyWindow(), m_parent );        
    }
};


class UpdateServerListDropDownMenu : public DropDownMenu
{
    void SelectOption( int _option )
    {
        if( m_parent )
        {
            ServerBrowserWindow *parent = (ServerBrowserWindow *)m_parent;                        
            parent->m_receivedServerListSize = -1;
        }

        DropDownMenu::SelectOption(_option);
    }
};


class FiltersButton : public InterfaceButton
{
    void MouseUp()
    {
        EclRegisterWindow( new FiltersWindow(), m_parent );
    }
};


class SortButton : public InterfaceButton
{
public:
    int m_sortType;

    void MouseUp()
    {
        ServerBrowserWindow *parent = (ServerBrowserWindow *)m_parent;                        
        
        if( parent->m_sortType != m_sortType )
        {
            parent->m_sortType = m_sortType;
            parent->m_sortInvert = false;            
        }
        else if( parent->m_sortType == m_sortType &&
                 !parent->m_sortInvert )
        {
            parent->m_sortInvert = true;
        }
        else if( parent->m_sortType == m_sortType &&
                 parent->m_sortInvert )
        {
            parent->m_sortType = ServerBrowserWindow::SortTypeDefault;
            parent->m_sortInvert = false;
        }

        parent->m_receivedServerListSize = -1;
    }

    void Render( int realX, int realY, bool highlighted, bool clicked )
    {      
        ServerBrowserWindow *parent = (ServerBrowserWindow *)m_parent;                        

        g_renderer->RectFill( realX, realY, m_w, m_h, Colour(200,200,255,20) );

        if( highlighted ) 
        {
            g_renderer->RectFill( realX, realY, m_w, m_h, Colour(200,200,255,50) );
        }

        if( parent->m_sortType == m_sortType )
        {
            g_renderer->RectFill( realX, realY, m_w, m_h, Colour(200,200,255,100) );
        }


        //
        // Render the caption

		if( m_captionIsLanguagePhrase )
		{
	        g_renderer->Text( realX + 10, realY+3, Colour(200,200,255,255), 12, LANGUAGEPHRASE(m_caption) );
		}
		else
		{
	        g_renderer->Text( realX + 10, realY+3, Colour(200,200,255,255), 12, m_caption );
		}


        float midPointY = realY + m_h/2.0f;

        if( parent->m_sortType == m_sortType )
        {
            glColor4ub( 200, 200, 255, 200 );

            if( parent->m_sortInvert )
            {
                glBegin( GL_TRIANGLES );
                    glVertex2i( realX + m_w - 16, midPointY+5 );
                    glVertex2i( realX + m_w - 4, midPointY+5 );
                    glVertex2i( realX + m_w - 10, midPointY-5 );
                glEnd();
            }
            else
            {
                glBegin( GL_TRIANGLES );
                    glVertex2i( realX + m_w - 16, midPointY-5 );
                    glVertex2i( realX + m_w - 4, midPointY-5 );
                    glVertex2i( realX + m_w - 10, midPointY+5 );
                glEnd();
            }
        }
    }
};


ServerBrowserWindow::ServerBrowserWindow()
:   InterfaceWindow( "Server Browser", "dialog_server_browser", true ),
    m_serverList(NULL),
    m_requestTimer(0.0f),
    m_relistTimer(0.0f),
    m_selection(-1),
    m_scrollBar(NULL),
    m_doubleClickTimer(0.0f),
    m_receivedServerListSize(0),
    m_showDefaultNamedServers(true),
    m_showSinglePlayerGames(true),
    m_showDemoGames(true),
    m_listType(ListTypeAll),
    m_sortType(SortTypeDefault),
    m_sortInvert(false)
{
    SetSize( 750, 550 );

    strcpy( m_serverIp, " " );
    
    MetaServer_RequestServerListWAN();
        
    g_app->GetClientToServer()->StartIdentifying();

    m_scrollBar = new ScrollBar(this);    
}


bool ServerBrowserWindow::ConnectToServer( Directory *_server, const char *_serverPassword )
{
    //
    // ClientToServer listener opened ok?

    if( !g_app->GetClientToServer()->m_listener )
    {
        char msgtext[1024];
#if defined(RETAIL_BRANDING_UK)
        sprintf( msgtext, "%s%s", LANGUAGEPHRASE("dialog_client_failedconnect"), LANGUAGEPHRASE("website_support_retail_uk") );
#elif defined(RETAIL_BRANDING)
        sprintf( msgtext, "%s%s", LANGUAGEPHRASE("dialog_client_failedconnect"), LANGUAGEPHRASE("website_support_retail") );
#elif defined(RETAIL_BRANDING_MULTI_LANGUAGE)
        sprintf( msgtext, "%s%s", LANGUAGEPHRASE("dialog_client_failedconnect"), LANGUAGEPHRASE("website_support_retail_multi_language") );
#elif defined(TARGET_OS_MACOSX)
        sprintf( msgtext, "%s%s", LANGUAGEPHRASE("dialog_client_failedconnect"), LANGUAGEPHRASE("website_support_macosx") );
#else
        sprintf( msgtext, "%s%s", LANGUAGEPHRASE("dialog_client_failedconnect"), LANGUAGEPHRASE("website_support") );
#endif

        MessageDialog *msg = new MessageDialog( "NETWORK ERROR",
                                                msgtext, false, 
                                                "dialog_client_failedtitle", true ); 
        EclRegisterWindow( msg );      
        return false;
    }


    //
    // What IP did we click to join?

    char *serverIp = _server->GetDataString( NET_METASERVER_IP );
    int serverPort = _server->GetDataInt( NET_METASERVER_PORT );

    char localIp[256];
    int localPort;
    g_app->GetClientToServer()->GetIdentity( localIp, &localPort );
    if( stricmp( serverIp, localIp ) == 0 &&
        _server->HasData( NET_METASERVER_LOCALIP, DIRECTORY_TYPE_STRING ) && 
        _server->HasData( NET_METASERVER_LOCALPORT, DIRECTORY_TYPE_INT ) )
    {
        // Their public IP matches ours
        // So this is a LAN game
        serverIp = _server->GetDataString( NET_METASERVER_LOCALIP );
        serverPort = _server->GetDataInt( NET_METASERVER_LOCALPORT );
    }

    //
    // Check the server can be joined first
    // Check the protocol numbers match up

    int protocolMatch = true;
    if( _server->HasData( NET_METASERVER_PROTOCOLMATCH, DIRECTORY_TYPE_INT ) )
    {
        protocolMatch = _server->GetDataInt( NET_METASERVER_PROTOCOLMATCH );
    }

    if( !protocolMatch )
    {
        char *theirVersion = _server->GetDataString( NET_METASERVER_GAMEVERSION );

        char msg[1024];
        sprintf( msg, LANGUAGEPHRASE("dialog_server_version_mismatch") );
        LPREPLACESTRINGFLAG( 'V', APP_VERSION, msg );
        LPREPLACESTRINGFLAG( 'T', theirVersion, msg );

        MessageDialog *dialog = new MessageDialog( "Cannot Join Server", msg, false, "dialog_cannot_join_server", true );                
        EclRegisterWindow( dialog );
        
        return false;
    }


    //
    // Check we have all required MODs installed

    if( _server->HasData( NET_METASERVER_MODPATH ) )
    {
        char tempModPath[4096];
        strcpy( tempModPath, _server->GetDataString( NET_METASERVER_MODPATH ) );
        LList<char *> *tokens = g_modSystem->ParseModPath( tempModPath );
        
        char reason[8192];
        reason[0] = '\x0';
        bool modsOk = true;

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
                modsOk = false;
            }

            strcat( reason, "\n" );
        }

        if( !modsOk )
        {
            char *websiteMods = NULL;
#if defined(RETAIL_BRANDING_UK)
            websiteMods = LANGUAGEPHRASE("website_mods_retail_uk");
#elif defined(RETAIL_BRANDING)
            websiteMods = LANGUAGEPHRASE("website_mods_retail");
#elif defined(RETAIL_BRANDING_MULTI_LANGUAGE)
            websiteMods = LANGUAGEPHRASE("website_mods_retail_multi_language");
#else
            websiteMods = LANGUAGEPHRASE("website_mods");
#endif

            char msg[1024];
            sprintf( msg, "%s%s", LANGUAGEPHRASE("dialog_no_join_mods_not_installed"), websiteMods );
            LPREPLACESTRINGFLAG( 'R', reason, msg );

            MessageDialog *dialog = new MessageDialog( "Cannot Join Server", msg, false, "dialog_cannot_join_server", true );                
            EclRegisterWindow( dialog );

            return false;
        }
    }


    //
    // Check to make sure this isn't our server

    bool ourServer = ServerBrowserWindow::IsOurServer( serverIp, serverPort );
    
    if( ourServer )
    {
        char msg[1024];
        sprintf( msg, LANGUAGEPHRASE("dialog_no_join_own_game") );

        MessageDialog *dialog = new MessageDialog( "Cannot Join Server", msg, false, "dialog_cannot_join_server", true );                
        EclRegisterWindow( dialog );
        
        return false;
    }


    //
    // Check for Demo limitations

    bool demoRestricted = _server->HasData( NET_METASERVER_DEMORESTRICTED );
    if( demoRestricted )
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

        return false;
    }


    char *serverName;
    if( _server->HasData( NET_METASERVER_SERVERNAME, DIRECTORY_TYPE_STRING ) )
    {
        serverName = _server->GetDataString( NET_METASERVER_SERVERNAME );
    }
    else
    {
        serverName = "unknown";
    }


    //
    // Check for Password

    bool serverHasPassword = _server->HasData( NET_METASERVER_PASSWORD, DIRECTORY_TYPE_STRING );
    if( serverHasPassword && !_serverPassword )
    {
        ServerPasswordWindow *spw = new ServerPasswordWindow();
        spw->m_port = serverPort;
        strncpy( spw->m_ip, serverIp, sizeof(spw->m_ip) );
        spw->m_ip[ sizeof(spw->m_ip) - 1 ] = '\0';
        strncpy( spw->m_serverName, serverName, sizeof(spw->m_serverName) );
        spw->m_serverName[ sizeof(spw->m_serverName) - 1 ] = '\0';
        EclRegisterWindow( spw );
    }
    else
    {
        // If we are connected to an existing server, disconnect now

        g_app->ShutdownCurrentGame();
        EclRemoveWindow( "LOBBY" );
        EclRemoveWindow( "Server Browser" );
        EclRemoveWindow( "Enter IP Manually" );

        //
        // Connect to the new server
        // Open up game windows

        if( _serverPassword )
        {
            strncpy( g_app->GetClientToServer()->m_password, _serverPassword, sizeof(g_app->GetClientToServer()->m_password) );
            g_app->GetClientToServer()->m_password[ sizeof(g_app->GetClientToServer()->m_password) - 1 ] = '\0';
        }

        g_app->GetClientToServer()->ClientJoin( serverIp, serverPort );
        g_app->InitWorld();

        g_gameHistory.JoinedGame( serverIp, serverPort, serverName );

        ConnectingWindow *connectWindow = new ConnectingWindow();
        connectWindow->m_popupLobbyAtEnd = true;
        EclRegisterWindow( connectWindow );
    }

    return true;
}


bool ServerBrowserWindow::IsOurServer( char *_ip, int _port )
{
    //
    // WAN

    char ourIp[256];
    int ourPort;
    g_app->GetServer()->GetIdentity( ourIp, &ourPort );
    if( strcmp(ourIp,_ip) == 0 && ourPort == _port ) 
    {
        return true;
    }


    //
    // LAN

    int localPort = g_app->GetServer()->GetLocalPort();
    
    
    //
    // Only get localHost now and then
    // (since its very slow)

    static double s_localHostTimer = 0.0f;
    static char s_localHost[256];
    
    double timeNow = GetHighResTime();
    if( timeNow > s_localHostTimer )
    {        
        GetLocalHostIP( s_localHost, sizeof(s_localHost) );
        s_localHostTimer = timeNow + 1.0f;
    }

    if( strcmp(s_localHost, _ip) == 0 && localPort == _port )
    {
        return true;
    }
    

    return false;
}


ServerBrowserWindow::~ServerBrowserWindow()
{
    g_app->GetClientToServer()->StopIdentifying();
}


void ServerBrowserWindow::Create()
{    
    InterfaceWindow::Create();
           
    //
    // Main box

    InvertedBox *box = new InvertedBox();
    box->SetProperties( "Invert", 10, 40, m_w-20, m_h-80, " ", " ", false, false );
    RegisterButton( box );


    //
    // Sort buttons

    SortButton *teamSort = new SortButton();
    teamSort->SetProperties( "Teams", m_w - 110, 22, 100, 18, "dialog_server_browser_teams", " ", true, false );
    teamSort->m_sortType = SortTypeTeams;
    RegisterButton( teamSort );

    //SortButton *specSort = new SortButton();
    //specSort->SetProperties( "Specs", teamSort->m_x - 55, 22, 50, 18, "dialog_server_browser_specs", " ", true, false );
    //specSort->m_sortType = SortTypeSpectators;
    //RegisterButton( specSort );

    SortButton *timeSort = new SortButton();
    timeSort->SetProperties( "Time", teamSort->m_x - 115, 22, 110, 18, "dialog_server_browser_time", " ", true, false );
    timeSort->m_sortType = SortTypeTime;
    RegisterButton( timeSort );

    SortButton *modSort = new SortButton();
    modSort->SetProperties( "MODs", timeSort->m_x - 115, 22, 110, 18, "dialog_server_browser_mods", " ", true, false );
    modSort->m_sortType = SortTypeMod;
    RegisterButton( modSort );

    SortButton *gameSort = new SortButton();
    gameSort->SetProperties( "Game Mode", modSort->m_x - 110, 22, 105, 18, "dialog_server_browser_game_mode", " ", true, false );
    gameSort->m_sortType = SortTypeGame;
    RegisterButton( gameSort );

    SortButton *nameSort = new SortButton();
    nameSort->SetProperties( "Server Name", 10, 22, gameSort->m_x - 15, 18, "dialog_server_browser_server_name", " ", true, false );
    nameSort->m_sortType = SortTypeName;
    RegisterButton( nameSort );

    
    //
    // One button per possible server

    int yPos = 45;
    int serverH = 14;
    int stepSize = serverH + 2;
    int numServers = box->m_h / (float) stepSize;

    for( int i = 0; i < numServers; ++i )
    {
        char name[256];
        sprintf( name, "Server%d", i );
        GameServerButton *button = new GameServerButton();
        button->SetProperties( name, box->m_x+5, yPos, box->m_w-30, serverH, " ", " ", false, false );
        button->m_index = i;
        RegisterButton( button );

        yPos += stepSize;
    }


    //
    // ScrollBar

    m_scrollBar->Create( "scrollbar", box->m_x+box->m_w-20, box->m_y, 20, box->m_h, 0, numServers );
    if( m_serverList && m_serverList->Size() ) m_scrollBar->SetNumRows( m_serverList->Size() + 2 );


    //
    // Control buttons

    //EnterIPManuallyButton *enterIp = new EnterIPManuallyButton();
    //enterIp->SetProperties( "Enter IP", 10, m_h - 30, 130, 20, "Enter Ip", "Type in an IP manually and join" );
    //RegisterButton( enterIp );

    //TextButton *text = new TextButton();
    //text->SetProperties( "Search :", 5, m_h - 30, 100, 20 );
    //RegisterButton( text );

//    WanOrLanDropDownMenu *dropDown = new WanOrLanDropDownMenu();
//    dropDown->SetProperties( "Search", 70, m_h - 30, 130, 20 );
//#ifdef WAN_PLAY_ENABLED
//    dropDown->AddOption( "Internet", 0 );
//#endif
//#ifdef LAN_PLAY_ENABLED
//    dropDown->AddOption( "Local Area Network", 1 );
//#endif
//    dropDown->AddOption( "Enter IP Manually", 2 );
//
//    dropDown->RegisterInt( &m_wanOrLan );
//    RegisterButton( dropDown );

    UpdateServerListDropDownMenu *dropDown = new UpdateServerListDropDownMenu();
    dropDown->SetProperties( "List", 10, m_h - 30, 170, 20, "dialog_server_browser_list", " ", true, false );
    dropDown->AddOption( "dialog_filter_all_games", ListTypeAll, true );
    dropDown->AddOption( "dialog_filter_games_in_lobby", ListTypeLobby, true );
    dropDown->AddOption( "dialog_filter_games_in_progress", ListTypeInProgress, true );
    dropDown->AddOption( "dialog_filter_recent_servers", ListTypeRecent, true );
    dropDown->AddOption( "dialog_filter_lan_games", ListTypeLAN, true );
    dropDown->AddOption( "dialog_filter_enter_ip_manually", ListTypeEnterIPManually, true );
    dropDown->RegisterInt( &m_listType );
    RegisterButton( dropDown );

    FiltersButton *filters = new FiltersButton();
	filters->SetProperties( "Filters", 190, m_h - 30, 110, 20, "dialog_filters", " ", true, false );
    RegisterButton( filters );
    
    RequestServerListButton *request = new RequestServerListButton();
    request->SetProperties( "Refresh", m_w - 360, m_h - 30, 110, 20, " ", " ", false, false );
    RegisterButton( request );

    JoinGameButton *join = new JoinGameButton();
    join->SetProperties( "Join", m_w - 240, m_h - 30, 110, 20, "dialog_join", " ", true, false );
    RegisterButton( join );

    CloseButton *close = new CloseButton();
    close->SetProperties( "Close", m_w - 120, m_h - 30, 110, 20, "dialog_close", " ", true, false );
    RegisterButton( close );
}


void ServerBrowserWindow::FilterServerList()
{
    for( int i = 0; i < m_serverList->Size(); ++i )
    {
        Directory *server = m_serverList->GetData(i);

        bool removeServer = false;


        //
        // Filter servers based on m_listType variable

        int inProgress = server->GetDataUChar( NET_METASERVER_GAMEINPROGRESS );

        switch( m_listType )
        {
            case ListTypeLobby:
                if( inProgress != 0 ) removeServer = true;
                break;

            case ListTypeInProgress:
                if( inProgress == 0 ) removeServer = true;
                break;

            case ListTypeRecent:
            {
                char *ip = server->GetDataString( NET_METASERVER_IP );
                int port = server->GetDataInt( NET_METASERVER_PORT );
                if( !g_gameHistory.HasJoinedGame( ip, port ) )
                {
                    removeServer = true;
                }
                break;
            }
        }

        char *serverName = server->GetDataString( NET_METASERVER_SERVERNAME );


        //
        // Filter servers with the default name "New Game Server"

        if( !m_showDefaultNamedServers )
        {
            if( stricmp( serverName, "New Game Server" ) == 0 || stricmp( serverName, LANGUAGEPHRASE("gameoption_New_Game_Server") ) == 0 )
            {
                removeServer = true;
            }
        }


        //
        // Filter single player games

        if( !m_showSinglePlayerGames )
        {
            if( inProgress > 0 &&
                server->HasData( NET_METASERVER_NUMHUMANTEAMS ) &&
                server->GetDataUChar( NET_METASERVER_NUMHUMANTEAMS ) <= 1 )
            {
                removeServer = true;
            }
        }


        //
        // Filter demo games

        if( !m_showDemoGames )
        {
            if( server->HasData( NET_METASERVER_DEMOSERVER ) )
            {
                removeServer = true;
            }
        }


        //
        // Remove the server if requested

        if( removeServer )
        {
            m_serverList->RemoveData(i);
            delete server;
            --i;
        }
    }


    //
    // If this is the "recent" list, we need to add all servers
    // that are in the Recent list but arent in the received list

    if( m_listType == ListTypeRecent )
    {
        for( int i = 0; i < g_gameHistory.m_servers.Size(); ++i )
        {
            GameHistoryServer *server = g_gameHistory.m_servers[i];

            char copiedId[512];
            sprintf( copiedId, server->m_identity );
          
            char *colon = strrchr( copiedId, ':' );
            if( colon )
            {
                int port = atoi( colon+1 );
                *colon = '\x0';
                char *ip = copiedId;

                if( !IsServerInList( ip, port ) )
                {
                    Directory *dir = new Directory();

                    //
                    // Basic variables

                    char fullName[512];
					sprintf( fullName, LANGUAGEPHRASE("dialog_server_name_not_responding") );
					LPREPLACESTRINGFLAG( 'S', server->m_name, fullName );

                    dir->CreateData( NET_METASERVER_SERVERNAME, fullName );
                    dir->CreateData( NET_METASERVER_IP, ip );
                    dir->CreateData( NET_METASERVER_PORT, port );

                    dir->CreateData( NET_METASERVER_GAMENAME,       "Defcon" );
                    dir->CreateData( NET_METASERVER_GAMEVERSION,    "unknown" );
                    dir->CreateData( NET_METASERVER_NUMTEAMS,       (unsigned char) 0 );
                    dir->CreateData( NET_METASERVER_NUMDEMOTEAMS,   (unsigned char) 0 );
                    dir->CreateData( NET_METASERVER_MAXTEAMS,       (unsigned char) 0 );
                    dir->CreateData( NET_METASERVER_NUMSPECTATORS,  (unsigned char) 0 );
                    dir->CreateData( NET_METASERVER_MAXSPECTATORS,  (unsigned char) 0 );
                    dir->CreateData( NET_METASERVER_GAMEMODE,       (unsigned char) 0 );
                    dir->CreateData( NET_METASERVER_SCOREMODE,      (unsigned char) 0 );
                    dir->CreateData( NET_METASERVER_GAMEINPROGRESS, (unsigned char) 0 );

                    m_serverList->PutData( dir );
                }
            }
        }
    }
}


bool ServerBrowserWindow::IsServerInList( char *_ip, int _port )
{
    for( int i = 0; i < m_serverList->Size(); ++i )
    {
        Directory *server = m_serverList->GetData(i);
     
        char *ip = server->GetDataString( NET_METASERVER_IP );
        int port = server->GetDataInt( NET_METASERVER_PORT );

        if( port == _port && strcmp( ip, _ip ) == 0 )
        {
            return true;
        }
    }


    return false;
}


int ServerBrowserWindow::ScoreServer_Default( Directory *server )
{
    int numTeams = server->GetDataUChar         ( NET_METASERVER_NUMTEAMS );
    int maxTeams = server->GetDataUChar         ( NET_METASERVER_MAXTEAMS );
    int numSpectators = server->GetDataUChar    ( NET_METASERVER_NUMSPECTATORS );
    int maxSpectators = server->GetDataUChar    ( NET_METASERVER_MAXSPECTATORS );
    int inProgress = server->GetDataUChar       ( NET_METASERVER_GAMEINPROGRESS );
    char *serverIp = server->GetDataString      ( NET_METASERVER_IP );

    int numDemoTeams = server->HasData( NET_METASERVER_NUMDEMOTEAMS, DIRECTORY_TYPE_INT ) ? 
        server->GetDataInt( NET_METASERVER_NUMDEMOTEAMS ) : 0;


    bool amIDemo = g_app->GetClientToServer()->AmIDemoClient();
    bool demoRestricted = false;
    if( amIDemo )
    {
        int maxDemoGameSize, maxDemoPlayers;
        bool allowDemoServers;
        g_app->GetClientToServer()->GetDemoLimitations( maxDemoGameSize, maxDemoPlayers, allowDemoServers );
        demoRestricted = (numDemoTeams >= maxDemoPlayers);
    }

    bool lanGame = false;
    char localIp[256];
    int localPort;
    g_app->GetClientToServer()->GetIdentity( localIp, &localPort );
    if( stricmp( serverIp, localIp ) == 0 ) lanGame = true;

    int protocolMatch = true;
    if( server->HasData( NET_METASERVER_PROTOCOLMATCH, DIRECTORY_TYPE_INT ) )
    {
        protocolMatch = server->GetDataInt( NET_METASERVER_PROTOCOLMATCH );
    }

    int score = 0;


    if( lanGame )
    {
        // LAN game, put at top of list
        score = 0;
    }
    else if( demoRestricted ) 
    {
        // Demo restriction - cant join
        score = 15;
    }
    else if( inProgress == 2 )
    {
        // Game over, not particuarly interesting
        score = 20;
    }
    else if( inProgress == 1 )
    {
        if( numSpectators >= maxSpectators )
        {
            // Running, but full of spectators
            score = 14;
        }
        else
        {
            // Running, room for spectators
            score = 14;
        }
    }
    else if( numTeams >= maxTeams )
    {
        if( numSpectators >= maxSpectators )
        {
            // Not yet running, but totally full of players and spectators
            score = 10;
        }
        else
        {
            // Not yet running, full of players, room for spectators
            score = 9;
        }
    }
    else if( numTeams < maxTeams )
    {
        // Not yet running, room for more teams
        score = 1 + (maxTeams - numTeams);
    }



    if( !protocolMatch )
    {
        // Wrong protocol, cant join
        score += 30;
    }

    return score;
}


int ServerBrowserWindow::ScoreServer_Name( Directory *_server )
{
    char name[512];
    sprintf( name, _server->GetDataString( NET_METASERVER_SERVERNAME ) );
    strlwr( name );
    
    int score = 0;

    score += (int) name[0];

    return score;
}


int ServerBrowserWindow::ScoreServer_Game( Directory *_server )
{
    return _server->GetDataUChar( NET_METASERVER_GAMEMODE );
}


int ServerBrowserWindow::ScoreServer_Mods( Directory *_server )
{
    if( !_server->HasData( NET_METASERVER_MODPATH ) )
    {
        return 0;
    }
    else
    {
        char name[512];
        sprintf( name, _server->GetDataString( NET_METASERVER_MODPATH ) );
        strlwr( name );

        int score = 0;
        score += (int) name[0];

        return score;
    }
}


int ServerBrowserWindow::ScoreServer_Time( Directory *_server )
{
    if( _server->HasData( NET_METASERVER_GAMETIME ) )
    {
        return _server->GetDataInt( NET_METASERVER_GAMETIME );
    }

    return 0;
}


int ServerBrowserWindow::ScoreServer_Spectators( Directory *_server )
{
    return _server->GetDataUChar( NET_METASERVER_NUMSPECTATORS );
}


int ServerBrowserWindow::ScoreServer_Teams( Directory *_server )
{
    if( _server->HasData( NET_METASERVER_NUMHUMANTEAMS ) )
    {
        return _server->GetDataUChar( NET_METASERVER_NUMHUMANTEAMS );
    }
    else
    {
        return _server->GetDataUChar( NET_METASERVER_NUMTEAMS );
    }
}


void ServerBrowserWindow::SortServerList()
{
    if( !m_serverList ) return;


    //
    // Score each server depending on the sort order

    for( int i = 0; i < m_serverList->Size(); ++i )
    {
        Directory *server = m_serverList->GetData(i);
        
        int score = 0;
        
        switch( m_sortType )
        {
            case SortTypeDefault:       score = ScoreServer_Default( server );      break;
            case SortTypeName:          score = ScoreServer_Name( server );         break;
            case SortTypeGame:          score = ScoreServer_Game( server );         break;
            case SortTypeMod:           score = ScoreServer_Mods( server );         break;
            case SortTypeTime:          score = ScoreServer_Time( server );         break;
            case SortTypeSpectators:    score = ScoreServer_Spectators( server );   break;
            case SortTypeTeams:         score = ScoreServer_Teams( server );        break;
        }


        if( m_sortInvert ) score = 10000 - score;
        server->CreateData( "OrderRank", score );
    }


    //
    // Build an ordered list based on the score

    LList<Directory *> *newList = new LList<Directory *>();

    for( int i = 0; i < m_serverList->Size(); ++i )
    {
        Directory *server = m_serverList->GetData(i);
        int score = server->GetDataInt( "OrderRank" );

        bool added = false;
        for( int j = 0; j < newList->Size(); ++j )
        {
            Directory *thisServer = newList->GetData(j);
            int thisScore = thisServer->GetDataInt( "OrderRank" );

            if( score <= thisScore )
            {
                newList->PutDataAtIndex( server, j );
                added = true;
                break;
            }
        }

        if( !added )
        {
            newList->PutDataAtEnd( server );
        }
    }


    //
    // Replace the un-ordered list with the ordered list

    delete m_serverList;
    m_serverList = newList;
}


void ServerBrowserWindow::Update ()
{
    if( m_listType == ListTypeEnterIPManually )
    {
        // The user wishes to enter their own IP manually
        m_listType = ListTypeAll;
        EclRegisterWindow( new EnterIPManuallyWindow(), this );
    }

    double timeNow = GetHighResTime();

    bool timeForRequest = ( timeNow > m_requestTimer );
    bool timeForRelist = ( timeNow > m_relistTimer );

    bool wanServers = ( m_listType != ListTypeLAN );
    bool lanServers = !wanServers;


    //
    // Get the latest list every second

    MetaServer_PurgeServerList();

    if( timeForRelist && MetaServer_GetNumServers(wanServers, lanServers) != m_receivedServerListSize )
    {
        if( m_serverList )
        {
            ClearServerList();
        }

        m_serverList = MetaServer_GetServerList(false, wanServers, lanServers);
        
        if( m_serverList ) 
        {
            m_receivedServerListSize = m_serverList->Size();

            FilterServerList();
            SortServerList();

            m_scrollBar->SetNumRows( m_serverList->Size() + 2 );
        }
        else
        {
            m_scrollBar->SetNumRows( 0 );
            m_receivedServerListSize = 0;
        }

        m_relistTimer = timeNow + 1.0f;
    }


    //
    // Request a new list every few seconds
    // The exact time interval is governed by a metaserver variable
    // called ServerTTL

    if( timeForRequest )
    {
        int serverTTL = MetaServer_GetServerTTL();
        MetaServer_SetServerTTL( serverTTL );

        MetaServer_RequestServerListWAN();       

        m_requestTimer = timeNow + serverTTL / 3.0f;
        m_requestTimer += sfrand(3.0f);
    }


    //
    // If we've failed authentication, exit now

#if AUTHENTICATION_LEVEL == 1 
    char authKey[256];
    Authentication_GetKey( authKey );
    int authResult = Authentication_GetStatus( authKey );
    if( authResult < 0 )
    {
        EclRemoveWindow( m_name );

        char *errorString = LANGUAGEPHRASE(Authentication_GetStatusStringLanguagePhrase(authResult));

        char authKey[256];
		if( Authentication_IsKeyFound() )
		{
			Authentication_GetKey( authKey );
		}
		else
		{
			sprintf( authKey, LANGUAGEPHRASE("dialog_authkey_not_found") );
		}

        BadKeyWindow *window = new BadKeyWindow();        

        sprintf( window->m_extraMessage, LANGUAGEPHRASE("dialog_not_permitted_join_game") );
		LPREPLACESTRINGFLAG( 'K', authKey, window->m_extraMessage );
		LPREPLACESTRINGFLAG( 'E', LANGUAGEPHRASE(errorString), window->m_extraMessage );

        if( authResult == AuthenticationWrongPlatform )
        {
#if defined TARGET_MSVC
            strcat( window->m_extraMessage, "\n" );
            strcat( window->m_extraMessage, LANGUAGEPHRASE("dialog_cannot_use_mac_key_on_pc") );
#endif

        }

        EclRegisterWindow( window );
        return;
    }
#endif
}


void ServerBrowserWindow::Render( bool _hasFocus )
{
    InterfaceWindow::Render( _hasFocus );
}


void ServerBrowserWindow::ClearServerList()
{
    if( m_serverList )
    {
        m_serverList->EmptyAndDelete();
        delete m_serverList;
        m_serverList = NULL;
    }
}


// ============================================================================


class ConnectToManualIPButton : public InterfaceButton
{
    void MouseUp()
    {
        EnterIPManuallyWindow *parent = (EnterIPManuallyWindow *)m_parent;
        Directory server;
        server.CreateData( NET_METASERVER_IP, parent->m_ip );
        server.CreateData( NET_METASERVER_LOCALIP, parent->m_ip );
        server.CreateData( NET_METASERVER_PORT, parent->m_port );
        server.CreateData( NET_METASERVER_LOCALPORT, parent->m_port );
        ServerBrowserWindow::ConnectToServer( &server );
    }
};


EnterIPManuallyWindow::EnterIPManuallyWindow()
:   InterfaceWindow( "Enter IP Manually", "dialog_enter_ip_manually", true )
{
    strcpy( m_ip, LANGUAGEPHRASE("dialog_enter_ip") );
    m_port = 5010;

    SetSize( 300, 200 );
}


void EnterIPManuallyWindow::Create()
{
    InterfaceWindow::Create();
        
    InputField *ip = new InputField();
    ip->SetProperties( "IP", 100, 40, m_w-120, 20, " ", " ", false, false );
    ip->RegisterString( m_ip );
    RegisterButton( ip );

    InputField *port = new InputField();
    port->SetProperties( "port", 100, 70, m_w-120, 20, " ", " ", false, false );
    port->RegisterInt( &m_port );
    RegisterButton( port );

    ConnectToManualIPButton *connect = new ConnectToManualIPButton();
    connect->SetProperties( "Connect", m_w/2-50, m_h-30, 100, 20, "dialog_connect", " ", true, false );
    RegisterButton( connect );
}


void EnterIPManuallyWindow::Render( bool _hasFocus )
{
    InterfaceWindow::Render( _hasFocus );

    g_renderer->TextSimple( m_x+15, m_y+45, White, 13, LANGUAGEPHRASE("dialog_server_ip") );
    g_renderer->TextSimple( m_x+15, m_y+75, White, 13, LANGUAGEPHRASE("dialog_server_port") );
}



// ============================================================================


class UpdateServerListCheckBox : public CheckBox
{
    void MouseUp()
    {
        CheckBox::MouseUp();

        ServerBrowserWindow *sbw = (ServerBrowserWindow *) EclGetWindow( "Server Browser" );
        if( sbw )
        {
            sbw->m_receivedServerListSize = -1;
        }
    }
};


FiltersWindow::FiltersWindow()
:   InterfaceWindow("Filters", "dialog_filters", true)
{
    SetSize( 270, 300 );
}


void FiltersWindow::Create()
{
    InterfaceWindow::Create();


    ServerBrowserWindow *sbw = (ServerBrowserWindow *) EclGetWindow( "Server Browser" );
    if( !sbw )
    {
        return;
    }

    InvertedBox *box = new InvertedBox();
    box->SetProperties( "invert", 10, 30, m_w - 20, m_h - 70, " ", " ", false, false );
    RegisterButton( box );

    int x = m_w - 50;
    int w = 15;
    int y = 20;
    int h = 15;
    int g = 5;

    UpdateServerListCheckBox *checkBox;

    checkBox = new UpdateServerListCheckBox();
    checkBox->SetProperties( "Single Player Games", x, y+=h+g, w, h, "dialog_single_player_games", " ", true, false );
    checkBox->RegisterBool( &sbw->m_showSinglePlayerGames );
    RegisterButton( checkBox );

    checkBox = new UpdateServerListCheckBox();
    checkBox->SetProperties( "Demo Games", x, y+=h+g, w, h, "dialog_demo_games", " ", true, false );
    checkBox->RegisterBool( &sbw->m_showDemoGames );
    RegisterButton( checkBox );

    checkBox = new UpdateServerListCheckBox();
    checkBox->SetProperties( "Default Named Servers", x, y+=h+g, w, h, "dialog_default_named_servers", " ", true, false );
    checkBox->RegisterBool( &sbw->m_showDefaultNamedServers );
    RegisterButton( checkBox );

    CloseButton *close = new CloseButton();
    close->SetProperties( "Close", 40, m_h - 30, m_w - 80, 20, "dialog_close", " ", true, false );
    RegisterButton( close );
}


void FiltersWindow::Render( bool _hasFocus )
{
    ServerBrowserWindow *sbw = (ServerBrowserWindow *) EclGetWindow( "Server Browser" );
    if( !sbw )
    {
        EclRemoveWindow( m_name );
        return;
    }

    InterfaceWindow::Render( _hasFocus );


    int x = m_x + 20;
    int y = m_y + 23;
    int h = 20;

    g_renderer->TextSimple( x, y+=h, White, 12, LANGUAGEPHRASE("dialog_show_single_player_games") );
    g_renderer->TextSimple( x, y+=h, White, 12, LANGUAGEPHRASE("dialog_show_demo_games") );
    g_renderer->TextSimple( x, y+=h, White, 12, LANGUAGEPHRASE("dialog_show_default_named_games") );
}



