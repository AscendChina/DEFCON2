#include "lib/universal_include.h"
#include "lib/hi_res_time.h"
#include "lib/gucci/window_manager.h"
#include "lib/render/renderer.h"
#include "lib/preferences.h"
#include "lib/resource/resource.h"
#include "lib/resource/image.h"
#include "lib/language_table.h"
//#include "lib/filesys/file_system.h"
//#include "lib/filesys/filesys_utils.h"
//#include "lib/filesys/binary_stream_readers.h"

#include "interface/demo_window.h"

#include "lib/multiline_text.h"
#include "app/app.h"
#include "app/globals.h"

#include "network/ClientToServer.h"


class ExitGameButton : public InterfaceButton
{
   /* void Render( int realX, int realY, bool highlighted, bool clicked )
    {       
      //  DemoWindow *parent = (DemoWindow *)m_parent;
      //  float timeSinceOpen = GetHighResTime() - parent->m_nagTimer;

        char caption[256];
        if( timeSinceOpen < 0.1f )
        {
            sprintf( caption, LANGUAGEPHRASE("dialog_exit_game_time") );
			LPREPLACEINTEGERFLAG( 'T', 5-(int)timeSinceOpen, caption );

            g_renderer->m_alpha = 0.5f;
            highlighted = false;
            clicked = false;
        }
        else
        {
            sprintf( caption, LANGUAGEPHRASE("dialog_exit_game") );
        }               
        SetCaption( caption, false );

        InterfaceButton::Render( realX, realY, highlighted, clicked );

        g_renderer->m_alpha = 1.0f;
    }*/

    void MouseUp()
    {
        //DemoWindow *parent = (DemoWindow *)m_parent;
       // float timeSinceOpen = GetHighResTime() - parent->m_nagTimer;

       // if( timeSinceOpen >= 5.0f )
        //{
            g_app->Shutdown();
       // }
    }
	//g_app->Shutdown();
};


// ============================================================================


BuyNowButton::BuyNowButton()
:   InterfaceButton(),
    m_closeOnClick(false)
{
};



void BuyNowButton::MouseUp()
{
    char *buyUrl = NULL;
	
#if defined(RETAIL_BRANDING_UK)
    buyUrl = LANGUAGEPHRASE("website_buy_retail_uk");
#elif defined(RETAIL_BRANDING)
    buyUrl = LANGUAGEPHRASE("website_buy_retail");
#elif defined(RETAIL_BRANDING_MULTI_LANGUAGE)
    buyUrl = LANGUAGEPHRASE("website_buy_retail_multi_language");
#else
    //buyUrl = LANGUAGEPHRASE("website_buy");
    buyUrl = new char[512];
    g_app->GetClientToServer()->GetPurchaseUrl( buyUrl );
#endif

    g_windowManager->OpenWebsite( buyUrl );

    if( m_closeOnClick )
    {
        g_app->Shutdown();
    }
    else
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
    }
}


// ============================================================================



DemoWindow::DemoWindow()
:   InterfaceWindow( "Demo", "dialog_demo", true ),
    m_exitGameButton(false),
    m_nagTimer(0.0f)
{
    if( EclGetWindow( "Demo") )
    {
        EclRemoveWindow("Demo");
    }

    m_nagTimer = GetHighResTime();

    SetSize( 620, 470 );
    Centralise();
}


void DemoWindow::Create()
{
    InterfaceWindow::Create();

#if !defined(RETAIL)
    BuyNowButton *buyNow = new BuyNowButton();
    buyNow->SetProperties( "Buy Now", m_w - 280, m_h - 30, 130, 20, "dialog_buy_now", " ", true, false );    
    buyNow->m_closeOnClick = m_exitGameButton;
    RegisterButton( buyNow );
#endif

    if( m_exitGameButton )
    {
        ExitGameButton *exit = new ExitGameButton();
        exit->SetProperties( "Exit Game", m_w - 140, m_h - 30, 130, 20, " ", "tooltip_demo_exit_the_game", false, true );
        RegisterButton( exit );
    }
    else
    {
        CloseButton *close = new CloseButton();
        close->SetProperties( "Close", m_w - 140, m_h - 30, 130, 20, "dialog_close", " ", true, false );
        RegisterButton( close );
    }
}


