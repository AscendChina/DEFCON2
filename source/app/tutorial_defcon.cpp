#include "lib/universal_include.h"
#include "lib/debug_utils.h"
#include "lib/hi_res_time.h"
#include "lib/language_table.h"
#include "lib/eclipse/eclipse.h"
#include "lib/filesys/text_stream_readers.h"
#include "lib/filesys/file_system.h"
#include "lib/render/styletable.h"
#include "lib/gucci/window_manager.h"
#include "lib/gucci/input.h"
#include "lib/math/math_utils.h"
#include "lib/math/random_number.h"
#include "lib/preferences.h"
#include "lib/resource/resource.h"

#include "renderer/map_renderer.h"

#include "network/Server.h"

#include "app/tutorial.h"
#include "app/globals.h"
#include "app/app.h"
#include "app/game.h"
#include "lib/multiline_text.h"

#include "interface/interface.h"
#include "interface/side_panel.h"
#include "interface/placement_icon.h"
#include "interface/tutorial_window.h"

#include "world/fleet.h"
#include "world/nuke.h"
#include "world/city.h"

#include <math.h> // for round()


class ClickNextPopup : public TutorialPopup
{
    // Click next to continue
    void Render( bool _hasFocus )
    {
        if( g_app->GetTutorial()->IsNextClickable() )
        {
            TutorialPopup::Render( _hasFocus );
        }
    }
};


// ============================================================================
// Mission 1 popups
// ============================================================================

class Mission1SiloPopup : public TutorialPopup
{
    // Change this silos state to Nuke Launch
    void Render( bool _hasFocus )
    {
        if( g_app->GetTutorial()->GetCurrentChapter() > 3 )
        {
            WorldObject *wobj = g_app->GetWorld()->GetWorldObject(m_objectId);
            if( wobj && wobj->m_currentState != 0 )
            {
                TutorialPopup::Render(_hasFocus);
            }
        }
    }
};


class Mission1SiloSelectPopup : public TutorialPopup
{
    // Select this Silo
    void Render( bool _hasFocus )
    {
        int radar1 = g_app->GetTutorial()->m_objectIds[1];
        int radar2 = g_app->GetTutorial()->m_objectIds[2];

        bool radar1Exists = g_app->GetWorld()->GetWorldObject(radar1);
        bool radar2Exists = g_app->GetWorld()->GetWorldObject(radar2);
        if( radar1Exists || radar2Exists )
        {
            int numInQueue1, numInFlight1;
            int numInQueue2, numInFlight2;
            g_app->GetWorld()->GetNumNukers( radar1, &numInQueue1, &numInFlight1 );
            g_app->GetWorld()->GetNumNukers( radar2, &numInQueue2, &numInFlight2 );

            if( !radar1Exists ) numInQueue1 = 1;
            if( !radar2Exists ) numInQueue2 = 1;

            if( numInQueue1+numInFlight1 < 1 || numInQueue2+numInFlight2 < 1 )
            {
                WorldObject *wobj = g_app->GetWorld()->GetWorldObject(m_objectId);

                if( wobj && wobj->m_currentState == 0 &&
                    g_app->GetMapRenderer()->GetCurrentSelectionId() != m_objectId )
                {
                    TutorialPopup::Render(_hasFocus);
                }
            }
        }
    }
};


class Mission1Deselect : public TutorialPopup
{
    // Press space to deselect
    void Render( bool _hasFocus )
    {
        if( g_app->GetMapRenderer()->GetCurrentSelectionId() == m_objectId )
        {
            int numAlive = 0;

            int radar1 = g_app->GetTutorial()->m_objectIds[1];
            int radar2 = g_app->GetTutorial()->m_objectIds[2];
            if( g_app->GetWorld()->GetWorldObject(radar1) ) numAlive++;
            if( g_app->GetWorld()->GetWorldObject(radar2) ) numAlive++;
            
            int numInQueue1, numInFlight1;
            int numInQueue2, numInFlight2;
            g_app->GetWorld()->GetNumNukers( radar1, &numInQueue1, &numInFlight1 );
            g_app->GetWorld()->GetNumNukers( radar2, &numInQueue2, &numInFlight2 );

            if( numInQueue1 || numInFlight1 ) numAlive--;
            if( numInQueue2 || numInFlight2 ) numAlive--;
            
            if( numAlive == 0 )
            {
                 TutorialPopup::Render(_hasFocus);
            }        
        }
    }
};


class Mission1RadarTarget : public TutorialPopup
{
    // Launch a nuke at this Radar
    void Render( bool _hasFocus )
    {
        int siloId = g_app->GetTutorial()->m_objectIds[0];
        WorldObject *silo = g_app->GetWorld()->GetWorldObject(siloId);
        if( silo && 
            silo->m_currentState == 0 &&
            g_app->GetMapRenderer()->GetCurrentSelectionId() == siloId )
        {
            WorldObject *wobj = g_app->GetWorld()->GetWorldObject(m_objectId);
            if( wobj )
            {
                int nukesInFlight, nukesInQueue;
                g_app->GetWorld()->GetNumNukers( m_objectId, &nukesInFlight, &nukesInQueue );

                if( nukesInFlight == 0 && nukesInQueue == 0 )
                {
                    TutorialPopup::Render(_hasFocus);
                }
            }
        }
    }
};


// ============================================================================
// Mission 2 popups
// ============================================================================

class UnitPanelPopup : public TutorialPopup
{
public:
    void Render( bool _hasFocus )
    {
        int unitsAvailable = g_app->GetWorld()->GetTeam(1)->m_unitsAvailable[WorldObject::TypeSilo] +
                             g_app->GetWorld()->GetTeam(1)->m_unitsAvailable[WorldObject::TypeRadarStation] + 
                             g_app->GetWorld()->GetTeam(1)->m_unitsAvailable[WorldObject::TypeAirBase] + 
                             g_app->GetWorld()->GetTeam(1)->m_unitsAvailable[WorldObject::TypeBattleShip] + 
                             g_app->GetWorld()->GetTeam(1)->m_unitsAvailable[WorldObject::TypeCarrier] + 
                             g_app->GetWorld()->GetTeam(1)->m_unitsAvailable[WorldObject::TypeSub];

        if( !EclGetWindow( "Side Panel" ) &&
             unitsAvailable > 0 )
        {
            TutorialPopup::Render( _hasFocus );
        }
    }
};


class SelectSiloPopup : public TutorialPopup
{
public:
    void Render( bool _hasFocus )
    {
        if( EclGetWindow( "Side Panel" ) &&
            !EclGetWindow( "Placement" ) &&
            g_app->GetWorld()->GetTeam(1)->m_unitsAvailable[WorldObject::TypeSilo] > 0 )
        {
            TutorialPopup::Render( _hasFocus );
        }
    }
};


class PlaceSilosPopup : public TutorialPopup
{
public:
    void Render( bool _hasFocus )
    {
        if(EclGetWindow( "Placement" ))
        {
            TutorialPopup::Render(_hasFocus);
        }
    }
};


// ============================================================================
// Mission 3 popups
// ============================================================================

class SelectRadarPopup : public TutorialPopup
{
    void Render( bool _hasFocus )
    {
        if( EclGetWindow( "Side Panel" ) &&
            !EclGetWindow( "Placement" ) &&
            g_app->GetWorld()->GetTeam(1)->m_unitsAvailable[WorldObject::TypeRadarStation] > 0 )
        {
            TutorialPopup::Render( _hasFocus );
        }
    }
};


class PlaceRadarPopup : public TutorialPopup
{
    void Render( bool _hasFocus )
    {
        if(EclGetWindow( "Placement" ) &&
           g_app->GetWorld()->GetTeam(1)->m_unitsAvailable[WorldObject::TypeRadarStation] > 0)
        {
            TutorialPopup::Render( _hasFocus );
        }
    }
};


class Mission3SelectSilosPopup : public SelectSiloPopup
{
    void Render( bool _hasFocus )
    {
        if(g_app->GetWorld()->GetTeam(1)->m_unitsAvailable[WorldObject::TypeRadarStation] == 0 )
        {
            SelectSiloPopup::Render(_hasFocus);
        }
    }
};


class Mission3PlaceSilosPopup : public PlaceSilosPopup
{
    void Render( bool _hasFocus )
    {
        if(g_app->GetWorld()->GetTeam(1)->m_unitsAvailable[WorldObject::TypeRadarStation] == 0 )
        {
            PlaceSilosPopup::Render( _hasFocus );
        }
    }
};


class AccelTimePopup : public TutorialPopup
{
    void Render( bool _hasFocus )
    {
        if(g_app->GetWorld()->GetTeam(1)->m_unitsAvailable[WorldObject::TypeRadarStation] == 0 &&
            g_app->GetWorld()->GetTeam(1)->m_unitsAvailable[WorldObject::TypeSilo] == 0 &&
            g_app->GetWorld()->GetTimeScaleFactor() < GAMESPEED_FAST &&
            g_app->GetWorld()->GetDefcon() > 1 )
        {
            TutorialPopup::Render( _hasFocus );
        }
    }
};


class SilosIntoNukeModePopup : public TutorialPopup
{
    void Render( bool _hasFocus )
    {
        //
        // Look for a Silo to attach to

        bool found = false;

        for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
        {
            if( g_app->GetWorld()->m_objects.ValidIndex(i) )
            {
                WorldObject *wobj = g_app->GetWorld()->m_objects[i];
                if( wobj &&
                    wobj->m_teamId == 1 &&
                    wobj->m_type == WorldObject::TypeSilo &&
                    wobj->m_currentState == 1 &&
                    wobj->m_states[0]->m_numTimesPermitted > 0 )
                {
                    m_targetLongitude = wobj->m_longitude.DoubleValue();
                    m_targetLatitude = wobj->m_latitude.DoubleValue();
                    found = true;
                    break;
                    //g_app->GetTutorial()->m_objectIds[2+numSilosFound] = wobj->m_objectId;
                }
            }
        }

        if( found &&
            g_app->GetWorld()->GetDefcon() == 1 &&
            g_app->GetMapRenderer()->GetCurrentSelectionId() == -1 )
        {
            TutorialPopup::Render( _hasFocus );
        }
    }
};



class NukeEnemySiloPopup : public TutorialPopup
{
    void Render( bool _hasFocus )
    {
        bool inChapter = g_app->GetTutorial()->InChapter("m3destroyenemy") ||
                         g_app->GetTutorial()->InChapter("m3silostaketime");

        if( inChapter  &&
            g_app->GetWorld()->GetWorldObject(m_objectId) &&
            g_app->GetMapRenderer()->GetCurrentSelectionId() != -1 )
        {
            int numInFlight;
            int numInQueue;
            g_app->GetWorld()->GetNumNukers( m_objectId, &numInFlight, &numInQueue );           
            int total = numInFlight + numInQueue;
            
            if( total < 20 )
            {
                TutorialPopup::Render( _hasFocus );
            }
        }
    }
};


class NukeEnemyAirBasePopup : public TutorialPopup
{
    void Render( bool _hasFocus )
    {
        bool inChapter = g_app->GetTutorial()->InChapter("m3destroyenemy") ||
                         g_app->GetTutorial()->InChapter("m3silostaketime");

        if( inChapter &&
            g_app->GetWorld()->GetWorldObject(m_objectId) &&
            g_app->GetMapRenderer()->GetCurrentSelectionId() != -1 )
        {
            int numInFlight=10;
            int numInQueue=10;

            int siloId = g_app->GetTutorial()->m_objectIds[0];
            bool siloExists = g_app->GetWorld()->GetWorldObject(siloId);

            g_app->GetWorld()->GetNumNukers( siloId, &numInFlight, &numInQueue );           
                        
            int total = numInFlight + numInQueue;

            if( !siloExists || total >= 10 )
            {
                g_app->GetWorld()->GetNumNukers( m_objectId, &numInFlight, &numInQueue );           
                total = numInFlight + numInQueue;

                if( total < 5 )
                {
                    TutorialPopup::Render( _hasFocus );
                }
            }
        }
    }
};


class NukeEnemyCitiesPopup : public TutorialPopup
{
    void Render( bool _hasFocus )
    {
        bool inChapter = g_app->GetTutorial()->InChapter("m3destroyenemy") ||
                         g_app->GetTutorial()->InChapter("m3silostaketime");

        if( inChapter &&
            g_app->GetMapRenderer()->GetCurrentSelectionId() != -1 )
        {            
            int siloId = g_app->GetTutorial()->m_objectIds[0];
            int airbase1Id = g_app->GetTutorial()->m_objectIds[1];
            int airbase2Id = g_app->GetTutorial()->m_objectIds[2];

            bool siloExists = g_app->GetWorld()->GetWorldObject( siloId );
            bool airbase1Exists = g_app->GetWorld()->GetWorldObject( airbase1Id );
            bool airbase2Exists = g_app->GetWorld()->GetWorldObject( airbase2Id );

            int numInFlight, numInQueue;

            if( siloExists )
            {
                g_app->GetWorld()->GetNumNukers( siloId, &numInFlight, &numInQueue );           
                if( numInFlight + numInQueue < 20 ) return;
            }

            if( airbase1Exists )
            {
                g_app->GetWorld()->GetNumNukers( airbase1Id, &numInFlight, &numInQueue );           
                if( numInQueue + numInFlight < 5 ) return;
            }

            if( airbase2Exists )
            {
                g_app->GetWorld()->GetNumNukers( airbase2Id, &numInFlight, &numInQueue );           
                if( numInQueue + numInFlight < 5 ) return;
            }

            TutorialPopup::Render( _hasFocus );
        }
    }
};


// ============================================================================
// Mission 4 popups
// ============================================================================

class SelectAirBasePopup : public TutorialPopup
{
    void Render( bool _hasFocus )
    {
        if( EclGetWindow( "Side Panel" ) &&
            !EclGetWindow( "Placement" ) &&
            g_app->GetWorld()->GetTeam(1)->m_unitsAvailable[WorldObject::TypeAirBase] > 0 )
        {
            TutorialPopup::Render( _hasFocus );
        }
    }
};


class PlaceAirBasePopup : public TutorialPopup
{
    void Render( bool _hasFocus )
    {
        if(EclGetWindow( "Placement" ) &&
           g_app->GetWorld()->GetTeam(1)->m_unitsAvailable[WorldObject::TypeAirBase] > 0)
        {
            TutorialPopup::Render( _hasFocus );
        }
    }
};


