#include "lib/universal_include.h"
#include "lib/gucci/input.h"
#include "lib/math/math_utils.h"
#include "lib/render/renderer.h"
#include "lib/render/styletable.h"
#include "lib/preferences.h"

#include "interface/fading_window.h"

#include "app/globals.h"



FadingWindow::FadingWindow( char *_name )
:   InterfaceWindow( _name ),
    m_alpha(1.0f)
{
}


void FadingWindow::Render( bool _hasFocus )
{
    Render( _hasFocus, true );
}


void FadingWindow::Render( bool _hasFocus, bool _renderButtons )
{
    //
    // Update our Alpha value

    EclWindow *mouseWindow = EclGetWindow( g_inputManager->m_mouseX,
                                           g_inputManager->m_mouseY );

    bool alphaIncreasing = false;

    if( mouseWindow == this )
    {
        alphaIncreasing = true;
        m_alpha += g_advanceTime * 0.2f;       

        if( g_inputManager->m_lmb )
        {
            m_alpha = 1.0f;
        }
    }
    else
    {
        m_alpha -= g_advanceTime;
    }

    Clamp( m_alpha, 0.0f, 1.0f );


    //
    // Render the window

    g_renderer->SetClip( m_x, m_y, m_w, m_h );

    //
    // Draw the window bars

    Colour windowColA  = g_styleTable->GetPrimaryColour( STYLE_WINDOW_BACKGROUND );
    Colour windowColB  = g_styleTable->GetSecondaryColour( STYLE_WINDOW_BACKGROUND );
    Colour borderCol   = g_styleTable->GetPrimaryColour( STYLE_WINDOW_OUTERBORDER );

    windowColA.m_a *= m_alpha;
    windowColB.m_a *= m_alpha;
    borderCol.m_a *= m_alpha;

    bool alignment = g_styleTable->GetStyle(STYLE_WINDOW_BACKGROUND)->m_horizontal;

    g_renderer->RectFill ( m_x, m_y, m_w, m_h, windowColA, windowColB, alignment );
    g_renderer->Rect     ( m_x, m_y, m_w-1, m_h-1, borderCol);
  
    //
    // Draw the buttons if we are focused

    float oldAlpha = g_renderer->m_alpha;
    g_renderer->m_alpha = m_alpha;

    EclWindow::Render( _hasFocus );

    g_renderer->m_alpha = oldAlpha;
    
    g_renderer->ResetClip();


    //
    // Drop shadow

    InterfaceWindow::RenderWindowShadow( m_x+m_w, m_y, m_h, m_w, 10, g_renderer->m_alpha*0.4f*m_alpha );
}


void FadingWindow::SaveProperties( char *_name )
{
    char s_result[256];
    sprintf( s_result, "%d.%d.%d.%d", m_x, m_y, m_w, m_h );

    g_preferences->SetString( _name, s_result );
}


void FadingWindow::LoadProperties( char *_name )
{
    char *theString = g_preferences->GetString( _name );
    if( theString )
    {
        int x, y, w, h;
        sscanf( theString, "%d.%d.%d.%d", &x, &y, &w, &h );

        //m_w = w;
        //m_h = h;
        
        m_w = max( w, m_minW );
        m_h = max( h, m_minH );
        
        SetPosition( x, y );
    }
}

