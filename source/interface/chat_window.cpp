#include "lib/universal_include.h"
#include "lib/gucci/window_manager.h"
#include "lib/gucci/input.h"
#include "lib/render/renderer.h"
#include "lib/render/styletable.h"
#include "lib/math/math_utils.h"
#include "lib/language_table.h"
#include "lib/string_utils.h"

#include "interface/components/inputfield.h"
#include "interface/components/scrollbar.h"

#include "chat_window.h"

#include "app/app.h"
#include "app/globals.h"
#include "lib/multiline_text.h"

#include "world/world.h"


class SetChannelButton : public InterfaceButton
{
public:
    int m_channel;

    void MouseUp()
    {
        ChatWindow *parent = (ChatWindow *) m_parent;
        parent->m_channel = m_channel;
    }

    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        ChatWindow *parent = (ChatWindow *) m_parent;
        if( parent->m_channel == m_channel ) clicked = true;        
        InterfaceButton::Render( realX, realY, highlighted, clicked );        
    }
};


// ============================================================================


ChatWindow::ChatWindow()
:   FadingWindow("Comms Window"),
    m_channel(CHATCHANNEL_PUBLIC),
    m_hasFocus(false),
    m_typingMessage(false),
    m_minimised(false)
{
    SetSize( 500, 200 );   
    SetPosition( 0, g_windowManager->WindowH()-m_h );
    
    m_minH = 10;

    m_message[0] = '\x0';

    m_scrollbar = new ScrollBar( this );

    if( g_app->GetWorld()->AmISpectating() )
    {
        m_channel = CHATCHANNEL_SPECTATORS;
    }
}



void ChatWindow::Create()
{
    InvertedBox *invert = new InvertedBox();
    invert->SetProperties( "Invert", 5, 20, m_w-115, m_h - 50, " ", " ", false, false );
    RegisterButton(invert);

    invert = new InvertedBox();
    invert->SetProperties( "Members", m_w - 107, 20, 82, m_h-50, " ", " ", false, false );
    RegisterButton( invert );
    
    ChatInputField *input = new ChatInputField();
    input->SetProperties( "Msg", 5, m_h - 25, m_w-10, 20, " ", " ", false, false );
    input->RegisterString( m_message );
    input->m_highBound = 128;
    input->m_align = -1;
    input->SetInputNoExtendedCharacters( g_renderer->IsFontLanguageSpecific() );
    RegisterButton( input );

    float buttonX = 5;

    SetChannelButton *channel = NULL;

    channel = new SetChannelButton();
    channel->SetProperties( "Channel 100", buttonX, 3, 80, 15, "dialog_chat_public", "tooltip_chat_public", true, true );
    channel->m_channel = CHATCHANNEL_PUBLIC;
    RegisterButton( channel );

    buttonX += 85;

    channel = new SetChannelButton();
    channel->SetProperties( "Channel 101", buttonX, 3, 80, 15, "dialog_chat_alliance", "tooltip_chat_alliance", true, true );
    channel->m_channel = CHATCHANNEL_ALLIANCE;
    RegisterButton( channel );

    buttonX += 85;


    int windowSize = invert->m_h;

    if( windowSize > 40 )
    {
        m_scrollbar->Create( "Scrollbar", m_w-22, 20, 18, m_h - 50, 0, windowSize );    
    }
}


