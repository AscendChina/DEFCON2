#include "lib/universal_include.h"

#include <time.h>

#include "lib/gucci/window_manager.h"
#include "lib/language_table.h"
#include "lib/preferences.h"
#include "lib/resource/resource.h"
#include "lib/render/renderer.h"
#include "lib/netlib/net_lib.h"
#include "lib/math/random_number.h"
#include "lib/hi_res_time.h"
#include "lib/metaserver/metaserver.h"
#include "lib/metaserver/metaserver_defines.h"
#include "lib/metaserver/authentication.h"

#include "interface/components/drop_down_menu.h"
#include "interface/components/message_dialog.h"
#include "interface/serverbrowser_window.h"
#include "interface/lobby_window.h"
#include "interface/options/soundoptions_window.h"
#include "interface/options/screenoptions_window.h"
#include "interface/debug_window.h"
#include "interface/chat_window.h"
#include "interface/interface.h"
#include "interface/authkey_window.h"
#include "interface/network_window.h"
#include "interface/demo_window.h"
#include "interface/mod_window.h"

#include "app/globals.h"
#include "app/app.h"
#include "app/game.h"
#include "app/tutorial.h"
#ifdef TARGET_OS_MACOSX
#include "app/macutil.h"
#endif

#include "network/network_defines.h"
#include "network/Server.h"

#include "world/earthdata.h"

#include "renderer/map_renderer.h"
#include "renderer/lobby_renderer.h"

#include "mainmenu.h"

#define ZOOM_SPEED_SLOW 0.4


#if 0
class MainMenuNetworkButton : public InterfaceButton
{
    void MouseUp()
    {
        if( !EclGetWindow( "Network" ) )
        {
            NetworkWindow *window = new NetworkWindow( "Network" );
            EclRegisterWindow( window, m_parent );
        }
        else
        {
            EclBringWindowToFront( "Network" );
        }
    }
};
#endif


class MainMenuNewGameButton : public InterfaceButton
{
    void MouseUp()
    {   
        if( g_app->GetServer() )
        {
            return;
        }

        //
        // If there is an existing game, shut it down now

        g_app->ShutdownCurrentGame();
        EclRemoveWindow( "LOBBY" );
        EclRemoveWindow( m_parent->m_name );

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
            return;
        }


        //
        // Start up a new game
        
        LobbyWindow *lobby = new LobbyWindow();
        bool success = lobby->StartNewServer();

        if( success )
        {
            ChatWindow *chat = new ChatWindow();
            chat->SetPosition( g_windowManager->WindowW()/2 - chat->m_w/2, 
                               g_windowManager->WindowH() - chat->m_h - 30 );
            EclRegisterWindow( chat );

            float lobbyX = g_windowManager->WindowW()/2 - lobby->m_w/2;
            float lobbyY = chat->m_y - lobby->m_h - 30;
            lobbyY = max( lobbyY, 0.0f );
            lobby->SetPosition(lobbyX, lobbyY);
            EclRegisterWindow( lobby );
        }
    }

    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        InterfaceButton::Render( realX, realY, highlighted, clicked );
    }
};


class MainMenuJoinGameButton : public InterfaceButton
{
    void MouseUp()
    {
        if( !EclGetWindow("Server Browser") )
        {
            EclRemoveWindow( m_parent->m_name );

            ServerBrowserWindow *window = new ServerBrowserWindow();
            EclRegisterWindow( window );
            window->Centralise();
        }
    }

    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        InterfaceButton::Render( realX, realY, highlighted, clicked );
    }
};


class MainMenuOptionsButton : public InterfaceButton
{
    void MouseUp()
    {
        EclRemoveWindow( m_parent->m_name );

        OptionsMenuWindow *window = new OptionsMenuWindow();
        EclRegisterWindow( window );
        window->Centralise();
    }
};

class ConfirmExitButton : public InterfaceButton
{
    void MouseUp()
    {
        g_app->Shutdown();
    }
};

class ConfirmExitWindow : public InterfaceWindow
{
public:
    ConfirmExitWindow()
    :   InterfaceWindow("Confirm Exit", "dialog_confirm_exit", true)
    {
        SetSize(200,100);
    }

    void Create()
    {
        ConfirmExitButton *yes = new ConfirmExitButton();
        yes->SetProperties( "ConfirmExitWindowYes", 10, m_h-30, 70, 20, "dialog_yes", " ", true, false );
        RegisterButton( yes );

        CloseButton *no = new CloseButton();
        no->SetProperties( "ConfirmExitWindowNo", m_w-80, m_h-30, 70, 20, "dialog_no", " ", true, false );
        RegisterButton( no );
    }

    void Render( bool hasFocus )
    {
        InterfaceWindow::Render( hasFocus );
        g_renderer->TextCentreSimple( m_x+m_w/2, m_y+30, White, 15.0f, LANGUAGEPHRASE("dialog_exit_defcon") );
    }
};

class MainMenuExitButton : public InterfaceButton
{
    void MouseUp()
    {
      //  if( g_app->GetClientToServer() &&
       //     g_app->GetClientToServer()->AmIDemoClient() )
       // {            
           // DemoWindow *demoWindow = new DemoWindow();
       //     demoWindow->m_exitGameButton = true;
           // EclRegisterWindow( demoWindow );
       // }
      //  else
      //  {
            if( !EclGetWindow("Confirm Exit") )
            {
                EclRemoveWindow( m_parent->m_name );

                ConfirmExitWindow *window = new ConfirmExitWindow();
                EclRegisterWindow( window );
                window->Centralise();
            }
            else
            {
                EclBringWindowToFront("Confirm Exit");
            }
       // }
    }
};

class ConfirmLeaveGameButton : public InterfaceButton
{
    void MouseUp()
    {
        g_app->ShutdownCurrentGame();
        EclRemoveAllWindows();
        g_app->GetInterface()->OpenSetupWindows();
    }
};

class ConfirmLeaveGameWindow : public InterfaceWindow
{
public:
    ConfirmLeaveGameWindow()
    :   InterfaceWindow("Leave Game", "dialog_leavegame", true)
    {
        SetSize(200,100);        
    }

    void Create()
    {
        ConfirmLeaveGameButton *yes = new ConfirmLeaveGameButton();
        yes->SetProperties( "ConfirmLeaveGameWindowYes", 10, m_h-30, 70, 20, "dialog_yes", " ", true, false );
        RegisterButton( yes );

        CloseButton *no = new CloseButton();
        no->SetProperties( "ConfirmLeaveGameWindowNo", m_w-80, m_h-30, 70, 20, "dialog_no", " ", true, false );
        RegisterButton( no );
    }

