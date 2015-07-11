#include "lib/universal_include.h"
#include "lib/language_table.h"
#include "lib/render/renderer.h"
#include "lib/render/styletable.h"
#include "lib/gucci/window_manager.h"
#include "lib/sound/soundsystem.h"

#include "interface/tutorial_window.h"

#include "app/app.h"
#include "app/globals.h"
#include "lib/multiline_text.h"
#include "app/tutorial.h"

#include "renderer/map_renderer.h"

#include "world/world.h"
#include "world/worldobject.h"


class TutorialNextButton : public InterfaceButton
{
    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        if( g_app->GetTutorial()->IsNextClickable() )
        {
            InterfaceButton::Render( realX, realY, highlighted, clicked );
        }
    }

    void MouseUp()
    {
        if( g_app->GetTutorial()->IsNextClickable() )
        {
            g_app->GetTutorial()->ClickNext();
        }
    }
};

class TutorialRestartButton : public InterfaceButton
{
    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        if( g_app->GetTutorial()->IsRestartClickable() )
        {
            InterfaceButton::Render( realX, realY, highlighted, clicked );
        }
    }

    void MouseUp()
    {
        if( g_app->GetTutorial()->IsRestartClickable() )
        {
            g_app->GetTutorial()->ClickRestart();
        }
    }
};

class LevelWindowButton : public InterfaceButton
{
    void MouseUp()
    {
        if( !EclGetWindow("Level Select" ) )
        {
            EclRegisterWindow( new TutorialChapterWindow() );
        }
    }
};


TutorialWindow::TutorialWindow()
:   EclWindow("Tutorial", "tutorial", true),
    m_explainingSomething(false)
{
    SetSize( 450, 200 );
	if( g_windowManager->WindowW() > 1024 )
	{
		SetPosition( 5, g_windowManager->WindowH() - 205 );
	}
	else
	{
		SetPosition( 5, g_windowManager->WindowH() - 205 - 65 );
	}

    g_app->GetTutorial()->StartTutorial();
}



void TutorialWindow::Create()
{    
#ifdef _DEBUG
    LevelWindowButton *lwb = new LevelWindowButton();
    lwb->SetProperties( "Levels", m_w - 17, 4, 13, 13, "?", "", false, false );
    RegisterButton( lwb );
#endif

    TutorialRestartButton *restart = new TutorialRestartButton();
    restart->SetProperties( "Restart", 10, m_h - 25, 100, 20, "tutorial_restart", " ", true, false );
    RegisterButton( restart );

    TutorialNextButton *next = new TutorialNextButton();
    next->SetProperties( "Next", m_w - 110, m_h - 25, 100, 20, "tutorial_next", " ", true, false );
    RegisterButton( next );
}


