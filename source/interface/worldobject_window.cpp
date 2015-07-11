#include "lib/universal_include.h"
#include "lib/render/renderer.h"

#include "app/app.h"
#include "app/globals.h"

#include "interface/worldobject_window.h"



WorldObjectWindow::WorldObjectWindow( char *name )
:   EclWindow( name )
{
    SetMovable(false);
}


void WorldObjectWindow::Render( bool hasFocus )
{
    g_renderer->Rect( m_x, m_y, m_w, m_h, White );
}

