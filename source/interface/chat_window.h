#ifndef _included_chatwindow_h
#define _included_chatwindow_h

#include "lib/tosser/bounded_array.h"

#include "interface/components/inputfield.h"
#include "interface/fading_window.h"

class ChatMessage;
class ScrollBar;



class ChatWindow : public FadingWindow
{
public:
    int     m_channel;
    char    m_message[1024];
    bool    m_hasFocus;
    bool    m_typingMessage;
    bool    m_minimised;

    LList   <ChatMessage *> m_sendQueue;

    ScrollBar *m_scrollbar;

protected:    
    void    RenderWindow();
    void    RenderTeams();
    void    RenderMessages();

    char    *GetChannelName( int _channelId, bool _includePublic );

public:
    ChatWindow();

    void Create();
    void Render( bool _hasFocus );
    void Update();

    void BeginTypingMessage();
    void EndTypingMessage();
};



class ChatInputField : public InputField
{
public:
    void MouseUp    ();
    void Render     ( int realX, int realY, bool highlighted, bool clicked );
	void Keypress   ( int keyCode, bool shift, bool ctrl, bool alt, unsigned char ascii );
};


#endif