    void Render( bool hasFocus )
    {
        InterfaceWindow::Render( hasFocus );
        g_renderer->TextCentreSimple( m_x+m_w/2, m_y+30, White, 15.0f, LANGUAGEPHRASE("dialog_leave_this_game") );
    }
};

class MainMenuLeaveGameButton : public InterfaceButton
{
    void MouseUp()
    {
        if( !EclGetWindow("Leave Game") )
        {
            EclRemoveWindow( m_parent->m_name );

            ConfirmLeaveGameWindow *window = new ConfirmLeaveGameWindow();
            EclRegisterWindow( window );
            window->Centralise();
        }
        else
        {
            EclBringWindowToFront("Leave Game");
        }
    }
};


class DebugButton : public InterfaceButton
{
#ifdef TOOLS_ENABLED
    void MouseUp()
    {
        EclRemoveWindow( m_parent->m_name );
        DebugWindow *debug = new DebugWindow();
        EclRegisterWindow( debug );
    }
#endif
};

class DemoModeButton : public InterfaceButton
{
public:
    bool m_disabled;
    float m_timer;    
    bool m_readyToStart;

    DemoModeButton()
    :   InterfaceButton(),
        m_timer(0.1f),
        m_disabled(false),
        m_readyToStart(false)
    {
    }

    void MouseUp()
    {
        if( g_app->GetServer() )
        {
            return;
        }
        if( !m_disabled )
        {
            m_disabled = true;
    
            g_app->ShutdownCurrentGame();

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
                return;
            }


            char ourIp[256];
            GetLocalHostIP( ourIp, sizeof(ourIp) );

            bool success = g_app->InitServer();

            if( success )
            {
                int ourPort = g_app->GetServer()->GetLocalPort();
	            g_app->GetClientToServer()->ClientJoin( ourIp, ourPort );

                g_app->InitWorld();

                g_app->GetGame()->GetOption("MaxTeams")->m_currentValue = 3;
                g_app->GetGame()->GetOption("TeamSwitching")->m_currentValue = 1;
                g_app->GetGame()->GetOption("MaxSpectators")->m_currentValue = 0;
                
                MessageDialog *dialog = new MessageDialog( "Preparing Game...", 
                                                           "dialog_game_starting", true, 
                                                           "dialog_preparing", true );
                EclRegisterWindow( dialog );
                dialog->RemoveButton( "Close" );

                //g_app->GetMapRenderer()->ToggleAutoCam();
            }
        }
    }

    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        InterfaceButton::Render( realX, realY, highlighted, clicked );

        if( !g_app->GetServer() )
        {
            m_disabled = false;
            m_timer = 2.0f;
        }

        if( m_disabled )
        {
            m_timer -= g_advanceTime;
            if( m_timer < 0.0f )
            {
                if( g_app->GetWorld()->m_teams.Size() < 3 )
                {
                    m_disabled = false;
                    g_app->GetWorld()->InitialiseSpectator( g_app->GetClientToServer()->m_clientId );

                    g_app->GetWorld()->InitialiseTeam( 0, Team::TypeAI, g_app->GetClientToServer()->m_clientId );
                    g_app->GetWorld()->InitialiseTeam( 1, Team::TypeAI, g_app->GetClientToServer()->m_clientId );
                    g_app->GetWorld()->InitialiseTeam( 2, Team::TypeAI, g_app->GetClientToServer()->m_clientId );

                    g_app->GetWorld()->GetTeam(0)->m_aiAssaultTimer = 1800 + syncsfrand( 900 );
                    g_app->GetWorld()->GetTeam(1)->m_aiAssaultTimer = 1800 + syncsfrand( 900 );
                    g_app->GetWorld()->GetTeam(2)->m_aiAssaultTimer = 1800 + syncsfrand( 900 );

                    g_app->GetWorld()->GetTeam(0)->m_readyToStart = true;
                    g_app->GetWorld()->GetTeam(1)->m_readyToStart = true;
                    g_app->GetWorld()->GetTeam(2)->m_readyToStart = true;                    

                    g_app->GetWorld()->GetTeam(0)->m_randSeed = time(NULL);
                }
            }
        }
    }
};

class TutorialModeButton : public EclButton
{
public:
    int m_chapter;

    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        int tutorialCompleted = g_preferences->GetInt( PREFS_TUTORIAL_COMPLETED );

        if( m_chapter <= tutorialCompleted+1 )
        {
            if( highlighted || clicked )
            {
                g_renderer->RectFill( realX, realY, m_w, m_h, Colour(255,255,255,50) );
                g_renderer->Rect( realX, realY, m_w, m_h, Colour(255,255,255,100) );
            }
        }

        float alpha = 255;
        if( m_chapter > tutorialCompleted+1 ) alpha = 50;
        if( m_chapter == tutorialCompleted+1 && fmodf(GetHighResTime()*2,2) < 1.0f ) alpha = 55;

		char caption[512];
		sprintf( caption, LANGUAGEPHRASE("dialog_chapter") );
		LPREPLACEINTEGERFLAG( 'C', m_chapter, caption );

        g_renderer->TextCentre( realX+m_w/2, realY, Colour(255,255,255,alpha), 16, caption );

        char stringId[256];
        sprintf( stringId, "tutorial_levelname_%d", m_chapter );
        char *description = LANGUAGEPHRASE(stringId);
        g_renderer->TextCentre( realX+m_w/2, realY+15, Colour(200,200,255,alpha), 12, description );
    }

#ifndef NON_PLAYABLE
    void MouseUp()
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
            return;
        }


        int tutorialCompleted = g_preferences->GetInt( PREFS_TUTORIAL_COMPLETED );

        if( m_chapter <= tutorialCompleted+1 )
        {
            g_app->ShutdownCurrentGame();

            char ourIp[256];
            GetLocalHostIP( ourIp, sizeof(ourIp) );

            bool success = g_app->InitServer();

            if( success )
            {
                int ourPort = g_app->GetServer()->GetLocalPort();

	            g_app->GetClientToServer()->ClientJoin( ourIp, ourPort );
                g_app->InitWorld();
                g_app->StartTutorial( m_chapter );

                EclRemoveAllWindows();

                MessageDialog *dialog = new MessageDialog( "Preparing Game...", 
                                                           "dialog_tutorial_starting", true, 
                                                           "dialog_preparing", true );
                EclRegisterWindow( dialog );
                dialog->RemoveButton( "Close" );
            }
        }
    }
