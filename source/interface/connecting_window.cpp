#include "lib/universal_include.h"
#include "lib/gucci/window_manager.h"
#include "lib/render/renderer.h"
#include "lib/math/math_utils.h"
#include "lib/hi_res_time.h"
#include "lib/language_table.h"
#include "lib/profiler.h"

#include "network/ClientToServer.h"

#include "interface/lobby_window.h"
#include "interface/chat_window.h"

#include "app/app.h"
#include "app/globals.h"

#include "connecting_window.h"

#include <algorithm>

class AbortButton : public InterfaceButton
{
    void MouseUp()
    {
        EclRemoveWindow( "LOBBY" );        
        EclRemoveWindow( m_parent->m_name );
        
        g_app->ShutdownCurrentGame();
    }
};


ConnectingWindow::ConnectingWindow()
:   InterfaceWindow("Connection Status", "dialog_connection_status", true),
    m_maxUpdatesRemaining(0),
    m_maxLagRemaining(0),
    m_popupLobbyAtEnd(false),
    m_stage(0),
    m_stageStartTime(-1.0f)
{
    SetSize( 300, 280 );
    SetPosition( g_windowManager->WindowW()/2-m_w/2, 
                 g_windowManager->WindowH()/2-m_h/2 );

    g_app->GetClientToServer()->m_synchronising = true;
}


ConnectingWindow::~ConnectingWindow()
{
    if( !EclGetWindow( "Connection Status") )
    {
        g_app->GetClientToServer()->m_synchronising = false;
    }
}


void ConnectingWindow::Create()
{
    InterfaceWindow::Create();
    
    InvertedBox *box = new InvertedBox();
    box->SetProperties( "invert", 10, 30, m_w-20, m_h-70, " ", " ", false, false );
    RegisterButton( box );

    AbortButton *abort = new AbortButton();
    abort->SetProperties( "Abort", m_w/2-50, m_h-30, 100, 20, "dialog_abort", "dialog_abort_current_connection", true, true );
    RegisterButton( abort );
}