void ChatWindow::Update()
{
    if( !g_app->GetClientToServer()->IsConnected() )
    {
        return;
    }

    EclButton *msg = GetButton( "Msg" );
    if( msg )
    {
        ( (ChatInputField *) msg )->SetInputNoExtendedCharacters( g_renderer->IsFontLanguageSpecific() );
    }

    //
    // Ensure the right chat channel buttons are here

    if( g_app->GetWorld()->AmISpectating() )
    {
        if( GetButton( "Channel 100" ) ) RemoveButton( "Channel 100" );
        if( GetButton( "Channel 101" ) ) RemoveButton( "Channel 101" );
        
        if( !GetButton( "Channel 102" ) )
        {
            SetChannelButton *channel = new SetChannelButton();
            channel->SetProperties( "Channel 102", 5, 3, 80, 15, "dialog_chat_spectators", "tooltip_chat_spectators", true, true );
            channel->m_channel = CHATCHANNEL_SPECTATORS;
            RegisterButton( channel );
        }

        m_channel = CHATCHANNEL_SPECTATORS;
    }
    else
    {
        if( GetButton( "Channel 102" ) ) RemoveButton( "Channel 102" );
        
        if( !GetButton( "Channel 100" ) )
        {
            SetChannelButton *channel = new SetChannelButton();
            channel->SetProperties( "Channel 100", 5, 3, 80, 15, "dialog_chat_public", "tooltip_chat_public", true, true );
            channel->m_channel = CHATCHANNEL_PUBLIC;
            RegisterButton( channel );
            m_channel = CHATCHANNEL_PUBLIC;
        }

        if( !GetButton( "Channel 101" ) )
        {
            SetChannelButton *channel = new SetChannelButton();
            channel->SetProperties( "Channel 101", 90, 3, 80, 15, "dialog_chat_alliance", "tooltip_chat_alliance", true, true );
            channel->m_channel = CHATCHANNEL_ALLIANCE;
            RegisterButton( channel );
            m_channel = CHATCHANNEL_PUBLIC;
        }
    }


    //
    // Make sure the input box is selected or not, 
    // based on our mode
    
    if( m_typingMessage )
    {
        if( g_inputManager->m_lmb &&
            !EclMouseInButton( this, GetButton("Msg") ) )
        {
            EndTypingMessage();
        }
        else
        {
            EclSetCurrentFocus( m_name );
            BeginTextEdit( "Msg" );
        }
    }
    else
    {
        EndTextEdit();
    }


    //
    // If we are typing lots of wwww aaaa ssss dddd most likely we are trying to move the camera

    if( g_app->m_gameRunning && strlen(m_message) > 5 )
    {
        bool realText = false;

        for( int i = 0; i < strlen(m_message); ++i )
        {
            if( m_message[i] != 'w' &&
                m_message[i] != 'a' &&
                m_message[i] != 's' &&
                m_message[i] != 'd' &&
				m_message[i] != 'q' &&
				m_message[i] != 'e')
            {
                realText = true;
                break;
            }
        }

        if( !realText )
        {
            EndTypingMessage();
            m_message[0] = '\x0';
        }
    }


    //
    // If the mouse isnt in the window, shrink vertically to get out the way

    if( g_app->m_gameRunning )
    {
        if( EclGetWindow() == this )
        {
            if( m_h < 200 )
            {
                m_h = 200;
                m_y = g_windowManager->WindowH() - m_h;
                m_alpha = std::max(m_alpha, 0.5f );
                Remove();
                Create();
                m_minimised = false;
            }
        }
        else
        {
            if( m_alpha < 0.1f &&
                m_h > 40 )
            {
                m_h = 40;
                m_y = g_windowManager->WindowH() - m_h;                
                m_minimised = true;
                Remove();
                Create();
            }
        }
    }
    else
    {
        m_minimised = false;
    }

    FadingWindow::Update();
}


void ChatWindow::BeginTypingMessage()
{
    m_typingMessage = true;
}


void ChatWindow::EndTypingMessage()
{
    m_typingMessage = false;
}


void ChatWindow::RenderWindow()
{
    Style *boxBg = g_styleTable->GetStyle(STYLE_BOX_BACKGROUND);
    Style *boxBorder = g_styleTable->GetStyle(STYLE_BOX_BORDER);

    int boxBgP = boxBg->m_primaryColour.m_a;
    int boxBorderP = boxBorder->m_primaryColour.m_a;
    int boxBorderS = boxBorder->m_secondaryColour.m_a;

    if( g_app->m_gameRunning )
    {
        boxBg->m_primaryColour.m_a *= m_alpha;
        boxBorder->m_primaryColour.m_a *= m_alpha;
        boxBorder->m_secondaryColour.m_a *= m_alpha;
    }

    FadingWindow::Render( m_hasFocus, false );
    
    EclButton *msg = GetButton("Msg");
    msg->Render( m_x+msg->m_x, m_y+msg->m_y, false, false );
    
    if( g_app->m_gameRunning )
    {
        boxBg->m_primaryColour.m_a = boxBgP;
        boxBorder->m_primaryColour.m_a = boxBorderP;
        boxBorder->m_secondaryColour.m_a *= boxBorderS;        
    }
}