#endif
};


class TutorialMenu : public InterfaceWindow
{
public:
    TutorialMenu()
        :   InterfaceWindow("TutorialMenu", "tutorial", true)
    {
        SetSize( 250, 360 );
        Centralise();
    }

    void Create()
    {
        InterfaceWindow::Create();

        InvertedBox *box = new InvertedBox();
        box->SetProperties( "box", 10, 30, m_w-20, m_h-70, " ", " ", false, false );
        RegisterButton( box );

        float yPos = 40;
        float h = 30;
        float gap = 10;

        for( int i = 1; i < 8; ++i )
        {
            char name[256];
            sprintf( name, "Tutorial %d", i );

			char caption[512];
			sprintf( caption, LANGUAGEPHRASE("dialog_tutorial") );
			LPREPLACEINTEGERFLAG( 'T', i, caption );

            TutorialModeButton *chapter = new TutorialModeButton();
            chapter->SetProperties( name, 20, yPos, m_w-40, h, caption, " ", false, false );
            chapter->m_chapter = i;
            RegisterButton( chapter );

            yPos += h;
            yPos += gap;
        }

        CloseButton *close = new CloseButton();
        close->SetProperties( "Close", 20, m_h-30, m_w-40, 20, "dialog_close", " ", true, false );
        RegisterButton( close );
    }
};


class ShowTutorialMenuButton : public InterfaceButton
{
    void MouseUp()
    {
        int tutorialCompleted = g_preferences->GetInt( PREFS_TUTORIAL_COMPLETED );

        if( tutorialCompleted < 1 )
        {
            g_app->ShutdownCurrentGame();

            char ourIp[256];
            GetLocalHostIP( ourIp, sizeof(ourIp) );

            bool success = g_app->InitServer();

            if( success )
            {
                int ourPort = g_app->GetServer()->GetLocalPort();

                g_app->GetClientToServer()->ClientJoin( ourIp, ourPort );
                g_app->InitWorld();
                g_app->StartTutorial( 1 );

                EclRemoveAllWindows();

                MessageDialog *dialog = new MessageDialog( "Preparing Game...", 
                                                           "dialog_tutorial_starting", true, 
                                                           "dialog_preparing", true );
                EclRegisterWindow( dialog );
                dialog->RemoveButton( "Close" );
            }
        }
        else
        {
            EclRemoveWindow( m_parent->m_name );
            EclRegisterWindow( new TutorialMenu() );
        }
    }
};


class ModWindowButton : public InterfaceButton
{
public:
    void MouseUp()
    {
        char authKey[256];
        Authentication_GetKey( authKey );
        bool demoUser = Authentication_IsDemoKey(authKey);
        bool authOk = ( Authentication_SimpleKeyCheck(authKey) >= 0 );

        if( !demoUser && authOk )
        {
            if( !EclGetWindow( "Mods" ) )
            {
                EclRegisterWindow( new ModWindow(), m_parent );
            }
        }
        else
        {
            DemoWindow *demoWindow = new DemoWindow();
            EclRegisterWindow( demoWindow );
        }
    }
};

#ifdef TARGET_OS_MACOSX
class UserManualButton : public InterfaceButton
{
	void MouseUp()
	{
		int windowed = g_preferences->GetInt( "ScreenWindowed", 1 );
        if( !windowed )
        {
            // Switch to windowed mode if required
            g_preferences->SetInt( "ScreenWindowed", 1 );
            g_preferences->SetInt( "ScreenWidth", 1024 );
            g_preferences->SetInt( "ScreenHeight", 768 );

            g_app->ReinitialiseWindow();

            g_preferences->Save();        
        }
		
		OpenBundleResource("DEFCON User Manual", "pdf");
	}
};
#endif

// ============================================================================

void ConfirmExit( const char *_parentWindowName )
{

		//if( g_app->GetClientToServer() &&
		//	g_app->GetClientToServer()->AmIDemoClient() )
		//{
			//DemoWindow *demoWindow = new DemoWindow();
			//demoWindow->m_exitGameButton = true;
			//EclRegisterWindow( demoWindow );
		//}
		//else
		//{
			if( !EclGetWindow("Confirm Exit") )
			{
			if (_parentWindowName != NULL)
					EclRemoveWindow( (char *)_parentWindowName );

				ConfirmExitWindow *window = new ConfirmExitWindow();
				EclRegisterWindow( window );
				window->Centralise();
			}
			else
			{
				EclBringWindowToFront("Confirm Exit");
			}
	//	}
}

void AttemptQuitImmediately()
{
	// Restrict quit immediately feature to paid users
	//if (g_app->GetClientToServer() && g_app->GetClientToServer()->AmIDemoClient())
		ConfirmExit( NULL );
	//else
		//g_app->Shutdown();
}

// ============================================================================

MainMenu::MainMenu()
:   InterfaceWindow( "Main Menu", "dialog_mainmenu", true )
{
#ifdef TARGET_OS_MACOSX
	// HACK: Make room for User Manual button
	SetSize( 190, 325 );
#else
    SetSize( 190, 300 );
#endif
    Centralise();
}