void DemoWindow::Render( bool _hasFocus )
{
    InterfaceWindow::Render( _hasFocus );

    char *filename = "gui/demo.bmp";

	//char filenameLang[512];
	//if( g_languageTable->m_lang && stricmp( g_languageTable->m_lang->m_name, "english" ) != 0 )
	//{
	//	snprintf( filenameLang, sizeof(filenameLang), "%s_%s.%s", RemoveExtension( filename ), g_languageTable->m_lang->m_name, GetExtensionPart( filename ) );
	//	filenameLang[ sizeof(filenameLang) - 1 ] = '\0';
	//
	//	char filenameLangTest[512];
	//	snprintf( filenameLangTest, sizeof(filenameLangTest), "data/%s", filenameLang );
	//	filenameLangTest[ sizeof(filenameLangTest) - 1 ] = '\0';
	//	BinaryReader *reader = g_fileSystem->GetBinaryReader( filenameLangTest );
	//	if( reader )
	//	{
	//		delete reader;
	//		filename = filenameLang;
	//	}
	//}

    Image *img = g_resource->GetImage( filename );

    g_renderer->Blit( img, m_x + 10, m_y + 30, White );
    g_renderer->Rect( m_x + 10, m_y + 30, img->Width(), img->Height(), Colour(0,0,0,255) );

	//g_renderer->SetFont( "lucon" );

	int oldBlendMode = g_renderer->m_blendMode;
	g_renderer->SetBlendMode( Renderer::BlendModeSubtractive );

    g_renderer->TextCentreSimple( m_x + m_w / 2, m_y + 168, Colour( 220, 220, 220, 0 ), 18, LANGUAGEPHRASE("demo_top1") );

    g_renderer->TextCentreSimple( m_x + m_w / 2, m_y + 200, Colour( 220, 220, 220, 0 ), 15, LANGUAGEPHRASE("demo_middle1") );
    g_renderer->TextCentreSimple( m_x + m_w / 2, m_y + 220, Colour( 220, 220, 220, 0 ), 15, LANGUAGEPHRASE("demo_middle2") );
    g_renderer->TextCentreSimple( m_x + m_w / 2, m_y + 240, Colour( 220, 220, 220, 0 ), 15, LANGUAGEPHRASE("demo_middle3") );
    g_renderer->TextCentreSimple( m_x + m_w / 2, m_y + 260, Colour( 220, 220, 220, 0 ), 15, LANGUAGEPHRASE("demo_middle4") );
    g_renderer->TextCentreSimple( m_x + m_w / 2, m_y + 280, Colour( 220, 220, 220, 0 ), 15, LANGUAGEPHRASE("demo_middle5") );
    g_renderer->TextCentreSimple( m_x + m_w / 2, m_y + 300, Colour( 220, 220, 220, 0 ), 15, LANGUAGEPHRASE("demo_middle6") );


    char metaserverPrice[512];
    g_app->GetClientToServer()->GetPurchasePrice( metaserverPrice );    


#if defined(RETAIL) && defined(RETAIL_BRANDING_UK)
    g_renderer->TextCentreSimple( m_x + m_w / 2, m_y + 340, Colour( 220, 220, 220, 0 ), 17, LANGUAGEPHRASE("demo_bottom1_retail_uk") );
    g_renderer->TextCentreSimple( m_x + m_w / 2, m_y + 380, Colour( 220, 220, 220, 0 ), 17, LANGUAGEPHRASE("demo_bottom2_retail_uk") );
#elif defined(RETAIL)
    g_renderer->TextCentreSimple( m_x + m_w / 2, m_y + 340, Colour( 220, 220, 220, 0 ), 17, LANGUAGEPHRASE("demo_bottom1_retail") );
    g_renderer->TextCentreSimple( m_x + m_w / 2, m_y + 380, Colour( 220, 220, 220, 0 ), 17, LANGUAGEPHRASE("demo_bottom2_retail") );
#elif defined(TARGET_OS_MACOSX)
    g_renderer->TextCentreSimple( m_x + m_w / 2, m_y + 340, Colour( 220, 220, 220, 0 ), 30, LANGUAGEPHRASE("demo_bottom1_macosx") );
    g_renderer->TextCentreSimple( m_x + m_w / 2, m_y + 380, Colour( 220, 220, 220, 0 ), 30, LANGUAGEPHRASE("demo_bottom2_macosx") );
#else
    g_renderer->TextCentreSimple( m_x + m_w / 2, m_y + 340, Colour( 220, 220, 220, 0 ), 30, LANGUAGEPHRASE("demo_bottom1") );
    g_renderer->TextCentreSimple( m_x + m_w / 2, m_y + 380, Colour( 220, 220, 220, 0 ), 30, metaserverPrice );    
#endif



	g_renderer->SetBlendMode( oldBlendMode );

	//g_renderer->SetFont();

}


