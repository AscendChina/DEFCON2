#include "lib/universal_include.h"
#include "lib/render/renderer.h"
#include "lib/language_table.h"

#include "lib/multiline_text.h"

#include "interface/demo_window.h"
#include "interface/badkey_window.h"
#include "interface/authkey_window.h"



BadKeyWindow::BadKeyWindow()
:   InterfaceWindow( "AUTHENTICATION FAILED", "dialog_bad_key_title", true ),
    m_offerDemo(true)
{
    m_extraMessage[0] = '\x0';

    SetSize( 450, 250 );
    Centralise();
}


void BadKeyWindow::Create()
{
    InterfaceWindow::Create();

    InvertedBox *box = new InvertedBox();
    box->SetProperties( "invert", 10, 30, m_w-20, m_h-70, " ", " ", false, false );
    RegisterButton( box );

#if !defined(RETAIL_DEMO)
    if( m_offerDemo )
    {
        PlayDemoButton *demo = new PlayDemoButton();
        demo->SetProperties( "Play Demo", m_w - 330, m_h - 30, 100, 20, "dialog_play_demo", " ", true, false );
        demo->m_forceVisible = true;
        RegisterButton( demo );
    }
#endif

#if !defined(RETAIL)
   // BuyNowButton *buyNow = new BuyNowButton();
  //  buyNow->SetProperties( "Buy Now", m_w - 220, m_h - 30, 100, 20, "dialog_buy_now", " ", true, false );    
   // buyNow->m_closeOnClick = false;
   // RegisterButton( buyNow );
#endif

    CloseButton *close = new CloseButton();
    close->SetProperties( "Close", m_w - 110, m_h - 30, 100, 20, "dialog_close", " ", true, false );
    RegisterButton( close );
}


void BadKeyWindow::Render( bool _hasFocus )
{
    InterfaceWindow::Render( _hasFocus );


    //
    // Extra message

    float yPos = m_y + 20;
    float size = 14;
    float gap = 17;
    
    if( m_extraMessage[0] != '\x0' )
    {
        MultiLineText wrapped( m_extraMessage, m_w-20, size );

        for( int i = 0; i < wrapped.Size(); ++i )
        {
            char *thisLine = wrapped[i];
            g_renderer->TextCentreSimple( m_x + m_w/2, yPos+=gap, White, size, thisLine );
        }

        yPos += gap*2;
    }


    //
    // DEMO message

    if( m_offerDemo )
    {
        g_renderer->TextCentreSimple( m_x + m_w/2, m_y + m_h - 60, White, size, LANGUAGEPHRASE("dialog_may_play_as_demo_user") );
    }
}