void MainMenu::Create()
{
    InterfaceWindow::Create();
    
    int y = 5;
    int h = 19;
    int g = 6;

    InterfaceButton *button = NULL;
    
    //
    // New Game

    if( !g_app->m_gameRunning )
    {
        button = new MainMenuNewGameButton();
        button->SetProperties( "New Game", 10, y+=h+g, m_w-20, h, "dialog_newgame", " ", true, false );
        RegisterButton( button );
    }


    //
    // Join Game

    button = new MainMenuJoinGameButton();
    button->SetProperties( "Join Game", 10, y+=h+g, m_w-20, h, "dialog_joingame", " ", true, false );
    RegisterButton( button );

    y+=h;

#ifdef TARGET_OS_MACOSX	
	button = new UserManualButton();
	button->SetProperties( "User Manual", 10, y+=h+g, m_w-20, h, "dialog_manual", " ", true, false );
	RegisterButton( button ); 
#endif

    //
    // Tutorial
    // Rolling Demo

    if( !g_app->m_gameRunning )
    {
#ifndef NON_PLAYABLE
        button = new ShowTutorialMenuButton();
        button->SetProperties( "Tutorial", 10, y+=h+g, m_w-20, h, "tutorial", " ", true, false );
        RegisterButton( button );
#endif

        DemoModeButton *demo = new DemoModeButton();
        demo->SetProperties( "Rolling Demo", 10, y+=h+g, m_w-20, h, "dialog_rolling_demo", " ", true, false );
        RegisterButton( demo );
    }
    else
    {
        m_h -= h*3;
    }

    //
    // Options

    button = new MainMenuOptionsButton();
    button->SetProperties( "Options", 10, y+=h+g, m_w-20, h, "dialog_options", " ", true, false );
    RegisterButton( button );


    //
    // Tools

#ifdef TOOLS_ENABLED
    DebugButton *debug = new DebugButton();
    debug->SetProperties( "Tools", 10, y+=h+g, m_w-20, h, "dialog_tools", " ", true, false );
    RegisterButton( debug );
#endif

    //
    // Mods

    ModWindowButton *mods = new ModWindowButton();
    mods->SetProperties( "Mods", 10, y+=h+g, m_w-20, h, "dialog_mod", " ", true, false );
    RegisterButton( mods );


    //
    // Buy Now

    char authKey[256];
    Authentication_GetKey(authKey);
    bool demoUser = Authentication_IsDemoKey(authKey);

    if( demoUser )
    {
#if !defined(RETAIL)
      //  BuyNowButton *buyNow = new BuyNowButton();
      //  buyNow->SetProperties( "BUY NOW", 10, y+=h+g, m_w-20, h, "dialog_buy_now_caps", " ", true, false );
       // buyNow->m_closeOnClick = false;
      //  RegisterButton( buyNow );
#endif
    }


    //
    // Leave Game

    if( g_app->GetClientToServer()->IsConnected() )
    {            
        button = new MainMenuLeaveGameButton();
        button->SetProperties( "Leave Game", 10, m_h-54, m_w-20, h, "dialog_leavegame", " ", true, false );
        RegisterButton( button );
    }


    //
    // Exit game

    button = new MainMenuExitButton();
    button->SetProperties( "Exit Defcon", 10, m_h-30, m_w-20, h, "dialog_exit", " ", true, false );
    RegisterButton( button );
}


// ***************************************************************************
// Class DefconScreenOptionsWindow
// ***************************************************************************

class DefconScreenOptionsWindow : public ScreenOptionsWindow
{    
    void RestartWindowManager()
    {
        g_app->ReinitialiseWindow();
    }
};


// ============================================================================


class PlayerNameOptionsButton : public InterfaceButton
{
    void MouseUp()
    {
		if( !g_app->GetClientToServer()->IsConnected() )
		{
			EclRegisterWindow( new RenameTeamWindow() );
		}
    }

    void Render( int realX, int realY, bool highlighted, bool clicked )
	{
		if( !g_app->GetClientToServer()->IsConnected() )
		{
			InterfaceButton::Render( realX, realY, highlighted, clicked );
		}
		else
		{
			float originalAlpha = g_renderer->m_alpha;
			g_renderer->m_alpha *= 0.667f;
			InterfaceButton::Render( realX, realY, false, false );
			g_renderer->m_alpha = originalAlpha;
		}
	}
};


class ScreenOptionsButton : public InterfaceButton
{
    void MouseUp()
    {
        DefconScreenOptionsWindow *window = new DefconScreenOptionsWindow();
		EclRegisterWindow( window );
        window->Centralise();
    }
};


class GraphicsOptionsButton : public InterfaceButton
{
    void MouseUp()
    {
	    EclRegisterWindow( new GraphicsOptionsWindow() );
    }
};


class SoundOptionsButton : public InterfaceButton
{
    void MouseUp()
    {
        SoundOptionsWindow *window = new SoundOptionsWindow();
        EclRegisterWindow( window );
        window->Centralise();
    }
};


class NetworkOptionsButton : public InterfaceButton
{
    void MouseUp()
    {
        EclRegisterWindow( new NetworkOptionsWindow(), m_parent );
    }
};

/*
class OtherOptionsButton : public InterfaceButton
{
    void MouseUp()
    {
		if (!EclGetWindow( "Other Options" )))
		{
			//EclRegisterWindow( new PrefsOtherWindow() );
		}
    }
};
*/

#if !defined(RETAIL_DEMO)
class AuthOptionsButton : public InterfaceButton
{
    void MouseUp()
    {
        EclRegisterWindow( new AuthKeyWindow(), m_parent );
    }
};
#endif

class ControlOptionsButton : public InterfaceButton
{
    void MouseUp()
    {
        EclRegisterWindow( new ControlOptionsWindow(), m_parent );
    }
};

// ***************************************************************************
// Class OptionsMenuWindow
// ***************************************************************************

OptionsMenuWindow::OptionsMenuWindow()
:   InterfaceWindow( "OptionsMenu", "dialog_options", true )
{
    SetSize( 190, 240 );
    Centralise();
}


void OptionsMenuWindow::Create()
{
    InterfaceWindow::Create();

    int y = 5;
    int h = 19;
    int g = 6;

    ScreenOptionsButton *screen = new ScreenOptionsButton();
    screen->SetProperties( "Screen", 10, y+=h+g, m_w-20, h, "dialog_screenoptions", " ", true, false );
    RegisterButton( screen );

    GraphicsOptionsButton *graphics = new GraphicsOptionsButton();
    graphics->SetProperties( "Graphics", 10, y+=h+g, m_w-20, h, "dialog_graphicsoptions", " ", true, false );
    RegisterButton( graphics );

    SoundOptionsButton *sound = new SoundOptionsButton();
    sound->SetProperties( "Sound", 10, y+=h+g, m_w-20, h, "dialog_soundoptions", " ", true, false );
    RegisterButton( sound );

    //OtherOptionsButton *other = new OtherOptionsButton();
    //other->SetProperties( "Other Options", 10, y+=h, m_w-20, h-3, "dialog_otheroptions", " ", true, false );
    //RegisterButton( other );

    NetworkOptionsButton *network = new NetworkOptionsButton();
    network->SetProperties( "Network", 10, y+=h+g, m_w-20, h, "dialog_networkoptions", " ", true, false );
    RegisterButton( network );

#if !defined(RETAIL_DEMO)
   // AuthOptionsButton *auth = new AuthOptionsButton();
   // auth->SetProperties( "Authentication", 10, y+=h+g, m_w-20, h, "dialog_authoptions", " ", true, false );
   // RegisterButton( auth );
#endif

    ControlOptionsButton *control = new ControlOptionsButton();
    control->SetProperties( "Interface", 10, y+=h+g, m_w-20, h, "dialog_controloptions", " ", true, false );
    RegisterButton( control );

    CloseButton *close = new CloseButton();
    close->SetProperties( "Close", 10, m_h - 30, m_w-20, h, "dialog_close", " ", true, false );
    RegisterButton( close );

}