class ClickOnAirbasePopup : public TutorialPopup
{
    void Render( bool _hasFocus )
    {
        bool inChapter = g_app->GetTutorial()->InChapter("M4LaunchFighters") ||
                         g_app->GetTutorial()->InChapter("M4AboutFighters");

        if( inChapter &&
            g_app->GetWorld()->GetTeam(1)->m_unitsAvailable[WorldObject::TypeAirBase] == 0 &&
            g_app->GetMapRenderer()->GetCurrentSelectionId() == -1 )
        {
            m_objectId = -1;
            
            for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
            {
                if( g_app->GetWorld()->m_objects.ValidIndex(i) )
                {
                    WorldObject *wobj = g_app->GetWorld()->m_objects[i];
                    if( wobj->m_type == WorldObject::TypeAirBase &&
                        wobj->m_teamId == 1 &&
                        wobj->m_currentState == 0 &&
                        wobj->m_states[0]->m_numTimesPermitted > 0 &&
                        wobj->m_actionQueue.Size() < wobj->m_states[0]->m_numTimesPermitted )
                    {
                        m_objectId = wobj->m_objectId;
                    }
                }
            }

            if( g_app->GetWorld()->GetWorldObject(m_objectId) )
            {
                TutorialPopup::Render( _hasFocus );
            }
        }
    }
};


class ScoutFightersHerePopup : public TutorialPopup
{
    void Render( bool _hasFocus )
    {
        bool inChapter = g_app->GetTutorial()->InChapter("M4LaunchFighters") ||
                         g_app->GetTutorial()->InChapter("M4AboutFighters");

        if( inChapter )
        {
            int selectionId = g_app->GetMapRenderer()->GetCurrentSelectionId();
            WorldObject *wobj = g_app->GetWorld()->GetWorldObject(selectionId);

            if( wobj &&
                wobj->m_type == WorldObject::TypeAirBase &&
                wobj->m_currentState == 0 &&
                wobj->m_states[0]->m_numTimesPermitted > 0 &&
                wobj->m_actionQueue.Size() < wobj->m_states[0]->m_numTimesPermitted )
            {
                TutorialPopup::Render( _hasFocus );
            }
        }
    }
};


class ToggleRadarPopup : public TutorialPopup
{
    void Render( bool _hasFocus )
    {
        if( g_app->GetTutorial()->InChapter("M4Radar") )
        {
            TutorialPopup::Render( _hasFocus );
        }
    }
};


class SwitchToBomberModePopup : public TutorialPopup
{
    void Render( bool _hasFocus )
    {
        if( g_app->GetTutorial()->InChapter("M4LaunchBombers") )
        {
            int numInBomberMode = 0;
            m_objectId = -1;

            for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
            {
                if( g_app->GetWorld()->m_objects.ValidIndex(i) )
                {
                    WorldObject *wobj = g_app->GetWorld()->m_objects[i];
                    if( wobj->m_type == WorldObject::TypeAirBase &&
                        wobj->m_teamId == 1 &&
                        wobj->m_states[1]->m_numTimesPermitted > 0 )
                    {
                        m_objectId = wobj->m_objectId;

                        if( wobj->m_currentState == 1 )
                        {                        
                            numInBomberMode++;
                        }
                    }
                }
            }

            if( numInBomberMode == 0 )
            {
                TutorialPopup::Render( _hasFocus );
            }
        }
    }
};


class ClickOnAirbaseBomberPopup : public TutorialPopup
{
    void Render( bool _hasFocus )
    {
        bool inChapter = g_app->GetTutorial()->InChapter("M4LaunchBombers") ||
                         g_app->GetTutorial()->InChapter("M4BomberInfo") ||
                         g_app->GetTutorial()->InChapter("M4NukesLaunched") ;

        int numDealtWith = 0;
        for( int i = 0; i < 3; ++i )
        {
            WorldObject *wobj = g_app->GetWorld()->GetWorldObject( g_app->GetTutorial()->m_objectIds[i] );
            if( !wobj ) numDealtWith++;
            if( wobj )
            {
                int inQueue, inFlight;
                g_app->GetWorld()->GetNumNukers( g_app->GetTutorial()->m_objectIds[i], &inFlight, &inQueue );
                int total = inQueue + inFlight;
                if( total > 0 ) numDealtWith++;
            }
        }

        if( inChapter &&
            numDealtWith < 3 &&
            g_app->GetMapRenderer()->GetCurrentSelectionId() == -1 )
        {
            m_objectId = -1;
            
            for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
            {
                if( g_app->GetWorld()->m_objects.ValidIndex(i) )
                {
                    WorldObject *wobj = g_app->GetWorld()->m_objects[i];
                    if( wobj->m_type == WorldObject::TypeAirBase &&
                        wobj->m_teamId == 1 &&
                        wobj->m_currentState == 1 &&
                        wobj->m_states[1]->m_numTimesPermitted > 0 &&
                        wobj->m_actionQueue.Size() < wobj->m_states[1]->m_numTimesPermitted )
                    {
                        m_objectId = wobj->m_objectId;
                    }
                }
            }

            if( g_app->GetWorld()->GetWorldObject(m_objectId) )
            {
                TutorialPopup::Render( _hasFocus );
            }
        }
    }
};


class NukeEnemyBuildingPopup : public TutorialPopup
{
    void Render( bool _hasFocus )
    {   
        bool inChapter = g_app->GetTutorial()->InChapter("M4LaunchBombers") ||
                         g_app->GetTutorial()->InChapter("M4BomberInfo") ||
                         g_app->GetTutorial()->InChapter("M4NukesLaunched");

        int selectionId = g_app->GetMapRenderer()->GetCurrentSelectionId();
        WorldObject *wobj = g_app->GetWorld()->GetWorldObject(selectionId);

        if( inChapter && 
            wobj &&
            wobj->m_type == WorldObject::TypeAirBase &&
            g_app->GetWorld()->GetWorldObject(m_objectId) )
        {
            int numInFlight, numInQueue;
            g_app->GetWorld()->GetNumNukers( m_objectId, &numInFlight, &numInQueue );
            int total = numInFlight + numInQueue;

            if( total < 2 )
            {
                TutorialPopup::Render( _hasFocus );
            }
        }
    }
};


// ============================================================================
// Mission 5 popups
// ============================================================================

class SelectFleetPopup : public TutorialPopup
{
    void Render( bool _hasFocus )
    {
        SidePanel *sidePanel = (SidePanel *)EclGetWindow("Side Panel");

        int shipsAvailable = g_app->GetWorld()->GetTeam(1)->m_unitsAvailable[WorldObject::TypeBattleShip] + 
                             g_app->GetWorld()->GetTeam(1)->m_unitsAvailable[WorldObject::TypeCarrier] +
                             g_app->GetWorld()->GetTeam(1)->m_unitsAvailable[WorldObject::TypeSub];

        if( shipsAvailable &&
            sidePanel && 
            sidePanel->m_mode != SidePanel::ModeFleetPlacement )
        {
            TutorialPopup::Render( _hasFocus );
        }
    }
};


class AddShipToFleetPopup : public TutorialPopup
{
public:
    int m_unitType;
    void Render( bool _hasFocus )
    {
        SidePanel *sidePanel = (SidePanel *)EclGetWindow("Side Panel");

        if( sidePanel &&
            sidePanel->m_mode == SidePanel::ModeFleetPlacement &&
            g_app->GetWorld()->GetTeam(1)->m_unitsAvailable[m_unitType] > 0 &&
            !EclGetWindow("Placement") )
        {
            TutorialPopup::Render( _hasFocus );
        }
    }
};


class DeployFleetPopup : public TutorialPopup
{
public:
    void Render( bool _hasFocus )
    {
        if( EclGetWindow("Placement") )
        {
            TutorialPopup::Render( _hasFocus );
        }
    }
};


class SelectDeployedFleetPopup : public TutorialPopup
{
    void Render( bool _hasFocus )
    {
        if( g_app->GetTutorial()->InChapter( "M5BattleshipInfo") &&
            g_app->GetMapRenderer()->GetCurrentSelectionId() == -1 )
        {
            //
            // Look for a target

            for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
            {
                if( g_app->GetWorld()->m_objects.ValidIndex(i) )
                {
                    WorldObject *wobj = g_app->GetWorld()->m_objects[i];
                    if( wobj->m_teamId == 1 &&
                        wobj->m_fleetId != -1 )
                    {
                        Fleet *fleet = g_app->GetWorld()->GetTeam(1)->GetFleet(wobj->m_fleetId);
                        m_targetLongitude = fleet->m_longitude.DoubleValue();
                        m_targetLatitude = fleet->m_latitude.DoubleValue();
                    }
                }
            }

            TutorialPopup::Render( _hasFocus );
        }
    }
};


class MoveFleetSouthPopup : public TutorialPopup
{
    void Render( bool _hasFocus )
    {
        if( g_app->GetTutorial()->InChapter( "M5BattleshipInfo") &&
            g_app->GetMapRenderer()->GetCurrentSelectionId() != -1 )
        {
            TutorialPopup::Render( _hasFocus );
        }
    }
};


class DestroyEnemyFleetPopup : public TutorialPopup
{
public:
    int m_fleetId;
    void Render( bool _hasFocus )
    {
        int shipsAvailable = g_app->GetWorld()->GetTeam(1)->m_unitsAvailable[WorldObject::TypeBattleShip] + 
                             g_app->GetWorld()->GetTeam(1)->m_unitsAvailable[WorldObject::TypeCarrier];

        bool inChapter = g_app->GetTutorial()->InChapter("M5DestroyEnemy") ||
                         g_app->GetTutorial()->InChapter("M5FleetInfo");

        Fleet *fleet = g_app->GetWorld()->GetTeam(0)->GetFleet(m_fleetId);

        if( shipsAvailable == 0 &&
            inChapter &&
            fleet &&
            fleet->m_fleetMembers.Size() > 0 )
        {
            TutorialPopup::Render( _hasFocus );
        }
    }
};


class ToggleOrdersPopup : public TutorialPopup
{
    void Render( bool _hasFocus )
    {
        if( g_app->GetTutorial()->InChapter("M5DestroyEnemy2") )
        {
            TutorialPopup::Render( _hasFocus );
        }
    }
};



// ============================================================================
// Mission 6 popups
// ============================================================================


class Mission6UnitPanelPopup : public UnitPanelPopup
{
    void Render( bool _hasFocus )
    {
        if( !g_app->GetTutorial()->InChapter("Mission6Start") )
        {
            UnitPanelPopup::Render( _hasFocus );
        }
    }
};


class ToggleTerritoryPopup : public TutorialPopup
{
    void Render( bool _hasFocus )
    {
        if( g_app->GetTutorial()->InChapter("Mission6Start") )
        {
            TutorialPopup::Render( _hasFocus );
        }
    }
};


class DeployCarriersPopup : public DeployFleetPopup
{
    void Render( bool _hasFocus )
    {
        if( g_app->GetTutorial()->InChapter( "Mission6SubIntro" ) )
        {
            DeployFleetPopup::Render( _hasFocus );
        }
    }
};


class AddSubToFleetPopup : public AddShipToFleetPopup
{
    void Render( bool _hasFocus )
    {
        if( g_app->GetTutorial()->InChapter( "Mission6PlaceSubs" ) )
        {
            m_unitType = WorldObject::TypeSub;
            AddShipToFleetPopup::Render( _hasFocus );
        }
    }
};


class DeploySubsPopup : public DeployFleetPopup
{
    void Render( bool _hasFocus )
    {
        if( g_app->GetTutorial()->InChapter( "Mission6PlaceSubs" ) )
        {
            DeployFleetPopup::Render( _hasFocus );
        }
    }
};


class SelectSubsPopup : public TutorialPopup
{
    void Render( bool _hasFocus )
    {
        if( g_app->GetTutorial()->InChapter( "M6SubInfo") &&
            g_app->GetMapRenderer()->GetCurrentSelectionId() == -1 )
        {
            //
            // Look for a target

            for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
            {
                if( g_app->GetWorld()->m_objects.ValidIndex(i) )
                {
                    WorldObject *wobj = g_app->GetWorld()->m_objects[i];
                    if( wobj->m_teamId == 1 &&
                        wobj->m_type == WorldObject::TypeSub )
                    {
                        Fleet *fleet = g_app->GetWorld()->GetTeam(1)->GetFleet(wobj->m_fleetId);
                        m_targetLongitude = fleet->m_longitude.DoubleValue();
                        m_targetLatitude = fleet->m_latitude.DoubleValue();
                    }
                }
            }

            TutorialPopup::Render( _hasFocus );
        }
    }
};


class MoveSubsToChinaPopup : public TutorialPopup
{
    void Render( bool _hasFocus )
    {
        if( g_app->GetMapRenderer()->GetCurrentSelectionId() != -1 )
        {
            if( g_app->GetTutorial()->InChapter( "M6SubInfo" ) ||
                g_app->GetTutorial()->InChapter( "M6MoveSubs" ) )
            {
                TutorialPopup::Render( _hasFocus );
            }
        }
    }
};


class SwitchToDepthChargeModePopup : public TutorialPopup
{
    void Render( bool _hasFocus )
    {
        if( g_app->GetTutorial()->InChapter("M6DestroySubs") )
        {
            int numInMode = 0;
            m_objectId = -1;

            for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
            {
                if( g_app->GetWorld()->m_objects.ValidIndex(i) )
                {
                    WorldObject *wobj = g_app->GetWorld()->m_objects[i];
                    if( wobj->m_type == WorldObject::TypeCarrier &&
                        wobj->m_teamId == 1 )
                    {
                        if( wobj->m_currentState != 2 &&
                            wobj->m_vel.Mag() == 0 )
                        {
                            m_objectId = wobj->m_objectId;
                        }

                        if( wobj->m_currentState == 2 )
                        {                        
                            numInMode++;
                        }
                    }
                }
            }

            if( numInMode < 3 && m_objectId != -1 )
            {
                TutorialPopup::Render( _hasFocus );
            }
        }
    }
};


class DestroyEnemySubsPopup : public TutorialPopup
{
public:
    int m_fleetId;

