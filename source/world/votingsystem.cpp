#include "lib/universal_include.h"
#include "lib/hi_res_time.h"
#include "lib/resource/resource.h"

#include "world/votingsystem.h"
#include "world/world.h"

#include "app/globals.h"
#include "app/app.h"
#include "app/game.h"
#include "app/statusicon.h"

#include "lib/eclipse/eclipse.h"
#include "lib/language_table.h"

#include "interface/alliances_window.h"
#include "interface/interface.h"



VotingSystem::VotingSystem()
: m_updateTimer(0)
{
}


void VotingSystem::RegisterNewVote( int _createTeamId, int _voteType, int _voteData )
{
    //
    // Game over?

    if( g_app->GetGame()->m_winner != -1 ) 
    {
        return;
    }


    //
    // Does this vote already exist?

    for( int i = 0; i < m_votes.Size(); ++i )
    {
        if( m_votes.ValidIndex(i) )
        {
            Vote *vote = m_votes[i];
            if( vote->m_result == Vote::VoteUnknown &&
                vote->m_createTeamId == _createTeamId &&
                vote->m_voteType == _voteType &&
                vote->m_voteData == _voteData )
            {
                // Already exists sucker
                return;
            }
        }
    }


    Vote *vote = new Vote();
    vote->m_createTeamId = _createTeamId;
    vote->m_voteType = _voteType;
    vote->m_voteData = _voteData;

    if( _voteType == Vote::VoteTypeKickPlayer ||
        _voteType == Vote::VoteTypeLeaveAlliance )
    {
        vote->m_votes[_createTeamId] = Vote::VoteYes;
    }

    int voteId = m_votes.PutData( vote );

    Team *myTeam = g_app->GetWorld()->GetMyTeam();
    bool canSeeVote = false;

    if( myTeam )
    {
        switch( vote->m_voteType )
        {
            case Vote::VoteTypeJoinAlliance:
                if( myTeam->m_teamId == vote->m_createTeamId )
                {
                    canSeeVote = true;
                }
                else if( myTeam->m_allianceId == vote->m_voteData )
                {
                    canSeeVote = true;
                }
                break;

            case Vote::VoteTypeKickPlayer:
            {
                Team *kickTeam = g_app->GetWorld()->GetTeam(vote->m_voteData);
                if( myTeam->m_teamId == vote->m_createTeamId )
                {
                    canSeeVote = true;
					// We made this kick vote, update stats
                }
                else if( myTeam->m_allianceId == kickTeam->m_allianceId &&
                         myTeam->m_teamId != kickTeam->m_teamId )
                {
                    canSeeVote = true;
                }
                break;
            }
        }
    }

    if( canSeeVote )
    {
        if( !EclGetWindow("Alliances") )
        {
            EclRegisterWindow( new AlliancesWindow() );
        }

        VotingWindow *vote = (VotingWindow *) EclGetWindow("Vote");
        if( !vote || vote->m_voteId != voteId )
        {
            VotingWindow *window = new VotingWindow();
            window->m_voteId = voteId;
            EclRegisterWindow( window );
        }
    }
}


void VotingSystem::CastVote( int _teamId, int _voteId, int _vote )
{
    if( m_votes.ValidIndex(_voteId) )
    {
        Vote *vote = m_votes[_voteId];
        vote->m_votes[_teamId] = _vote;
    }
}


Vote *VotingSystem::LookupVote( int _voteId )
{
    if( m_votes.ValidIndex(_voteId) )
    {
        return m_votes[_voteId];
    }

    return NULL;
}


void VotingSystem::Update()
{
    if( g_app->GetGame()->m_winner != -1 ) 
    {
        return;
    }

    m_updateTimer += SERVER_ADVANCE_PERIOD;
    if( m_updateTimer >= 1 )
    {
        m_updateTimer = 0;

        for( int i = 0; i < m_votes.Size(); ++i )
        {
            if( m_votes.ValidIndex(i) )
            {
                Vote *vote = m_votes[i];
                if( vote->m_result == Vote::VoteUnknown )
                {
                    vote->m_timer -= 1;
                    int required = vote->GetVotesRequired();
                    int yes, no, abstain;
                    vote->GetCurrentVote( &yes, &no, &abstain );
                
                    if( yes >= required )
                    {
                        vote->Finish( Vote::VoteYes );
                    }
                    else if( no >= required || abstain == 0 || vote->m_timer <= 0 )
                    {
                        vote->Finish( Vote::VoteNo );
                    }
                }                
            }
        }
    }
}


// ============================================================================


Vote::Vote()
:   m_voteType(VoteTypeInvalid),
    m_result(VoteUnknown),
    m_resultYes(0),
    m_resultNo(0),
    m_resultAbstain(0)
{
    m_votes.Initialise( MAX_TEAMS );

    for(int i = 0; i < MAX_TEAMS; ++i )
    {
        m_votes[i] = VoteAbstain;
    }

    m_timer = 60.0f;
}


int Vote::GetRequiredAllianceId()
{
    int requiredAllianceId = -1;

    if( m_voteType == VoteTypeJoinAlliance ) 
    {
        requiredAllianceId = m_voteData;
    }

    if( m_voteType == VoteTypeKickPlayer ||
        m_voteType == VoteTypeLeaveAlliance )
    {
        Team *kickMe = g_app->GetWorld()->GetTeam(m_voteData);
        requiredAllianceId = kickMe->m_allianceId;
    }

    return requiredAllianceId;
}


