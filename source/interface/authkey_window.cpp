#include "lib/universal_include.h"
#include "lib/metaserver/authentication.h"
#include "lib/render/renderer.h"
#include "lib/render/styletable.h"
#include "lib/hi_res_time.h"
#include "lib/language_table.h"
#include "lib/gucci/input.h"

#include "interface/components/inputfield.h"

#include "authkey_window.h"
#include "demo_window.h"

#include "app/app.h"

#if !defined(RETAIL_DEMO)

#ifdef TARGET_OS_MACOSX
	// There's an unavoidable namespace collision between a couple symbols
	// in MacTypes.h and SystemIV. The MacTypes versions aren't used by us
	// so we rename them using the preprocessor.
	#define Style Style_Unused
	#define StyleTable StyleTable_Unused
	#include <CoreFoundation/CoreFoundation.h>
	#include <ApplicationServices/ApplicationServices.h>
	#undef Style
	#undef StyleTable
#endif

class ApplyKeyButton : public InterfaceButton
{
public:
    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        AuthKeyWindow *parent = (AuthKeyWindow *)m_parent;

        char realKey[256];
        Authentication_GetKey( realKey );
        if( strcmp( realKey, parent->m_key ) != 0 )
        {
            InterfaceButton::Render( realX, realY, highlighted, clicked );
        }
    }

    void MouseUp()
    {
        AuthKeyWindow *parent = (AuthKeyWindow *)m_parent;

        strupr( parent->m_key );

        Authentication_SetKey( parent->m_key );
        Authentication_SaveKey( parent->m_key, App::GetAuthKeyPath() );
    }
};


class PasteKeyButton : public InterfaceButton
{
public:
    void MouseUp()
    {
        AuthKeyWindow *parent = (AuthKeyWindow *)m_parent;
        parent->PasteFromClipboard();
    }
};


PlayDemoButton::PlayDemoButton()
:   InterfaceButton(),
    m_forceVisible(false)
{

}

bool PlayDemoButton::IsVisible()
{
    if( m_forceVisible )
    {
        return true;
    }

    char authKey[256];
    Authentication_GetKey( authKey );
    int authStatus = Authentication_GetStatus(authKey);
    return( authStatus != AuthenticationAccepted &&
            authStatus != AuthenticationUnknown );
}

void PlayDemoButton::Render( int realX, int realY, bool highlighted, bool clicked )
{
    if( IsVisible() )
    {
        InterfaceButton::Render(realX,realY,highlighted,clicked);
    }
}

void PlayDemoButton::MouseUp()
{
    if( IsVisible() )
    {
        char authKey[256];
        Authentication_GenerateKey( authKey, true );
        Authentication_SetKey( authKey );
        Authentication_SaveKey( authKey, App::GetAuthKeyPath() );
    
        EclRemoveWindow( m_parent->m_name );


        //EclRegisterWindow( new WelcomeToDemoWindow() );
    }
}


class AuthInputField : public InputField
{
public:
    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        //
        // Background

        Colour background = g_styleTable->GetPrimaryColour(STYLE_INPUT_BACKGROUND);
        Colour borderA = g_styleTable->GetPrimaryColour(STYLE_INPUT_BORDER);
        Colour borderB = g_styleTable->GetSecondaryColour(STYLE_INPUT_BORDER);

        g_renderer->RectFill    ( realX, realY, m_w, m_h, background );
        g_renderer->Line        ( realX, realY, realX + m_w, realY, borderA );
        g_renderer->Line        ( realX, realY, realX, realY + m_h, borderA );
        g_renderer->Line        ( realX + m_w, realY, realX + m_w, realY + m_h, borderB );
        g_renderer->Line        ( realX, realY + m_h, realX + m_w, realY + m_h, borderB );

        //
        // Caption

        g_renderer->SetFont( "lucon" );

        strncpy(m_buf, m_string, sizeof(m_buf) - 1);
        strupr( m_buf );

        float fontSize = 20;
        //float textWidth = g_renderer->TextWidth(m_buf, fontSize);
        //float fieldX = realX + m_w/2 - textWidth/2;
        //float yPos = realY + m_h/2 - fontSize/2;

        //g_renderer->TextSimple( fieldX, yPos, White, fontSize, m_buf );

        float xPos = realX + 5;
        float yPos = realY + 5;
        bool authKeyFound = (stricmp( m_buf, "authkey not found" ) != 0);

        for( int i = 0; i < AUTHENTICATION_KEYLEN-1; ++i )
        {
            if( authKeyFound )
            {
                char thisChar = ' ';
                if( i < strlen(m_buf) )
                {
                    thisChar = m_buf[i];
                }

                g_renderer->Text( xPos, yPos, White, fontSize, "%c", thisChar );
            }

            Colour lineCol(255,255,255,20);
            if( i % 7 >= 5 ) lineCol.m_a = 100;
            g_renderer->Line( xPos+16, realY+1, xPos+16, realY+m_h-1, lineCol );

            xPos += fontSize * 1.0f;
        }
        
