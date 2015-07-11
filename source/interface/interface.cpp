#include "lib/universal_include.h"

#include <math.h>
#include <time.h>

#include "lib/eclipse/eclipse.h"
#include "lib/hi_res_time.h"
#include "lib/gucci/input.h"
#include "lib/gucci/window_manager.h"
#include "lib/resource/resource.h"
#include "lib/resource/image.h"
#include "lib/language_table.h"
#include "lib/math/math_utils.h"
#include "lib/preferences.h"
#include "lib/profiler.h"
#include "lib/string_utils.h"
#include "lib/metaserver/matchmaker.h"
#include "lib/metaserver/metaserver.h"
#include "lib/render/styletable.h"

#include "network/Server.h"
#include "network/network_defines.h"

#include "app/app.h"
#include "app/globals.h"
#include "app/game.h"
#include "lib/multiline_text.h"
#include "app/tutorial.h"

#include "renderer/map_renderer.h"

#include "world/world.h"

#include "interface/components/core.h"
#include "interface/components/message_dialog.h"
#include "interface/interface.h"
#include "interface/network_window.h"
#include "interface/profile_window.h"
#include "interface/worldstatus_window.h"
#include "interface/mainmenu.h"
#include "interface/side_panel.h"
#include "interface/chat_window.h"
#include "interface/soundsystem/soundeditor_window.h"
#include "interface/toolbar.h"
#include "interface/tutorial_window.h"
#include "interface/info_window.h"
#include "interface/connecting_window.h"
#include "interface/resynchronise_window.h"


static bool s_toggleFullscreen = false;

void Interface::TooltipRender( EclWindow *_window, EclButton *_button, float _timer )
{
    int showTooltips = g_preferences->GetInt( PREFS_INTERFACE_TOOLTIPS );

    if( showTooltips && 
        _timer > 0.8f &&
        _button && 
        _button->m_tooltip &&
        strlen(_button->m_tooltip) > 1 )
    {
        float mouseX = g_inputManager->m_mouseX;
        float mouseY = g_inputManager->m_mouseY;
        float boxX = mouseX + 10;
        float boxY = mouseY + 20;
        float maxW = 200;

        float alpha = (_timer-0.8f) * 2.0f;
        Clamp( alpha, 0.0f, 0.9f );

        Colour windowColP  = g_styleTable->GetPrimaryColour( STYLE_TOOLTIP_BACKGROUND );
        Colour windowColS  = g_styleTable->GetSecondaryColour( STYLE_TOOLTIP_BACKGROUND );
        Colour borderCol   = g_styleTable->GetPrimaryColour( STYLE_TOOLTIP_BORDER );
        Colour fontCol     = g_styleTable->GetPrimaryColour( FONTSTYLE_TOOLTIP );

        windowColP.m_a *= alpha;
        windowColS.m_a *= alpha;
        borderCol.m_a *= alpha;
        fontCol.m_a *= alpha;

        Style *fontStyle = g_styleTable->GetStyle( FONTSTYLE_TOOLTIP );
        float fontSize = fontStyle->m_size;


        //
        // Wrap the text and work out how much of it there is

        g_renderer->SetFont( fontStyle->m_fontName, false, fontStyle->m_negative, false );

		MultiLineText wrapped( ( _button->m_tooltipIsLanguagePhrase )? LANGUAGEPHRASE(_button->m_tooltip) : _button->m_tooltip, maxW, fontSize, true );
        
        float widestLine = 0.0f;
        int numLines = wrapped.Size();
        for( int i = 0; i < numLines; ++i )
        {
            float thisWidth = g_renderer->TextWidth( wrapped[i], fontSize );
            widestLine = max( widestLine, thisWidth );
        }

        float boxW = widestLine + 10;
        float boxH = numLines * (fontSize+2) + 4;

        if( boxX + boxW > g_windowManager->WindowW() )
        {
            boxX = mouseX - boxW - 10;
        }

        if( boxY + boxH > g_windowManager->WindowH() )
        {
            boxY = mouseY - boxH - 10;
        }

        
        //
        // Render the box

       
        g_renderer->RectFill ( boxX, boxY, boxW, boxH, windowColP, windowColP, windowColS, windowColS );
        g_renderer->Rect     ( boxX, boxY, boxW, boxH, borderCol);


        //
        // Render the text

        for( int i = 0; i < numLines; ++i )
        {
            float thisAlpha = alpha;            
            g_renderer->TextSimple( boxX+5, boxY+2+i*(fontSize+2), fontCol, fontSize, wrapped[i] );
        }


        g_renderer->SetFont();


        //
        // Drop shadow

        InterfaceWindow::RenderWindowShadow( boxX+boxW+0.5f, boxY, boxH+0.5f, boxW, 10, g_renderer->m_alpha*0.5f );
    }
}