    void Render( bool _hasFocus )
    {
        if( g_app->GetTutorial()->InChapter("M6DestroySubs") )
        {
            int numInMode = 0;
            for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
            {
                if( g_app->GetWorld()->m_objects.ValidIndex(i) )
                {
                    WorldObject *wobj = g_app->GetWorld()->m_objects[i];
                    if( wobj->m_type == WorldObject::TypeCarrier &&
                        wobj->m_teamId == 1 &&
                        wobj->m_currentState == 2 )
                    {                        
                        numInMode++;
                    }
                }
            }

            if( numInMode >= 3 )
            {
                Fleet *fleet = g_app->GetWorld()->GetTeam(0)->GetFleet(m_fleetId);

                if( fleet &&
                    fleet->m_fleetMembers.Size() > 0 )
                {
                    TutorialPopup::Render( _hasFocus );
                }
            }
        }
    }
};


class SwitchSubsToNukeModePopup : public TutorialPopup
{
    void Render( bool _hasFocus )
    {
        if( g_app->GetTutorial()->InChapter("M6EnemyLaunch") ||
            g_app->GetTutorial()->InChapter("M6LaunchExplanation") )
        {
            int numInMode = 0;
            m_objectId = -1;

            for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
            {
                if( g_app->GetWorld()->m_objects.ValidIndex(i) )
                {
                    WorldObject *wobj = g_app->GetWorld()->m_objects[i];
                    if( wobj->m_type == WorldObject::TypeSub &&
                        wobj->m_teamId == 1 )
                    {
                        if( wobj->m_currentState != 2 )
                        {
                            m_objectId = wobj->m_objectId;
                        }

                        if( wobj->m_currentState == 2 )
                        {                        
                            numInMode++;
                        }
                    }
                }
            }

            if( m_objectId != -1 &&
                g_app->GetMapRenderer()->GetCurrentSelectionId() == -1 &&
                numInMode < 3 )
            {
                TutorialPopup::Render( _hasFocus );
            }
        }
    }
};


class SubNukeEnemySiloPopup : public TutorialPopup
{
    void Render( bool _hasFocus )
    {
        if( g_app->GetTutorial()->InChapter("M6EnemyLaunch") ||
            g_app->GetTutorial()->InChapter("M6LaunchExplanation") )
        {
            WorldObject *wobj = g_app->GetWorld()->GetWorldObject( g_app->GetMapRenderer()->GetCurrentSelectionId() );
            if( wobj && 
                wobj->m_type == WorldObject::TypeSub &&
                g_app->GetWorld()->GetWorldObject(m_objectId) )
            {
                TutorialPopup::Render( _hasFocus );
            }
        }
    }
};


// ============================================================================
// Mission 7 popups
// ============================================================================

class ShowTerritoryPopup : public TutorialPopup
{
    void Render( bool _hasFocus )
    {
        TutorialPopup::Render( _hasFocus );

        if( g_app->GetMapRenderer()->m_showAllTeams )
        {
            EclRemoveWindow( m_name );
        }
    }
};


class Mission7PlacementPopup : public TutorialPopup
{
public:
    int m_unitType;
    void Render( bool _hasFocus )
    {    
        PlacementIconWindow *window = (PlacementIconWindow *)EclGetWindow("Placement");
        if( window &&
            window->m_unitType == m_unitType )
        {
            int numAlreadyFound = 0;

            for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
            {
                if( g_app->GetWorld()->m_objects.ValidIndex(i) )
                {
                    WorldObject *wobj = g_app->GetWorld()->m_objects[i];
                    if( wobj->m_type == m_unitType &&
                        wobj->m_teamId == 1 )
                    {
                        Fixed distance = g_app->GetWorld()->GetDistance(wobj->m_longitude, wobj->m_latitude, 
																		Fixed::FromDouble(m_targetLongitude),
																		Fixed::FromDouble(m_targetLatitude) );
                        if( distance.DoubleValue() < m_distance )
                        {
                            ++numAlreadyFound;
                        }
                    }
                }
            }

            if( numAlreadyFound < 3 )
            {
                TutorialPopup::Render( _hasFocus );
            }
        }
    }
};


class PlaceFleetsInPacificPopup : public TutorialPopup
{
    void Render( bool _hasFocus )
    {
        SidePanel *sidePanel = (SidePanel *)EclGetWindow("Side Panel");
        
        if( sidePanel &&
            sidePanel->m_mode == SidePanel::ModeFleetPlacement &&
            EclGetWindow("Placement") )
        {
            TutorialPopup::Render( _hasFocus );
        }
    }
};


class Misssion7FleetsToChinaPopup : public TutorialPopup
{
    void Render( bool _hasFocus )
    {
        Team *team = g_app->GetWorld()->GetTeam(1);

        int unitsAvailable = team->m_unitsAvailable[WorldObject::TypeSilo] +
                            team->m_unitsAvailable[WorldObject::TypeRadarStation] + 
                            team->m_unitsAvailable[WorldObject::TypeAirBase] + 
                            team->m_unitsAvailable[WorldObject::TypeBattleShip] + 
                            team->m_unitsAvailable[WorldObject::TypeCarrier] + 
                            team->m_unitsAvailable[WorldObject::TypeSub];

        if( unitsAvailable > 0 &&
            g_app->GetWorld()->GetDefcon() > 3 )
        {
            return;
        }
        
        int numFleets = 0;
        int fleetsMoving = 0;

        
        for( int i = 0; i < team->m_fleets.Size(); ++i )
        {
            Fleet *fleet = team->m_fleets[i];

            Fixed distance = g_app->GetWorld()->GetDistance( fleet->m_longitude, fleet->m_latitude, Fixed(131), Fixed(23) );

            if( fleet && 
                fleet->m_fleetMembers.Size() > 0 &&
                distance > 20 )
            {
                numFleets++;
                
                for( int j = 0; j < fleet->m_fleetMembers.Size(); ++j )
                {
                    MovingObject *mobj = (MovingObject*)g_app->GetWorld()->GetWorldObject( fleet->m_fleetMembers[j] );
                    if( mobj && mobj->m_vel.Mag() > 0 )
                    {
                        fleetsMoving++;
                        break;
                    }
                }
            }
        }


        if( numFleets > 0 && fleetsMoving == 0 )
        {
            TutorialPopup::Render( _hasFocus );
        }
    }
};


class Mission7ScoutChinaPopup : public TutorialPopup
{
    void Render( bool _hasFocus )
    {
        bool found = false;

        Team *team = g_app->GetWorld()->GetTeam(1);

        for( int i = 0; i < team->m_fleets.Size(); ++i )
        {
            Fleet *fleet = team->m_fleets[i];
            if( fleet && 
                fleet->m_longitude > 0 &&
                fleet->m_teamId == 1 )
            {
                Fixed distance = g_app->GetWorld()->GetDistance( fleet->m_longitude, fleet->m_latitude, Fixed(131), Fixed(23) );
                if( distance < 10 )
                {
                    for( int j = 0; j < fleet->m_fleetMembers.Size(); ++j )
                    {
                        MovingObject *mobj = (MovingObject*)g_app->GetWorld()->GetWorldObject( fleet->m_fleetMembers[j] );
                        if( mobj && 
                            mobj->m_type == WorldObject::TypeCarrier )
                        {
                            int numTimesPermitted = mobj->m_states[0]->m_numTimesPermitted;
                            if( mobj->m_currentState == 0 ) numTimesPermitted -= mobj->m_actionQueue.Size();
                            
                            if( numTimesPermitted > 0 )
                            {
                                found = true;
                                break;
                            }
                        }
                    }
                }
            }
        }

        if( found )
        {
            TutorialPopup::Render( _hasFocus );
        }
    }
};


class Mission7EnemySubsPopup : public TutorialPopup
{
    void Render( bool _hasFocus )
    {
        Fleet *fleet = g_app->GetWorld()->GetTeam(0)->GetFleet(2);
        if( fleet &&
            fleet->m_fleetMembers.Size() > 0 )
        {
            int numUsingNukes = 0;
            for( int i = 0; i < fleet->m_fleetMembers.Size(); ++i )
            {
                WorldObject *wobj = g_app->GetWorld()->GetWorldObject( fleet->m_fleetMembers[i] );
                if( wobj &&
                    wobj->m_type == WorldObject::TypeSub &&
                    wobj->m_currentState == 2 )
                {
                    numUsingNukes++;
                }
            }


            if( numUsingNukes > 0 )
            {
                m_targetLongitude = fleet->m_longitude.DoubleValue();
                m_targetLatitude = fleet->m_latitude.DoubleValue();

                TutorialPopup::Render( _hasFocus );
            }
        }
    }
};


// ============================================================================


void Tutorial::InitialiseWorld()
{
    bool serverRunning = g_app->GetServer() && g_app->GetServer()->IsRunning();
    bool clientRunning = g_app->GetClientToServer() && g_app->GetClientToServer()->IsConnected();
    
    if( serverRunning && clientRunning )
    {        
        g_app->GetGame()->m_lockVictoryTimer = true;            
        g_app->GetGame()->GetOption("TerritoriesPerTeam")->m_currentValue = 1;
        g_app->GetGame()->GetOption("CitiesPerTerritory")->m_currentValue = 15;
        g_app->GetGame()->GetOption("PopulationPerTerritory")->m_currentValue = 100;
        g_app->GetGame()->GetOption("MaxSpectators")->m_currentValue = 0;
        
        g_app->GetWorld()->InitialiseTeam( 0, Team::TypeLocalPlayer, g_app->GetClientToServer()->m_clientId );
        g_app->GetWorld()->InitialiseTeam( 1, Team::TypeLocalPlayer, g_app->GetClientToServer()->m_clientId );

        Team *cpuTeam = g_app->GetWorld()->GetTeam(0);
        Team *playerTeam = g_app->GetWorld()->GetTeam(1);

        AppAssert(cpuTeam);
        AppAssert(playerTeam);

        cpuTeam->SetTeamName( LANGUAGEPHRASE("tutorial_team_name_cpu") );
        cpuTeam->m_readyToStart = true;

        playerTeam->SetTeamName( LANGUAGEPHRASE("tutorial_team_name_player") );
        playerTeam->m_readyToStart = true;

        m_worldInitialised = true;
    }
}


