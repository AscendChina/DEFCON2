
#ifndef _included_date_h
#define _included_date_h

#include "lib/math/fixed.h"


#define     DATE_SECONDS        1
#define     DATE_MINUTES        (DATE_SECONDS * 60)
#define     DATE_HOURS          (DATE_MINUTES * 60)
#define     DATE_DAYS           (DATE_HOURS * 24)
#define     DATE_YEARS          (DATE_DAYS * 365)


class Date
{
public:
    int     m_numDays;
    Fixed   m_theDate;

public:    
    Date();

    void    AdvanceTime( Fixed seconds );
    
    int     GetSeconds();
    int     GetMinutes();
    int     GetHours();
    int     GetDays();
    
    char *GetTheDate();

    void    ResetClock();

};



#endif