Interface::Interface()
:   m_message(NULL),
    m_mouseMessage(-1),
    m_mouseMessageTimer(0.0f),
	m_mouseCursor(NULL),
    m_escTimer(0.0f)
{
}

void Interface::Init()
{
    //
    // Get Eclipse up and running\  

    EclInitialise( g_windowManager->WindowW(), 
                   g_windowManager->WindowH() );

    EclRegisterTooltipCallback( TooltipRender );
}

static inline bool ScreenshotKeyPressed()
{
	return g_keyDeltas[KEY_P];
}

static inline bool QuitKeyPressed()
{
	return
#ifdef	TARGET_OS_MACOSX
		(g_keys[KEY_COMMAND] && g_keys[KEY_Q] && !g_keys[KEY_ALT]) ||
#endif
		false;
}

static inline bool QuitImmediatelyKeyPressed()
{
	return
#ifdef	TARGET_OS_MACOSX
		(g_keys[KEY_COMMAND] && g_keys[KEY_Q] && g_keys[KEY_ALT]) ||
#endif
		false; 	
}

static inline bool ToggleFullscreenKeyPressed()
{
	return
#ifdef	TARGET_OS_MACOSX
		(g_keys[KEY_COMMAND] && g_keys[KEY_F]) ||
#endif
		false; 	
}

//
// We can't toggle fullscreen directly from GUI code, because we're in the
// middle of rendering when we allow the GUI to run. So instead we just
// set a flag to toggle during Interface::Update().
void ToggleFullscreenAsync()
{
	s_toggleFullscreen = true;
}

void Interface::Update()
{
    START_PROFILE( "EclUpdate" );
    EclUpdate();
    END_PROFILE( "EclUpdate" );
    
    

    //
    // Screenshot?

    if( g_keyDeltas[KEY_P] &&
        !UsingChatWindow() &&
        g_app->m_gameRunning )
    {
        g_renderer->SaveScreenshot();

        if( g_app->GetWorld()->AmISpectating() )
        {
            int teamId = g_app->GetClientToServer()->m_clientId;
            g_app->GetClientToServer()->SendChatMessageReliably( teamId, 100, LANGUAGEPHRASE("dialog_saved_screenshot"), true );
        }
        else
        {
            int teamId = g_app->GetWorld()->m_myTeamId;
            g_app->GetClientToServer()->SendChatMessageReliably( teamId, 100, LANGUAGEPHRASE("dialog_saved_screenshot"), false );
        }
    }


    //
    // Pop up main menu?

    if( m_escTimer > 0.0f )
    {
        m_escTimer -= g_advanceTime;
    }


	if (QuitKeyPressed())
		ConfirmExit( NULL );	
	
	if (QuitImmediatelyKeyPressed() || g_inputManager->m_quitRequested) 
	{
		g_inputManager->m_quitRequested = false;
		AttemptQuitImmediately();
	}
	
	if (ToggleFullscreenKeyPressed() || s_toggleFullscreen)
	{
		g_preferences->SetInt( PREFS_SCREEN_WINDOWED, !g_preferences->GetInt( PREFS_SCREEN_WINDOWED ));
		g_app->ReinitialiseWindow();
		s_toggleFullscreen = false;
	}
		
	if( g_keyDeltas[KEY_ESC] )
    {
        //
        // panic key (double esc)

        bool panicKeyEnabled = g_preferences->GetInt(PREFS_INTERFACE_PANICKEY);
        bool officeMode = (g_app->GetGame()->GetOptionValue("GameMode") == GAMEMODE_OFFICEMODE);
        if( officeMode ) panicKeyEnabled = true;

        if( panicKeyEnabled && m_escTimer > 0.0f )
        {
            g_app->HideWindow();
            m_escTimer = 0.0f;
            g_keys[KEY_ESC] = 0;
            g_keyDeltas[KEY_ESC] = 0;
        }        
        
        if( !EclGetWindow( "Main Menu" ) )
        {
            EclRegisterWindow( new MainMenu() );
        }
        else
        {
            EclRemoveWindow( "Main Menu" );
        }

        m_escTimer = 0.5f;
    }


    if( !g_app->m_gameRunning &&
        EclGetWindows()->Size() == 0 )
    {
        EclRegisterWindow( new MainMenu() );
    }

}