void Tutorial::SetupCurrentChapter()
{
#ifndef NON_PLAYABLE
    //
    // Run generic code for the chapter

    if( IsNextClickable() )
    {
        if( !EclGetWindow("clicknextpopup") )
        {
            ClickNextPopup *popup = new ClickNextPopup();
            popup->SetMessage( "clicknextpopup", "tutorial_clicknext_popup" );
            popup->SetDimensions( 90, 170, 0 );
            popup->SetUITarget( "Tutorial", "Next" );
            EclRegisterWindow(popup);            
        }
    }


    //
    // Run specific code for the chapter

    if( InChapter("Start") ) // start of mission 1
    {       
        SetCurrentLevelName( "tutorial_levelname_1" );

        g_app->GetGame()->m_lockVictoryTimer = true;
        EclRemoveWindow("Side Panel");
        g_app->GetMapRenderer()->LockRadarRenderer();

        g_app->GetWorld()->ClearWorld();
        g_app->GetWorld()->GetTeam(0)->m_territories.PutData( World::TerritoryRussia );
        g_app->GetWorld()->GetTeam(1)->m_territories.PutData( World::TerritoryEurope );
        g_app->GetWorld()->AssignCities();
        g_app->GetWorld()->GetTeam(1)->m_unitsAvailable.SetAll(0);

        g_app->GetClientToServer()->RequestGameSpeed( 0, GAMESPEED_FAST );
        g_app->GetClientToServer()->RequestGameSpeed( 1, GAMESPEED_FAST );

        m_objectIds[0] = ObjectPlacement( 1, WorldObject::TypeSilo, 18.0f, 52.0f, 255 );
        m_objectIds[1] = ObjectPlacement( 0, WorldObject::TypeRadarStation, 42.0f, 54.0f, 255 );
        m_objectIds[2] = ObjectPlacement( 0, WorldObject::TypeRadarStation, 41.0f, 45.0f, 255 );

        ObjectPlacement( 1, WorldObject::TypeRadarStation, 27.0f, 65.0f, 255 );
        ObjectPlacement( 1, WorldObject::TypeRadarStation, 25.0f, 45.0f, 255 );

        Mission1SiloPopup *popup = new Mission1SiloPopup();
        popup->SetMessage("silo", "tutorial_silofocus_popup1");
        popup->SetDimensions( 300, 130, 50 );
        popup->SetTargetObject( m_objectIds[0] );
        EclRegisterWindow(popup);

        Mission1SiloSelectPopup *siloSelect = new Mission1SiloSelectPopup();
        siloSelect->SetMessage("siloselect", "tutorial_silofocus_selectpopup" );
        siloSelect->SetDimensions( 300, 130, 50 );
        siloSelect->SetTargetObject( m_objectIds[0] );
        EclRegisterWindow(siloSelect);

        Mission1Deselect *deselect = new Mission1Deselect();
        deselect->SetMessage("deselect", "tutorial_mission1deselect_popup" );
        deselect->SetDimensions( 300, 130, 50 );
        deselect->SetTargetObject( m_objectIds[0] );
        EclRegisterWindow( deselect );
        
        Mission1RadarTarget *radarTarget1 = new Mission1RadarTarget();
        radarTarget1->SetMessage("radar1", "tutorial_mission1destroyradar_popup" );
        radarTarget1->SetDimensions( 90, 150, 50 );
        radarTarget1->SetTargetObject( m_objectIds[1] );
        EclRegisterWindow(radarTarget1);

        Mission1RadarTarget *radarTarget2 = new Mission1RadarTarget();
        radarTarget2->SetMessage("radar2", "tutorial_mission1destroyradar_popup" );
        radarTarget2->SetDimensions( 90, 150, 50 );
        radarTarget2->SetTargetObject( m_objectIds[2] );
        EclRegisterWindow(radarTarget2);

        g_app->GetWorld()->m_theDate.AdvanceTime(1800);

        g_app->GetWorld()->GetTeam(0)->m_type = Team::TypeLocalPlayer;
    }

    if( InChapter("SiloFocus") )
    {        
        //g_app->GetMapRenderer()->CenterViewport( m_objectIds[0], 10 );
    }

    if( InChapter("Mission1DestroyRadar" ) )
    {
    }


    if( InChapter("Mission2Start") )
    {
        SetCurrentLevelName( "tutorial_levelname_2" );

        g_app->GetGame()->m_lockVictoryTimer = true;
        g_app->GetMapRenderer()->LockRadarRenderer();
        
        g_app->GetWorld()->ClearWorld();
        g_app->GetWorld()->GetTeam(0)->m_territories.PutData( World::TerritoryRussia );
        g_app->GetWorld()->GetTeam(1)->m_territories.PutData( World::TerritoryEurope );
        g_app->GetWorld()->AssignCities();

        g_app->GetWorld()->GetTeam(1)->m_unitsAvailable.SetAll(0);
        g_app->GetWorld()->GetTeam(1)->m_unitsAvailable[WorldObject::TypeSilo] = 4;

        g_app->GetClientToServer()->RequestGameSpeed( 0, GAMESPEED_FAST );
        g_app->GetClientToServer()->RequestGameSpeed( 1, GAMESPEED_SLOW );

        int id = ObjectPlacement( 1, WorldObject::TypeRadarStation, 10.0f, 50.0f, 255 );
        WorldObject *obj = g_app->GetWorld()->GetWorldObject( id );
        obj->m_states[0]->m_radarRange = 180;

        //g_app->GetMapRenderer()->CenterViewport( id, 10 );        
        g_app->GetWorld()->GetTeam(0)->m_type = Team::TypeLocalPlayer;
        

        UnitPanelPopup *unitPopup = new UnitPanelPopup();
        unitPopup->SetMessage( "unit", "tutorial_unitpanel_popup" );
        unitPopup->SetDimensions( 340, 70, 0 );
        unitPopup->SetUITarget( "Toolbar", "Placement" );
        EclRegisterWindow(unitPopup);

        SelectSiloPopup *selectSilo = new SelectSiloPopup();        
        selectSilo->SetMessage( "selectsilo", "tutorial_silo_popup" );
        selectSilo->SetDimensions( 90, 160, 0 );
        selectSilo->SetUITarget( "Side Panel", "Silo" );
        EclRegisterWindow(selectSilo);
        
        PlaceSilosPopup *placeSilos = new PlaceSilosPopup();
        placeSilos->SetMessage( "placesilo", "tutorial_silo_place" );
        placeSilos->SetDimensions( 130, 200, 70 );
        placeSilos->SetTargetWorldPos( 11.3, 50.6 );
        EclRegisterWindow(placeSilos);
    }

    if( InChapter("M2DestroyNukes" ) )
    {
        g_app->GetWorld()->m_theDate.AdvanceTime(2500);
        g_app->GetClientToServer()->RequestGameSpeed( 1, GAMESPEED_FAST );
        
        int launchId = ObjectPlacement( 0, WorldObject::TypeSub, -37.0f, 44.5f, 255 );

        Fixed nyLongitude = 0;
        Fixed nyLatitude = 0;
        Fixed chLongitude = 0;
        Fixed chLatitude = 0;
        bool radarSet = false;
        for( int i = 0; i < g_app->GetWorld()->m_cities.Size(); ++i )
        {
            City *city = g_app->GetWorld()->m_cities[i];
            if( strcmp( city->m_name, "LONDON" ) == 0 )
            {
                nyLongitude = city->m_longitude;
                nyLatitude = city->m_latitude;
            }

            if( strcmp( city->m_name, "BERLIN" ) == 0 )
            {
                chLongitude = city->m_longitude;
                chLatitude = city->m_latitude;
            }
        }

        g_app->GetWorld()->LaunchNuke(0, launchId, nyLongitude, nyLatitude, 0 );
        g_app->GetWorld()->LaunchNuke(0, launchId, chLongitude, chLatitude, 0 );
    }


    if( InChapter("Mission3Start") )
    {
        SetCurrentLevelName( "tutorial_levelname_3" );

        g_app->GetGame()->m_lockVictoryTimer = true;
        g_app->GetMapRenderer()->UnlockRadarRenderer();
        g_app->GetWorld()->ClearWorld();        
        g_app->GetWorld()->GetTeam(0)->m_territories.PutData( World::TerritoryRussia );
        g_app->GetWorld()->GetTeam(1)->m_territories.PutData( World::TerritoryEurope );
        g_app->GetWorld()->AssignCities();
        g_app->GetWorld()->GetTeam(1)->m_unitsAvailable.SetAll(0);
        g_app->GetWorld()->GetTeam(1)->m_unitsAvailable[WorldObject::TypeSilo] = 4;
        g_app->GetWorld()->GetTeam(1)->m_unitsAvailable[WorldObject::TypeRadarStation] = 3;

        m_objectIds[0] = ObjectPlacement(0, WorldObject::TypeSilo, 42.0f, 54.0f, 255 );
        m_objectIds[1] = ObjectPlacement(0, WorldObject::TypeAirBase, 45.0f, 48.0f, 255 );
        m_objectIds[2] = ObjectPlacement(0, WorldObject::TypeAirBase, 40.0f, 61.0f, 255 );
        g_app->GetMapRenderer()->CenterViewport( m_objectIds[2], 10 );

        g_app->GetClientToServer()->RequestGameSpeed( 0, GAMESPEED_FAST );
        g_app->GetClientToServer()->RequestGameSpeed( 1, GAMESPEED_REALTIME );
        
        g_app->GetWorld()->GetTeam(0)->m_type = Team::TypeLocalPlayer;

        g_app->GetWorld()->m_theDate.AdvanceTime( 360 );
        
        UnitPanelPopup *unitPopup = new UnitPanelPopup();
        unitPopup->SetMessage( "unit", "tutorial_unitpanel_popup" );
        unitPopup->SetDimensions( 340, 70, 0 );
        unitPopup->SetUITarget( "Toolbar", "Placement" );
        EclRegisterWindow(unitPopup);

        SelectRadarPopup *selectRadar = new SelectRadarPopup();
        selectRadar->SetMessage("selectradar", "tutorial_radar_popup" );
        selectRadar->SetDimensions( 90, 160, 0 );
        selectRadar->SetUITarget( "Side Panel", "Radar" );
        EclRegisterWindow( selectRadar );

        PlaceRadarPopup *placeRadar = new PlaceRadarPopup();
        placeRadar->SetMessage("placeradar", "tutorial_radar_place" );
        placeRadar->SetDimensions( 170, 200, 60 );
        placeRadar->SetTargetWorldPos( 27, 48 );
        EclRegisterWindow( placeRadar );

        Mission3SelectSilosPopup *selectSilo = new Mission3SelectSilosPopup();
        selectSilo->SetMessage( "selectsilo", "tutorial_silo_popup" );
        selectSilo->SetDimensions( 90, 160, 0 );
        selectSilo->SetUITarget( "Side Panel", "Silo" );
        EclRegisterWindow(selectSilo);

        Mission3PlaceSilosPopup *placeSilos = new Mission3PlaceSilosPopup();
        placeSilos->SetMessage( "placesilo", "tutorial_silo_place" );
        placeSilos->SetDimensions( 130, 200, 60 );
        placeSilos->SetTargetWorldPos( 12, 50 );
        EclRegisterWindow(placeSilos);

        AccelTimePopup *accelTime = new AccelTimePopup();
        accelTime->SetMessage( "acceltime", "tutorial_acceltime_popup" );
        accelTime->SetDimensions( 100, 300, 0 );
        accelTime->SetUITarget( LANGUAGEPHRASE("dialog_world_status"), "non" );
        EclRegisterWindow(accelTime);

        SilosIntoNukeModePopup *silosNuke = new SilosIntoNukeModePopup();
        silosNuke->SetMessage( "silosnuke", "tutorial_silosnuke_popup" );
        silosNuke->SetDimensions( 300, 200, 20 );
        silosNuke->SetTargetWorldPos( 12, 50 );
        EclRegisterWindow( silosNuke );

        NukeEnemySiloPopup *nukeEnemySilo = new NukeEnemySiloPopup();
        nukeEnemySilo->SetMessage( "nukeenemysilo", "tutorial_nukenemeysilo_popup" );
        nukeEnemySilo->SetDimensions( 90, 200, 50 );
        nukeEnemySilo->SetTargetObject( m_objectIds[0] );
        EclRegisterWindow(nukeEnemySilo);

        NukeEnemyAirBasePopup *nukeAirbase1 = new NukeEnemyAirBasePopup();
        nukeAirbase1->SetMessage( "nukeairbase1", "tutorial_nukeenemyairbase_popup" );
        nukeAirbase1->SetDimensions( 130, 100, 50 );
        nukeAirbase1->SetTargetObject( m_objectIds[1] );
        EclRegisterWindow(nukeAirbase1);

        NukeEnemyAirBasePopup *nukeAirbase2 = new NukeEnemyAirBasePopup();
        nukeAirbase2->SetMessage( "nukeairbase2", "tutorial_nukeenemyairbase_popup" );
        nukeAirbase2->SetDimensions( 50, 100, 50 );
        nukeAirbase2->SetTargetObject( m_objectIds[2] );
        EclRegisterWindow(nukeAirbase2);

        NukeEnemyCitiesPopup *nukeCities = new NukeEnemyCitiesPopup();
        nukeCities->SetMessage( "nukecities", "tutorial_nukenemeycities_popup" );
        nukeCities->SetDimensions( 50, 200, 50 );
        nukeCities->SetTargetWorldPos( 42.3, 56.8 );
        EclRegisterWindow( nukeCities );
    }

    if( InChapter("M3SiloPlacement") )
    {
        if( g_app->GetWorld()->GetDefcon() <= 2 )
        {
            g_app->GetWorld()->m_theDate.ResetClock();
            g_app->GetWorld()->m_theDate.AdvanceTime( 360 );
        }
    }

    if( InChapter("M3GameSpeed" ) )
    {
        g_app->GetClientToServer()->RequestGameSpeed( 0, GAMESPEED_FAST );
    }

    if( InChapter("M3Defcon1") )
    {
        if( g_app->GetWorld()->GetDefcon() > 2 )
        {
            g_app->GetWorld()->m_theDate.AdvanceTime( Fixed::FromDouble(max(0.0f, 800.0f - g_app->GetWorld()->m_theDate.GetSeconds())) );
        }
    }

    if( InChapter("Mission4Start") )
    {
        SetCurrentLevelName( "tutorial_levelname_4" );

        g_app->GetGame()->m_lockVictoryTimer = true;
        g_app->GetWorld()->ClearWorld();
        g_app->GetMapRenderer()->UnlockRadarRenderer();
        
        g_app->GetWorld()->GetTeam(0)->m_territories.PutData( World::TerritoryRussia );
        g_app->GetWorld()->GetTeam(1)->m_territories.PutData( World::TerritoryEurope );
        g_app->GetWorld()->AssignCities();

        g_app->GetWorld()->GetTeam(1)->m_unitsAvailable.SetAll(0);
        g_app->GetWorld()->GetTeam(1)->m_unitsAvailable[WorldObject::TypeAirBase] = 3;

        m_objectIds[0] = ObjectPlacement(0, WorldObject::TypeRadarStation, 42.0f, 54.0f, 255 );
        m_objectIds[1] = ObjectPlacement(0, WorldObject::TypeAirBase, 51.0f, 60.0f, 255 );
        m_objectIds[2] = ObjectPlacement(0, WorldObject::TypeAirBase, 54.0f, 54.0f, 255 );
        g_app->GetMapRenderer()->CenterViewport( m_objectIds[2], 10 );

        g_app->GetClientToServer()->RequestGameSpeed( 0, GAMESPEED_FAST );
        g_app->GetClientToServer()->RequestGameSpeed( 1, GAMESPEED_SLOW );

        g_app->GetWorld()->GetTeam(0)->m_type = Team::TypeLocalPlayer;

        UnitPanelPopup *unitPopup = new UnitPanelPopup();
        unitPopup->SetMessage( "unit", "tutorial_unitpanel_popup" );
        unitPopup->SetDimensions( 340, 70, 0 );
        unitPopup->SetUITarget( "Toolbar", "Placement" );
        EclRegisterWindow(unitPopup);

        SelectAirBasePopup *selectAirbase = new SelectAirBasePopup();
        selectAirbase->SetMessage( "selectairbase", "tutorial_selectairbase_popup" );
        selectAirbase->SetDimensions( 90, 160, 0 );
        selectAirbase->SetUITarget( "Side Panel", "AirBase" );
        EclRegisterWindow( selectAirbase );

        PlaceAirBasePopup *placeAirbase = new PlaceAirBasePopup();
        placeAirbase->SetMessage( "placeairbase", "tutorial_placeairbase_popup" );
        placeAirbase->SetDimensions( 170, 200, 60 );
        placeAirbase->SetTargetWorldPos( 27, 48 );
        EclRegisterWindow( placeAirbase );

        ClickOnAirbasePopup *clickonAirbase = new ClickOnAirbasePopup();
        clickonAirbase->SetMessage( "clickairbase", "tutorial_clickairbase_popup" );
        clickonAirbase->SetDimensions( 270, 200, 50 );
        clickonAirbase->SetTargetObject( -1 );
        EclRegisterWindow( clickonAirbase );

        ScoutFightersHerePopup *scoutHere = new ScoutFightersHerePopup();
        scoutHere->SetMessage( "scouthere", "tutorial_scouthere_popup" );
        scoutHere->SetDimensions( 180, 100, 60 );
        scoutHere->SetTargetWorldPos( 53, 57 );
        EclRegisterWindow( scoutHere );

        ToggleRadarPopup *radar = new ToggleRadarPopup();
        radar->SetMessage( "toggleradar", "tutorial_radartoolbar_popup" );
        radar->SetDimensions( 0, 70, 0 );
        radar->SetUITarget( "Toolbar", "Radar" );
        EclRegisterWindow( radar );

        SwitchToBomberModePopup *switchToBombers = new SwitchToBomberModePopup();
        switchToBombers->SetMessage( "switchtobombers", "tutorial_switchtobombers_popup" );
        switchToBombers->SetDimensions( 270, 200, 50 );
        switchToBombers->SetTargetObject(-1);
        EclRegisterWindow(switchToBombers);

        ClickOnAirbaseBomberPopup *clickOnAirbaseBomber = new ClickOnAirbaseBomberPopup();
        clickOnAirbaseBomber->SetMessage( "clickonairbasebomber", "tutorial_clickairbasebomber_popup" );
        clickOnAirbaseBomber->SetDimensions( 270, 200, 50 );
        clickOnAirbaseBomber->SetTargetObject(-1);
        EclRegisterWindow( clickOnAirbaseBomber );
        
        NukeEnemyBuildingPopup *nukeRadar = new NukeEnemyBuildingPopup();
        nukeRadar->SetMessage( "nukeradar", "tutorial_nukebuilding_popup" );
        nukeRadar->SetDimensions( 140, 110, 50 );
        nukeRadar->SetTargetObject( m_objectIds[0] );
        EclRegisterWindow( nukeRadar );

        NukeEnemyBuildingPopup *nukeRadar2 = new NukeEnemyBuildingPopup();
        nukeRadar2->SetMessage( "nukeradar2", "tutorial_nukebuilding_popup" );
        nukeRadar2->SetDimensions( 60, 150, 50 );
        nukeRadar2->SetTargetObject( m_objectIds[1] );
        EclRegisterWindow( nukeRadar2 );

        NukeEnemyBuildingPopup *nukeRadar3 = new NukeEnemyBuildingPopup();
        nukeRadar3->SetMessage( "nukeradar3", "tutorial_nukebuilding_popup" );
        nukeRadar3->SetDimensions( 90, 150, 50 );
        nukeRadar3->SetTargetObject( m_objectIds[2] );
        EclRegisterWindow( nukeRadar3 );
    }

    if( InChapter("M4LaunchFighters") )
    {
        g_app->GetWorld()->m_theDate.AdvanceTime( 600 );
    }

    if( InChapter("M4LaunchBombers") )
    {
        g_app->GetMapRenderer()->m_showRadar = false;
    }

    if( InChapter("Mission5Start") )
    {
        SetCurrentLevelName( "tutorial_levelname_5" );

        g_app->GetGame()->m_lockVictoryTimer = true;
        EclRemoveWindow("Side Panel");
        g_app->GetWorld()->ClearWorld();
        g_app->GetMapRenderer()->UnlockRadarRenderer();

        g_app->GetWorld()->GetTeam(1)->m_territories.PutData( World::TerritoryEurope );
        g_app->GetWorld()->GetTeam(0)->m_territories.PutData( World::TerritoryRussia );
        g_app->GetWorld()->AssignCities();

        g_app->GetWorld()->GetTeam(1)->m_unitsAvailable.SetAll(0);
        g_app->GetWorld()->GetTeam(1)->m_unitsAvailable[WorldObject::TypeBattleShip] = 6;
        g_app->GetWorld()->GetTeam(1)->m_unitsAvailable[WorldObject::TypeCarrier] = 6;

        g_app->GetMapRenderer()->CenterViewport( -20.0f, 40.0f, 10 );

        g_app->GetWorld()->GetTeam(0)->CreateFleet();
        ObjectPlacement(0, WorldObject::TypeBattleShip, -28.0f, 5.0f, 0 );
        ObjectPlacement(0, WorldObject::TypeBattleShip, -32.0f, 9.0f, 0 );
        ObjectPlacement(0, WorldObject::TypeCarrier, -26.0f, 6.0f, 0 );

        g_app->GetWorld()->GetTeam(0)->CreateFleet();
        ObjectPlacement(0, WorldObject::TypeCarrier, -61.0f, 32.0f, 1 );
        ObjectPlacement(0, WorldObject::TypeBattleShip, -66.0f, 33.0f, 1 );
        ObjectPlacement(0, WorldObject::TypeCarrier, -64.0f, 36.0f, 1 );
                
        g_app->GetClientToServer()->RequestGameSpeed( 0, GAMESPEED_FAST );
        g_app->GetClientToServer()->RequestGameSpeed( 1, GAMESPEED_SLOW );

        g_app->GetWorld()->GetTeam(0)->m_type = Team::TypeLocalPlayer;

        UnitPanelPopup *unitPopup = new UnitPanelPopup();
        unitPopup->SetMessage( "unit", "tutorial_unitpanel_popup" );
        unitPopup->SetDimensions( 340, 70, 0 );
        unitPopup->SetUITarget( "Toolbar", "Placement" );
        EclRegisterWindow(unitPopup);

        SelectFleetPopup *selectFleet = new SelectFleetPopup();
        selectFleet->SetMessage( "selectfleet", "tutorial_selectfleet_popup" );
        selectFleet->SetDimensions( 90, 160, 0 );
        selectFleet->SetUITarget( "Side Panel", "FleetMode" );
        EclRegisterWindow( selectFleet );

        AddShipToFleetPopup *addBattleship = new AddShipToFleetPopup();
        addBattleship->SetMessage( "addbattleship", "tutorial_addbattleship_popup" );
        addBattleship->SetDimensions( 45, 150, 0 );
        addBattleship->SetUITarget( "Side Panel", "BattleShip" );
        addBattleship->m_unitType = WorldObject::TypeBattleShip;
        EclRegisterWindow( addBattleship );

        AddShipToFleetPopup *addCarrier = new AddShipToFleetPopup();
        addCarrier->SetMessage( "addcarrier", "tutorial_addcarrier_popup" );
        addCarrier->SetDimensions( 130, 150, 0 );
        addCarrier->SetUITarget( "Side Panel", "Carrier" );
        addCarrier->m_unitType = WorldObject::TypeCarrier;
        EclRegisterWindow( addCarrier );

        DeployFleetPopup *deployFleet = new DeployFleetPopup();
        deployFleet->SetMessage( "deployfleet", "tutorial_deployfleet_popup" );
        deployFleet->SetDimensions( 70, 150, 70 );
        deployFleet->SetTargetWorldPos( -25, 47 );
        EclRegisterWindow( deployFleet );

        SelectDeployedFleetPopup *selectDeployed = new SelectDeployedFleetPopup();
        selectDeployed->SetMessage( "selectdeployedfleed", "tutorial_selectdeployedfleet_popup" );
        selectDeployed->SetDimensions( 70, 150, 30 );
        selectDeployed->SetTargetWorldPos(0,0);
        EclRegisterWindow( selectDeployed );

        MoveFleetSouthPopup *moveFleet = new MoveFleetSouthPopup();
        moveFleet->SetMessage( "movefleetsouth", "tutorial_movefleet_popup" );
        moveFleet->SetDimensions( 100, 150, 40 );
        moveFleet->SetTargetWorldPos( -29.5, 17 );
        EclRegisterWindow( moveFleet );

        DestroyEnemyFleetPopup *destroyFleet1 = new DestroyEnemyFleetPopup();
        destroyFleet1->SetMessage( "destroyfleet1", "tutorial_destroyfleet_popup" );
        destroyFleet1->SetDimensions( 130, 150, 50 );
        destroyFleet1->SetTargetWorldPos( -28, 5 );
        destroyFleet1->m_fleetId = 0;
        EclRegisterWindow( destroyFleet1 );

        DestroyEnemyFleetPopup *destroyFleet2 = new DestroyEnemyFleetPopup();
        destroyFleet2->SetMessage( "destroyfleet2", "tutorial_destroyfleet_popup" );
        destroyFleet2->SetDimensions( 350, 150, 50 );
        destroyFleet2->SetTargetWorldPos( -63, 34 );
        destroyFleet2->m_fleetId = 1;
        EclRegisterWindow( destroyFleet2 );

        ToggleOrdersPopup *orders = new ToggleOrdersPopup();
        orders->SetMessage( "toggleradar", "tutorial_orderstoolbar_popup" );
        orders->SetDimensions( 0, 70, 0 );
        orders->SetUITarget( "Toolbar", "Orders" );
        EclRegisterWindow( orders );
    }

    if( InChapter("M5BattleshipInfo") )
    {
        g_app->GetWorld()->m_theDate.AdvanceTime( 720 );
    }

    if( InChapter("Mission6Start") )
    {
        SetCurrentLevelName( "tutorial_levelname_6" );

        g_app->GetGame()->m_lockVictoryTimer = true;

        EclRemoveWindow("Side Panel");
        g_app->GetWorld()->ClearWorld();
        g_app->GetMapRenderer()->UnlockRadarRenderer();

        g_app->GetWorld()->GetTeam(0)->m_territories.PutData( World::TerritorySouthAsia );
        g_app->GetWorld()->GetTeam(1)->m_territories.PutData( World::TerritoryNorthAmerica );
        g_app->GetWorld()->AssignCities();

        g_app->GetWorld()->GetTeam(1)->m_unitsAvailable.SetAll(0);
        g_app->GetWorld()->GetTeam(1)->m_unitsAvailable[WorldObject::TypeCarrier] = 6;
        g_app->GetWorld()->GetTeam(1)->m_unitsAvailable[WorldObject::TypeSub] = 6;

        g_app->GetMapRenderer()->CenterViewport( -150.0f, 45.0f, 10 );

        m_objectIds[0] = ObjectPlacement( 0, WorldObject::TypeSilo, 123.0f, 46.0f, 255 );
        m_objectIds[1] = ObjectPlacement( 0, WorldObject::TypeSilo, 104.0f, 29.0f, 255 );
        
        g_app->GetWorld()->GetTeam(0)->CreateFleet();
        ObjectPlacement( 0, WorldObject::TypeSub, -147.0f, 41.0f, 0 );
        ObjectPlacement( 0, WorldObject::TypeSub, -152.0f, 42.0f, 0 );
        ObjectPlacement( 0, WorldObject::TypeSub, -150.0f, 45.0f, 0 );
        
        g_app->GetWorld()->GetTeam(0)->CreateFleet();
        ObjectPlacement( 0, WorldObject::TypeSub, -127.0f, 14.0f, 1 );
        ObjectPlacement( 0, WorldObject::TypeSub, -133.0f, 15.0f, 1 );
        ObjectPlacement( 0, WorldObject::TypeSub, -130.0f, 19.0f, 1 );                             

        g_app->GetWorld()->GetTeam(0)->m_type = Team::TypeLocalPlayer;

        g_app->GetClientToServer()->RequestGameSpeed( 0, GAMESPEED_FAST );
        g_app->GetClientToServer()->RequestGameSpeed( 1, GAMESPEED_SLOW );

        ShowTerritoryPopup *showTerritories = new ShowTerritoryPopup();
        showTerritories->SetMessage( "showterritories", "tutorial_territorytoolbar_popup" );
        showTerritories->SetDimensions( 0, 70, 0 );
        showTerritories->SetUITarget( "Toolbar", "Territory" );
        EclRegisterWindow( showTerritories );

        Mission6UnitPanelPopup *unitPopup = new Mission6UnitPanelPopup();
        unitPopup->SetMessage( "unit", "tutorial_unitpanel_popup" );
        unitPopup->SetDimensions( 340, 70, 0 );
        unitPopup->SetUITarget( "Toolbar", "Placement" );
        EclRegisterWindow(unitPopup);

        SelectFleetPopup *selectFleet = new SelectFleetPopup();
        selectFleet->SetMessage( "selectfleet", "tutorial_selectfleet_popup" );
        selectFleet->SetDimensions( 90, 160, 0 );
        selectFleet->SetUITarget( "Side Panel", "FleetMode" );
        EclRegisterWindow( selectFleet );

        AddShipToFleetPopup *addCarrier = new AddShipToFleetPopup();
        addCarrier->SetMessage( "addcarrier", "tutorial_addcarrier_popup" );
        addCarrier->SetDimensions( 130, 150, 0 );
        addCarrier->SetUITarget( "Side Panel", "Carrier" );
        addCarrier->m_unitType = WorldObject::TypeCarrier;
        EclRegisterWindow( addCarrier );

        DeployCarriersPopup *deployFleet = new DeployCarriersPopup();
        deployFleet->SetMessage( "deployfleet", "tutorial_deployfleet_popup" );
        deployFleet->SetDimensions( 70, 150, 70 );
        deployFleet->SetTargetWorldPos( -149, 39 );
        EclRegisterWindow( deployFleet );
        
        AddSubToFleetPopup *addSub = new AddSubToFleetPopup();
        addSub->SetMessage( "addsub", "tutorial_addsub_popup" );
        addSub->SetDimensions( 50, 150, 0 );
        addSub->SetUITarget( "Side Panel", "Sub" );
        EclRegisterWindow( addSub );

        DeploySubsPopup *deploySubs = new DeploySubsPopup();
        deploySubs->SetMessage( "deploysubs", "tutorial_deployfleet_popup" );
        deploySubs->SetDimensions( 70, 150, 70 );
        deploySubs->SetTargetWorldPos( -167, 29 );
        EclRegisterWindow( deploySubs );

        SelectSubsPopup *selectSubs = new SelectSubsPopup();
        selectSubs->SetMessage( "selectsubs", "tutorial_selectdeployedfleet_popup" );
        selectSubs->SetDimensions( 70, 150, 30 );
        selectSubs->SetTargetWorldPos(0,0);
        EclRegisterWindow( selectSubs );

        MoveSubsToChinaPopup *moveSubs = new MoveSubsToChinaPopup();
        moveSubs->SetMessage( "movesubs", "tutorial_movesubs_popup" );
        moveSubs->SetDimensions( 180, 150, 50 );
        moveSubs->SetTargetWorldPos( 137, 23 );
        EclRegisterWindow( moveSubs );

        SwitchToDepthChargeModePopup *switchToDepth = new SwitchToDepthChargeModePopup();
        switchToDepth->SetMessage( "switchtodepth", "tutorial_switchtodepth_popup" );
        switchToDepth->SetDimensions( 0, 150, 60 );
        switchToDepth->SetTargetObject(-1);
        EclRegisterWindow( switchToDepth );

        DestroyEnemySubsPopup *destroySubs1 = new DestroyEnemySubsPopup();
        destroySubs1->SetMessage( "destroysubs1", "tutorial_destroysubs_popup" );
        destroySubs1->SetDimensions( 300, 150, 40 );
        destroySubs1->SetTargetWorldPos( -150, 43 );
        destroySubs1->m_fleetId = 0;
        EclRegisterWindow( destroySubs1 );

        DestroyEnemySubsPopup *destroySubs2 = new DestroyEnemySubsPopup();
        destroySubs2->SetMessage( "destroysubs2", "tutorial_destroysubs_popup" );
        destroySubs2->SetDimensions( 70, 150, 40 );
        destroySubs2->SetTargetWorldPos( -130, 16 );
        destroySubs2->m_fleetId = 1;
        EclRegisterWindow( destroySubs2 );

        SwitchSubsToNukeModePopup *subsToNuke = new SwitchSubsToNukeModePopup();
        subsToNuke->SetMessage( "substonuke", "tutorial_substonuke_popup" );
        subsToNuke->SetDimensions( 0, 150, 60 );
        subsToNuke->SetTargetObject(-1);
        EclRegisterWindow(subsToNuke);

        SubNukeEnemySiloPopup *nukeSilo1 = new SubNukeEnemySiloPopup();
        nukeSilo1->SetMessage( "nukesilo1", "tutorial_nukesilo_popup" );
        nukeSilo1->SetDimensions( 300, 150, 50 );
        nukeSilo1->SetTargetObject( m_objectIds[0] );
        EclRegisterWindow(nukeSilo1);

        SubNukeEnemySiloPopup *nukeSilo2 = new SubNukeEnemySiloPopup();
        nukeSilo2->SetMessage( "nukesilo2", "tutorial_nukesilo_popup" );
        nukeSilo2->SetDimensions( 230, 150, 50 );
        nukeSilo2->SetTargetObject( m_objectIds[1] );
        EclRegisterWindow(nukeSilo2);
    }

    if( InChapter("M6DestroySubs" ) )
    {
        g_app->GetWorld()->m_theDate.AdvanceTime( 360 );
    }


    if( InChapter("M6EnemyLaunch") )
    {
        g_app->GetGame()->m_lockVictoryTimer = false;
        g_app->GetClientToServer()->RequestGameSpeed( 1, GAMESPEED_SLOW );

        for( int j = 0; j < g_app->GetWorld()->m_objects.Size(); ++j )
        {
            if( g_app->GetWorld()->m_objects.ValidIndex(j) )
            {
                WorldObject *obj = g_app->GetWorld()->m_objects[j];
                if( obj &&
                    obj->m_type == WorldObject::TypeSilo &&
                    obj->m_teamId == 0 )
                {
                    if( obj->m_currentState != 0 )
                    {
                        obj->SetState(0);
                        obj->m_stateTimer = 0;
                    }

                    for( int i = 0; i < 10; ++i )
                    {
                        Fixed longitude = 0;
                        Fixed latitude = 0;
                        Nuke::FindTarget(0, 1, obj->m_objectId, 360, &longitude, &latitude );

                        ActionOrder *action = new ActionOrder();
                        action->m_longitude = longitude;
                        action->m_latitude = latitude;
                        obj->RequestAction( action );
                    }
                }
            }
        }

        g_app->GetWorld()->m_theDate.AdvanceTime( 120 );
    }

    if( InChapter("Mission7Start") )
    {
        SetCurrentLevelName( "tutorial_levelname_7" );

        EclRemoveWindow("Side Panel");
        g_app->GetWorld()->ClearWorld();
        g_app->GetMapRenderer()->UnlockRadarRenderer();
        
#ifdef _DEBUG
        g_app->GetGame()->SetOptionValue( "TeamSwitching", 1 );
#endif

        g_app->GetGame()->GetOption("CitiesPerTerritory")->m_currentValue = 25;
        g_app->GetGame()->GetOption("PopulationPerTerritory")->m_currentValue = 100;

        g_app->GetWorld()->GetTeam(0)->m_territories.PutData( World::TerritorySouthAsia );
        g_app->GetWorld()->GetTeam(1)->m_territories.PutData( World::TerritoryNorthAmerica );        
        g_app->GetWorld()->AssignCities();
        
        g_app->GetWorld()->GetTeam(1)->m_unitsAvailable[WorldObject::TypeBattleShip] = 6;
        g_app->GetWorld()->GetTeam(1)->m_unitsAvailable[WorldObject::TypeCarrier] = 6;
        g_app->GetWorld()->GetTeam(1)->m_unitsAvailable[WorldObject::TypeSub] = 6;
        g_app->GetWorld()->GetTeam(1)->m_unitsAvailable[WorldObject::TypeSilo] = 6;
        g_app->GetWorld()->GetTeam(1)->m_unitsAvailable[WorldObject::TypeRadarStation] = 6;        
        g_app->GetWorld()->GetTeam(1)->m_unitsAvailable[WorldObject::TypeAirBase] = 4;

        g_app->GetWorld()->GetTeam(0)->m_unitsAvailable[WorldObject::TypeBattleShip] = 0;
        g_app->GetWorld()->GetTeam(0)->m_unitsAvailable[WorldObject::TypeCarrier] = 0;
        g_app->GetWorld()->GetTeam(0)->m_unitsAvailable[WorldObject::TypeSub] = 0;
        g_app->GetWorld()->GetTeam(0)->m_unitsAvailable[WorldObject::TypeSilo] = 3;
        g_app->GetWorld()->GetTeam(0)->m_unitsAvailable[WorldObject::TypeRadarStation] = 4;
        g_app->GetWorld()->GetTeam(0)->m_unitsAvailable[WorldObject::TypeAirBase] = 2;

        g_app->GetClientToServer()->RequestGameSpeed( 0, GAMESPEED_FAST );
        g_app->GetClientToServer()->RequestGameSpeed( 1, GAMESPEED_SLOW );

        Team *ai = g_app->GetWorld()->GetTeam(0);
        ai->m_type = Team::TypeAI;
        ai->m_aiActionTimer = syncfrand( ai->m_aiActionSpeed);
        ai->m_aiAssaultTimer = 4000;
        
        g_app->GetWorld()->GetTeam(0)->CreateFleet();       //166, 13
        ObjectPlacement( 0, WorldObject::TypeBattleShip, 166, 13, 0 );
        ObjectPlacement( 0, WorldObject::TypeBattleShip, 168, 15, 0 );
        ObjectPlacement( 0, WorldObject::TypeCarrier, 164, 15, 0 );
        ObjectPlacement( 0, WorldObject::TypeCarrier, 166, 17, 0 );

        g_app->GetWorld()->GetTeam(0)->CreateFleet();           //158, 29
        ObjectPlacement( 0, WorldObject::TypeBattleShip, 158, 29, 1 );
        ObjectPlacement( 0, WorldObject::TypeBattleShip, 160, 31, 1 );
        ObjectPlacement( 0, WorldObject::TypeCarrier, 156, 31, 1 );
        ObjectPlacement( 0, WorldObject::TypeCarrier, 158, 33, 1 );

        g_app->GetWorld()->GetTeam(0)->CreateFleet();           //170, 6
        ObjectPlacement( 0, WorldObject::TypeSub, 170, 6, 2 );
        ObjectPlacement( 0, WorldObject::TypeSub, 172, 8, 2 );
        ObjectPlacement( 0, WorldObject::TypeSub, 168, 8, 2 );
        ObjectPlacement( 0, WorldObject::TypeSub, 172, 10, 2 );
        
        g_app->GetGame()->m_lockVictoryTimer = false;

        ShowTerritoryPopup *showTerritory = new ShowTerritoryPopup();
        showTerritory->SetMessage( "showterritory", "tutorial_showterritory_popup" );
        showTerritory->SetDimensions( 340, 70, 0 );
        showTerritory->SetUITarget( "Toolbar", "Territory" );
        EclRegisterWindow(showTerritory);

        UnitPanelPopup *unitPopup = new UnitPanelPopup();
        unitPopup->SetMessage( "unit", "tutorial_unitpanel_popup" );
        unitPopup->SetDimensions( 340, 70, 0 );
        unitPopup->SetUITarget( "Toolbar", "Placement" );
        EclRegisterWindow(unitPopup);

        Mission7PlacementPopup *placeRadar = new Mission7PlacementPopup();
        placeRadar->SetMessage( "placeradar", "tutorial_m7placeradar_popup" );
        placeRadar->SetDimensions( 220, 200, 90 );
        placeRadar->m_unitType = WorldObject::TypeRadarStation;
        placeRadar->SetTargetWorldPos( -132, 50 );
        EclRegisterWindow( placeRadar );

        Mission7PlacementPopup *placeSilos = new Mission7PlacementPopup();
        placeSilos->SetMessage( "placesilos", "tutorial_m7placesilos_popup" );
        placeSilos->SetDimensions( 140, 200, 70 );
        placeSilos->m_unitType = WorldObject::TypeSilo;
        placeSilos->SetTargetWorldPos( -92, 39 );        
        EclRegisterWindow( placeSilos );

        Mission7PlacementPopup *placeairbases = new Mission7PlacementPopup();
        placeairbases->SetMessage( "placeairbases", "tutorial_m7placeairbases_popup" );
        placeairbases->SetDimensions( 140, 200, 90 );
        placeairbases->m_unitType = WorldObject::TypeAirBase;
        placeairbases->SetTargetWorldPos( -132, 50 );
        EclRegisterWindow( placeairbases );

        PlaceFleetsInPacificPopup *placeFleets = new PlaceFleetsInPacificPopup();
        placeFleets->SetMessage( "placefleets", "tutorial_placefleetspacific_popup" );
        placeFleets->SetDimensions( 130, 200, 70 );
        placeFleets->SetTargetWorldPos( -155, 25 );
        EclRegisterWindow(placeFleets);

        Misssion7FleetsToChinaPopup *moveFleets = new Misssion7FleetsToChinaPopup();
        moveFleets->SetMessage( "movefleets", "tutorial_m7fleettarget_popup" );
        moveFleets->SetDimensions( 180, 150, 70 );
        moveFleets->SetTargetWorldPos( 135, 21 );
        EclRegisterWindow( moveFleets );

        Mission7ScoutChinaPopup *scoutChina = new Mission7ScoutChinaPopup();
        scoutChina->SetMessage( "scoutchina", "tutorial_m7scoutchina_popuup" );
        scoutChina->SetDimensions( 180, 150, 80 );
        scoutChina->SetTargetWorldPos( 109, 34 );
        EclRegisterWindow( scoutChina );

        Mission7EnemySubsPopup *enemySubs = new Mission7EnemySubsPopup();
        enemySubs->SetMessage( "enemysubs", "tutorial_m7enemysubs_popup" );
        enemySubs->SetDimensions( 180, 150, 40 );
        enemySubs->SetTargetWorldPos(0,0);
        EclRegisterWindow( enemySubs );
    }
#endif
}


