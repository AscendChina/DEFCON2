
#ifndef _included_networkinterface_h
#define _included_networkinterface_h

#include "interface/components/core.h"


class NetworkWindow : public InterfaceWindow
{
public:
    NetworkWindow( char *name, char *title = NULL, bool titleIsLanguagePhrase = false );

    void Create();
    void Render( bool hasFocus );
};


#endif
