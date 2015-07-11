#include "lib/universal_include.h"
#include "lib/filesys/file_system.h"
#include "lib/filesys/text_stream_readers.h"
#include "lib/math/vector3.h"
#include "lib/hi_res_time.h"
#include "lib/preferences.h"

#include "city.h"
#include "earthdata.h"

#include "renderer/map_renderer.h"


EarthData::EarthData()
{
}


void EarthData::Initialise()
{        
    LoadCoastlines();
    LoadBorders();
    LoadCities();
}


void EarthData::LoadBorders()
{
    double startTime = GetHighResTime();
    
    m_borders.EmptyAndDelete();

    int numIslands = 0;    
    Island *island = NULL;

    TextReader *international = g_fileSystem->GetTextReader( "data/earth/international.dat" );
    AppAssert( international && international->IsOpen() );

    while( international->ReadLine() )
    {
        char *line = international->GetRestOfLine();        
        if( line[0] == 'b' )
        {
            if( island )
            {
                m_borders.PutData( island );                  
                ++numIslands;
            }
            island = new Island();            
        }
        else
        {
            float longitude, latitude;
            sscanf( line, "%f %f", &longitude, &latitude );
            island->m_points.PutData( new Vector3<float>( longitude, latitude, 0.0f ) );
        }
    }
    
    delete international;

    double totalTime = GetHighResTime() - startTime;
    AppDebugOut( "Parsing International data (%d islands) : %dms\n", numIslands, int( totalTime * 1000.0f ) );
}


void EarthData::LoadCities()
{
    float startTime = GetHighResTime();

    m_cities.EmptyAndDelete();

    TextReader *cities = g_fileSystem->GetTextReader( "data/earth/cities.dat" );
    AppAssert( cities && cities->IsOpen() );
    
    int numCities = 0;
    
    while( cities->ReadLine() )
    {
        char *line = cities->GetRestOfLine();

        char name[256];
        char country[256];
        float latitude, longitude;
        int population;
        int capital;
        
        strncpy( name, line, 40 );
        for( int i = 39; i >= 0; --i )
        {
            if( name[i] != ' ' ) 
            {
                name[i+1] = '\x0';
                break;
            }
        }

        strncpy( country, line+41, 40 );
        for( int i = 39; i >= 0; --i )
        {
            if( country[i] != ' ' )
            {
                country[i+1] = '\x0';
                break;
            }
        }

        sscanf( line+82, "%f %f %d %d", &longitude, &latitude, &population, &capital );

        City *city = new City();
        city->m_name = strdup( strupr(name) );
        city->m_country = strdup( strupr(country) );
        city->m_longitude = Fixed::FromDouble(longitude);
        city->m_latitude = Fixed::FromDouble(latitude);
        city->m_population = population;
        city->m_capital = capital;         
        city->SetRadarRange( Fixed::FromDouble(sqrtf( sqrtf(city->m_population) ) / 4.0f) );

        m_cities.PutData( city );
        ++numCities;
    }
    
    delete cities;

    float totalTime = GetHighResTime() - startTime;
    AppDebugOut( "Parsing City data (%d cities) : %dms\n", numCities, int( totalTime * 1000.0f ) );
}


void EarthData::LoadCoastlines()
{
    double startTime = GetHighResTime();

    m_islands.EmptyAndDelete();

    int numIslands = 0;

    char coastFile[1024];
    if( g_preferences->GetInt(PREFS_GRAPHICS_LOWRESWORLD) == 0 )
    {
        strcpy(coastFile, "data/earth/coastlines.dat");
    }
    else
    {
        strcpy(coastFile, "data/earth/coastlines-low.dat");
    }

    TextReader *coastlines = g_fileSystem->GetTextReader( coastFile );
    AppAssert( coastlines && coastlines->IsOpen() );
    Island *island = NULL;
    
    while( coastlines->ReadLine() )
    {        
        char *line = coastlines->GetRestOfLine();
        if( line[0] == 'b' )
        {
            if( island )
            {           
                m_islands.PutData( island );                  
            }
            island = new Island();
            ++numIslands;
        }
        else
        {
            float longitude, latitude;
            sscanf( line, "%f %f", &longitude, &latitude );
            island->m_points.PutData( new Vector3<float>( longitude, latitude, 0.0f ) );            
        }
    }

    delete coastlines;

    double totalTime = GetHighResTime() - startTime;
    AppDebugOut( "Parsing Coastline data (%d islands) : %dms\n", numIslands, int( totalTime * 1000.0f ) );
}