void ConnectingWindow::Render( bool _hasFocus )
{
    InterfaceWindow::Render( _hasFocus );


    float yPos = m_y + 40;


    //
    // Render our connection state

    g_renderer->SetFont( "kremlin" );


    float fraction = 0.0f;
    char *caption = NULL;
    Colour col;

    switch( g_app->GetClientToServer()->m_connectionState )
    {
        case ClientToServer::StateDisconnected:     
            col.Set(255,0,0,255);
            caption = LANGUAGEPHRASE("dialog_state_disconnected");
            fraction = 0.0f;
            break;

        case ClientToServer::StateConnecting:
            col.Set(255,255,0,255);
            caption = LANGUAGEPHRASE("dialog_state_connecting");
            fraction = 0.5f;
            break;

        case ClientToServer::StateHandshaking:
        case ClientToServer::StateConnected:
            col.Set(0,255,0,255);
            caption = LANGUAGEPHRASE("dialog_state_connected");
            fraction = 1.0f;
            break;
    }

    g_renderer->TextCentreSimple( m_x+m_w/2, yPos, col, 20, caption );

    yPos += 20;

    if( fraction > 0.0f )
    {
        g_renderer->RectFill( m_x + 30, yPos, (m_w-60)*fraction, 20, col );
        g_renderer->Rect( m_x+30, yPos, (m_w-60), 20, White );

        int numConnectionAttempts = g_app->GetClientToServer()->m_connectionAttempts;
        if( fraction < 1.0f && numConnectionAttempts > 0 )
        {
			char caption[512];
			sprintf( caption, LANGUAGEPHRASE("dialog_state_attempts") );
			LPREPLACEINTEGERFLAG( 'A', numConnectionAttempts, caption );
            g_renderer->TextCentreSimple( m_x + m_w/2, m_y + m_h - 60, White, 14, caption );
        }
    }


    //
    // Render our sync status (ie receiving all data from the server)

    yPos += 40;

    if( g_app->GetClientToServer()->IsConnected() )
    {
        if( m_stage == 0 )
        {
            m_stage = 1;
            m_stageStartTime = GetHighResTime();
        }

        int serverSeqId = g_app->GetClientToServer()->m_serverSequenceId;
        int latestSeqId = g_app->GetClientToServer()->m_lastValidSequenceIdFromServer;
        int numRemaining = serverSeqId-latestSeqId;
        numRemaining--;

        if( numRemaining > m_maxUpdatesRemaining )
        {
            m_maxUpdatesRemaining = numRemaining;
        }

        if( m_maxUpdatesRemaining > 0 )
        {
            fraction = numRemaining / (float) m_maxUpdatesRemaining;
        }
        else
        {
            fraction = 0.0f;
        }

        Clamp( fraction, 0.0f, 1.0f );
        fraction = 1.0f - fraction;
        
        col.Set( (1-fraction)*255, fraction*255, 0, 255 );

        const char *caption = numRemaining > 5 ? LANGUAGEPHRASE("dialog_state_receiving") : LANGUAGEPHRASE("dialog_state_received");
        g_renderer->TextCentreSimple( m_x+m_w/2, yPos, col, 20, caption );

        yPos += 20;
    
        g_renderer->RectFill( m_x+30, yPos, (m_w-60)*fraction, 20, col );
        g_renderer->Rect( m_x+30, yPos, (m_w-60), 20, White );

        if( m_stage == 1 )
        {
            RenderTimeRemaining(fraction);
        }

        //
        // Render how much of the received data we have successfully parsed

        int lagRemaining = 0;
        if( g_lastProcessedSequenceId > 0 && numRemaining < 10 )
        {
            if( m_stage != 2 )
            {
                m_stage = 2;
                m_stageStartTime = GetHighResTime();
            }

            yPos += 40;
        
            int serverSeqId = g_app->GetClientToServer()->m_serverSequenceId;
            lagRemaining = serverSeqId - g_lastProcessedSequenceId;
            lagRemaining --;
        
            if( lagRemaining > m_maxLagRemaining )
            {
                m_maxLagRemaining = lagRemaining;
            }

            fraction = lagRemaining / (float) m_maxLagRemaining;
            Clamp( fraction, 0.0f, 1.0f );
            fraction = 1.0f - fraction;
        
            col.Set( (1-fraction)*255, fraction*255, 0, 255 );

            const char *caption = lagRemaining > 5 ? LANGUAGEPHRASE("dialog_state_synchronising") : LANGUAGEPHRASE("dialog_state_synchronised");
            g_renderer->TextCentreSimple( m_x+m_w/2, yPos, col, 20, caption );

            yPos += 20;
    
            g_renderer->RectFill( m_x+30, yPos, (m_w-60)*fraction, 20, col );
            g_renderer->Rect( m_x+30, yPos, (m_w-60), 20, White );

            if( m_stage == 2 )
            {
                RenderTimeRemaining( fraction );
            }
        }

    
        //
        // Connection is done, we can shut down now
        // Pop up lobby if we were asked to do so

        if( g_app->GetClientToServer()->m_connectionState == ClientToServer::StateConnected )
        {
            if( m_popupLobbyAtEnd )
            {
                if( !EclGetWindow( "LOBBY" ) )              
                {
                    LobbyWindow *lobby = new LobbyWindow();                   
                    ChatWindow *chat = new ChatWindow();

                    chat->SetPosition( g_windowManager->WindowW()/2 - chat->m_w/2, 
                                       g_windowManager->WindowH() - chat->m_h - 30 );
                    EclRegisterWindow( chat );

                    float lobbyX = g_windowManager->WindowW()/2 - lobby->m_w/2;
                    float lobbyY = chat->m_y - lobby->m_h - 30;
                    lobbyY = std::max( lobbyY, 0.0f );
                    lobby->SetPosition(lobbyX, lobbyY);
                    EclRegisterWindow( lobby );
                }
            }

            if( numRemaining < 5 && lagRemaining < 5 )
            {
                EclRemoveWindow(m_name);
            }
        }

    }
    else
    {
        if( g_app->GetClientToServer()->m_connectionState == ClientToServer::StateDisconnected )
        {
            EclRemoveWindow(m_name);
        }
    }

    g_renderer->SetFont();
}

void ConnectingWindow::RenderTimeRemaining( float _fractionDone )
{
    static float s_timeRemaining = 0;
    static float s_timer = 0;

    float timeNow = GetHighResTime();

    if( timeNow > s_timer + 0.5f )
    {
        s_timer = timeNow;
        float timeSoFar = timeNow - m_stageStartTime;
        float timeForOnePercent = timeSoFar/_fractionDone;
        float percentRemaining = 1.0f - _fractionDone;
        s_timeRemaining = percentRemaining * timeForOnePercent;
    }

    if( s_timeRemaining > 0 && s_timeRemaining < 100000 )
    {
        int minutes = int(s_timeRemaining / 60.0f);
        int seconds = s_timeRemaining - minutes * 60;

		char caption[512];
		sprintf( caption, LANGUAGEPHRASE("dialog_state_time_remaining") );
		LPREPLACEINTEGERFLAG( 'M', minutes, caption );
		char number[32];
		sprintf( number, "%02d", seconds );
		LPREPLACESTRINGFLAG( 'S', number, caption );
        g_renderer->TextCentreSimple( m_x + m_w/2, m_y + m_h - 60, White, 14, caption );
    }
}
