#include "lib/universal_include.h"
#include "lib/render/renderer.h"
#include "lib/filesys/filesys_utils.h"
#include "lib/filesys/binary_stream_readers.h"
#include "lib/resource/bitmap.h"
#include "lib/resource/image.h"
#include "lib/gucci/window_manager.h"
#include "lib/preferences.h"
#include "lib/language_table.h"

#include "interface/components/message_dialog.h"
#include "interface/components/scrollbar.h"

#include "interface/mod_window.h"

#include "app/globals.h"
#include "app/app.h"
#include "app/modsystem.h"
#include "lib/multiline_text.h"



class InstalledModButton : public EclButton
{
public:
    int m_index;

    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        ModWindow *parent = (ModWindow *)m_parent;
        int actualIndex = parent->m_scrollbar->m_currentValue + m_index;

        if( g_modSystem->m_mods.ValidIndex(actualIndex) )
        {
            g_renderer->RectFill( realX, realY, m_w, m_h, Colour(255,255,255,10), Colour(255,255,255,50), false );

            InstalledMod *mod = g_modSystem->m_mods[actualIndex];
            
            if( mod->m_active )
            {
                g_renderer->RectFill( realX, realY, m_w, m_h, Colour(0,255,0,50) );
                g_renderer->TextRightSimple( realX + m_w - 10, realY + 3, White, 14, LANGUAGEPHRASE("dialog_mod_active") );
            }

            if( highlighted )
            {
                g_renderer->RectFill( realX, realY, m_w, m_h, Colour(255,255,255,50) );
                g_renderer->Rect( realX, realY, m_w, m_h, Colour(255,255,255,200) );
            }
            
            if( strcmp( mod->m_name, parent->m_selectionName ) == 0 &&
                strcmp( mod->m_version, parent->m_selectionVersion ) == 0 )
            {
                g_renderer->RectFill( realX, realY, m_w, m_h, Colour(255,255,255,50) );
                g_renderer->Rect( realX, realY, m_w, m_h, Colour(255,255,255,200) );
            }

            Colour textColour( 255, 255, 255, 200 );

            if( mod->m_critical )
            {                
                textColour.Set( 255, 0, 0, 255 );
            }

            g_renderer->TextSimple( realX + 10, realY + 3, textColour, 12, mod->m_name );
            g_renderer->TextSimple( realX + 180, realY + 4, textColour, 12, mod->m_version );
        }
    }


    void MouseUp()
    {
        ModWindow *parent = (ModWindow *)m_parent;
        int actualIndex = parent->m_scrollbar->m_currentValue + m_index;

        if( g_modSystem->m_mods.ValidIndex(actualIndex) )
        {
            InstalledMod *mod = g_modSystem->m_mods[actualIndex];
            parent->SelectMod( mod->m_name, mod->m_version );
        }
        else
        {
            parent->SelectMod(NULL, NULL);
        }
    }
};


class ActivateDeactivateButton : public InterfaceButton
{
    int IsVisible()
    {
        ModWindow *parent = (ModWindow *)m_parent;

        InstalledMod *mod = g_modSystem->GetMod( parent->m_selectionName, 
                                                 parent->m_selectionVersion );

        if( mod && mod->m_active ) return 1;
        if( mod && !mod->m_active ) return -1;
        
        return 0;
    }

    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        switch( IsVisible() )
        {
            case 0:     SetCaption( "dialog_mod_clear_all", true );          break;
            case 1:     SetCaption( "dialog_mod_deactivate", true );         break;
            case -1:    SetCaption( "dialog_mod_activate", true );           break;
        }

        InterfaceButton::Render( realX, realY, highlighted, clicked );
    }

    void MouseUp()
    {
        ModWindow *parent = (ModWindow *)m_parent;

        InstalledMod *mod = g_modSystem->GetMod( parent->m_selectionName, parent->m_selectionVersion );
        int isVisible = IsVisible();

        bool criticalChange = ( mod && mod->m_critical );

        if( isVisible == 0 && g_modSystem->IsCriticalModRunning() ) criticalChange = true;

        if( g_app->m_gameRunning && criticalChange )
        {
            MessageDialog *dialog = new MessageDialog( "ERROR",
                                                       "dialog_mod_no_arrangement_change", true,
                                                       "dialog_mod_error", true );
            EclRegisterWindow( dialog );
        }
        else
        {
            switch( IsVisible() )
            {
                case 0:     
                    g_modSystem->DeActivateAllMods();
                    break;

                case 1:     
                    g_modSystem->DeActivateMod( parent->m_selectionName );      
                    break;

                case -1:    
                    g_modSystem->ActivateMod( parent->m_selectionName, parent->m_selectionVersion );        
                    break;
            }

            parent->m_scrollbar->SetCurrentValue(0);
        }
    }
};


