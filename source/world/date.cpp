#include "lib/universal_include.h"

#include <stdio.h>

#include "world/date.h"


Date::Date()
:   m_theDate(0),
    m_numDays(0)
{
}


void Date::AdvanceTime( Fixed seconds )
{
    m_theDate += seconds;

    if( GetHours() >= 24 )
    {
        ++m_numDays;
        m_theDate = 0;
    }
}

int Date::GetSeconds()
{
    Fixed remainder = m_theDate;
    
    int hours = ( remainder / DATE_HOURS ).IntValue();
    remainder -= hours * DATE_HOURS;
    
    int minutes = ( remainder / DATE_MINUTES ).IntValue();
    remainder -= minutes * DATE_MINUTES;

    int seconds = ( remainder / DATE_SECONDS ).IntValue();
    remainder -= seconds * DATE_SECONDS;

    return seconds;
}

int Date::GetMinutes()
{
    Fixed remainder = m_theDate;

    int hours = ( remainder / DATE_HOURS ).IntValue();
    remainder -= hours * DATE_HOURS;

    int minutes = ( remainder / DATE_MINUTES ).IntValue();
    remainder -= minutes * DATE_MINUTES;

    return minutes;
}

int Date::GetHours()
{
    Fixed remainder = m_theDate;
    int hours = ( remainder / DATE_HOURS ).IntValue();
    return hours;
}

int Date::GetDays()
{
    return m_numDays;
}

char *Date::GetTheDate()
{
    Fixed remainder = m_theDate;

    int hours = ( remainder / DATE_HOURS ).IntValue();
    remainder -= hours * DATE_HOURS;

    int minutes = ( remainder / DATE_MINUTES ).IntValue();
    remainder -= minutes * DATE_MINUTES;

    int seconds = ( remainder / DATE_SECONDS ).IntValue();
    remainder -= seconds * DATE_SECONDS;

    static char result[256];
    sprintf( result, "%.2d:%.2d.%.2d", hours, minutes, seconds );

    return result;
}

void Date::ResetClock()
{
    m_theDate = 0;
    m_numDays = 0;
}