void Interface::Render()
{
    g_renderer->SetBlendMode( Renderer::BlendModeNormal );
    g_renderer->SetFont( "kremlin" );    

    bool connectionScrewed = EclGetWindow("Connection Status");


    if( !connectionScrewed )
    {
        //
        // Network warning messages

        int myClientId = g_app->GetClientToServer()->m_clientId;
        if( !g_app->GetClientToServer()->IsSynchronised( myClientId ) )
        {
            Colour col( 255, 50, 50, 255 );
            float size = ( g_windowManager->WindowW() / 20.0f );
            float xPos = g_windowManager->WindowW()/2.0f;
            float yPos = g_windowManager->WindowH()*0.2f;
            g_renderer->TextCentreSimple( xPos, yPos, col, size, LANGUAGEPHRASE("dialog_synchronisation_error") );

            if( !EclGetWindow("Resynchronise" ))
            {
                EclRegisterWindow( new ResynchroniseWindow() );
            }
        }


        if( g_app->GetClientToServer()->IsConnected() &&
            g_app->GetClientToServer()->m_connectionState == ClientToServer::StateConnected )
        {
            Colour col( 255, 50, 50, 255 );
            float size = ( g_windowManager->WindowW() / 20.0f );
            float xPos = g_windowManager->WindowW()/2.0f;
            float yPos = g_windowManager->WindowH()*0.3f;
            float yPos2 = g_windowManager->WindowH()*0.4f;

            static int s_connectionProblem = 0;             // 0=no, 1=yes, 2=recovery
            static float s_maxLatency = 0.0f;
            static float s_connProblemDetected = -1.0f;

            float estimatedLatency = g_app->GetClientToServer()->GetEstimatedLatency();
            
            if( !g_app->m_gameRunning ) s_connProblemDetected = -1.0f;
            
            if( s_connectionProblem == 0 &&
                estimatedLatency > 5.0f )
            {
                // Connection problem beginning
                s_connectionProblem = 1;
                s_maxLatency = 5.0f;
            }
            else if( s_connectionProblem == 1 )
            {
                if( estimatedLatency < 1.0f )
                {
                    s_connectionProblem = 0;
                }
                else
                {
                    // Connection problem getting worse
                    float timeNow = GetHighResTime();
                    if( s_connProblemDetected < 0.0f ) s_connProblemDetected = timeNow;
                    
                    if( timeNow-s_connProblemDetected > 2.0f )
                    {
                        col.m_a = 255.0f * min( 1.0f, timeNow-s_connProblemDetected-2.0f );
						char caption[512];
						sprintf( caption, LANGUAGEPHRASE("dialog_connection_problem_seconds") );
						LPREPLACEINTEGERFLAG( 'S', int(estimatedLatency), caption );
                        g_renderer->TextCentreSimple( xPos, yPos, col, size, caption );                            
                    }

                    s_maxLatency = max( s_maxLatency, estimatedLatency );
                
                    float maxLat = s_maxLatency;
                    float estLat = estimatedLatency;
                
                    if( estimatedLatency < s_maxLatency - 1.0f )
                    {
                        s_connectionProblem = 2;
                        if( !EclGetWindow("Connection Status") ) 
                        {
                            EclRegisterWindow( new ConnectingWindow() );
                        }
                    }
                }
            }
            else if( s_connectionProblem == 2 )
            {
                if( estimatedLatency < 1.0f )
                {
                    s_connectionProblem = 0;
                }
                else
                {
                    // Connection problem recovering
                    if( estimatedLatency > s_maxLatency + 3 )
                    {
                        s_connectionProblem = 1;
                    }
                    else if( !EclGetWindow( "Connection Status") )
                    {
                        s_connectionProblem = 0;
                        s_maxLatency = 0.0f;
                    }
                }
            }

            if( !g_app->GetServer() )
            {
                int packetLoss = g_app->GetClientToServer()->CountPacketLoss();
                int totalBad = packetLoss * 4 + estimatedLatency * 10;
                Clamp( totalBad, 0, 20 );

                int red = totalBad*12;
                int green = 255 - red;
                col.Set(red,green,0,255);
                g_renderer->RectFill( 5, 5, 13, 13, col );

                g_renderer->SetFont();

                float yPos = 20;
                if( estimatedLatency > 1.0f )
                {
                    g_renderer->Text( 5, yPos, Colour(255,0,0,255), 12, LANGUAGEPHRASE("dialog_high_latency") );
                    yPos += 13;
                }

                if( packetLoss > 2 )
                {
                    g_renderer->Text( 5, yPos, Colour(255,0,0,255), 12, LANGUAGEPHRASE("dialog_high_packet_loss") );
                }

                g_renderer->SetFont( "kremlin" );
            }

        }
        
        if( m_message )
        {
            float timeOnScreen = GetHighResTime() - m_messageTimer;
            if( timeOnScreen < 5.0f )
            {
                char msg[256];
                sprintf( msg, m_message );
                float fractionShown = timeOnScreen/1.5f;
                if( fractionShown < 1.0f ) msg[ int(strlen(msg) * fractionShown) ] = '\x0';

                float alpha = 1.0f;
                if( timeOnScreen > 4.0f ) alpha = 5.0f - timeOnScreen;
                alpha = max(alpha, 0.0f);
                alpha *= 255;
                            
                Colour col( 255, 255, 255, alpha );
                float size = ( g_windowManager->WindowW() / 12.0f );
                float xPos = g_windowManager->WindowW()/2.0f;
                float yPos = g_windowManager->WindowH()*0.8f;            

                float textWidth = g_renderer->TextWidth( m_message, size );
                if( textWidth > g_windowManager->WindowW() )
                {
                    // message is longer than the window size, rescale it
                    size *= g_windowManager->WindowW() / textWidth * 0.95f;
                    textWidth = g_renderer->TextWidth( m_message, size );
                }

                g_renderer->TextSimple( xPos-textWidth/2.0f, yPos, col, size, msg );
            }
            else
            {
                delete m_message;
                m_message = NULL;
            }
        }


        //
        // Render Defcon counter (if applicable)

        if( g_app->m_gameRunning &&
            g_app->GetWorld()->GetDefcon() > 1 &&
            !m_message )
        {
            int currentDefcon = g_app->GetWorld()->GetDefcon();
            int minutes = g_app->GetWorld()->m_defconTime[ currentDefcon-1 ] - (g_app->GetWorld()->m_theDate.GetMinutes() + 1);
            int seconds = 60 - g_app->GetWorld()->m_theDate.GetSeconds();
            if( seconds == 60 )
            {
                minutes += 1;
                seconds = 0;
            }
            char msg[256];
			sprintf( msg, LANGUAGEPHRASE("dialog_defcon_x_in_x_time") );
			LPREPLACEINTEGERFLAG( 'D', currentDefcon - 1, msg );
			LPREPLACEINTEGERFLAG( 'M', minutes, msg );
			char number[32];
			sprintf( number, "%02d", seconds );
			LPREPLACESTRINGFLAG( 'S', number, msg );

            float xPos = g_windowManager->WindowW()/2.0f;
            float yPos = g_windowManager->WindowH()*0.8f;
            float size = ( g_windowManager->WindowW() / 30.0f );

            g_renderer->TextCentre(xPos, yPos, Colour(255,0,0,255), size, msg );

            // If Defcon 3 is nearly here and we still have units left, give us some prompting

            if( currentDefcon == 4 )
            {
                Team *myTeam = g_app->GetWorld()->GetMyTeam();
                if( myTeam )
                {
                    int totalUnits = 0;
                    for( int i = 0; i < WorldObject::NumObjectTypes; ++i )
                    {
                        totalUnits += myTeam->m_unitsAvailable[i];
                    }
        
                    if( totalUnits > 0 )
                    {
                        yPos += size;
                        size *= 0.35f;
                        g_renderer->TextCentre( xPos, yPos, Colour(255,0,0,255), size, LANGUAGEPHRASE("message_placeunits") );
                    }
                }
            }
        }


        //
        // Render victory timer (if applicable)
        
        float victoryTimer = g_app->GetGame()->m_victoryTimer.DoubleValue();
        if( victoryTimer > 0.0f )
        {
            int minutes = victoryTimer / 60;
            int seconds = victoryTimer - minutes * 60;

            float xPos = g_windowManager->WindowW()/2.0f;
            float yPos = g_windowManager->WindowH()*0.75f;
            float size = ( g_windowManager->WindowW() / 30.0f );

            char caption[256];
			sprintf( caption, LANGUAGEPHRASE("dialog_time_remaining") );
			LPREPLACEINTEGERFLAG( 'M', minutes, caption );
			char number[32];
			sprintf( number, "%02d", seconds );
			LPREPLACESTRINGFLAG( 'S', number, caption );

            g_renderer->TextCentre( xPos, yPos, Colour(255,0,0,255), size, caption );
        }
        else if( victoryTimer == 0.0f )
        {
            float xPos = g_windowManager->WindowW()/2.0f;
            float yPos = g_windowManager->WindowH()*0.75f;
            float size = ( g_windowManager->WindowW() / 30.0f );

            g_renderer->TextCentre( xPos, yPos, Colour(255,0,0,255), size, LANGUAGEPHRASE("dialog_gameover") );
        }

        if( g_app->m_gameRunning )
        {
            if( g_app->GetWorld()->GetTimeScaleFactor() == 0 && g_app->GetGame()->m_winner == -1 )
            {
                float xPos = g_windowManager->WindowW()/2.0f;
                float yPos = g_windowManager->WindowH()*0.5f;
                float size = ( g_windowManager->WindowW() / 20.0f );

                g_renderer->TextCentreSimple( xPos, yPos, White, size, LANGUAGEPHRASE("dialog_paused") );
            }
        }

        if( g_app->m_gameRunning && g_app->GetMapRenderer()->GetAutoCam() )
        {
            g_renderer->TextSimple( 10.0f, 10.0f, White, 20, LANGUAGEPHRASE("dialog_autocam") );
        }
    }


    //
    // Test Bed stuff

#ifdef TESTBED
    if( g_app->GetServer() )
    {
        float xPos = g_windowManager->WindowW()/2.0f;
        float yPos = g_windowManager->WindowH()*0.4f;
        float size = ( g_windowManager->WindowW() / 12.0f );

        char *modPath = g_preferences->GetString( "ModPath" );

        g_renderer->TextCentreSimple( xPos, yPos, White, size, "SERVER" );
        g_renderer->TextCentreSimple( xPos, yPos+size, White, size/3, modPath );

        ProfiledElement *element = g_profiler->m_rootElement->m_children.GetData( "Server Main Loop" );
        if( element && element->m_lastNumCalls > 0 )
        {
            float percent = element->m_lastNumCalls * 10.0f;
            g_renderer->TextCentre( xPos, yPos + size*3, White, size, "%d%%", (int)percent );
        }

    }
#endif


    //
    // Version

    //if( !g_app->m_gameRunning )
    //{
    //    char caption[256];
    //    sprintf( caption, "%s %s", APP_NAME, APP_VERSION );
    //    float width = g_renderer->TextWidth( caption, 12 );

    //    g_renderer->RectFill( 18, 3, 4+width, 15, Colour(0,0,0,150) );
    //    g_renderer->SetFont();
    //    g_renderer->TextSimple( 20, 5, White, 12, caption );

    //    g_renderer->SetFont( "kremlin" );
    //}

#ifdef NON_PLAYABLE
    g_renderer->Text( 10, 40,Colour(255,50,50,255), 20, LANGUAGEPHRASE("dialog_non_playable_demo") );
#endif

    if( g_preferences->GetInt( PREFS_NETWORKTRACKSYNCRAND ))
    {
        g_renderer->Text( 10, 60, Colour(255,50,50,255), 20, LANGUAGEPHRASE("dialog_tracking_synchronisation") );
    }

    g_renderer->SetFont();
}