// ***************************************************************************
// Class GraphicsOptionsWindow
// ***************************************************************************

class ApplyGraphicsButton : public InterfaceButton
{
    void MouseUp()
    {
        GraphicsOptionsWindow *gow = (GraphicsOptionsWindow *) m_parent;

        g_preferences->SetInt( PREFS_GRAPHICS_SMOOTHLINES, gow->m_smoothLines );
        g_preferences->SetInt( PREFS_GRAPHICS_BORDERS, gow->m_borders );
        g_preferences->SetInt( PREFS_GRAPHICS_CITYNAMES, gow->m_cityNames );
        g_preferences->SetInt( PREFS_GRAPHICS_COUNTRYNAMES, gow->m_countryNames );
        g_preferences->SetInt( PREFS_GRAPHICS_WATER, gow->m_water );
        g_preferences->SetInt( PREFS_GRAPHICS_RADIATION, gow->m_radiation );
        g_preferences->SetInt( PREFS_GRAPHICS_LOWRESWORLD, gow->m_lowResWorld );
        g_preferences->SetInt( PREFS_GRAPHICS_TRAILS, gow->m_trails );
        g_preferences->SetInt( PREFS_GRAPHICS_LOBBYEFFECTS, gow->m_lobbyEffects );
        
        g_app->GetEarthData()->LoadCoastlines();
        
        g_resource->DeleteDisplayList( "MapCoastlines" );
        g_resource->DeleteDisplayList( "LobbyGlobe" );

        g_preferences->Save();
    }
};


GraphicsOptionsWindow::GraphicsOptionsWindow()
:   InterfaceWindow( "Graphics", "dialog_graphicsoptions", true )
{
    SetSize( 390, 400 );
    Centralise();

    m_smoothLines   = g_preferences->GetInt( PREFS_GRAPHICS_SMOOTHLINES );
    m_borders       = g_preferences->GetInt( PREFS_GRAPHICS_BORDERS );
    m_cityNames     = g_preferences->GetInt( PREFS_GRAPHICS_CITYNAMES );
    m_countryNames  = g_preferences->GetInt( PREFS_GRAPHICS_COUNTRYNAMES );
    m_water         = g_preferences->GetInt( PREFS_GRAPHICS_WATER );
    m_radiation     = g_preferences->GetInt( PREFS_GRAPHICS_RADIATION );
    m_lowResWorld   = g_preferences->GetInt( PREFS_GRAPHICS_LOWRESWORLD );
    m_trails        = g_preferences->GetInt( PREFS_GRAPHICS_TRAILS );
    m_lobbyEffects  = g_preferences->GetInt( PREFS_GRAPHICS_LOBBYEFFECTS );
}


void GraphicsOptionsWindow::Create()
{
    InterfaceWindow::Create();
    
    int x = 200;
    int w = m_w - x - 20;
    int y = 30;
    int h = 30;

    InvertedBox *box = new InvertedBox();
    box->SetProperties( "invert", 10, 50, m_w - 20, m_h - 110, " ", " ", false, false );        
    RegisterButton( box );

    DropDownMenu *dropDown = new DropDownMenu();
    dropDown->SetProperties( "Low-detail World", x, y+=h, w, 20, "dialog_lowdetailworld", " ", true, false );
    dropDown->AddOption( "dialog_enabled", 1, true );
    dropDown->AddOption( "dialog_disabled", 0, true );
    dropDown->RegisterInt( &m_lowResWorld );
    RegisterButton(dropDown);

    dropDown = new DropDownMenu();
    dropDown->SetProperties( "Smooth Lines", x, y+=h, w, 20, "dialog_smoothlines", " ", true, false );
    dropDown->AddOption( "dialog_enabled", 1, true );
    dropDown->AddOption( "dialog_disabled", 0, true );
    dropDown->RegisterInt( &m_smoothLines );
    RegisterButton(dropDown);

    dropDown = new DropDownMenu();
    dropDown->SetProperties( "Show Borders", x, y+=h, w, 20, "dialog_borders", " ", true, false );
    dropDown->AddOption( "dialog_enabled", 1, true );
    dropDown->AddOption( "dialog_disabled", 0, true );
    dropDown->RegisterInt( &m_borders );
    RegisterButton(dropDown);

    dropDown = new DropDownMenu();
    dropDown->SetProperties( "Show City Names", x, y+=h, w, 20, "dialog_citynames", " ", true, false );
    dropDown->AddOption( "dialog_enabled", 1, true );
    dropDown->AddOption( "dialog_disabled", 0, true );
    dropDown->RegisterInt( &m_cityNames );
    RegisterButton(dropDown);

    dropDown = new DropDownMenu();
    dropDown->SetProperties( "Show Country Names", x, y+=h, w, 20, "dialog_countrynames", " ", true, false );
    dropDown->AddOption( "dialog_enabled", 1, true );
    dropDown->AddOption( "dialog_disabled", 0, true );
    dropDown->RegisterInt( &m_countryNames );
    RegisterButton(dropDown);

    dropDown = new DropDownMenu();
    dropDown->SetProperties( "Show Water", x, y+=h, w, 20, "dialog_water", " ", true, false );
    dropDown->AddOption( "dialog_disabled", 0, true );
    dropDown->AddOption( "dialog_waternormal", 1, true );
    dropDown->AddOption( "dialog_watershaded", 2, true );
    dropDown->RegisterInt( &m_water );
    RegisterButton(dropDown);

    dropDown = new DropDownMenu();
    dropDown->SetProperties( "Show Radiation", x, y+=h, w, 20, "dialog_radiation", " ", true, false );
    dropDown->AddOption( "dialog_enabled", 1, true );
    dropDown->AddOption( "dialog_disabled", 0, true );
    dropDown->RegisterInt( &m_radiation );
    RegisterButton(dropDown);

    dropDown = new DropDownMenu();
    dropDown->SetProperties( "Show Object Trails", x, y+=h, w, 20, "dialog_objecttrails", " ", true, false );
    dropDown->AddOption( "dialog_enabled", 1, true );
    dropDown->AddOption( "dialog_disabled", 0, true );
    dropDown->RegisterInt( &m_trails );
    RegisterButton(dropDown);

    dropDown = new DropDownMenu();
    dropDown->SetProperties( "Lobby Effects", x, y+=h, w, 20, "dialog_lobbyeffects", " ", true, false );
    dropDown->AddOption( "dialog_enabled", 1, true );
    dropDown->AddOption( "dialog_disabled", 0, true );
    dropDown->RegisterInt( &m_lobbyEffects );
    RegisterButton(dropDown);

    CloseButton *cancel = new CloseButton();
    cancel->SetProperties( "Close", 10, m_h - 30, m_w / 2 - 15, 20, "dialog_close", " ", true, false );
    RegisterButton( cancel );

    ApplyGraphicsButton *apply = new ApplyGraphicsButton();
    apply->SetProperties( "Apply", cancel->m_x+cancel->m_w+10, m_h - 30, m_w / 2 - 15, 20, "dialog_apply", " ", true, false );
    RegisterButton( apply );     

}