void ChatWindow::RenderTeams()
{
    bool mouseInWindow = EclMouseInWindow(this) || EclGetWindow() == this;

    if( mouseInWindow || !g_app->m_gameRunning || m_hasFocus )
    {
        int x = m_x + m_w - 100;
        int y = m_y + 25;
        int myTeam = g_app->GetWorld()->m_myTeamId;
        bool showAI = false;

#ifdef NON_PLAYABLE
        showAI = true;
#endif

        bool spectating = g_app->GetWorld()->AmISpectating();

        //
        // Players

        for( int i = 0; i < g_app->GetWorld()->m_teams.Size(); ++i )
        {
            Team *team = g_app->GetWorld()->m_teams[i];
            if( (team->m_type != Team::TypeAI && team->m_type != Team::TypeUnassigned) ||
                (showAI && team->m_type != Team::TypeUnassigned) )
            {
                char name[64];
                strncpy(name, team->GetTeamName(), sizeof(name));
                name[sizeof(name)] = '\0';
                
                int width = 70;
                int mouseX = g_inputManager->m_mouseX;
                int mouseY = g_inputManager->m_mouseY;
                if( team->m_teamId != myTeam &&
                    !spectating &&
                    mouseX > x && 
                    mouseX < x + width &&
                    mouseY > y - 2 && 
                    mouseY < y + 15 )
                {
                    g_renderer->RectFill( x, y - 2, width + 6, 15, Colour(50,50,150,200));
                    if( g_inputManager->m_lmbUnClicked )
                    {
                        m_channel = team->m_teamId;
                    }
                }

                if( m_channel == team->m_teamId )
                {
                    g_renderer->RectFill( x, y - 2, width + 6, 15, Colour(150,150,150,100));
                }

                Colour col = team->GetTeamColour();
                col.m_a *= m_alpha;

                if( !g_app->GetWorld()->IsChatMessageVisible( team->m_teamId, m_channel, false ) )
                {                    
                    col.Set(255,255,255,50);
                }

				if ( strlen(name) > 8 )
				{
					strcpy ( &name[8], "..." );
				}
				
                g_renderer->TextSimple( x, y, col, 14.0f, name );

                if( !g_app->GetWorld()->IsChatMessageVisible( team->m_teamId, m_channel, false ) )
                {
                    g_renderer->Line( x, y+6, x+width, y+6, Colour(255,255,255,100) );
                }

                y+= 16;
            }
        }


        //
        // Render spectator names
       
        if( g_app->GetWorld()->m_spectators.Size() > 0 )
        {
            y += 15;
            g_renderer->Text( x, y, Colour(255,255,255,255*m_alpha), 12, LANGUAGEPHRASE("dialog_chat_spectators") );
            y += 13;

            for( int i = 0; i < g_app->GetWorld()->m_spectators.Size(); ++i )
            {    
                Colour col(255,255,255,200);
                if( m_channel < CHATCHANNEL_PUBLIC )
                {
                    // Private channel, specs cant see
                    col.m_a = 50;
                }
                col.m_a *= m_alpha;

                Spectator *spec = g_app->GetWorld()->m_spectators[i];
                g_renderer->TextSimple( x, y, col, 10, spec->m_name );

                if( m_channel < CHATCHANNEL_PUBLIC )
                {
                    g_renderer->Line( x, y+5, x+70, y+5, Colour(255,255,255,100) );
                }

                y += 11;
            }
        }
    }
}


