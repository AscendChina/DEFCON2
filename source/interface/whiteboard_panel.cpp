#include "lib/universal_include.h"
#include "lib/gucci/window_manager.h"
#include "lib/language_table.h"

#include "app/app.h"
#include "app/globals.h"

#include "interface/toolbar.h"
#include "interface/whiteboard_panel.h"

#include "renderer/map_renderer.h"

#include "world/world.h"
#include "world/whiteboard.h"


class PlanningButton : public ToolbarButton
{
    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        m_toggle = g_app->GetMapRenderer()->GetShowPlanning();
        
        ToolbarButton::Render( realX, realY, highlighted, clicked );
    }
    void MouseUp()
    {
		ToolbarButton::MouseUp();

		g_app->GetMapRenderer()->SetShowPlanning( !g_app->GetMapRenderer()->GetShowPlanning() );
    }
};

class EraseAllButton : public ToolbarButton
{
    void MouseUp()
	{
		ToolbarButton::MouseUp();

		Team *myTeam = g_app->GetWorld()->GetMyTeam();
		if ( !myTeam )
		{
			return;
		}

		WhiteBoard *whiteBoard = &g_app->GetWorld()->m_whiteBoards[ myTeam->m_teamId ];
		whiteBoard->RequestEraseAllLines();

		m_toggle = false;
    }
};

class ToggleShowAllWhiteBoardsButton : public ToolbarButton
{
    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        m_toggle = g_app->GetMapRenderer()->GetShowAllWhiteBoards();
        
        ToolbarButton::Render( realX, realY, highlighted, clicked );
    }
    void MouseUp()
    {
		ToolbarButton::MouseUp();

		g_app->GetMapRenderer()->SetShowAllWhiteBoards( !m_toggle );
    }
};

WhiteBoardPanel::WhiteBoardPanel( float iconSize, float gap )
:   FadingWindow( "WhiteBoardPanel" ),
	m_iconSize(iconSize),
	m_gap(gap)
{
	m_gap += 10;
    SetSize( 80, m_gap * 3 + 10 );
    SetPosition( g_windowManager->WindowW() - m_w,
	             max<int> ( 155, ( g_windowManager->WindowH() - m_h ) / 2 ) );

    LoadProperties( "WindowWhiteBoardPanel" );
}

WhiteBoardPanel::~WhiteBoardPanel()
{
    SaveProperties( "WindowWhiteBoardPanel" );
}

void WhiteBoardPanel::Create()
{
    float x = m_w / 2 - m_iconSize / 2;
    float y = 5;

    PlanningButton *planning = new PlanningButton();
    planning->SetProperties( "whiteBoardPlanning", x, y, m_iconSize, m_iconSize, "dialog_white_board_draw_erase", "tooltip_whiteboard_planning", true, true );
    strcpy(planning->m_iconFilename, "gui/tb_planning.bmp");
    RegisterButton( planning );

    EraseAllButton *eraseAll = new EraseAllButton();
    eraseAll->SetProperties( "whiteBoardEraseAll", x, y+=m_gap, m_iconSize, m_iconSize, "dialog_white_board_clear_all", "tooltip_whiteboard_eraseall", true, true );
    strcpy(eraseAll->m_iconFilename, "gui/tb_eraseall.bmp");
    RegisterButton( eraseAll );

    ToggleShowAllWhiteBoardsButton *toggleShowAll = new ToggleShowAllWhiteBoardsButton();
    toggleShowAll->SetProperties( "whiteBoardShowAll", x, y+=m_gap, m_iconSize, m_iconSize, "dialog_white_board_show_all", "tooltip_whiteboard_showall", true, true );
    strcpy(toggleShowAll->m_iconFilename, "gui/tb_showall.bmp");
    RegisterButton( toggleShowAll );
}

void WhiteBoardPanel::Render( bool _hasFocus )
{
    FadingWindow::Render( _hasFocus, true );    
}