void GraphicsOptionsWindow::Render( bool _hasFocus )
{
    InterfaceWindow::Render( _hasFocus );

    int x = m_x + 20;
    int y = m_y + 35;
    int h = 30;
    int size = 13;

    g_renderer->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_lowdetailworld") );
    g_renderer->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_smoothlines") );
    g_renderer->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_borders") );
    g_renderer->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_citynames") );
    g_renderer->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_countrynames") );
    g_renderer->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_water") );
    g_renderer->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_radiation") );
    g_renderer->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_objecttrails") );
    g_renderer->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_lobbyeffects") );
}


// ***************************************************************************
// Class ControlOptionsWindow
// ***************************************************************************

class ApplyControlsButton : public InterfaceButton
{
    void MouseUp()
    {
        ControlOptionsWindow *cow = (ControlOptionsWindow *) m_parent;

        g_preferences->SetInt( PREFS_INTERFACE_SIDESCROLL, cow->m_sideScrolling );
		g_preferences->SetFloat( PREFS_INTERFACE_ZOOM_SPEED,
								 cow->m_zoomSlower ? ZOOM_SPEED_SLOW : 1.0 );		
        g_preferences->SetInt( PREFS_INTERFACE_CAMDRAGGING, cow->m_camDragging );
        g_preferences->SetInt( PREFS_INTERFACE_TOOLTIPS, cow->m_tooltips );
        g_preferences->SetFloat( PREFS_INTERFACE_POPUPSCALE, cow->m_popupScale );
        g_preferences->SetInt( PREFS_INTERFACE_PANICKEY, cow->m_panicKey );
        g_preferences->SetInt( PREFS_INTERFACE_KEYBOARDLAYOUT, cow->m_keyboardLayout );

        g_preferences->Save();

		int index = 0;
		for( int i = 0; i < g_languageTable->m_languages.Size(); ++i )
		{
			Language *thisLang = g_languageTable->m_languages.GetData( i );
			if( cow->m_language == index )
			{
				g_languageTable->SaveCurrentLanguage( thisLang );
				if( !g_languageTable->m_lang || stricmp( g_languageTable->m_lang->m_name, thisLang->m_name ) != 0 )
				{
					if( g_app->GetClientToServer()->IsConnected() && g_app->m_world )
					{
						EclRegisterWindow( new MessageDialog( "Restart Required", "dialog_restart_required_language", true, "dialog_restart_required_title", true ) );
					}
					else
					{
						g_languageTable->LoadCurrentLanguage( thisLang->m_name );
						g_app->InitFonts();
					}
				}
				break;
			}
			if( thisLang->m_selectable )
			{
				index++;
			}
		}
    }
};

ControlOptionsWindow::ControlOptionsWindow()
:   InterfaceWindow( "Interface", "dialog_controloptions", true )
{
    SetSize( 390, 420 );
    Centralise();

    m_sideScrolling   = g_preferences->GetInt( PREFS_INTERFACE_SIDESCROLL );
	m_zoomSlower	  = ( g_preferences->GetFloat( PREFS_INTERFACE_ZOOM_SPEED ) < 1.0 );
    m_camDragging     = g_preferences->GetInt( PREFS_INTERFACE_CAMDRAGGING );
    m_tooltips        = g_preferences->GetInt( PREFS_INTERFACE_TOOLTIPS );
    m_popupScale      = g_preferences->GetFloat( PREFS_INTERFACE_POPUPSCALE );
    m_panicKey        = g_preferences->GetInt( PREFS_INTERFACE_PANICKEY );
    m_keyboardLayout  = g_preferences->GetInt( PREFS_INTERFACE_KEYBOARDLAYOUT );

	if ( m_keyboardLayout == -1 ) // Not yet defined
	{
#ifdef TARGET_OS_MACOSX
		char* layout = GetKeyboardLayout();
		if ( !strcmp( layout, "French" ) || !strcmp( layout, "Belgian" ) )
			m_keyboardLayout = 2;
		else if ( !strcmp( layout, "Italian" ) )
			m_keyboardLayout = 3;
		else if ( !strcmp( layout, "Dvorak" ) )
			m_keyboardLayout = 4;
		else
			m_keyboardLayout = 1;
		delete layout;
			
		g_preferences->SetInt( PREFS_INTERFACE_KEYBOARDLAYOUT, m_keyboardLayout );
#endif
	}
	
    m_language = -1;
	int index = 0;
	if( g_languageTable->m_lang )
	{
		for( int i = 0; i < g_languageTable->m_languages.Size(); ++i )
		{
			Language *thisLang = g_languageTable->m_languages.GetData( i );
			if( stricmp( thisLang->m_name, g_languageTable->m_lang->m_name ) == 0 )
			{
				m_language = index;
				break;
			}
			if( thisLang->m_selectable )
			{
				index++;
			}
		}
	}
}

