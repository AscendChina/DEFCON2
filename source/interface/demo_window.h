#ifndef _included_demowindow_h
#define _included_demowindow_h

#include "interface/components/core.h"


// ============================================================================
// Shows the demo bitmap, and asks them to buy

class DemoWindow : public InterfaceWindow
{    
public:
    bool    m_exitGameButton;
    float   m_nagTimer;

public:
    DemoWindow();

    void Create();
    void Render( bool _hasFocus );
};



// ============================================================================
// Gives them a message saying "welcome to demo, here are the limits"

class WelcomeToDemoWindow : public InterfaceWindow
{    
public:
    WelcomeToDemoWindow();

    void Create();
    void Render( bool _hasFocus );
};





// ============================================================================
//  Generic BUY NOW button, can be used anywhere
 

class BuyNowButton : public InterfaceButton
{
public:
    bool m_closeOnClick;

public:
    BuyNowButton();

    void MouseUp();
};


#endif
