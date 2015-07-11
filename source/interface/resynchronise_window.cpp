#include "lib/universal_include.h"

#include <float.h>

#include "lib/language_table.h"
#include "lib/sound/soundsystem.h"
#include "lib/gucci/window_manager.h"

#include "app/app.h"
#include "app/globals.h"
#include "lib/multiline_text.h"

#include "world/world.h"

#include "connecting_window.h"
#include "resynchronise_window.h"


class ResynchronisedButton : public InterfaceButton
{
    void MouseUp()
    {
        g_startTime = FLT_MAX;
        g_gameTime = 0.0f;
        g_advanceTime = 0.0f;
        g_lastServerAdvance = 0.0f;
        g_predictionTime = 0.0f;
        g_lastProcessedSequenceId = -1; 
        g_app->m_gameStartTimer = -1.0f;
        g_app->m_gameRunning = false;

        delete g_app->m_world;
        g_app->m_world = NULL;
        g_app->InitWorld();

        g_app->GetClientToServer()->Resynchronise();

        g_soundSystem->StopAllSounds( SoundObjectId(), "StartMusic StartMusic" );

        ConnectingWindow *connectWindow = new ConnectingWindow();
        connectWindow->m_popupLobbyAtEnd = false;
        EclRegisterWindow( connectWindow );
    }
};



ResynchroniseWindow::ResynchroniseWindow()
:   InterfaceWindow("Resynchronise", "dialog_resync_title", true ),
    m_debugVersion(false)
{
    SetSize( 300, 200 );
    SetPosition( g_windowManager->WindowW()/2 - m_w/2,
                 g_windowManager->WindowH()/2 - m_h/2 );
    
}


void ResynchroniseWindow::Create()
{
    if( m_debugVersion )
    {
        InterfaceWindow::Create();
    }

    InvertedBox *box = new InvertedBox();
    box->SetProperties( "invert", 10, 30, m_w-20, m_h - 70, " ", " ", false, false );
    RegisterButton(box);

    ResynchronisedButton *resync = new ResynchronisedButton();
    resync->SetProperties( "Resynchronise", m_w/2-50, m_h-30, 100, 20, "dialog_resync_button", " ", true, false );
    RegisterButton( resync );
}


void ResynchroniseWindow::Update()
{
    InterfaceWindow::Update();

    if( !m_debugVersion &&
        g_app->GetClientToServer()->IsSynchronised( g_app->GetClientToServer()->m_clientId ) )
    {
        EclRemoveWindow( m_name );
    }
}


void ResynchroniseWindow::Render( bool _hasFocus )
{
    InterfaceWindow::Render( _hasFocus );

    EclButton *button = GetButton("invert");
    
    char *caption = LANGUAGEPHRASE( "dialog_resync_caption" );

    g_renderer->SetFont( "zerothre" );

    MultiLineText wrapped( caption, button->m_w-20, 13 );

    float yPos = m_y + button->m_y+10;
    float h = 15;

    for( int i = 0; i < wrapped.Size(); ++i )
    {
        char *thisLine = wrapped[i];
        g_renderer->TextSimple( m_x + button->m_x + 10, yPos, White, 13, thisLine );

        yPos += h;
    }

	 g_renderer->SetFont();
}



