
#ifndef _included_city_h
#define _included_city_h

#include "world/worldobject.h"


class City : public WorldObject
{
public:
    char    *m_name;
    char    *m_country;
    int     m_population;   
    bool    m_capital;
    int     m_numStrikes;
    int     m_dead;
    
public:    
    City();
    
    bool    Update();
    bool    NuclearStrike( int causedBy, Fixed intensity, Fixed range, bool directHitPossible );

    static int GetEstimatedPopulation( int teamId, int cityId, int numNukes );    // returns the estimated population after all targeted nukes from teamId hit
    
};


#endif
