#include "lib/universal_include.h"
#include "lib/render/colour.h"
#include "lib/render/renderer.h"
#include "lib/language_table.h"

#include "app/app.h"
#include "app/globals.h"

#include "world/world.h"
#include "world/city.h"

#include "interface/casualties_window.h"
#include "interface/components/scrollbar.h"



CasualtiesWindow::CasualtiesWindow( char *name )
:   InterfaceWindow(name)
{
    m_scrollbar = new ScrollBar( this );
}

void CasualtiesWindow::Create()
{
    InterfaceWindow::Create();

    int winSize = (m_h-55);
    int numRows = m_scrollbar->m_numRows;

    m_scrollbar->Create( "Scrollbar", m_w - 20, 50, 15, m_h - 55, numRows, winSize );
}

void CasualtiesWindow::Remove()
{
    InterfaceWindow::Remove();
    
    m_scrollbar->Remove();
}


void CasualtiesWindow::Render( bool hasFocus )
{
    InterfaceWindow::Render( hasFocus );
    
    g_renderer->SetClip( m_x, m_y+20, m_w, m_h-20 );

    int x = m_x + 10;
    int y = m_y + 30;
    int titleSize = 18;
    int textSize = 13;

    y-= m_scrollbar->m_currentValue;
    
    g_renderer->TextSimple( x, y, White, titleSize, LANGUAGEPHRASE("dialog_casualties_city") );
    g_renderer->TextSimple( x+200, y, White, titleSize, LANGUAGEPHRASE("dialog_casualties_strikes") );
    g_renderer->TextSimple( x+300, y, White, titleSize, LANGUAGEPHRASE("dialog_casualties_deaths") );

    y+=20;

    int numEntries = 0;

    for( int i = 0; i < g_app->GetWorld()->m_cities.Size(); ++i )
    {
        if( g_app->GetWorld()->m_cities.ValidIndex(i) )
        {
            City *city = g_app->GetWorld()->m_cities[i];
            if( city->m_dead > 0 )
            {
                char strikes[64];
                char deaths[64];

                sprintf( strikes, "%d", city->m_numStrikes );
				sprintf( deaths, LANGUAGEPHRASE("dialog_casualties_in_million") );
				char number[32];
				sprintf( number, "%.1f", city->m_dead / 1000000.0f );
				LPREPLACESTRINGFLAG( 'C', number, deaths );

                g_renderer->TextSimple( x, y+=18, White, textSize, LANGUAGEPHRASEADDITIONAL(city->m_name) );
                g_renderer->TextSimple( x+200, y, White, textSize, strikes );
                g_renderer->TextSimple( x+300, y, White, textSize, deaths );

                ++numEntries;
            }
        }
    }

    g_renderer->ResetClip();

    m_scrollbar->SetNumRows( (numEntries+1) * 18 );
}



