#ifndef _included_modwindow_h
#define _included_modwindow_h

#include "interface/components/core.h"

class Bitmap;
class Image;
class ScrollBar;



class ModWindow : public InterfaceWindow
{
public:
    char m_selectionName[256];
    char m_selectionVersion[256];
    bool m_truncatePaths;

    Bitmap      *m_bitmap;
    Image       *m_image;
    ScrollBar   *m_scrollbar;

public:
    ModWindow();

    void SelectMod( char *_name, char *_version );

    void Create();
    void Render( bool _hasFocus );
};


#endif