class MoveModButton : public InterfaceButton
{
public:
    int m_move;

    int IsVisible()
    {
        ModWindow *parent = (ModWindow *)m_parent;

        InstalledMod *mod = g_modSystem->GetMod( parent->m_selectionName, 
                                                 parent->m_selectionVersion );

        return( mod != NULL );
    }

    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        if( IsVisible() )
        {
            InterfaceButton::Render( realX, realY, highlighted, clicked );
        }
    }

    void MouseUp()
    {
        if( IsVisible() )
        {
            ModWindow *parent = (ModWindow *)m_parent;
    
            InstalledMod *mod = g_modSystem->GetMod( parent->m_selectionName, parent->m_selectionVersion );            
            
            if( g_app->m_gameRunning && mod && mod->m_critical )
            {
                MessageDialog *dialog = new MessageDialog( "ERROR",
                                                           "dialog_mod_no_arrangement_change", true,
                                                           "dialog_mod_error", true );
                EclRegisterWindow( dialog );
            }
            else
            {
                g_modSystem->MoveMod( parent->m_selectionName,
                                      parent->m_selectionVersion,
                                      m_move );
            }
        }
    }
};


class ApplyModsButton : public InterfaceButton
{
    bool IsVisible()
    {
        return g_modSystem->CommitRequired();
    }

    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        if( IsVisible() )
        {
            InterfaceButton::Render( realX, realY, highlighted, clicked );
        }
    }

    void MouseUp()
    {
        if( IsVisible() )
        {
            g_modSystem->Commit();
			g_app->RestartAmbienceMusic();
        }
    }
};


class FindMoreModsButton : public EclButton
{
    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        if( highlighted )
        {
            g_renderer->Rect( realX, realY, m_w, m_h, White );
        }

        g_renderer->TextCentre( realX+m_w/2, realY+5, Colour(200,200,255,200), 12, LANGUAGEPHRASE("dialog_mod_click_download_mods") );
    }

    void MouseUp()
    {
#if defined(RETAIL_BRANDING_UK)
        g_windowManager->OpenWebsite( LANGUAGEPHRASE("website_mods_retail_uk") );
#elif defined(RETAIL_BRANDING)
        g_windowManager->OpenWebsite( LANGUAGEPHRASE("website_mods_retail") );
#elif defined(RETAIL_BRANDING_MULTI_LANGUAGE)
        g_windowManager->OpenWebsite( LANGUAGEPHRASE("website_mods_retail_multi_language") );
#else
        g_windowManager->OpenWebsite( LANGUAGEPHRASE("website_mods") );
#endif

        int windowed = g_preferences->GetInt( "ScreenWindowed", 1 );
        if( !windowed )
        {
            // Switch to windowed mode if required
            g_preferences->SetInt( "ScreenWindowed", 1 );
            g_preferences->SetInt( "ScreenWidth", 1024 );
            g_preferences->SetInt( "ScreenHeight", 768 );

            g_app->ReinitialiseWindow();
            g_preferences->Save();        
        }
    }
};


class VisitModWebsiteButton : public EclButton
{
    char *GetValidURL()
    {
        ModWindow *parent = (ModWindow *)m_parent;
        InstalledMod *mod = g_modSystem->GetMod( parent->m_selectionName, parent->m_selectionVersion );

        if( mod && strcmp( mod->m_website, "unknown" ) != 0 )
        {
            return mod->m_website;
        }

        return NULL;
    }

    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        char *validUrl = GetValidURL();