        g_renderer->SetFont();


        //
        // Cursor

        if (m_parent->m_currentTextEdit && 
            strcmp(m_parent->m_currentTextEdit, m_name) == 0 )
        {
            if (fmodf(GetHighResTime(), 1.0f) < 0.5f ||
                GetHighResTime() < m_lastKeyPressTimer+1.0f )
            {
                float cursorX = realX + 5 + strlen(m_buf) * fontSize;
                g_renderer->RectFill( cursorX, realY + 5, 10, 20, White );
            }
        }


        g_renderer->Rect( realX, realY, m_w, m_h, Colour(255,255,255,10) );
    }

    void Keypress( int keyCode, bool shift, bool ctrl, bool alt, unsigned char ascii )
    {
        InputField::Keypress( keyCode, shift, ctrl, alt, ascii );

        AuthKeyWindow *parent = (AuthKeyWindow *)m_parent;

        if (keyCode == KEY_ENTER)
        {
            strupr( parent->m_key );
            Authentication_SetKey( parent->m_key );
            Authentication_SaveKey( parent->m_key, App::GetAuthKeyPath() );
        }

        //
        // CTRL-V paste support
        if( keyCode == KEY_V && g_keys[KEY_COMMAND] )
        {
            parent->PasteFromClipboard();            
        }
    }

    void MouseUp()
    {        
        if( strcmp( m_string, "authkey not found" ) == 0 )
        {
            m_string[0] = '\x0';
        }

        InputField::MouseUp();
    }
};


AuthKeyWindow::AuthKeyWindow()
:   InterfaceWindow( "Authentication", "dialog_authoptions", true ),
    m_keyCheckTimer(0.0f),
    m_timeOutTimer(0.0f)
{
    SetSize( 700, 250 );
    Centralise();

    Authentication_GetKey( m_key );
}


void AuthKeyWindow::Create()
{
    InterfaceWindow::Create();

    InvertedBox *invert = new InvertedBox();
    invert->SetProperties( "invert", 10, 30, m_w-20, m_h - 70, " ", " ", false, false );
    RegisterButton( invert );
    
    AuthInputField *input = new AuthInputField();
    input->SetProperties( "Key", 40, 50, 620, 30, " ", " ", false, false );
    input->RegisterString( m_key );
    RegisterButton( input );

	// Only show the paste button on platforms with a paste implementation
#if defined(TARGET_MSVC) || defined(TARGET_OS_MACOSX)
    PasteKeyButton *paste = new PasteKeyButton();
    paste->SetProperties( "Paste", 10, m_h-30, 100, 20, "dialog_paste", "dialog_paste_tooltip", true, true );
    RegisterButton( paste );
#endif
    
    ApplyKeyButton *apply = new ApplyKeyButton();
    apply->SetProperties( "Apply", m_w-220, m_h-30, 100, 20, "dialog_apply", " ", true, false );
    RegisterButton( apply );

    PlayDemoButton *demo = new PlayDemoButton();
    demo->SetProperties( "Play Demo", m_w/2-50, m_h-30, 100, 20, "dialog_play_demo", " ", true, false );
    demo->m_forceVisible = false;
    RegisterButton( demo );

    CloseButton *close = new CloseButton();
    close->SetProperties( "Close", m_w-110, m_h-30, 100, 20, "dialog_close", " ", true, false );
    RegisterButton(close);



    //
    // Auto-select the chat box if the key is empty

    char authKey[256];
    Authentication_GetKey( authKey );
    if( stricmp(authKey, "authkey not found") == 0 ||
        strlen(authKey) < 3 )
    {
        input->MouseUp();
    }
}


