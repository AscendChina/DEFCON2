#ifndef _included_versionmanager_h
#define _included_versionmanager_h


class Directory;



class VersionManager
{
public:
    static float    VersionStringToNumber   ( char *_version );
    static bool     DoesSupportModSystem    ( char *_version );
    static bool     DoesSupportWhiteBoard   ( char *_version );
    static bool     DoesSupportSendTeamScore( char *_version );


    static void     EnsureCompatability     ( char *_version, Directory *_letter );                 // for server to client messages
};


#endif