void Tutorial::UpdateCurrentChapter()
{
#ifndef NON_PLAYABLE
    //
    // Decide when/which chapter to jump to next
    // assuming the user has done as he was supposed to
    switch( m_currentLevel )
    {
        case 1: UpdateLevel1(); break;
        case 2: UpdateLevel2(); break;
        case 3: UpdateLevel3(); break;
        case 4: UpdateLevel4(); break;
        case 5: UpdateLevel5(); break;
        case 6: UpdateLevel6(); break;
        case 7: UpdateLevel7(); break;
        default: break;
    }   
#endif
}

void Tutorial::UpdateLevel1()
{
    //if( InChapter( "MoveCamera" ) )
    //{
    //    float middleX, middleY;
    //    g_app->GetMapRenderer()->GetPosition( middleX, middleY );
    //    if( fabs(middleX) > 90 || fabs(middleY) > 90 )
    //    {
    //        RunNextChapter(5.0f);
    //    }        
    //}

    //if( InChapter( "MoveCamera2" ) )
    //{
    //    RunNextChapter( 10.0f );
    //}

    //if( InChapter( "ZoomCamera" ) )
    //{
    //    float cameraZoom = g_app->GetMapRenderer()->GetZoomFactor();
    //    if( cameraZoom < 0.5f )
    //    {
    //        RunNextChapter(5.0f);
    //    }     
    //}

    if( InChapter("SiloFocus") )
    {        
        WorldObject *obj = g_app->GetWorld()->GetWorldObject( m_objectIds[0] );
        if( obj &&
            obj->m_currentState == 0 )
        {
            RunNextChapter();
        }
    }

    if( InChapter("Mission1DestroyRadar" ) )
    {
        Team *team = g_app->GetWorld()->GetTeam(0);
        if( team->m_unitsInPlay[WorldObject::TypeRadarStation] == 0 )
        {
            MissionComplete();
            RunNextChapter();
            SetCurrentLevel( 2 );
        }
        else
        {
            int nukeCount = 0;
            for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
            {
                if( g_app->GetWorld()->m_objects.ValidIndex(i) )
                {
                    WorldObject *obj = g_app->GetWorld()->m_objects[i];
                    if( obj->m_type == WorldObject::TypeNuke )
                    {
                        nukeCount++;
                    }
                    else if( obj->m_type == WorldObject::TypeSilo )
                    {
                        nukeCount += obj->m_states[0]->m_numTimesPermitted;
                    }
                }
            }
            if( nukeCount == 0 )
            {
                MissionFailed();
                RunNextChapter(0.5f, GetChapterId("Mission1Failed"));
            }
        }
    }
}