void AuthKeyWindow::PasteFromClipboard()
{
	bool gotClipboardText = false;
	
	// Read key into clipboard
#ifdef TARGET_MSVC	
    bool opened = OpenClipboard(NULL);
    if( opened )
    {
        bool textAvailable = IsClipboardFormatAvailable(CF_TEXT);
        if( textAvailable )
        {
            HANDLE clipTextHandle = GetClipboardData(CF_TEXT);
            if( clipTextHandle )
            {
                char *text = (char *) GlobalLock(clipTextHandle); 
                if(clipTextHandle) 
                { 
                    strncpy( m_key, text, AUTHENTICATION_KEYLEN-1 );
					m_key[AUTHENTICATION_KEYLEN] = '\0';
					gotClipboardText = true;
					
                    GlobalUnlock(text); 
                }
            }
        }
        CloseClipboard();
    }
#elif TARGET_OS_MACOSX
	PasteboardRef clipboard = NULL;
	ItemCount numItems = 0;
	CFDataRef clipboardData = NULL;
	CFStringRef clipboardString;
	
	PasteboardCreate(kPasteboardClipboard, &clipboard);
	if ( clipboard )
	{
		PasteboardGetItemCount(clipboard, &numItems);
		
		// Use the first item, if it exists. Multiple items are only for drag-and-drop, AFAIK
		if ( numItems > 0 )
		{
			PasteboardItemID firstItem;
			PasteboardGetItemIdentifier( clipboard, 1, &firstItem );
			PasteboardCopyItemFlavorData( clipboard, firstItem,
										  CFSTR("public.utf16-plain-text"), &clipboardData);
			if ( clipboardData )
			{
				clipboardString = CFStringCreateWithBytes(NULL, CFDataGetBytePtr(clipboardData),
														  CFDataGetLength(clipboardData),
														  kCFStringEncodingUnicode, false);
				
				// Convert to Latin 1 encoding, and copy as much as will fit
				memset(m_key, 0, sizeof(m_key));
				CFStringGetBytes(clipboardString, CFRangeMake(0, CFStringGetLength(clipboardString)),
								 kCFStringEncodingWindowsLatin1, 0, false,
								 (UInt8 *)m_key, AUTHENTICATION_KEYLEN-1, NULL);
				gotClipboardText = true;
				
				CFRelease(clipboardString);
				CFRelease(clipboardData);
			}
		}
	}
	
	CFRelease( clipboard );
#endif // platform specific

	// Cross-platform code, once we've gotten the clipboard contents into m_key
	//
	if ( gotClipboardText )
	{
		strupr( m_key );
		Authentication_SetKey( m_key );
		Authentication_SaveKey( m_key, App::GetAuthKeyPath() );
	}
}


void AuthKeyWindow::Render( bool _hasFocus )
{
    InterfaceWindow::Render( _hasFocus );
    
    char authKey[256];
    Authentication_GetKey( authKey );
    

    //
    // Render results of our Auth Check

    int simpleResult = Authentication_SimpleKeyCheck(authKey);

    if( stricmp( authKey, "authkey not found" ) == 0 ||
        strlen(authKey) < 3 )
    {
        g_renderer->TextCentreSimple( m_x + m_w/2, m_y + 130, White, 18, LANGUAGEPHRASE("dialog_type_authkey_or_demo_1") );
        g_renderer->TextCentreSimple( m_x + m_w/2, m_y + 160, White, 18, LANGUAGEPHRASE("dialog_type_authkey_or_demo_2") );
    }
    else
    {
        double timeNow = GetHighResTime();
        if( timeNow > m_keyCheckTimer )
        {
            Authentication_RequestStatus( authKey );
            m_keyCheckTimer = timeNow + 1.0f;
        }

        Colour mainText = White;
        Colour subtext = White;

        int authStatus = Authentication_GetStatus(authKey);
        char authStatusString[256];
        sprintf( authStatusString,"%s", LANGUAGEPHRASE(Authentication_GetStatusStringLanguagePhrase(authStatus)) );
        strupr( authStatusString );

        if( authStatus == AuthenticationUnknown )
        {
            if( m_timeOutTimer == 0.0f )
            {
                m_timeOutTimer = timeNow + 5.0f;
            }
            else if( timeNow > m_timeOutTimer )
            {
                sprintf( authStatusString, LANGUAGEPHRASE("dialog_auth_status_unknown") );
            }
            else
            {
                sprintf( authStatusString, LANGUAGEPHRASE("dialog_auth_status_checking") );
                if( fmodf(timeNow*2,2) < 1 ) subtext.m_a *= 0.5f;
            }            
        }
        else
        {
            m_timeOutTimer = 0.0f;
        }

        if( Authentication_IsDemoKey(authKey) &&
            authStatus == AuthenticationAccepted )
        {
            sprintf( authStatusString, LANGUAGEPHRASE("dialog_auth_status_demo_user") );
        }

        g_renderer->SetFont( "kremlin" );
        g_renderer->TextCentreSimple( m_x+m_w/2, m_y+140, mainText, 20, LANGUAGEPHRASE("dialog_auth_status") );
        g_renderer->TextCentreSimple( m_x+m_w/2, m_y+160, subtext, 30, authStatusString );
        g_renderer->SetFont();

        if( authStatus == AuthenticationWrongPlatform )
        {
        #if defined TARGET_MSVC
			g_renderer->TextCentreSimple( m_x+m_w/2, m_y+195, subtext, 14, LANGUAGEPHRASE("dialog_cannot_use_mac_key_on_pc") );
        #endif

        }

    }
}

#endif
