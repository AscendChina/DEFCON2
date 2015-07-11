#ifndef _included_profilewindow_h
#define _included_profilewindow_h

#include "interface/components/core.h"


class ProfiledElement;


//*****************************************************************************
// Class ProfileWindow
//*****************************************************************************

class ProfileWindow : public InterfaceWindow
{
protected:	
	void RenderElementProfile(ProfiledElement *_pe, unsigned int _indent);

    void RenderFrameTimes( float x, float y, float w, float h );

protected:
	int m_yPos;
    
public:
    ProfileWindow( char *name, char *title = NULL, bool titleIsLanguagePhrase = false );
    ~ProfileWindow();

    void Render( bool hasFocus );
	void Create();
};


#endif

