 
/*
 * =========
 * INTERFACE
 * =========
 *
 * A set of window and button definitions
 * that allow player interaction
 *
 */

#ifndef _included_interface_h
#define _included_interface_h

#include "lib/tosser/llist.h"

class Image;
class WorldMessage;
class EclWindow;
class EclButton;

#define STYLE_TOOLTIP_BACKGROUND       "TooltipBackground"
#define STYLE_TOOLTIP_BORDER           "TooltipBorder"
#define FONTSTYLE_TOOLTIP              "FontTooltip"



class Interface
{
protected:
    LList   <WorldMessage *> m_messages;                
    char    *m_message;
    float   m_messageTimer;

    int     m_mouseMessage;
    float   m_mouseMessageTimer;

	char    *m_mouseCursor;

    float   m_escTimer;                 // tracks escape key hits (office mode panic-button)

protected:
    static void TooltipRender( EclWindow *_window, EclButton *_button, float _timer );

public:
    Interface();

    void Init();
    void Update();
    void Render();
    void RenderMouse();
    void Shutdown();

    void OpenSetupWindows();
    void OpenGameWindows();

    bool UsingChatWindow();

    void ShowMessage( Fixed longitude, Fixed latitude, int teamId, char *msg, bool showLarge=false );    

	void SetMouseCursor( const char *filename = NULL );
};


#endif