void TutorialWindow::Render( bool _hasFocus )
{
    //
    // Render the window

    Colour windowColA  = g_styleTable->GetPrimaryColour( STYLE_WINDOW_BACKGROUND );
    Colour windowColB  = g_styleTable->GetSecondaryColour( STYLE_WINDOW_BACKGROUND );
    Colour borderCol   = g_styleTable->GetPrimaryColour( STYLE_WINDOW_OUTERBORDER );

    g_renderer->RectFill ( m_x, m_y, m_w, m_h, windowColA, windowColA, windowColB, windowColB );
    g_renderer->Rect     ( m_x, m_y, m_w-1, m_h-1, borderCol);

    EclWindow::Render( _hasFocus );

    
    //
    // Render the shadow

    InterfaceWindow::RenderWindowShadow( m_x+m_w, m_y, m_h, m_w, 15, g_renderer->m_alpha*0.5f );


    
    if( g_app->GetTutorial() )
    {
        //
        // Render the title

        char *title = g_app->GetTutorial()->GetCurrentLevelName();
        char fullTitle[512];
		sprintf( fullTitle, LANGUAGEPHRASE("tutorial_chapter_desc") );
		LPREPLACEINTEGERFLAG( 'C', g_app->GetTutorial()->GetCurrentLevel(), fullTitle );
		LPREPLACESTRINGFLAG( 'N', title, fullTitle );
        strupr( fullTitle );

        g_renderer->SetFont( "kremlin" );
        g_renderer->TextCentreSimple( m_x+m_w/2, m_y+5, White, 16.0f, fullTitle );
        g_renderer->SetFont();
        

        //
        // Render the message

        static float s_previousMaxChars = 0;

        char *msg = g_app->GetTutorial()->GetCurrentMessage();
        if( msg )
        {
            char *msgCopy = strdup(msg);
            int lineSize = m_w - 40;
            
            MultiLineText wrapped( msgCopy, lineSize, 12 );

            float timePassed = g_app->GetTutorial()->GetMessageTimer();
            float fractionVisible = 50.0f * timePassed / (float) strlen(msgCopy);
            int maxChars = 99999;
            if( fractionVisible < 0.0f ) fractionVisible = 0.0f;
            if( fractionVisible < 1.0f ) 
            {
                maxChars = int(fractionVisible * strlen(msgCopy));
            }

            int charsSoFar = 0;
            for( int i = 0; i < wrapped.Size(); ++i )
            {
                float xPos = m_x + 20;
                float yPos = m_y + 40 + i * 12;

				if (maxChars - charsSoFar < strlen( wrapped[i] ))
					wrapped[ i ][ maxChars - charsSoFar ] = '\0';
					
                g_renderer->TextSimple( xPos, yPos, White, 11, wrapped[i] );

                charsSoFar += strlen( wrapped[i] );
                if( charsSoFar >= maxChars ) break;
            }

            delete msgCopy;

            if( fabs(float(s_previousMaxChars - maxChars)) > 2 )
            {
                g_soundSystem->TriggerEvent( "Interface", "Text" );
                s_previousMaxChars = maxChars;
            }

            m_explainingSomething = ( fractionVisible < 1.0f );
        }


        //
        // Render the highlight

        char windowHighlight[256];
        char buttonHighlight[256];
        float x, y, w, h;
        bool highlightFound = false;


        //
        // Button/window highlight?

        int interfaceHighlight = g_app->GetTutorial()->GetButtonHighlight( windowHighlight, buttonHighlight );
        if( interfaceHighlight )
        {
            EclWindow *window = EclGetWindow( windowHighlight );
            if( window )
            {
                x = window->m_x;
                y = window->m_y;
                w = window->m_w;
                h = window->m_h;
                highlightFound = true;

                if( interfaceHighlight == 2 ) 
                {
                    EclButton *button = window->GetButton(buttonHighlight);
                    if( button )
                    {
                        x += button->m_x;
                        y += button->m_y;
                        w = button->m_w;
                        h = button->m_h;
                    }
                }
            }
        }

        
        //
        // World object highlight?

        float xPos = m_x + 10;
        float yPos = m_y + m_h - 20;
        
        WorldObject *wobj = g_app->GetWorld()->GetWorldObject( g_app->GetTutorial()->GetObjectHighlight() );
        if( wobj )
        {
            City *city = (City *) wobj;
            
            float left, right, top, bottom;
            g_app->GetMapRenderer()->GetWindowBounds( &left, &right, &top, &bottom );

            float fractionX = (wobj->m_longitude.DoubleValue() - left) / (right - left);
            float fractionY = (wobj->m_latitude.DoubleValue() - top) / (bottom - top);

            x = g_windowManager->WindowW() * fractionX;
            y = g_windowManager->WindowH() * fractionY;
            x -= 25;
            y -= 25;            
            w = 50;
            h = 50;
            highlightFound = true;
        }


        if( highlightFound )
        {
            float alpha = 55 + fabs(sinf(g_gameTime*2)) * 200;
            g_renderer->Rect( x-10, y-10, w+20, h+20, Colour(255,255,0,alpha), 5 );
                            
            g_renderer->BeginLines( Colour(255,255,0,alpha), 1 );
            g_renderer->Line( xPos-7, yPos+8 );
            g_renderer->Line( xPos-7, y+h/2 );
            g_renderer->Line( x-5, y+h/2 );
            g_renderer->EndLines();
        }
    }
}

class LevelChangeButton : public InterfaceButton
{
public:
    int m_level;
    void MouseUp()
    {
        g_app->GetTutorial()->SetCurrentLevel( m_level );
        g_app->GetTutorial()->ClickRestart();
    }
};


TutorialChapterWindow::TutorialChapterWindow()
:   InterfaceWindow("Level Select", "levelselect", true)
{
    SetSize( 250, 90 );
}

void TutorialChapterWindow::Create()
{
    InterfaceWindow::Create();
    int x = 10;
    int y = 30;
    for( int i = 1; i <= 7; ++i )
    {
        char name[128];
        sprintf( name, "Level %i", i );
		char caption[128];
		sprintf( caption, LANGUAGEPHRASE("tutorial_level") );
		LPREPLACEINTEGERFLAG( 'L', i, caption );

		LevelChangeButton *lcb = new LevelChangeButton();
        lcb->SetProperties( name, x, y, 50, 20, caption, "", false, false );
        lcb->m_level = i;
        RegisterButton( lcb );
        x+= 60;
        if( i == 4 )
        {
            x = 10;
            y += 30;
        }
    }
}



