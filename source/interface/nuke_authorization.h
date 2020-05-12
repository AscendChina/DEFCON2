#ifndef _included_nuke_authorization_H
#define _included_nuke_authorization_H
#include <string>
#include <ctime>
#include "sha512.h"

class nuke_authorization
{
static const int len = 5, parts = 2;
private:
	char buffer[80], srandChar[10], char_array[10 + len * parts + parts];
	std::time_t rawtime, newTime;
	std::tm* timeinfo;
	std::string challengeStr, activation, activationShort;
public:
	nuke_authorization(){};
    std::string makeChallengeCode(int teamId);

	void printCode(std::string challengeStr,int teamId);

	bool challenge(std::string challengeKeygen, int teamId);
};
#endif