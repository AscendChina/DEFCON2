#ifndef _included_earthdata_h
#define _included_earthdata_h

class Island;
class City;

#include "lib/tosser/llist.h"
#include "lib/math/vector3.h"


class EarthData
{
public:
    LList           <Island *>          m_islands;
    LList           <Island *>          m_borders;
    LList           <City *>            m_cities;

public:
    EarthData();

    void Initialise();

    void LoadBorders();
    void LoadCities();
    void LoadCoastlines();
};



// ============================================================================

class Island
{
public:
    LList<Vector3<float> *> m_points;

};



#endif
