#ifndef _included_networkdefines_h
#define _included_networkdefines_h


/*
 *	    Put this define in place if you want the optimised network symbols
 *      to be replaced with human readable symbols 
 *      (much higher bandwidth usage)
 *
 */

//#define     NET_DEFCON_USEDEBUGSYMBOLS


/*
 *	    Basic Stuff
 */


#define     PREFS_NETWORKCLIENTPORT                 "NetworkClientPort"
#define     PREFS_NETWORKSERVERPORT                 "NetworkServerPort"
#define     PREFS_NETWORKUSEPORTFORWARDING          "NetworkUsePortForwarding"
#define     PREFS_NETWORKTRACKSYNCRAND              "NetworkTrackSynchronisation"


/*
 *      All Defcon network messages should 
 *      have this name.  If not then they're
 *      not for us.
 *
 */

#ifdef NET_DEFCON_USEDEBUGSYMBOLS
#include "network_defines_debug.h"
#else


#define     NET_DEFCON_MESSAGE                      "d"
#define     NET_DEFCON_COMMAND                      "c"



/*
 *      CLIENT TO SERVER
 *      Commands
 *
 */


#define     NET_DEFCON_CLIENT_JOIN                  "ca"
#define     NET_DEFCON_CLIENT_LEAVE                 "cb"
#define     NET_DEFCON_START_GAME                   "cc"
#define     NET_DEFCON_REQUEST_TEAM                 "cd"
#define     NET_DEFCON_REQUEST_SPECTATE             "cz"
#define     NET_DEFCON_REQUEST_ALLIANCE             "ce"
#define     NET_DEFCON_REQUEST_TERRITORY            "cf"
#define     NET_DEFCON_REQUEST_FLEET                "cg"
#define     NET_DEFCON_RESYNCHRONISE                "cres"

#define     NET_DEFCON_OBJSTATECHANGE               "ch"
#define     NET_DEFCON_OBJACTION                    "ci"
#define     NET_DEFCON_OBJPLACEMENT                 "cj"
#define     NET_DEFCON_OBJSPECIALACTION             "ck"
#define     NET_DEFCON_OBJSETWAYPOINT               "cl"
#define     NET_DEFCON_OBJCLEARACTIONQUEUE          "cm"
#define     NET_DEFCON_FLEETMOVE                    "cn"

#define     NET_DEFCON_CHANGEOPTION                 "co"
#define     NET_DEFCON_SETGAMESPEED                 "cp"
#define     NET_DEFCON_SETTEAMNAME                  "cq"
#define     NET_DEFCON_AGGRESSIONCHANGE             "cr"
#define     NET_DEFCON_CHATMESSAGE                  "cs"
#define     NET_DEFCON_BEGINVOTE                    "ct"
#define     NET_DEFCON_CASTVOTE                     "cu"
#define     NET_DEFCON_CEASEFIRE                    "cv"
#define     NET_DEFCON_SHARERADAR                   "cw"
#define     NET_DEFCON_REMOVEAI                     "cx"
#define     NET_DEFCON_OBJCLEARLASTACTION           "cy"
#define     NET_DEFCON_WHITEBOARD                   "c1"
#define     NET_DEFCON_TEAM_SCORE                   "c2"

#define     NET_DEFCON_SYNCHRONISE                  "s"



/*
 *	    CLIENT TO SERVER
 *      Data fields
 *
 */

#define     NET_DEFCON_TEAMID                       "db"
#define     NET_DEFCON_ALLIANCEID                   "dc"
#define     NET_DEFCON_OBJECTID                     "dd"
#define     NET_DEFCON_FLEETID                      "de"
#define     NET_DEFCON_TERRITORYID                  "df"
#define     NET_DEFCON_TEAMTYPE                     "dg"
#define     NET_DEFCON_STATE                        "dh"
#define     NET_DEFCON_TARGETOBJECTID               "di"
#define     NET_DEFCON_LONGITUDE                    "dj"
#define     NET_DEFCON_LATTITUDE                    "dk"
#define     NET_DEFCON_UNITTYPE                     "dl"
#define     NET_DEFCON_ACTIONTYPE                   "dm"
#define     NET_DEFCON_OPTIONID                     "dn"
#define     NET_DEFCON_OPTIONVALUE                  "do"
#define     NET_DEFCON_TARGETTEAMID                 "dp"
#define     NET_DEFCON_CHATCHANNEL                  "dq"
#define     NET_DEFCON_CHATMSG                      "dr"
#define     NET_DEFCON_VOTETYPE                     "ds"
#define     NET_DEFCON_VOTEDATA                     "dt"
#define     NET_DEFCON_SPECTATOR                    "du"
#define     NET_DEFCON_CHATMSGID                    "dv"
#define     NET_DEFCON_RANDSEED                     "dw"
#define     NET_DEFCON_LONGITUDE2                   "dx"
#define     NET_DEFCON_LATTITUDE2                   "dy"
#define     NET_DEFCON_SCORE                        "da"
#define     NET_DEFCON_LASTPROCESSEDSEQID           "z"
#define     NET_DEFCON_SYNCVALUE                    "v"


/*
 *	    SERVER TO CLIENT
 *      Commands and data
 */

#define     NET_DEFCON_DISCONNECT                   "sa"
#define     NET_DEFCON_CLIENTHELLO                  "sb"
#define     NET_DEFCON_CLIENTGOODBYE                "sc"
#define     NET_DEFCON_CLIENTID                     "sd"
#define     NET_DEFCON_TEAMASSIGN                   "se"
#define     NET_DEFCON_NETSYNCERROR                 "sf"
#define     NET_DEFCON_NETSYNCFIXED                 "sg"
#define     NET_DEFCON_UPDATE                       "sh"
#define     NET_DEFCON_PREVUPDATE                   "si"
#define     NET_DEFCON_NUMEMPTYUPDATES              "sj"
#define     NET_DEFCON_SPECTATORASSIGN              "sk"
#define     NET_DEFCON_SYNCERRORID                  "sl"
#define     NET_DEFCON_CLIENTISDEMO                 "sm"
#define     NET_DEFCON_SETMODPATH                   "sn"
#define     NET_DEFCON_VERSION                      "so"
#define     NET_DEFCON_SYSTEMTYPE                   "sp"
#define     NET_DEFCON_SEQID                        "i"
#define     NET_DEFCON_LASTSEQID                    "l"



/*
 *	    Other Data fields
 *
 */

#define     NET_DEFCON_FROMIP                       "FromIP"
#define     NET_DEFCON_FROMPORT                     "FromPort"



#endif          // NET_DEFCON_USEDEBUGSYMBOLS
#endif