void Tutorial::UpdateLevel2()
{
    if( InChapter("Mission2Start") )
    {
        if( EclGetWindow("Side Panel") )
        {
            RunNextChapter();
        }
    }

    if( InChapter( "M2SelectSilo") )
    {
        if( EclGetWindow("Placement") )
        {
            RunNextChapter();
        }
    }

    if( InChapter("M2PlaceSilos") )
    {
        if( g_app->GetWorld()->GetTeam(1)->m_unitsInPlay[WorldObject::TypeSilo] == 4 )
        {
            RunNextChapter();
        }
    }


    if( InChapter("M2DestroyNukes") ) 
    {
        if( g_app->GetWorld()->GetTeam(0)->m_unitsInPlay[WorldObject::TypeNuke] == 0 &&
            g_app->GetWorld()->GetTeam(1)->m_friendlyDeaths == 0 )
        {
            MissionComplete();
            RunNextChapter();
            SetCurrentLevel( 3 );
        }
        else if( g_app->GetWorld()->GetTeam(1)->m_friendlyDeaths > 0 )
        {
            RunNextChapter( 0.5f, GetChapterId("Mission2Failed"));
        }            
    }
}

void Tutorial::UpdateLevel3()
{
    if( InChapter("Mission3Start") )
    {
        if( EclGetWindow("Placement") )
        {
            RunNextChapter();
        }
    }

    if( InChapter("M3RadarPlacement" ) )
    {
        Team *team = g_app->GetWorld()->GetMyTeam();
        int visibleTargets = team->GetNumTargets();
        if( team->m_unitsInPlay[WorldObject::TypeRadarStation] == 3 &&
            visibleTargets == 3 )
        {
            RunNextChapter();
        }
        else if( team->m_unitsInPlay[WorldObject::TypeRadarStation] == 3 &&
                 visibleTargets < 3 )
        {
            RunNextChapter( 0.5f, GetChapterId("Mission3Failed1") );
        }
    }

    if( InChapter("M3SiloPlacement") )
    {
        if( g_app->GetWorld()->GetTeam(1)->m_unitsInPlay[WorldObject::TypeSilo] == 4 )
        {
            RunNextChapter();
        }
    }

    if( InChapter("M3GameSpeed") ) 
    {
        if( g_app->GetWorld()->GetTimeScaleFactor() > 1 )
        {
            RunNextChapter();
        }
    }

    if( InChapter("M3Defcon1") )
    {
        if( g_app->GetWorld()->GetDefcon() == 1 )
        {
            RunNextChapter();
        }
    }

    if( InChapter("M3DestroyEnemy") )
    {
        if( g_app->GetMapRenderer()->GetCurrentSelectionId() == -1 )
        {
            int nukeCount = 0;
            for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
            {
                if( g_app->GetWorld()->m_objects.ValidIndex(i) )
                {
                    WorldObject *obj = g_app->GetWorld()->m_objects[i];
                    if( obj->m_teamId == 1 &&
                        obj->m_type == WorldObject::TypeSilo &&
                        obj->m_currentState == 0 )
                    {
                        RunNextChapter(2.0f);
                    }
                }
            }
        }
    }

    if( InChapter("M3SilosTakeTime") )
    {
        Team *team = g_app->GetWorld()->GetTeam(0);
        if( team->m_unitsInPlay[WorldObject::TypeAirBase] == 0 &&
            team->m_unitsInPlay[WorldObject::TypeSilo] == 0 )
        {
            MissionComplete();
            RunNextChapter();
            SetCurrentLevel( 4 );
        }
        else
        {
            if( m_miscTimer > 0.0f )
            {
                m_miscTimer -= SERVER_ADVANCE_PERIOD.DoubleValue();
                m_miscTimer = max( m_miscTimer, 0.0f );
            }
            int nukeCount = 0;
            for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
            {
                if( g_app->GetWorld()->m_objects.ValidIndex(i) )
                {
                    WorldObject *obj = g_app->GetWorld()->m_objects[i];
                    if( obj->m_type == WorldObject::TypeNuke )
                    {
                        nukeCount++;
                    }
                    else if( obj->m_teamId == 1 && obj->m_type == WorldObject::TypeSilo )
                    {
                        nukeCount += obj->m_states[0]->m_numTimesPermitted;
                    }
                }
            }
            if( nukeCount == 0 )
            {
                if( m_miscTimer == -1.0f )
                {
                    m_miscTimer = 1.0f;
                }
            }

            if( m_miscTimer == 0.0f )
            {
                MissionFailed();
                RunNextChapter(0.5f, GetChapterId("Mission3Failed2"));
                m_miscTimer = -1.0f;
            }            
        }
    }
}