void ControlOptionsWindow::Create()
{
    InterfaceWindow::Create();
    
    int x = 200;
    int w = m_w - x - 20;
    int y = 30;
    int h = 30;

    InvertedBox *box = new InvertedBox();
    box->SetProperties( "invert", 10, 50, m_w - 20, m_h - 110, " ", " ", false, false );        
    RegisterButton( box );

    DropDownMenu *dropDown = new DropDownMenu();
    dropDown->SetProperties( "Mouse Scrolling", x, y+=h, w, 20, "dialog_sidescrolling", "tooltip_controls_sidescrolling", true, true );
    dropDown->AddOption( "dialog_enabled", 1, true );
    dropDown->AddOption( "dialog_disabled", 0, true );
    dropDown->RegisterInt( &m_sideScrolling );
    RegisterButton(dropDown);
	
	dropDown = new DropDownMenu();
    dropDown->SetProperties( "Zoom Speed", x, y+=h, w, 20, "dialog_zoomspeed", "tooltip_controls_zoomspeed", true, true );
	dropDown->AddOption( "dialog_zoomnormal", 0, true );
    dropDown->AddOption( "dialog_zoomslow", 1, true );
    dropDown->RegisterInt( &m_zoomSlower );
    RegisterButton(dropDown);

    dropDown = new DropDownMenu();
    dropDown->SetProperties( "Camera Dragging", x, y+=h, w, 20, "dialog_camdragging", "tooltip_controls_camdragging", true, true );
    dropDown->AddOption( "dialog_enabled", 1, true );
    dropDown->AddOption( "dialog_disabled", 0, true );
    dropDown->RegisterInt( &m_camDragging );
    RegisterButton(dropDown);

    dropDown = new DropDownMenu();
    dropDown->SetProperties( "Show Tooltips", x, y+=h, w, 20, "dialog_tooltips", " ", true, false );
    dropDown->AddOption( "dialog_enabled", 1, true );
    dropDown->AddOption( "dialog_disabled", 0, true );
    dropDown->RegisterInt( &m_tooltips );
    RegisterButton(dropDown);

    dropDown = new DropDownMenu();
    dropDown->SetProperties( "Panic Key", x, y+=h, w, 20, "dialog_panickey", " ", true, false );
    dropDown->AddOption( "dialog_enabled", 1, true );
    dropDown->AddOption( "dialog_disabled", 0, true );
    dropDown->RegisterInt( &m_panicKey );
    RegisterButton(dropDown);

    CreateValueControl( "Popup Scale", x, y+=h, w, 20, InputField::TypeFloat, &m_popupScale, 0.1f, 0.1f, 9.0f, NULL, " ", false );

    dropDown = new DropDownMenu();
    dropDown->SetProperties( "Keyboard Layout", x, y+=h, w, 20, "dialog_keyboardlayout", "tooltip_controls_keyboardlayout", true, true );
    dropDown->AddOption( "QWERTY / QWERTZ", 1 );
    dropDown->AddOption( "AZERTY", 2 );
    dropDown->AddOption( "QZERTY", 3 );
    dropDown->AddOption( "DVORAK", 4 );
    dropDown->RegisterInt( &m_keyboardLayout );
    RegisterButton(dropDown);

    PlayerNameOptionsButton *playerName = new PlayerNameOptionsButton();
    playerName->SetProperties( "Player Name", x, y+=h, w, 20, "dialog_playername", " ", true, false );
    RegisterButton( playerName );

    DropDownMenu *language = new DropDownMenu();
    language->SetProperties( "Language", x, y+=h, w, 20, "dialog_language", " ", true, false );
	int langCount = 0;
    for( int i = 0; i < g_languageTable->m_languages.Size(); ++i )
    {
		Language *thisLang = g_languageTable->m_languages.GetData( i );
		if( thisLang->m_selectable )
		{
			language->AddOption( thisLang->m_caption, langCount++, false, thisLang->m_name );
		}
    }
    language->RegisterInt( &m_language );
    RegisterButton( language );

    CloseButton *cancel = new CloseButton();
    cancel->SetProperties( "Close", 10, m_h - 30, m_w / 2 - 15, 20, "dialog_close", " ", true, false );
    RegisterButton( cancel );

    ApplyControlsButton *apply = new ApplyControlsButton();
    apply->SetProperties( "Apply", cancel->m_x+cancel->m_w+10, m_h - 30, m_w / 2 - 15, 20, "dialog_apply", " ", true, false );
    RegisterButton( apply );     

}

void ControlOptionsWindow::Render( bool _hasFocus )
{
    InterfaceWindow::Render( _hasFocus );

    int x = m_x + 20;
    int y = m_y + 35;
    int h = 30;
    int size = 13;

    g_renderer->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_sidescrolling") );
	g_renderer->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_zoomspeed") );
    g_renderer->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_camdragging") );
    g_renderer->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_tooltips") );
    g_renderer->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_panickey") );
    g_renderer->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_popupscale") );
    g_renderer->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_keyboardlayout") );
    g_renderer->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_playername") );

    g_renderer->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_language") );
}



// ============================================================================

class ApplyNetworkButton : public InterfaceButton
{
    void MouseUp()
    {
        NetworkOptionsWindow *parent = (NetworkOptionsWindow *) m_parent;
        
        if( g_app->GetClientToServer()->IsConnected() )
        {
            sprintf( parent->m_clientPort, "%d", g_preferences->GetInt( PREFS_NETWORKCLIENTPORT ) );
            sprintf( parent->m_serverPort, "%d", g_preferences->GetInt( PREFS_NETWORKSERVERPORT ) );

            EclRegisterWindow( new MessageDialog( "NetworkFailed",
                                                  "dialog_networkfailedgamerunning", true,  
                                                  "dialog_networkfailed", true ),
                               parent );
        }
        else
        {
            //
            // Update client port

            int clientPort = atoi(parent->m_clientPort);
            if( clientPort == 0 ||
                (clientPort > 1024 && clientPort < 65535) )
            {
                int currentValue = g_preferences->GetInt( PREFS_NETWORKCLIENTPORT );
                g_preferences->SetInt( PREFS_NETWORKCLIENTPORT, clientPort );

                if( currentValue != clientPort )
                {
                    g_app->GetClientToServer()->OpenConnections();
                }
            }
            else
            {
                sprintf( parent->m_clientPort, "%d", g_preferences->GetInt( PREFS_NETWORKCLIENTPORT ) );
            }

            //
            // Update server port

            int serverPort = atoi(parent->m_serverPort);
            if( serverPort == 0 ||
                (serverPort > 1024 && serverPort < 65535) )
            {
                g_preferences->SetInt( PREFS_NETWORKSERVERPORT, serverPort );
            }
            else
            {
                sprintf( parent->m_serverPort, "%d", g_preferences->GetInt( PREFS_NETWORKSERVERPORT ) );
            }


            //
            // Update metaserver port

            int metaserverPort = atoi(parent->m_metaserverPort);
            g_preferences->SetInt( PREFS_NETWORKMETASERVER, metaserverPort );
            MetaServer_Disconnect();
            MetaServer_Connect( "metaserver.introversion.co.uk", PORT_METASERVER_LISTEN, metaserverPort );


            //
            // Other

            g_preferences->SetInt( PREFS_NETWORKUSEPORTFORWARDING, parent->m_portForwarding );
            g_preferences->SetInt( PREFS_NETWORKTRACKSYNCRAND, parent->m_trackSync );            
            g_preferences->Save();

            EclRegisterWindow( new MessageDialog( "NetworkSuccess",
                                                  "dialog_networksuccessfull", true, 
                                                  "dialog_networksuccess", true ), 
                               parent );
        }
    }
};


