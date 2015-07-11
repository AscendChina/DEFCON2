
#ifndef _included_worldobjectwindow_h
#define _included_worldobjectwindow_h

#include "lib/eclipse/eclipse.h"


class WorldObjectWindow : public EclWindow
{

public:
    WorldObjectWindow( char *name );

    void Render( bool hasFocus );
};


#endif

