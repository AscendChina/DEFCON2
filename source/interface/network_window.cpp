#include "lib/universal_include.h"
 
#include <stdio.h>
#include <algorithm>

#include "lib/render/renderer.h"
#include "lib/hi_res_time.h"
#include "lib/metaserver/authentication.h"
#include "lib/language_table.h"

#include "app/app.h"
#include "app/globals.h"
#include "app/game.h"

#include "world/world.h"

#include "network/ClientToServer.h"
#include "network/Server.h"
#include "network/ServerToClient.h"

#include "interface/components/core.h"
#include "interface/network_window.h"


NetworkWindow::NetworkWindow( char *name, char *title, bool titleIsLanguagePhrase )
:   InterfaceWindow( name, title, titleIsLanguagePhrase )
{
    SetSize( 600, 200 );
}


void NetworkWindow::Create()
{
    InterfaceWindow::Create();

    if( g_app->GetServer() )
    {
        int maxClients = g_app->GetGame()->GetOptionValue("MaxTeams") +
                         g_app->GetGame()->GetOptionValue("MaxSpectators");

        int boxH = maxClients * 20 + 15;
        SetSize( m_w, 170 + boxH );
    
        InvertedBox *box = new InvertedBox();
        box->SetProperties( "invert", 10, 110, m_w-20, boxH, " ", " ", false, false );
        RegisterButton( box );
    }
}