// ============================================================================


WelcomeToDemoWindow::WelcomeToDemoWindow()
:   InterfaceWindow( "WELCOME TO THE DEMO", "dialog_welcome_to_demo", true )
{
    SetSize( 420, 280 );
    Centralise();
}


void WelcomeToDemoWindow::Create()
{
    InterfaceWindow::Create();

    InvertedBox *box = new InvertedBox();
    box->SetProperties( "invert", 10, 30, m_w-20, m_h-70, " ", " ", false, false );
    RegisterButton( box );

#if !defined(RETAIL)
    BuyNowButton *buyNow = new BuyNowButton();
    buyNow->SetProperties( "Buy Now", m_w - 220, m_h - 30, 100, 20, "dialog_buy_now", " ", true, false );    
    buyNow->m_closeOnClick = false;
    RegisterButton( buyNow );
#endif

    CloseButton *close = new CloseButton();
    close->SetProperties( "Close", m_w - 110, m_h - 30, 100, 20, "dialog_close", " ", true, false );
    RegisterButton( close );
}


void WelcomeToDemoWindow::Render( bool _hasFocus )
{
    InterfaceWindow::Render( _hasFocus );

    g_renderer->TextCentreSimple( m_x + m_w/2, m_y + 40, White, 16, LANGUAGEPHRASE("dialog_welcome_to_demo_defcon") );

    //
    // Extra message

    int maxGameSize;
    int maxDemoPlayers;
    bool allowDemoServers;
    g_app->GetClientToServer()->GetDemoLimitations( maxGameSize, maxDemoPlayers, allowDemoServers );


    char extraMessage[10240];
    sprintf( extraMessage, "\n" );

    if( allowDemoServers )
    {
        char demoServers[1024];

		sprintf( demoServers, LANGUAGEPHRASE("dialog_demo_player_limited") );
		LPREPLACEINTEGERFLAG( 'P', maxGameSize, demoServers );

        strcat( extraMessage, demoServers );
    }

    char demoJoin[1024];
	sprintf( demoJoin, LANGUAGEPHRASE("dialog_demo_player_join_limited") );
	LPREPLACEINTEGERFLAG( 'P', maxDemoPlayers, demoJoin );
    
    strcat( extraMessage, demoJoin );

    strcat( extraMessage, LANGUAGEPHRASE("dialog_demo_spectate_any_game") );

    float yPos = m_y + 40;
    float size = 13;
    float gap = 17;

    MultiLineText wrapped( extraMessage, m_w-20, size );

    for( int i = 0; i < wrapped.Size(); ++i )
    {
        char *thisLine = wrapped[i];
        g_renderer->TextCentreSimple( m_x + m_w/2, yPos+=gap, White, size, thisLine );
    }

    yPos += gap*2;

}
