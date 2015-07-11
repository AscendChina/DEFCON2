#ifndef _included_networkdefinesdebug_h
#define _included_networkdefinesdebug_h



/*
 *      All Defcon network messages should 
 *      have this name.  If not then they're
 *      not for us.
 *
 */


#define     NET_DEFCON_MESSAGE                      "DEFCON"
#define     NET_DEFCON_COMMAND                      "CMD"



/*
 *      CLIENT TO SERVER
 *      Commands
 *
 */


#define     NET_DEFCON_CLIENT_JOIN                  "ClientJoin"
#define     NET_DEFCON_CLIENT_LEAVE                 "ClientLeave"
#define     NET_DEFCON_START_GAME                   "StartGame"
#define     NET_DEFCON_REQUEST_TEAM                 "RequestTeam"
#define     NET_DEFCON_REQUEST_ALLIANCE             "RequestAlliance"
#define     NET_DEFCON_REQUEST_TERRITORY            "RequestTerritory"
#define     NET_DEFCON_REQUEST_FLEET                "RequestFleet"

#define     NET_DEFCON_OBJSTATECHANGE               "ObjStateChange"
#define     NET_DEFCON_OBJACTION                    "ObjAction"
#define     NET_DEFCON_OBJPLACEMENT                 "ObjPlacement"
#define     NET_DEFCON_OBJSPECIALACTION             "ObjSpecialAction"
#define     NET_DEFCON_OBJSETWAYPOINT               "ObjSetWaypoint"
#define     NET_DEFCON_OBJCLEARACTIONQUEUE          "ObjClrActions"
#define     NET_DEFCON_FLEETMOVE                    "FleetMove"

#define     NET_DEFCON_CHANGEOPTION                 "ChangeOption"
#define     NET_DEFCON_SETGAMESPEED                 "SetGameSpeed"
#define     NET_DEFCON_SETTEAMNAME                  "SetTeamName"
#define     NET_DEFCON_AGGRESSIONCHANGE             "AggrChange"
#define     NET_DEFCON_CHATMESSAGE                  "ChatMessage"
#define     NET_DEFCON_BEGINVOTE                    "BeginVote"
#define     NET_DEFCON_CASTVOTE                     "CastVote"
#define     NET_DEFCON_CEASEFIRE                    "Ceasefire"
#define     NET_DEFCON_SHARERADAR                   "ShareRadar"
#define     NET_DEFCON_REMOVEAI                     "RemoveAITeam"
#define     NET_DEFCON_CLEARLASTACTION              "ClearLastAction"

#define     NET_DEFCON_SYNCHRONISE                  "Sync"



/*
 *	    CLIENT TO SERVER
 *      Data fields
 *
 */

#define     NET_DEFCON_TEAMID                       "TeamId"
#define     NET_DEFCON_ALLIANCEID                   "AllianceId"
#define     NET_DEFCON_OBJECTID                     "ObjId"
#define     NET_DEFCON_FLEETID                      "FleetId"
#define     NET_DEFCON_TERRITORYID                  "TerritoryId"
#define     NET_DEFCON_TEAMTYPE                     "TeamType"
#define     NET_DEFCON_STATE                        "State"
#define     NET_DEFCON_TARGETOBJECTID               "TargetObjId"
#define     NET_DEFCON_LONGITUDE                    "Longitude"
#define     NET_DEFCON_LATTITUDE                    "Lattitude"
#define     NET_DEFCON_UNITTYPE                     "UnitType"
#define     NET_DEFCON_ACTIONTYPE                   "ActionType"
#define     NET_DEFCON_OPTIONID                     "OptionID"
#define     NET_DEFCON_OPTIONVALUE                  "OptionValue"
#define     NET_DEFCON_TARGETTEAMID                 "TargetTeamId"
#define     NET_DEFCON_CHATCHANNEL                  "ChatChannel"
#define     NET_DEFCON_CHATMSG                      "ChatMsg"
#define     NET_DEFCON_VOTETYPE                     "VoteType"
#define     NET_DEFCON_VOTEDATA                     "VoteData"
#define     NET_DEFCON_LASTPROCESSEDSEQID           "LastProcSeqId"
#define     NET_DEFCON_SYNCVALUE                    "SyncValue"


/*
 *	    SERVER TO CLIENT
 *      Commands and data
 */

#define     NET_DEFCON_DISCONNECT                   "Disconnect"
#define     NET_DEFCON_CLIENTHELLO                  "ClientHello"
#define     NET_DEFCON_CLIENTGOODBYE                "ClientGoodbye"
#define     NET_DEFCON_CLIENTID                     "ClientID"
#define     NET_DEFCON_TEAMASSIGN                   "TeamAssign"
#define     NET_DEFCON_NETSYNCERROR                 "SyncError"
#define     NET_DEFCON_UPDATE                       "Update"
#define     NET_DEFCON_PREVUPDATE                   "PrevUpdate"

#define     NET_DEFCON_SEQID                        "SeqId"
#define     NET_DEFCON_LASTSEQID                    "LastSeqId"



/*
 *	    Other Data fields
 *
 */

#define     NET_DEFCON_FROMIP                       "FromIP"
#define     NET_DEFCON_FROMPORT                     "FromPort"



#endif