class ShowNetworkingHelpButton : public TextButton
{
    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        if( highlighted || clicked )
        {
            g_renderer->RectFill( realX, realY, m_w, m_h, Colour(255,255,255,50) );
            g_renderer->Rect( realX, realY, m_w, m_h, Colour(255,255,255,200) );
        }

        TextButton::Render( realX, realY, highlighted, clicked );
    }

    void MouseUp()
    {
        int windowed = g_preferences->GetInt( "ScreenWindowed", 1 );
        if( !windowed )
        {
            // Switch to windowed mode if required
            g_preferences->SetInt( "ScreenWindowed", 1 );
            g_preferences->SetInt( "ScreenWidth", 1024 );
            g_preferences->SetInt( "ScreenHeight", 768 );

            g_app->ReinitialiseWindow();

            g_preferences->Save();        
        }

#if defined(TARGET_OS_MACOSX)
        g_windowManager->OpenWebsite( LANGUAGEPHRASE("website_support_macosx") );
#elif defined(RETAIL_BRANDING_UK)
        g_windowManager->OpenWebsite( LANGUAGEPHRASE("website_support_retail_uk") );
#elif defined(RETAIL_BRANDING)
        g_windowManager->OpenWebsite( LANGUAGEPHRASE("website_support_retail") );
#elif defined(RETAIL_BRANDING_MULTI_LANGUAGE)
        g_windowManager->OpenWebsite( LANGUAGEPHRASE("website_support_retail_multi_language") );
#else
        g_windowManager->OpenWebsite( LANGUAGEPHRASE("website_support") );
#endif
    }
};


NetworkOptionsWindow::NetworkOptionsWindow()
:   InterfaceWindow( "Network", "dialog_networkoptions", true )
{
    SetSize( 390, 330 );
    Centralise();

    sprintf( m_clientPort, "%d", g_preferences->GetInt( PREFS_NETWORKCLIENTPORT ) );
    sprintf( m_serverPort, "%d", g_preferences->GetInt( PREFS_NETWORKSERVERPORT ) );
    sprintf( m_metaserverPort, "%d", g_preferences->GetInt( PREFS_NETWORKMETASERVER ) );

    m_portForwarding = g_preferences->GetInt( PREFS_NETWORKUSEPORTFORWARDING );
    m_trackSync = g_preferences->GetInt( PREFS_NETWORKTRACKSYNCRAND );
}


void NetworkOptionsWindow::Create()
{
    InterfaceWindow::Create();

    int x = 200;
    int w = m_w - x - 20;
    int y = 30;
    int h = 30;

    InvertedBox *box = new InvertedBox();
    box->SetProperties( "invert", 10, 40, m_w - 20, m_h - 110, " ", " ", false, false );        
    RegisterButton( box );

    CreateValueControl( "Server Port", x, y+=h, w, 20, InputField::TypeString, &m_serverPort, 1, 0, 10, NULL, "dialog_networkporttooltip", true );
    CreateValueControl( "Client Port", x, y+=h, w, 20, InputField::TypeString, &m_clientPort, 1, 0, 10, NULL, "dialog_networkporttooltip", true );
    CreateValueControl( "MetaServer Port", x, y+=h, w, 20, InputField::TypeString, &m_metaserverPort, 1, 0, 10, NULL, "dialog_networkporttooltip", true );

    DropDownMenu *dropDown = new DropDownMenu();
    dropDown->SetProperties( "Port Forwarding", x, y+=h, w, 20, "dialog_networkuseportforwarding", "dialog_networkportforwardtooltip", true, true );
    dropDown->AddOption( "dialog_disabled", 0, true );
    dropDown->AddOption( "dialog_enabled", 1, true );
    dropDown->RegisterInt( &m_portForwarding );
    RegisterButton( dropDown );

#ifdef TRACK_SYNC_RAND
    dropDown = new DropDownMenu();
    dropDown->SetProperties( "Track Sync Errors", x, y+=h, w, 20, "dialog_networktracksync", "dialog_networktracksynctooltip", true, true );
    dropDown->AddOption( "dialog_disabled", 0, true );
    dropDown->AddOption( "dialog_enabled", 1, true );
    dropDown->RegisterInt( &m_trackSync );
    RegisterButton( dropDown );
#endif

    ShowNetworkingHelpButton *help = new ShowNetworkingHelpButton();
    help->SetProperties( "Troubleshooting help", 20, y+=h*2, m_w-40, 20, "dialog_troubleshooting", "dialog_troubleshooting_tooltip", true, true );
    RegisterButton( help );

    CloseButton *cancel = new CloseButton();
    cancel->SetProperties( "Close", 10, m_h - 30, m_w / 2 - 15, 20, "dialog_close", " ", true, false );
    RegisterButton( cancel );

    ApplyNetworkButton *apply = new ApplyNetworkButton();
    apply->SetProperties( "Apply", cancel->m_x+cancel->m_w+10, m_h - 30, m_w / 2 - 15, 20, "dialog_apply", " ", true, false );
    RegisterButton( apply );     
}


void NetworkOptionsWindow::Render( bool _hasFocus )
{
    InterfaceWindow::Render( _hasFocus );

    int x = m_x+20;
    int y = m_y+35;
    int h = 30;
    int size = 13;

    g_renderer->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_networkserverport") );
    g_renderer->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_networkclientport") );
    g_renderer->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_networkmetaserverport") );
    g_renderer->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_networkuseportforwarding") );

#ifdef TRACK_SYNC_RAND
    g_renderer->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_networktracksync") );
#endif

    if( g_app->GetClientToServer()->IsConnected() )
    {
        g_renderer->TextCentreSimple( m_x+m_w/2, m_y+m_h-60, White, 12, LANGUAGEPHRASE("dialog_no_change_while_in_game_1") );
        g_renderer->TextCentreSimple( m_x+m_w/2, m_y+m_h-45, White, 12, LANGUAGEPHRASE("dialog_no_change_while_in_game_2") );
    }
}