void Tutorial::UpdateLevel4()
{
    if( InChapter("Mission4Start") )
    {
        if( g_app->GetWorld()->GetTeam(1)->m_unitsInPlay[WorldObject::TypeAirBase] == 3 )
        {
            RunNextChapter();
        }
    }

    if( InChapter("M4LaunchFighters") )
    {
        if( g_app->GetWorld()->GetTeam(1)->m_unitsInPlay[WorldObject::TypeFighter] > 4 )
        {
            RunNextChapter();
        }
    }

    if( InChapter("M4Radar") )
    {
        if( g_app->GetWorld()->GetTeam(1)->GetNumTargets() == 3 )
        {
            RunNextChapter(5.0f);
            if( g_app->GetWorld()->GetDefcon() > 1 )
            {
                g_app->GetWorld()->m_theDate.AdvanceTime( 1200 - g_app->GetWorld()->m_theDate.GetSeconds() );
            }
        }
    }

    if( InChapter("M4LaunchBombers") ) 
    {
        if( g_app->GetWorld()->GetTeam(1)->m_unitsInPlay[WorldObject::TypeBomber] >= 3 )
        {
            RunNextChapter();
        }
    }

    if( InChapter("M4BomberInfo") ) 
    {
        if( g_app->GetWorld()->GetTeam(1)->m_unitsInPlay[WorldObject::TypeNuke] > 0 )
        {
            RunNextChapter(10.0f);
        }
        else
        {
            int nukeCount = 0;
            for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
            {
                if( g_app->GetWorld()->m_objects.ValidIndex(i) )
                {
                    WorldObject *obj = g_app->GetWorld()->m_objects[i];
                    if( obj->m_type == WorldObject::TypeNuke )
                    {
                        nukeCount++;
                    }
                    else if( obj->m_type == WorldObject::TypeAirBase )
                    {
                        if( obj->m_states[1]->m_numTimesPermitted > 0 )
                        {
                            nukeCount += obj->m_nukeSupply;
                        }
                    }
                    else if( obj->m_type == WorldObject::TypeBomber )
                    {
                        nukeCount += obj->m_states[1]->m_numTimesPermitted;
                    }
                }
            }
            if( nukeCount == 0 )
            {
                MissionFailed();
                RunNextChapter( 0.5f, GetChapterId("Mission4Failed") );
            }            
        }
    }

    if( InChapter("M4NukesLaunched") )
    {
        if( g_app->GetWorld()->GetTeam(1)->GetNumTargets() == 0)
        {
            MissionComplete();
            RunNextChapter();
            SetCurrentLevel( 5 );
        }
        else
        {
            int nukeCount = 0;
            for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
            {
                if( g_app->GetWorld()->m_objects.ValidIndex(i) )
                {
                    WorldObject *obj = g_app->GetWorld()->m_objects[i];
                    if( obj->m_type == WorldObject::TypeNuke )
                    {
                        nukeCount++;
                    }
                    else if( obj->m_type == WorldObject::TypeAirBase )
                    {
                        if( obj->m_states[1]->m_numTimesPermitted > 0 )
                        {
                            nukeCount += obj->m_nukeSupply;
                        }
                    }
                    else if( obj->m_type == WorldObject::TypeBomber )
                    {
                        nukeCount += obj->m_states[1]->m_numTimesPermitted;
                    }
                }
            }
            if( nukeCount == 0 )
            {
                MissionFailed();
                RunNextChapter( 0.5f, GetChapterId("Mission4Failed") );
            }            
        }
    }
}

void Tutorial::UpdateLevel5()
{
    if( InChapter("Mission5Failed") )
    {
        return;
    }

    if( InChapter("Mission5Start") )
    {
        SidePanel *panel = (SidePanel *)EclGetWindow("Side Panel");
        if( panel )
        {
            if( panel->m_mode == SidePanel::ModeFleetPlacement )
            {
                RunNextChapter();
            }
        }
    }

    if( InChapter("M5CreateFleet") ) 
    {
        if( g_app->GetWorld()->GetTeam(1)->GetFleet(0) &&
            g_app->GetWorld()->GetTeam(1)->GetFleet(0)->m_active &&
            g_app->GetWorld()->GetTeam(1)->GetFleet(1) &&
            g_app->GetWorld()->GetTeam(1)->GetFleet(1)->m_active )
        {
            RunNextChapter();
        }
    }

    if( m_currentChapter > GetChapterId("M5CreateFleet") )
    {
        if( g_app->GetWorld()->GetTeam(1)->m_unitsInPlay[WorldObject::TypeBattleShip] == 0 &&
            g_app->GetWorld()->GetTeam(1)->m_unitsInPlay[WorldObject::TypeCarrier] == 0 )
        {
            MissionFailed();
            RunNextChapter(0.5f, GetChapterId("Mission5Failed"));
        }
    }

    if( InChapter("M5BattleShipInfo") )
    {
        Fleet *fleet1 = g_app->GetWorld()->GetTeam(1)->GetFleet(0);
        Fleet *fleet2 = g_app->GetWorld()->GetTeam(1)->GetFleet(1);

        if( fleet1->IsFleetMoving() ||
            fleet2->IsFleetMoving() )
        {
            RunNextChapter();
        }
    }

    if( InChapter("M5DestroyEnemy") ) 
    {
        if( g_app->GetWorld()->m_gunfire.NumUsed() > 3 )
        {
            RunNextChapter( 5 );
        }
    }

    if( InChapter("M5DestroyEnemy2") ) 
    {
        if( g_app->GetWorld()->GetTeam(0)->m_unitsInPlay[WorldObject::TypeBattleShip] == 0 &&
            g_app->GetWorld()->GetTeam(0)->m_unitsInPlay[WorldObject::TypeCarrier] == 0 )
        {
            MissionComplete();
            RunNextChapter();
            SetCurrentLevel( 6 );
        }
    }
}


void Tutorial::UpdateLevel6()
{
    if( InChapter("Mission6Failed1") ||
        InChapter("Mission6Failed2"))
    {
        return;
    }

    if( InChapter("Mission6Start") )
    {
        if( g_app->GetMapRenderer()->m_showAllTeams )
        {
            RunNextChapter(5);
        }
    }

    if( InChapter("Mission6SubIntro") )
    {
        if( g_app->GetWorld()->GetTeam(1)->m_unitsInPlay[WorldObject::TypeCarrier] == 6 )
        {
            RunNextChapter();
        }
    }

    if( InChapter("Mission6PlaceSubs") )
    {
        if( g_app->GetWorld()->GetTeam(1)->m_unitsInPlay[WorldObject::TypeSub] == 6 )
        {
            RunNextChapter();
        }
    }

    if( InChapter("M6SubInfo") )
    {
        Team *team = g_app->GetWorld()->GetMyTeam();
        int movingFleets = 0;
        
        for( int i = 0; i < team->m_fleets.Size(); ++i )
        {
            Fleet *fleet = team->GetFleet(i);
            if( fleet && fleet->IsFleetMoving() )
            {
                int numSubs = 0;
                for( int i = 0; i < fleet->m_fleetMembers.Size(); ++i )
                {
                    int shipId = fleet->m_fleetMembers[i];
                    WorldObject *wobj = g_app->GetWorld()->GetWorldObject(shipId);
                    if( wobj->m_type == WorldObject::TypeSub )
                    {
                        ++numSubs;
                    }
                }

                if( numSubs > 0 )
                {
                    movingFleets++;
                }
            }
        }
        if( movingFleets > 0 )
        {
            RunNextChapter();
        }
    }


    if( InChapter("M6DestroySubs" ) )
    {
        if( g_app->GetWorld()->GetTeam(0)->m_unitsInPlay[WorldObject::TypeSub] == 0 )
        {
            RunNextChapter();
        }
    }

    if( InChapter("M6MoveSubs") )
    {
        int subsInRange = 0;
        int siloId = g_app->GetWorld()->GetNearestObject( 0, 0, 0, WorldObject::TypeSilo );
        WorldObject *enemySilo = g_app->GetWorld()->GetWorldObject(siloId);
        for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
        {
            if( g_app->GetWorld()->m_objects.ValidIndex(i) )
            {
                WorldObject *obj = g_app->GetWorld()->m_objects[i];
                if( obj->m_type == WorldObject::TypeSub &&
                    g_app->GetWorld()->GetDistance( obj->m_longitude, obj->m_latitude, enemySilo->m_longitude, enemySilo->m_latitude ) < 45 )
                {
                    subsInRange++;
                }
            }
        }

        if( subsInRange >= 3 )
        {
            RunNextChapter(2);
        }
    }
   

    if( InChapter("M6EnemyLaunch") )
    {
        int nukeCount = 0;
        for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
        {
            if( g_app->GetWorld()->m_objects.ValidIndex(i) )
            {
                WorldObject *obj = g_app->GetWorld()->m_objects[i];
                if( obj->m_teamId == 1 &&
                    obj->m_type == WorldObject::TypeNuke )
                {
                    nukeCount++;
                }
            }
        }

        if( nukeCount > 0 )
        {
            RunNextChapter(10);
        }
    }

    if( InChapter("M6LaunchExplanation") )
    {
        if( g_app->GetWorld()->GetTeam(0)->m_unitsInPlay[WorldObject::TypeSilo] == 0 )
        {
            MissionComplete();
            RunNextChapter();
            SetCurrentLevel( 7 );
        }
    }
}

void Tutorial::UpdateLevel7()
{
    //
    // Remove the tutorial window at defcon 3

    if( g_app->GetWorld()->GetDefcon() <= 3 &&
        EclGetWindow( "Tutorial" ) )
    {
        //EclRemoveWindow( "Tutorial" );   
    }

    if( InChapter("Mission7Start") )
    {
        if( g_app->GetGame()->m_winner == 1 )
        {
            RunNextChapter(1.0f, GetChapterId("M7Victory"));
        }
        else if( g_app->GetGame()->m_winner == 0 )
        {
            RunNextChapter(1.0f, GetChapterId("Mission7Failed") );
        }


        for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
        {
            if( g_app->GetWorld()->m_objects.ValidIndex(i) )
            {
                WorldObject *wobj = g_app->GetWorld()->m_objects[i];
                if( wobj &&
                    wobj->m_teamId == 0 &&
                    wobj->m_type == WorldObject::TypeNuke )
                {
                    RunNextChapter(3);                    
                    break;
                }
            }
        }
    }

    if( InChapter("Mission7EnemyAttack") )
    {
        if( g_app->GetGame()->m_winner == 1 )
        {
            RunNextChapter(1.0f, GetChapterId("M7Victory"));
        }
        else if( g_app->GetGame()->m_winner == 0 )
        {
            RunNextChapter(1.0f, GetChapterId("Mission7Failed") );
        }

        if( g_app->GetGame()->m_nukeCount[0] < 10 )
        {
            RunNextChapter(10);
        }
    }


    if( InChapter("Mission7Retaliate") )
    {
        if( g_app->GetGame()->m_winner == 1 )
        {
            RunNextChapter(1.0f, GetChapterId("M7Victory"));
            MissionComplete();
        }
        else if( g_app->GetGame()->m_winner == 0 )
        {
            RunNextChapter(1.0f, GetChapterId("Mission7Failed") );
        }
    }
}