void Interface::RenderMouse()
{
    if( g_app->MousePointerIsVisible() )
    {
        int mouseX = g_inputManager->m_mouseX;
        int mouseY = g_inputManager->m_mouseY;

        Image *mouse = NULL;

        switch( EclMouseStatus() )
        {
            case EclMouseStatus_NoWindow:
				if ( m_mouseCursor )
				{
					mouse = g_resource->GetImage( m_mouseCursor );
				}
				else
				{
					mouse = g_resource->GetImage( "gui/mouse.bmp" );
				}
				break;
            case EclMouseStatus_ResizeHV:   mouse = g_resource->GetImage( "gui/resize_hv.bmp" );    break;
            case EclMouseStatus_ResizeH:    mouse = g_resource->GetImage( "gui/resize_h.bmp" );     break;
            case EclMouseStatus_ResizeV:    mouse = g_resource->GetImage( "gui/resize_v.bmp" );     break;
            case EclMouseStatus_Draggable:  mouse = g_resource->GetImage( "gui/move.bmp" );         break;
        }

        if( !mouse )
        {
			mouse = g_resource->GetImage( "gui/mouse.bmp" );
        }

        g_renderer->Blit( mouse, mouseX-8, mouseY-8, White );
    }
}


void Interface::ShowMessage( Fixed longitude, Fixed latitude, int teamId, char *msg, bool showLarge )
{
    WorldMessage *themsg = new WorldMessage();
    themsg->m_longitude = longitude;
    themsg->m_latitude = latitude;
    themsg->m_teamId = teamId;
    themsg->SetMessage( msg );
    m_messages.PutDataAtStart( themsg );

    int maxMessages = (g_windowManager->WindowH() / 4.0f) / (g_windowManager->WindowW() / 80.0f);
    while( m_messages.ValidIndex(maxMessages) )
    {
        WorldMessage *thisMsg = m_messages[maxMessages];
        m_messages.RemoveData(maxMessages);
        delete thisMsg;
    }

    if( showLarge )
    {
        if( m_message ) delete m_message;
        m_message = strdup( msg );
        m_messageTimer = GetHighResTime();
    }
}


