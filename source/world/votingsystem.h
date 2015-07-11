#ifndef _included_votingsystem_h
#define _included_votingsystem_h

#include "lib/tosser/darray.h"
#include "lib/tosser/bounded_array.h"
#include "lib/math/fixed.h"

class Vote;


// ============================================================================
// class VotingSystem


class VotingSystem
{
public:
    DArray<Vote *> m_votes;

    Fixed m_updateTimer;
    
public:
    VotingSystem();

    void Update();
    
    void RegisterNewVote    ( int _createTeamId, int _voteType, int _voteData );
    void CastVote           ( int _teamId, int _voteId, int _vote );

    Vote    *LookupVote     ( int _voteId );
};




// ============================================================================
// class Vote

class Vote 
{
public:
    enum
    {
        VoteUnknown,
        VoteYes,
        VoteNo,
        VoteAbstain
    };

    enum
    {
        VoteTypeInvalid,
        VoteTypeJoinAlliance,
        VoteTypeKickPlayer,
        VoteTypeLeaveAlliance
    };

public:
    int             m_createTeamId;
    int             m_voteType;
    int             m_voteData;
    float           m_timer;
    
    BoundedArray    <int> m_votes;                         // Current vote for each team

    int             m_result;
    int             m_resultYes;
    int             m_resultNo;
    int             m_resultAbstain;
    
public:
    Vote();

    int  GetRequiredAllianceId();
    bool CanSeeVote( int _teamId );

    int  GetCurrentVote( int _teamId );
    int  GetVotesRequired();

    void GetCurrentVote( int *_yes, int *_no, int *_abstain );


    void Finish( int _result );
};



#endif