        if( validUrl )
        {
            if( highlighted )
            {
                //g_renderer->Rect( realX, realY, m_w, m_h, White );
                g_renderer->Line( realX+m_w/2 - 9, realY+m_h, realX+m_w, realY+m_h, White );
            }

            g_renderer->TextSimple( realX+m_w/2 - 6, realY+5, White, 11, LANGUAGEPHRASE("dialog_mod_click_here") );

            SetTooltip( validUrl, false );
        }
        else
        {
            g_renderer->TextSimple( realX+m_w/2 - 1, realY+5, White, 11, LANGUAGEPHRASE("unknown") );
            SetTooltip( " ", false );
        }
    }

    void MouseUp()
    {
        char *validUrl = GetValidURL();

        if( validUrl )
        {
            g_windowManager->OpenWebsite( validUrl );

            int windowed = g_preferences->GetInt( "ScreenWindowed", 1 );
            if( !windowed )
            {
                // Switch to windowed mode if required
                g_preferences->SetInt( "ScreenWindowed", 1 );
                g_preferences->SetInt( "ScreenWidth", 1024 );
                g_preferences->SetInt( "ScreenHeight", 768 );

                g_app->ReinitialiseWindow();
                g_preferences->Save();        
            }
        }
    }
};


ModWindow::ModWindow()
:   InterfaceWindow( "Mods", "dialog_mod", true ),
    m_image(NULL),
    m_bitmap(NULL),
#ifdef TARGET_OS_MACOSX
    m_truncatePaths(true)
#else
	m_truncatePaths(false)
#endif
{
    SetSize( 560, 400 );

    m_selectionName[0] = '\x0';
    m_selectionVersion[0] = '\x0';
}


void ModWindow::Create()
{
    InterfaceWindow::Create();

    int listW = 320;

    InvertedBox *box = new InvertedBox();
    box->SetProperties( "box", 10, 30, listW, m_h - 70, " ", " ", false, false );
    RegisterButton( box );


    //
    // One button for each possible mod

    int modH = 20;
    int modG = 2;
    int maxMods = (m_h - 100) / (modH + modG);
    int y = 20;
    int modX = 20;
    int modW = listW - 40;

    for( int i = 0; i < maxMods; ++i )
    {
        char name[256];
        sprintf( name, "mod %d", i );

        InstalledModButton *button = new InstalledModButton();
        button->SetProperties( name, modX, y+=modH+modG, modW, modH, name, " ", false, false );
        button->m_index = i;
        RegisterButton( button );
    }

    //
    // Find more mods button

    FindMoreModsButton *moreMods = new FindMoreModsButton();
    moreMods->SetProperties( "MoreMods", modX, box->m_y + box->m_h - 20, modW, 20, " ", " ", false, false );
    RegisterButton( moreMods );


    //
    // Control buttons

    ActivateDeactivateButton *activate = new ActivateDeactivateButton();
    activate->SetProperties( "activate", 10, m_h - 30, 100, 20, "dialog_mod_activate", " ", true, false );
    RegisterButton( activate );

    MoveModButton *moveUp = new MoveModButton();
    moveUp->SetProperties( "Move Up", 120, m_h - 30, 100, 20, "dialog_mod_move_up", " ", true, false );
    moveUp->m_move = -1;
    RegisterButton( moveUp );

    MoveModButton *moveDown = new MoveModButton();
    moveDown->SetProperties( "Move Down", 230, m_h - 30, 100, 20, "dialog_mod_move_down", " ", true, false );
    moveDown->m_move = 1;
    RegisterButton( moveDown );


    ApplyModsButton *apply = new ApplyModsButton();
    apply->SetProperties( "Apply", m_w - 220, m_h - 30, 100, 20, "dialog_apply", " ", true, false );
    RegisterButton( apply );

    CloseButton *close = new CloseButton();
    close->SetProperties( "Close", m_w - 110, m_h - 30, 100, 20, "dialog_close", " ", true, false );
    RegisterButton( close );


    //
    // Visit URL button
    
    VisitModWebsiteButton *visitUrl = new VisitModWebsiteButton();
    visitUrl->SetProperties( "visiturl", m_w - 110, 185, 100, 15, " ", " ", false, false );
    RegisterButton( visitUrl );


    //
    // Scrollbar

    int numRows = g_modSystem->m_mods.Size();

    m_scrollbar = new ScrollBar( this );
    m_scrollbar->Create( "scroll", 10 + listW - 27, 42, 18, m_h - 110, numRows, maxMods );        
}