void Interface::Shutdown()
{
    EclShutdown();
}

void Interface::OpenSetupWindows()
{
    if( !EclGetWindow("Main Menu" ) )
    {
        MainMenu *mainmenu = new MainMenu();
        EclRegisterWindow( mainmenu );
    }
}

void Interface::OpenGameWindows()
{    
    EclRegisterWindow( new WorldStatusWindow( "WorldStatus" ) );
    EclRegisterWindow( new ScoresWindow() );

#ifndef TESTBED
    EclRegisterWindow( new Toolbar() );

    //
    // If there are human opponents open the Chat window

    bool humanPlayers = false;
    bool localPlayer = false;
    for( int i = 0; i < g_app->GetWorld()->m_teams.Size(); ++i )
    {
        Team *team = g_app->GetWorld()->m_teams[i];
        if( team->m_type == Team::TypeRemotePlayer )
        {
            humanPlayers = true;            
        }

        if( team->m_type == Team::TypeLocalPlayer )
        {
            localPlayer = true;
        }
    }

    if( humanPlayers )
    {
        ChatWindow *chat = new ChatWindow();
        EclRegisterWindow( chat );
    }

    //
    // If tooltips are on, open up the Unit Help window

    if( g_preferences->GetInt(PREFS_INTERFACE_TOOLTIPS) == 1)
    {
        EclRegisterWindow( new InfoWindow() );
    }
#endif


#ifdef TESTBED
    // Auto-open the Net-stats window if this is a testbed
    EclRegisterWindow( new NetworkWindow("Network", ) );
#endif


    //
    // If the Local Player is participating open up the Units window
#ifndef NON_PLAYABLE
    if( localPlayer )
    {
        SidePanel *sidePanel = new SidePanel("Side Panel");
        EclRegisterWindow( sidePanel );
    }
#endif

   

    //
    // If this is a tutorial game pop up the Tutorial Window

    if( g_app->GetTutorial() )
    {
        EclRegisterWindow( new TutorialWindow() );
    }   
}


bool Interface::UsingChatWindow()
{
    EclWindow *currentWindow = EclGetWindow();
    if( currentWindow &&
        stricmp(currentWindow->m_name, "Comms Window") == 0 )
    {
        ChatWindow *chat = (ChatWindow *)currentWindow;
        if( chat->m_typingMessage )
        {
            return true;
        }
    }

    ChatWindow *chat = (ChatWindow *)EclGetWindow("Comms Window");
    if( chat &&
        chat->m_typingMessage )
    {
        return true;
    }

    return false;
}

void Interface::SetMouseCursor( const char *filename )
{
	if ( m_mouseCursor )
	{
		delete [] m_mouseCursor;
		m_mouseCursor = NULL;
	}

	if ( filename )
	{
		size_t m_mouseCursor_size = strlen ( filename ) + 1;
		m_mouseCursor = new char [ m_mouseCursor_size ];
		strncpy ( m_mouseCursor, filename, m_mouseCursor_size );
		m_mouseCursor[ m_mouseCursor_size - 1 ] = '\0';
	}
}
