
/*
 * ========
 * RENDERER
 * ========
 *
 */

#ifndef _included_renderer_h
#define _included_renderer_h

#include "lib/tosser/btree.h"

#include "colour.h"

class Image;
class BitmapFont;

#define     White           Colour(255,255,255)
#define     Black           Colour(0,0,0)

#define     LightGray       Colour(200,200,200)
#define     DarkGray        Colour(100,100,100)

#define     PREFS_GRAPHICS_SMOOTHLINES          "RenderSmoothLines"



class Renderer
{
protected:
    char    *m_defaultFontName;
    char    *m_defaultFontFilename;
    bool    m_defaultFontLanguageSpecific;
    char    *m_currentFontName;
    char    *m_currentFontFilename;
    bool    m_currentFontLanguageSpecific;
    bool    m_horizFlip;
    bool    m_fixedWidth;
    bool    m_negative;
    BTree <float> m_fontSpacings;

public:
    float   m_alpha;
    int     m_colourDepth;
    int     m_mouseMode;
    int     m_blendMode;
    
    enum
    {
        BlendModeDisabled,
        BlendModeNormal,
        BlendModeAdditive,
        BlendModeSubtractive
    };

public:
    Renderer();
    virtual ~Renderer();

    void    Shutdown            ();
    
    void    Set2DViewport       ( float l, float r, float b, float t,
                                    int x, int y, int w, int h );
    void    Reset2DViewport     ();

    void    BeginScene          ();
    void    ClearScreen         ( bool _colour, bool _depth );

    void    SaveScreenshot      ();


    //
    // Rendering modes

    void    SetBlendMode        ( int _blendMode );
    void    SetDepthBuffer      ( bool _enabled, bool _clearNow );

    //
    // Text output

    void    SetDefaultFont      ( const char *font, const char *_langName = NULL );
    void    SetFontSpacing      ( const char *font, float _spacing );
    float   GetFontSpacing      ( const char *font );
    void    SetFont             ( const char *font = NULL, 
                                  bool horizFlip = false, 
                                  bool negative = false,
                                  bool fixedWidth = false,
                                  const char *_langName = NULL );
    void    SetFont             ( const char *font, 
                                  const char *_langName );
    bool    IsFontLanguageSpecific ();
    BitmapFont *GetBitmapFont   ();

    void    Text                ( float x, float y, Colour const &col, float size, const char *text, ... );
    void    TextCentre          ( float x, float y, Colour const &col, float size, const char *text, ... );
    void    TextRight           ( float x, float y, Colour const &col, float size, const char *text, ... );
    void    TextSimple          ( float x, float y, Colour const &col, float size, const char *text );
    void    TextCentreSimple    ( float x, float y, Colour const &col, float size, const char *text );
    void    TextRightSimple     ( float x, float y, Colour const &col, float size, const char *text );
    
    float   TextWidth           ( const char *text, float size );
    float   TextWidth           ( const char *text, unsigned int textLen, float size, BitmapFont *bitmapFont );

    //
    // Drawing primitives

    void    Rect                ( float x, float y, float w, float h, Colour const &col, float lineWidth=1.0f );
    void    RectFill            ( float x, float y, float w, float h, Colour const &col );    
    void    RectFill            ( float x, float y, float w, float h, Colour const &colTL, Colour const &colTR, Colour const &colBR, Colour const &colBL );    
    void    RectFill            ( float x, float y, float w, float h, Colour const &col1, Colour const &col2, bool horizontal );    
    
    void    Circle              ( float x, float y, float radius, int numPoints, Colour const &col, float lineWidth=1.0f );        
    void    CircleFill          ( float x, float y, float radius, int numPoints, Colour const &col );
    void    Line                ( float x1, float y1, float x2, float y2, Colour const &col, float lineWidth=1.0f );
    
    void    BeginLines          ( Colour const &col, float lineWidth );    
    void    Line                ( float x, float y );
    void    EndLines            ();

    void    SetClip             ( int x, int y, int w, int h );      
    void    ResetClip           ();

    void    Blit                ( Image *src, float x, float y, Colour const &col);
    void    Blit                ( Image *src, float x, float y, float w, float h, Colour const &col);
    void    Blit                ( Image *src, float x, float y, float w, float h, Colour const &col, float angle);

protected:
    char *ScreenshotsDirectory();
};


extern Renderer *g_renderer;

#endif