void NetworkWindow::Render( bool hasFocus )
{
    InterfaceWindow::Render( hasFocus );
    
    int y = m_y+15;
    int h = 15;

    if( g_app->GetServer() )
    {
		char caption[128];
		char number[32];
		sprintf( caption, LANGUAGEPHRASE("dialog_network_server_seqid") );
		LPREPLACEINTEGERFLAG( 'S', g_app->GetServer()->m_sequenceId, caption );
        g_renderer->TextSimple( m_x + 10, y+=h, White, 12, caption );

		sprintf( caption, LANGUAGEPHRASE("dialog_network_server_send") );
		sprintf( number, "%2.1f", g_app->GetServer()->m_sendRate/1024.0f );
		LPREPLACESTRINGFLAG( 'R', number, caption );
		g_renderer->TextSimple( m_x + 10, y+=h, White, 12, caption );

		sprintf( caption, LANGUAGEPHRASE("dialog_network_server_receive") );
		sprintf( number, "%2.1f", g_app->GetServer()->m_receiveRate/1024.0f );
		LPREPLACESTRINGFLAG( 'R', number, caption );
		g_renderer->TextSimple( m_x + 10, y+=h, White, 12, caption );

        g_renderer->Line( m_x + 10, y + 20, m_x + m_w - 10, y + 20, White, 1 );
        
        int clientX = m_x + 20;
        int ipX = clientX + 60;
        int seqX = ipX + 140;
        int playerX = seqX + 60;
        int lagX = playerX + 150;
        int syncX = lagX + 90;

        y+=h*2;

        g_renderer->TextSimple( clientX, y, White, 14, LANGUAGEPHRASE("dialog_network_id") );
        g_renderer->TextSimple( ipX, y, White, 14, LANGUAGEPHRASE("dialog_network_ip_port") );
        g_renderer->TextSimple( seqX, y, White, 14, LANGUAGEPHRASE("dialog_network_seqid") );
        g_renderer->TextSimple( playerX, y, White, 14, LANGUAGEPHRASE("dialog_network_name") );
        g_renderer->TextSimple( lagX, y, White, 14, LANGUAGEPHRASE("dialog_network_status") );
        
        y+=h*2;

        int maxClients = g_app->GetGame()->GetOptionValue("MaxTeams") +
                         g_app->GetGame()->GetOptionValue("MaxSpectators");
        
        for( int i = 0; i < maxClients; ++i )
        {
            if( g_app->GetServer()->m_clients.ValidIndex(i) )
            {
                ServerToClient *sToc = g_app->GetServer()->m_clients[i];
                
                char netLocation[256];
                sprintf( netLocation, "%s:%d", sToc->m_ip, sToc->m_port);

                char caption[256];
                Colour col;

                float timeBehind = GetHighResTime() - sToc->m_lastMessageReceived;
                if( timeBehind > 2.0f )
                {
                    col.Set( 255, 0, 0, 255 );
					sprintf( caption, LANGUAGEPHRASE("dialog_network_lost_con") );
					LPREPLACEINTEGERFLAG( 'S', (int)timeBehind, caption );
                }
                else if( !sToc->m_caughtUp ) 
                {
                    col.Set( 200, 200, 30, 255 );
                    int percent = 100 * (sToc->m_lastKnownSequenceId / (float)g_app->GetServer()->m_sequenceId);
					sprintf( caption, LANGUAGEPHRASE("dialog_network_synching") );
					LPREPLACEINTEGERFLAG( 'P', percent, caption );
                }
                else
                {
                    float latency = (g_app->GetServer()->m_sequenceId - sToc->m_lastKnownSequenceId);
                    float latencyMs = latency * 100.0f;
					sprintf( caption, LANGUAGEPHRASE("dialog_network_ping") );
					LPREPLACEINTEGERFLAG( 'P', (int)latencyMs, caption );
                    int red = std::min(255,(int) latency*20);
                    int green = 255 - red;
                    col.Set(red,green,0,255);                
                }

                Colour normalCol(200,200,255,200);

                char *playerName = NULL;
                if( sToc->m_spectator )
                {
                    for( int j = 0; j < g_app->GetWorld()->m_spectators.Size(); ++j )
                    {
                        Spectator *thisTeam = g_app->GetWorld()->m_spectators[j];
                        if( thisTeam->m_clientId == sToc->m_clientId )
                        {
                            playerName = thisTeam->m_name;
                            break;
                        }
                    }
                }
                else
                {
                    for( int j = 0; j < g_app->GetWorld()->m_teams.Size(); ++j )
                    {
                        Team *thisTeam = g_app->GetWorld()->m_teams[j];
                        if( thisTeam->m_clientId == sToc->m_clientId )
                        {
                            playerName = thisTeam->m_name;
                            break;
                        }
                    }
                }

                g_renderer->Text( clientX, y, normalCol, 12, "%d", sToc->m_clientId );
                if( playerName )
                {
                    g_renderer->TextSimple( playerX, y, normalCol, 12, playerName );
                }
                g_renderer->TextSimple( ipX, y, normalCol, 12, netLocation );
                g_renderer->Text( seqX, y, normalCol, 12, "%d", sToc->m_lastKnownSequenceId );                
                g_renderer->TextSimple( lagX, y, col, 12, caption );

                if( sToc->m_spectator )
                {
                    g_renderer->TextSimple( clientX+10, y, normalCol, 12, LANGUAGEPHRASE("dialog_network_spec") );
                }

                if( sToc->m_syncErrorSeqId != -1 )
                {
                    g_renderer->Text( syncX, y, Colour(255,0,0,255), 12, LANGUAGEPHRASE("dialog_worldstatus_out_of_sync_2") );
                }
            }
            else
            {
                g_renderer->Text( clientX, y, Colour(255,255,255,50), 12, LANGUAGEPHRASE("dialog_network_empty") );
            }
            
            y += 20;
        }
    }


    //
    // Show which packets are queued up if we're a client

    if( g_app->GetClientToServer() && g_lastProcessedSequenceId >= 0 )
    {
        float yPos = m_y + m_h - 25;

        g_renderer->TextSimple( m_x + 10, yPos-25, White, 13, LANGUAGEPHRASE("dialog_network_sequence_msg_queue") );

        float xPos = m_x + 10;
        float width = (m_w - 120) / 10;
        float gap = width * 0.2f;
        
        for( int i = 0; i <= 10; ++i )
        {
            if( i != 0 )
            {
                g_renderer->RectFill( xPos, yPos-5, width-gap, 20, Colour(20,20,50,255) );

                if( g_app->GetClientToServer()->IsSequenceIdInQueue( g_lastProcessedSequenceId+i ) )
                {
                    g_renderer->RectFill( xPos, yPos-5, width-gap, 20, Colour(0,255,0,255) );
                }

                g_renderer->Rect( xPos, yPos-5, width-gap, 20, Colour(255,255,255,100) );
            }

            if( i == 0 )
            {
				char caption[128];
				sprintf( caption, LANGUAGEPHRASE("dialog_network_seqid_number") );
				LPREPLACEINTEGERFLAG( 'S', g_lastProcessedSequenceId, caption );
                g_renderer->TextSimple( xPos, yPos, White, 13, caption );
                xPos += width * 1;
            }
            
            xPos += width;
        }
    }


    if( g_app->GetClientToServer() )
    {
		char caption[128];
		char number[32];

		if( !g_app->GetServer() )
        {
			sprintf( caption, LANGUAGEPHRASE("dialog_network_svr_known_seqid") );
			LPREPLACEINTEGERFLAG( 'S', g_app->GetClientToServer()->m_serverSequenceId, caption );
            g_renderer->TextSimple( m_x + 10, y+=h, White, 12, caption );

			sprintf( caption, LANGUAGEPHRASE("dialog_network_svr_estimated_seqid") );
			LPREPLACEINTEGERFLAG( 'S', g_app->GetClientToServer()->GetEstimatedServerSeqId(), caption );
            g_renderer->TextSimple( m_x + 10, y+=h, White, 12, caption );

			sprintf( caption, LANGUAGEPHRASE("dialog_network_estimated_latency") );
			sprintf( number, "%2.1f", g_app->GetClientToServer()->GetEstimatedLatency() );
			LPREPLACESTRINGFLAG( 'L', number, caption );
            g_renderer->TextCentreSimple( m_x + m_w/2, y+40, White, 15, caption );
        }

        y = m_y + 15;

		sprintf( caption, LANGUAGEPHRASE("dialog_network_client_seqid") );
		LPREPLACEINTEGERFLAG( 'S', g_app->GetClientToServer()->m_lastValidSequenceIdFromServer, caption );
        g_renderer->TextSimple( m_x + 250, y+=h, White, 12, caption );

		sprintf( caption, LANGUAGEPHRASE("dialog_network_client_send") );
		sprintf( number, "%2.1f", g_app->GetClientToServer()->m_sendRate/1024.0f );
		LPREPLACESTRINGFLAG( 'R', number, caption );
        g_renderer->TextSimple( m_x + 250, y+=h, White, 12, caption );

		sprintf( caption, LANGUAGEPHRASE("dialog_network_client_receive") );
		sprintf( number, "%2.1f", g_app->GetClientToServer()->m_receiveRate/1024.0f );
		LPREPLACESTRINGFLAG( 'R', number, caption );
        g_renderer->TextSimple( m_x + 250, y+=h, White, 12, caption );
    }
}