void ChatWindow::RenderMessages()
{    
    int x = m_x + 10;
    int y = m_y + m_h - 45;
    int h = 15;
 
    bool mouseInWindow = EclMouseInWindow(this) || EclGetWindow() == this;
    
    float offset = m_scrollbar->m_numRows - m_scrollbar->m_winSize;
    if( m_scrollbar->m_winSize >= m_scrollbar->m_numRows )
    {
        offset = 0;
    }

    y += (offset - m_scrollbar->m_currentValue);

    if( m_minimised ) 
    {
        offset = 0;
        y = m_y + m_h - 45;
    }

    float startingY = y;
    
    EclButton *clip = GetButton("Invert");

    if( !m_minimised )
    {
        g_renderer->SetClip( m_x+clip->m_x, m_y+clip->m_y, clip->m_w, clip->m_h );
    }
    else
    {
        g_renderer->SetClip( m_x+clip->m_x, m_y+m_h-200, clip->m_w, m_y+m_h );
    }
    
    int extraLines = 0;

    for( int i = 0; i <= g_app->GetWorld()->m_chat.Size(); i++ )
    {
        ChatMessage *chatMsg = g_app->GetWorld()->m_chat[i];

        if( chatMsg && chatMsg->m_visible )
        {
            Team *team = chatMsg->m_spectator ? 
                                            NULL : 
                                            g_app->GetWorld()->GetTeam(chatMsg->m_fromTeamId);
                       
            char newMsg[1024];
            bool action = false;

            if( chatMsg->m_messageId == -1 )
            {
                action = true;
                snprintf( newMsg, sizeof(newMsg), "* %s %s", chatMsg->m_playerName, chatMsg->m_message );
            }
            else
            {
                snprintf( newMsg, sizeof(newMsg), chatMsg->m_message );
            }
            newMsg[ sizeof(newMsg) - 1 ] = '\0';
            
            if( strncmp(newMsg, "/me ", 4 ) == 0 )
            {
                action = true;
                snprintf( newMsg, sizeof(newMsg), "* %s %s", chatMsg->m_playerName, chatMsg->m_message+4 );
                newMsg[ sizeof(newMsg) - 1 ] = '\0';
            }

            Colour teamCol = White;
            if( team ) teamCol = team->GetTeamColour();
            teamCol.m_a = 255;
            if( chatMsg->m_spectator || !team )
            {
                teamCol.Set(255,255,255,125);
            }

            if( chatMsg->m_messageId == -1 )
            {
                teamCol.Set(255,255,255,255);
            }

            char *channelName = GetChannelName(chatMsg->m_channel, false);
            float channelNameTextWidth = g_renderer->TextWidth( channelName, 12 );

            float teamW = 0;    
            teamW += channelNameTextWidth + 5;
            
            float playerNameTextWidth = 0;

            int width = GetButton("Invert")->m_w;
            if( !action )
            {
                playerNameTextWidth = g_renderer->TextWidth( chatMsg->m_playerName, 12 );
                width -= teamW;
                width -= playerNameTextWidth;
            }
            width -= 10;
            MultiLineText wrapped( newMsg, width, 12, true );

            bool done = false;
            extraLines += wrapped.Size() - 1;
            for( int w = wrapped.Size()-1; w>=0; --w )
            {
                done = true;
                int xPos = x;
                if( !action )
                {
                    if( w == 0 ) xPos += playerNameTextWidth + 10;           
                    xPos += teamW;
                }
                
                if( y >= 0 && y <= g_windowManager->WindowH() )
                {
                    g_renderer->TextSimple( xPos, y, teamCol, 12, wrapped[w] );
                }
                if( w>0) y -= h;
            }

            float textX = x;

            if( done )
            {                
                if( channelName[0] != '\x0' )
                {
                    g_renderer->TextSimple( x, y, White, 12, channelName );
                    textX += channelNameTextWidth;
                    textX += 5;
                }

                if( !action )
                {
                    g_renderer->Text( textX, y, teamCol, 12, "%s:", chatMsg->m_playerName );
                    g_renderer->Text( textX, y, teamCol, 12, "%s:", chatMsg->m_playerName );                
                }
            
                y -=h;
            }
        }
    }

    g_renderer->ResetClip();
    
    if( g_keys[KEY_ENTER] && g_keyDeltas[KEY_ENTER])
    {
        BeginTypingMessage();
    }

    bool viewingLatest = ( m_scrollbar->m_currentValue == m_scrollbar->m_numRows - m_scrollbar->m_winSize ) ||
                           m_scrollbar->m_winSize >= m_scrollbar->m_numRows;

    m_scrollbar->SetNumRows(startingY - y);

    if( viewingLatest )
    {
        m_scrollbar->SetCurrentValue( m_scrollbar->m_numRows );
    }
}