int Tutorial::ObjectPlacement( int teamId, int unitType, float longitude, float latitude, int fleetId)
{
    Team *team = g_app->GetWorld()->GetTeam(teamId);
	WorldObject *newUnit = WorldObject::CreateObject( unitType );
	newUnit->SetTeamId(teamId);
    int id = g_app->GetWorld()->AddWorldObject( newUnit );
	newUnit->SetPosition( Fixed::FromDouble(longitude), Fixed::FromDouble(latitude) );
    if( fleetId != 255 )
    {
        team->m_fleets[fleetId]->m_fleetMembers.PutData( newUnit->m_objectId );
        newUnit->m_fleetId = fleetId;
    }
    return id;
}

int Tutorial::GetMissionStart( int _mission )
{
    if( _mission == -1 )
    {
        _mission = m_currentLevel;
    }

    switch( _mission )
    {
        case 1: return GetChapterId( "Start" );
        case 2: return GetChapterId( "Mission2Start");
        case 3: return GetChapterId( "Mission3Start");
        case 4: return GetChapterId( "Mission4Start");
        case 5: return GetChapterId( "Mission5Start");
        case 6: return GetChapterId( "Mission6Start");
        case 7: return GetChapterId( "Mission7Start");
        default: return -1;
    }
}

void Tutorial::MissionComplete()
{
    char msg[256];
    sprintf(msg, LANGUAGEPHRASE("tutorial_missioncomplete"));
    g_app->GetInterface()->ShowMessage(0, 0, -1, msg, true );

    int tutorialCompleted = g_preferences->GetInt( PREFS_TUTORIAL_COMPLETED );
    if( tutorialCompleted < m_currentLevel )
    {
        g_preferences->SetInt( PREFS_TUTORIAL_COMPLETED, m_currentLevel );
        g_preferences->Save();
    }
}

void Tutorial::MissionFailed()
{
    char msg[256];
    sprintf(msg, LANGUAGEPHRASE("tutorial_missionfailed"));
    g_app->GetInterface()->ShowMessage(0, 0, -1, msg, true );
}

void Tutorial::SetCurrentLevel( int _level )
{
    m_currentLevel = _level;
}

int Tutorial::GetCurrentLevel()
{
    return m_currentLevel;
}



// ============================================================================

TutorialPopup::TutorialPopup()
:   EclWindow( "TutorialPopup", "tutorial_popup", true ),
    m_objectId(-1),
    m_angle(0.0f),
    m_distance(200.0f),
    m_rendered(true),
    m_dragOffsetX(0.0f),
    m_dragOffsetY(0.0f),
    m_targetType(0),
    m_size(50),
    m_renderTimer(0.0f)
{
    m_stringId[0] = '\x0';
    m_windowTarget[0] = '\x0';
    m_buttonTarget[0] = '\x0';

    SetSize( 100, 50 );
    m_x = -1000;
    m_y = -1000;

    m_currentLevel = g_app->GetTutorial()->GetCurrentLevel();
}


void TutorialPopup::SetMessage( char *_name, char *_stringId )
{
    while( EclGetWindow(_name) )
    {
        EclRemoveWindow(_name);
    }

    SetName( _name );    
    strcpy( m_stringId, _stringId );  
}


void TutorialPopup::SetDimensions( float _angle, float _distance, float _size )
{
    m_angle = _angle;
    m_distance = _distance;
    m_size = _size;
}


void TutorialPopup::SetTargetObject( int _objectId )
{
    m_objectId = _objectId;
    m_targetType = 1;
}


void TutorialPopup::SetUITarget( char *_window, char *_button )
{
    strcpy( m_windowTarget, _window );
    strcpy( m_buttonTarget, _button );

    m_targetType = 3;
}


void TutorialPopup::SetTargetWorldPos( float _longitude, float _latitude )
{
    m_targetLongitude = _longitude;
    m_targetLatitude = _latitude;

    m_targetType = 2;
}


void TutorialPopup::Update()
{
    //
    // If we are no longer on the same level, kill this window

    if( g_app->GetTutorial()->GetCurrentLevel() != m_currentLevel )
    {
        EclRemoveWindow( m_name );
        return;
    }
    

    //
    // If we weren't drawn in the last frame move us way off screen
    // so we dont get in the way

    if( !m_rendered )
    {
        SetPosition(-1000,-1000);
    }


    //
    // Make sure we are infront of everything

    if( m_rendered )
    {
        EclBringWindowToFront( m_name );
    }


    m_rendered = false;
}


void TutorialPopup::Render( bool _hasFocus )
{
    //
    // If the Tutorial Window is explaining something,
    // Dont render any captions until it has finished

    bool renderCaptions = true;
    bool renderHighlight = true;

    TutorialWindow *tutorial = (TutorialWindow *) EclGetWindow("Tutorial");
    if( tutorial && tutorial->m_explainingSomething )
    {
        renderCaptions = false;
        renderHighlight = false;
        m_renderTimer = 0.0f;
    }

    m_renderTimer += g_advanceTime;
    if( m_renderTimer < 1.5f ) 
    {
        renderCaptions = false;
        renderHighlight = false;
    }

    if( g_app->GetTutorial()->InChapter( "SiloFocus" ) )
    {
        // Yes, I know.
        // Shut up ok?
        renderHighlight = true;
    }


    m_rendered = true;

    Colour windowColP  = g_styleTable->GetPrimaryColour( STYLE_TOOLTIP_BACKGROUND );
    Colour windowColS  = g_styleTable->GetSecondaryColour( STYLE_TOOLTIP_BACKGROUND );
    Colour borderCol   = g_styleTable->GetPrimaryColour( STYLE_TOOLTIP_BORDER );

    bool alignment = g_styleTable->GetStyle(STYLE_TOOLTIP_BACKGROUND)->m_horizontal;

    Colour fontCol     = g_styleTable->GetPrimaryColour( FONTSTYLE_TOOLTIP );
    Style *fontStyle    = g_styleTable->GetStyle( FONTSTYLE_TOOLTIP );

    float fontSize = fontStyle->m_size;


    if( renderCaptions )
    {
        //
        // Render the window

        g_renderer->RectFill ( m_x, m_y, m_w, m_h, windowColP, windowColS, alignment );
        g_renderer->Rect     ( m_x, m_y, m_w-1, m_h-1, borderCol);


        //
        // Render the shadow

        InterfaceWindow::RenderWindowShadow( m_x+m_w, m_y, m_h, m_w, 15, g_renderer->m_alpha*0.5f );


        //
        // Render the caption

        float gapSize = 2;

        g_renderer->SetFont( fontStyle->m_fontName, false, fontStyle->m_negative, false );

        if( g_languageTable->DoesPhraseExist(m_stringId) )
        {
            char *string = LANGUAGEPHRASE(m_stringId);
            MultiLineText wrapped( string, m_w-5, fontSize);

			for( int i = 0; i < wrapped.Size(); ++i )
			{
				char *thisLine = wrapped[i];
				float x = m_x + 5;
				float y = m_y + 5 + i * (fontSize+gapSize);
				
				g_renderer->TextSimple( x, y, White, fontSize, thisLine );
			}

			float newWidth = 200;
			//if( wrapped.Size() > 4 ) newWidth = 250;
			SetSize( newWidth, wrapped.Size() * (fontSize+gapSize) + 10 );
        }

		g_renderer->SetFont();
    }


    //
    // Find out what we are focused on

    float worldLongitude = 0.0f;
    float worldLatitude = 0.0f;
    float radius = 0.0f;
    float targetScreenX = m_x;
    float targetScreenY = m_y;

    g_renderer->SetBlendMode(Renderer::BlendModeNormal);
    
    if( m_targetType > 0 )
    {
        switch( m_targetType )
        {
            case 1:             // Linked to world object
            {
                WorldObject *wobj = g_app->GetWorld()->GetWorldObject( m_objectId );
                if( wobj )
                {
                    worldLongitude = wobj->m_longitude.DoubleValue() + 0.3f;
                    worldLatitude = wobj->m_latitude.DoubleValue() + 0.3f;
        
                    g_app->GetMapRenderer()->ConvertAngleToPixels( worldLongitude, worldLatitude, &targetScreenX, &targetScreenY );
                    radius = m_size * ( 1.0f + sinf(g_gameTime*5) * 0.1f );                    
                }
                break;
            }
        
            case 2:             // Linked to world position
            {
                worldLongitude = m_targetLongitude;
                worldLatitude = m_targetLatitude;
                g_app->GetMapRenderer()->ConvertAngleToPixels( m_targetLongitude, m_targetLatitude, &targetScreenX, &targetScreenY );
                radius = m_size / g_app->GetMapRenderer()->GetZoomFactor();
                radius += sinf(g_gameTime*5) * 4;
                break;
            }

            case 3:             // Linked to interface button
            {
                EclWindow *window = EclGetWindow( m_windowTarget );
                if( window && renderHighlight )
                {
                    EclButton *button = window->GetButton( m_buttonTarget );
                    float outSize = 4.0f * ( 1.0f + sinf(g_gameTime*5) * 0.5f );
                    if( button )
                    {
                        targetScreenX = window->m_x + button->m_x + button->m_w/2;
                        targetScreenY = window->m_y + button->m_y + button->m_h/2;
                        g_renderer->Rect( window->m_x + button->m_x-outSize, 
                                          window->m_y + button->m_y-outSize, 
                                          button->m_w+outSize*2, 
                                          button->m_h+outSize*2, borderCol, 3.0f );
                    }
                    else
                    {
                        targetScreenX = window->m_x + window->m_w/2;
                        targetScreenY = window->m_y + window->m_h/2;
                        g_renderer->Rect( window->m_x-outSize, 
                                          window->m_y-outSize, 
                                          window->m_w+outSize*2, 
                                          window->m_h+outSize*2, borderCol, 3.0f );
                    }
                }
                break;
            }
        }

        //
        // Render a circle over the target object in the world
        // Or render an arrow if it is offscreen

        if( renderHighlight )
        {
            if( m_targetType == 1 || m_targetType == 2 )
            {
                bool offScreen = !g_app->GetMapRenderer()->IsOnScreen(worldLongitude, worldLatitude);
                if( !offScreen )
                {
                    g_renderer->Circle( targetScreenX, targetScreenY, radius, 40, borderCol, 4.0f );
                }
                else if( offScreen )
                {
                    float longitude = worldLongitude;

                    float distance1 = g_app->GetWorld()->GetDistance( Fixed::FromDouble(longitude), 
																	  Fixed::FromDouble(worldLatitude), 
                                                                      Fixed::FromDouble(g_app->GetMapRenderer()->m_middleX), 
                                                                      Fixed::FromDouble(g_app->GetMapRenderer()->m_middleY), true ).DoubleValue();

                    float distance2 = g_app->GetWorld()->GetDistance( Fixed::FromDouble(longitude + 360), 
																	  Fixed::FromDouble(worldLatitude), 
																	  Fixed::FromDouble(g_app->GetMapRenderer()->m_middleX), 
																	  Fixed::FromDouble(g_app->GetMapRenderer()->m_middleY), true ).DoubleValue();

                    float distance3 = g_app->GetWorld()->GetDistance( Fixed::FromDouble(longitude - 360), 
																	  Fixed::FromDouble(worldLatitude), 
                                                                      Fixed::FromDouble(g_app->GetMapRenderer()->m_middleX), 
																	  Fixed::FromDouble(g_app->GetMapRenderer()->m_middleY), true ).DoubleValue();

                    if( distance2 < distance1 ) longitude += 360;
                    if( distance3 < distance1 ) longitude -= 360;

                    float edgeLongitude, edgeLatitude;
                    g_app->GetMapRenderer()->FindScreenEdge( Vector3<float>(longitude, worldLatitude, 0L), &edgeLongitude, &edgeLatitude);
                    Vector3<float> targetDir = (Vector3<float>( longitude, worldLatitude, 0L ) -
                                                Vector3<float>( edgeLongitude, edgeLatitude, 0L )).Normalise();
                    float angle = atan( targetDir.x / targetDir.y );
                    if( targetDir.y > 0.0f ) angle += M_PI;
                    
                    g_app->GetMapRenderer()->ConvertAngleToPixels( edgeLongitude, edgeLatitude, &targetScreenX, &targetScreenY );
                    Image *img = g_resource->GetImage( "graphics/arrow.bmp" );
                    Colour col = borderCol;
                    col.m_a = 255;
                    float size = 30 * ( 1.0f + sinf(g_gameTime*5) * 0.1f );
                    g_renderer->SetBlendMode(Renderer::BlendModeAdditive );
                    g_renderer->Blit( img, targetScreenX, targetScreenY, size, size, col, angle );
                    g_renderer->SetBlendMode(Renderer::BlendModeNormal);
                }
            }
        }


        //
        // Move the window to the best position

        float angleRads = 2.0f * M_PI * (m_angle-90)/360.0f;

        targetScreenX = targetScreenX + cosf(angleRads) * m_distance;
        targetScreenY = targetScreenY + sinf(angleRads) * m_distance;
        targetScreenX -= m_w/2.0f;
        targetScreenY -= m_h/2.0f;


        if( EclGetWindow() == this && g_inputManager->m_lmb )
        {
            m_positionX = m_x;
            m_positionY = m_y;

            m_dragOffsetX = m_positionX - targetScreenX;
            m_dragOffsetY = m_positionY - targetScreenY;
        }
        else if( m_x < -500 && m_y < -500 )
        {
            m_positionX = targetScreenX;
            m_positionY = targetScreenY;
        }
        else
        {
            targetScreenX += m_dragOffsetX;
            targetScreenY += m_dragOffsetY;

            targetScreenX = max( targetScreenX, 10.0f );
            targetScreenY = max( targetScreenY, 10.0f );
            targetScreenX = min( targetScreenX, g_windowManager->WindowW() - m_w - 10.0f );
            targetScreenY = min( targetScreenY, g_windowManager->WindowH() - m_h - 10.0f );
        
            float timeFactor = g_advanceTime*20.0f;
            m_positionX = m_positionX * (1-timeFactor) + targetScreenX * timeFactor;
            m_positionY = m_positionY * (1-timeFactor) + targetScreenY * timeFactor;
        }

		// Need to use floating point round(), since just truncating to int can magnify 
		// floating point errors and cause window to jitter when it should be still.
        SetPosition( round(m_positionX), round(m_positionY) );
    }
}
