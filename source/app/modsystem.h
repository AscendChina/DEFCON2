#ifndef _included_modsystem_h
#define _included_modsystem_h

#include "lib/tosser/llist.h"

class InstalledMod;




class ModSystem
{    
public:
    LList<InstalledMod *> m_mods;
    LList<char *> m_criticalFiles;
	
protected:
    char *m_modsDir;

public:
    ModSystem();
    ~ModSystem();

    void Initialise();        
    void LoadInstalledMods();
    
    LList<char *>  *ParseModPath( char *_modPath );                             // _modPath is modified        

    bool    CanSetModPath       ( char *_modPath );                             // Are all the mods installed?
    void    SetModPath          ( char *_modPath );
    void    GetCriticalModPath  ( char *_modPath );
    bool    IsCriticalModPathSet( char *_modPath );                             // Are these critical mods already running
    bool    IsCriticalModRunning();

    void    LoadModData         ( InstalledMod *_mod, char *_path );

    void    ActivateMod         ( char *_mod, char *_version );
    void    MoveMod             ( char *_mod, char *_version, int _move );
    void    DeActivateMod       ( char *_mod );
    
    void    DeActivateAllMods           ();                                     // Does NOT commit
    void    DeActivateAllCriticalMods   ();                                     // Does NOT commit

    bool            IsModInstalled  ( char *_mod, char *_version );
    InstalledMod    *GetMod         ( char *_mod, char *_version );


    bool    CommitRequired      ();
    void    Commit              ();                                             // Commits the mod settings to the file system
};



// ============================================================================


class InstalledMod
{
public:
    char    m_name      [256];
    char    m_version   [256];    
    char    m_path      [1024];
    char    m_author    [256];
    char    m_website   [256];
    char    m_comment   [10240];
    
    bool    m_critical;
    bool    m_active;

public:
    InstalledMod();
};




extern ModSystem *g_modSystem;


#endif