char *ChatWindow::GetChannelName( int _channelId, bool _includePublic )
{
    static char channelName[256];
    channelName[0] = '\x0';

    if( _channelId == CHATCHANNEL_PUBLIC && _includePublic )
    {
        sprintf( channelName, LANGUAGEPHRASE("dialog_chat_channel_public") );
    }
    else if( _channelId == CHATCHANNEL_ALLIANCE )
    {
        sprintf( channelName, LANGUAGEPHRASE("dialog_chat_channel_alliance") );
    }
    else if( _channelId == CHATCHANNEL_SPECTATORS )
    {
        sprintf( channelName, LANGUAGEPHRASE("dialog_chat_channel_spectators") );
    }
    else if( _channelId == g_app->GetWorld()->m_myTeamId )
    {
        sprintf( channelName, LANGUAGEPHRASE("dialog_chat_channel_private") );
    }
    else
    {
        Team *team = g_app->GetWorld()->GetTeam(_channelId);
        if( team )
		{
			sprintf( channelName, LANGUAGEPHRASE("dialog_chat_channel_to_team_name") );
			LPREPLACESTRINGFLAG( 'T', team->m_name, channelName );
		}
    }                

    return channelName;
}


void ChatWindow::Render( bool _hasFocus )
{
    if( !g_app->m_gameRunning ) m_alpha = 1.0f;
    
    if( !g_app->GetServer() &&
        !g_app->GetClientToServer()->IsConnected() )
    {
        EclRemoveWindow( m_name );
        return;
    }

    m_hasFocus = _hasFocus;
    
    RenderWindow();
    RenderTeams();
    RenderMessages();

    if( strlen(m_message) < 1 )
    {
        EclButton *input = GetButton("Msg");
        g_renderer->Text( m_x + input->m_x + 5, 
                          m_y + input->m_y + 3 ,
                          Colour(255,255,255,50),
                          14, GetChannelName(m_channel,true) );
    }
}


// ============================================================================


void ChatInputField::MouseUp()
{
    InputField::MouseUp();
    
    ChatWindow *parent = (ChatWindow *) m_parent;
    parent->BeginTypingMessage();    
}


void ChatInputField::Render( int realX, int realY, bool highlighted, bool clicked )
{
    InputField::Render( realX, realY, highlighted, clicked );

    if( EclMouseInButton(m_parent,this) )
    {
        Colour col(255,255,200,100);
        g_renderer->Rect( realX, realY, m_w, m_h, col, 1.0f );
    }

    ChatWindow *parent = (ChatWindow *) m_parent;
    if( parent->m_typingMessage )
    {
        int alpha = 100 + 155 * fabs(sinf(g_gameTime*3));
        Colour col(255,255,200,alpha);
        g_renderer->Rect( realX, realY, m_w, m_h, col, 1.0f );
    }
}


void ChatInputField::Keypress( int keyCode, bool shift, bool ctrl, bool alt, unsigned char ascii )
{
   if( keyCode == KEY_ENTER )
    {
        //m_parent->EndTextEdit();

        ChatWindow *parent = (ChatWindow *) m_parent;
        char *msg = parent->m_message;

        if( strlen( msg ) > 0 )
        {
            int myTeamId = g_app->GetWorld()->m_myTeamId;
            int channel = parent->m_channel;
            int clientId = g_app->GetClientToServer()->m_clientId;
            int specIndex = g_app->GetWorld()->IsSpectating( clientId );
            if( specIndex != -1 ) myTeamId = clientId;
            
            //
            // Parse special commands

            if( strncmp(msg, "/name ", 6) == 0 )
            {
                char *newName = msg+6;    
                Spectator *spec = g_app->GetWorld()->GetSpectator(g_app->GetClientToServer()->m_clientId);
                if( spec )
                {
                    g_app->GetClientToServer()->RequestSetTeamName( g_app->GetClientToServer()->m_clientId, newName, true );
                }
                else
                {
                    Team *myTeam = g_app->GetWorld()->GetMyTeam();
                    if( myTeam )
                    {
                        g_app->GetClientToServer()->RequestSetTeamName( myTeam->m_teamId, newName, false );
                    }
                }
            }
            else
            {
                g_app->GetClientToServer()->SendChatMessageReliably( myTeamId, channel, msg, (specIndex != -1) );
            }

            msg[0] = '\x0';
            Refresh();
        }
        else
        {
            EclSetCurrentFocus( "None" );                        
        }
    }
    else
    {
        InputField::Keypress( keyCode, shift, ctrl, alt, ascii );
    }
}