bool Vote::CanSeeVote( int _teamId )
{
    Team *team = g_app->GetWorld()->GetTeam(_teamId);
    if( !team ) return false;

    int reqAllianceId = GetRequiredAllianceId();

    switch( m_voteType )
    {
        case VoteTypeJoinAlliance:
            return( team->m_allianceId == reqAllianceId ||
                    team->m_teamId == m_createTeamId );

        case VoteTypeKickPlayer:
            if( team->m_teamId == m_voteData ) 
            {
                return false;
            }
            return team->m_allianceId == reqAllianceId;

        case VoteTypeLeaveAlliance:
            return( team->m_teamId == m_voteData );
    }

    return false;
}


int Vote::GetCurrentVote( int _teamId )
{
    return m_votes[_teamId];
}


int Vote::GetVotesRequired()
{
    if( m_voteType == VoteTypeLeaveAlliance )
    {
        return 1;
    }
    
    int allianceId = GetRequiredAllianceId();
    int numMembers = 0;

    for( int i = 0; i < g_app->GetWorld()->m_teams.Size(); ++i )
    {
        Team *team = g_app->GetWorld()->m_teams[i];
        if( team->m_allianceId == allianceId )
        {
            ++numMembers;
        }
    }

    if( m_voteType == VoteTypeKickPlayer ) numMembers--;
    
    return( ceil((numMembers+0.5f) / 2.0f) );
}


void Vote::GetCurrentVote( int *_yes, int *_no, int *_abstain )
{
    if( m_result == VoteUnknown )
    {
        *_yes = 0;
        *_no = 0;
        *_abstain = 0;

        int requiredAllianceId = GetRequiredAllianceId();

        for( int i = 0; i < g_app->GetWorld()->m_teams.Size(); ++i )
        {
            Team *team = g_app->GetWorld()->m_teams[i];

            if( m_voteType == VoteTypeKickPlayer && m_voteData == i )
            {
                // This is the player being kicked, hence his vote is not counted
                continue;
            }

            if( team->m_allianceId == requiredAllianceId )
            {       
                int thisVote = m_votes[team->m_teamId];
                if( thisVote == VoteYes )       (*_yes) ++;
                if( thisVote == VoteNo )        (*_no) ++;
                if( thisVote == VoteAbstain )   (*_abstain) ++;
            }
        }
    }
    else
    {
        *_yes = m_resultYes;
        *_no = m_resultNo;
        *_abstain = m_resultAbstain;
    }
}


void Vote::Finish( int _result )
{
    GetCurrentVote( &m_resultYes, &m_resultNo, &m_resultAbstain );

    m_result = _result;

    if( m_result == VoteYes )
    {
        Team *team = NULL;
        char msg[512];

        switch( m_voteType )
        {
            case VoteTypeJoinAlliance:
            {
                team = g_app->GetWorld()->GetTeam( m_createTeamId );
                team->m_allianceId = m_voteData;
                team->m_alwaysSolo = false;

                sprintf( msg, LANGUAGEPHRASE("message_alliance_join") );
                LPREPLACESTRINGFLAG('P', team->GetTeamName(), msg);

                char *allianceName = g_app->GetWorld()->GetAllianceName(m_voteData);

                LPREPLACESTRINGFLAG('N', allianceName, msg );
                break;
            }

            case VoteTypeKickPlayer:
            {
                team = g_app->GetWorld()->GetTeam( m_voteData );

                sprintf( msg, LANGUAGEPHRASE("message_alliance_kicked") );
                LPREPLACESTRINGFLAG('P', team->GetTeamName(), msg);

                char *allianceName = g_app->GetWorld()->GetAllianceName(team->m_allianceId);

                LPREPLACESTRINGFLAG('N', allianceName, msg );

                team->m_allianceId = g_app->GetWorld()->FindFreeAllianceId();

                break;
            }

            case VoteTypeLeaveAlliance:
            {
                team = g_app->GetWorld()->GetTeam( m_voteData );

                sprintf( msg, LANGUAGEPHRASE("message_alliance_leave") );
                LPREPLACESTRINGFLAG('P', team->GetTeamName(), msg);

                char *allianceName = g_app->GetWorld()->GetAllianceName(team->m_allianceId);

                LPREPLACESTRINGFLAG('N', allianceName, msg );

                for( int i = 0; i < MAX_TEAMS; ++i )
                {
                    Team *allianceTeam = g_app->GetWorld()->GetTeam( i );
                    if( allianceTeam )
                    {
                        if( team->m_allianceId == allianceTeam->m_allianceId )
                        {
                            team->m_leftAllianceTimer[i] = GetHighResTime();
                        }
                    }
                }

                team->m_allianceId = g_app->GetWorld()->FindFreeAllianceId();
                break;
            }
        }

        strupr( msg );
        g_app->GetInterface()->ShowMessage( 0, 0, -1, msg, true );
        
        if( g_app->m_hidden )
        {
            g_app->GetStatusIcon()->SetCaption( msg );
            g_app->GetStatusIcon()->SetSubIcon( STATUS_ICON_EVENT );
        }

        g_resource->DeleteDisplayList( "MapCountryControl" );


        //
        // Update everyones CeaseFire status
        // Update everyones radar sharing 
        // Note : we only ever turn off radar sharing.  We never turn it on by default

        for(int i = 0; i < g_app->GetWorld()->m_teams.Size(); ++i )
        {
            Team *thisTeam = g_app->GetWorld()->m_teams[i];

            bool isAFriend = thisTeam->m_teamId != team->m_teamId && 
                             g_app->GetWorld()->IsFriend( team->m_teamId, thisTeam->m_teamId );

            team->m_ceaseFire[thisTeam->m_teamId] = isAFriend;
            thisTeam->m_ceaseFire[team->m_teamId] = isAFriend;
            
            if( !isAFriend )
            {
                team->m_sharingRadar[thisTeam->m_teamId] = isAFriend;
                thisTeam->m_sharingRadar[team->m_teamId] = isAFriend;
            }
        }
    }
}