void ModWindow::SelectMod( char *_name, char *_version )
{
    //
    // Copy the mod names

    m_selectionName[0] = '\x0';
    m_selectionVersion[0] = '\x0';

    if( _name && _version )
    {
        strcpy( m_selectionName, _name );
        strcpy( m_selectionVersion, _version );
    }
   

    //
    // Delete existing image

    if( m_image )
    {
        delete m_image;
        m_image = NULL;
    }

    if( m_bitmap )
    {
        delete m_bitmap;
        m_bitmap = NULL;
    }


    //
    // Get the mod

    InstalledMod *mod = g_modSystem->GetMod( m_selectionName, m_selectionVersion );
    if( !mod )
    {
        return;
    }


    //
    // Load the image

    char fullFilename[512];
    sprintf( fullFilename, "%sscreenshot.bmp", mod->m_path );
    
    if( DoesFileExist(fullFilename) )
    {
        BinaryFileReader reader( fullFilename );
        m_bitmap = new Bitmap( &reader, "bmp" );
        m_image = new Image( m_bitmap );
        m_image->MakeTexture(true,false);
    }
}



void ModWindow::Render( bool _hasFocus )
{
    InterfaceWindow::Render( _hasFocus );

    int xPos = m_x + m_w - 220;
    int w = 210;

    InstalledMod *mod = g_modSystem->GetMod( m_selectionName, m_selectionVersion );

    //
    // Preview image

    if( m_image )
    {
        g_renderer->Blit( m_image, xPos, m_y + 30, w, 118, White );
    }
    else
    {
        if( !mod )
        {
            g_renderer->TextCentreSimple( xPos+w/2, m_y + 60, White, 14, LANGUAGEPHRASE("dialog_mod_no_mod_selected") );
        }
        else
        {
            g_renderer->TextCentreSimple( xPos+w/2, m_y + 60, White, 14, LANGUAGEPHRASE("dialog_mod_screenshot_not_found") );
            g_renderer->TextCentre( xPos+w/2, m_y + 80, White, 10, "%sscreenshot.bmp", mod->m_path );
        }
    }

    g_renderer->Rect( xPos, m_y + 30, w, w*9/16.0f, Colour(200,200,255,200) );


    //
    // Mod details

    if( mod )
    {        
        int yPos = m_y + 160;
        int fontSize = 11;
        int gap = 15;
        Colour nameColour(200,200,255,200);

        g_renderer->TextSimple( xPos, yPos, nameColour, fontSize, LANGUAGEPHRASE("dialog_mod_install_dir") );
        g_renderer->TextRightSimple( xPos + w, yPos, White, fontSize,
                                     m_truncatePaths ? mod->m_name : mod->m_path ); // m_name was generated from dir name

        yPos += gap;

        g_renderer->TextSimple( xPos, yPos, nameColour, fontSize, LANGUAGEPHRASE("dialog_mod_author") );
        if( strcmp( mod->m_author, "unknown" ) != 0 )
		{
			g_renderer->TextRightSimple( xPos + w, yPos, White, fontSize, mod->m_author );
		}
		else
		{
			g_renderer->TextRightSimple( xPos + w, yPos, White, fontSize, LANGUAGEPHRASE("unknown") );
		}

        yPos += gap;

        g_renderer->TextSimple( xPos, yPos, nameColour, fontSize, LANGUAGEPHRASE("dialog_mod_website") );
        //g_renderer->TextRightSimple( xPos + w, yPos, White, fontSize, mod->m_website );

        yPos += gap;

        //
        // Comment

        if( stricmp( mod->m_comment, "unknown" ) != 0 )
        {
            MultiLineText wrapped( mod->m_comment, w-10, 10 );

            for( int i = 0; i < wrapped.Size(); ++i )
            {
                char *thisLine = wrapped[i];
                g_renderer->Text( xPos, yPos += 12, Colour(200,200,255,200), 10, thisLine );
            }
        }
    }


    //
    // Critical help message

    if( mod && mod->m_critical )
    {
        g_renderer->TextCentre( xPos+w/2, m_y + m_h - 50, Colour(255,0,0,255), 12, LANGUAGEPHRASE("dialog_mod_required_all_players") );
    }

}
