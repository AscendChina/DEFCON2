#include "lib/universal_include.h"
#include "lib/resource/resource.h"
#include "lib/language_table.h"

#include "app/app.h"
#include "app/globals.h"

#include "world/radarstation.h"



RadarStation::RadarStation()
:   WorldObject()
{
    SetType( TypeRadarStation );
    
    strcpy( bmpImageFilename, "graphics/radarstation.bmp" );

    m_radarRange = 80;
    m_selectable = false;  
    
    m_life = 1;
    
    AddState( LANGUAGEPHRASE("state_radar"), 0, 0, 40, 0, false );

}
