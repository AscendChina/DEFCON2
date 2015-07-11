
#ifndef _included_trayicon_h
#define _included_trayicon_h

#include "statusicon.h"

class X11Icon : public StatusIcon
{
protected:

public:
    X11Icon();
    ~X11Icon();
    
    void SetIcon    ( const char *_iconId );
    void SetSubIcon ( const char *_iconId );
    void SetCaption ( const char *_caption );
    void RemoveIcon ();

    void Update     ();
};


#endif
