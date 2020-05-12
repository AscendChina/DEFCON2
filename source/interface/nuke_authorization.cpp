#include "lib/universal_include.h"
#include "app/app.h"
#include "app/globals.h"
#include "app/game.h"

#include "world/world.h"

#include "nuke_authorization.h"

void gen_random(std::string* s, const int len, const int parts) 
{
	static const char alphanum[] =
				   "0123456789"
				   //"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
				   "abcdefghijklmnopqrstuvwxyz";

	s->clear();
	for (int i = 0; i < parts; ++i) {
		for (int j = 0; j < len; ++j) {
			s->push_back(alphanum[rand() % (sizeof(alphanum) - 1)]);
			}
		s->push_back('-');
	}
	s->pop_back();
}
std::string nuke_authorization::makeChallengeCode(int teamId)
{
	challengeStr = std::string(len * parts + parts - 1, '0');

	srand(time(NULL));

	gen_random(&challengeStr, len, parts);
	g_app->GetWorld()->GetTeam(teamId)->challengeStr = challengeStr;

	return challengeStr;
}
void nuke_authorization::printCode(std::string challengeStrP,int teamId)
{
	challengeStrP.insert(0,"Challenge: ");
	
	strcpy(char_array, challengeStrP.c_str()); 
	g_app->GetWorld()->AddChatMessage( teamId, CHATCHANNEL_PRIVATE_SYS, char_array, -1, false );

	g_app->GetWorld()->GetTeam(teamId)->nuclearCodesPrinted = true;
}

bool nuke_authorization::challenge(std::string challengeKeygen, int teamId)
{
	srand(1);
				
	for (int i = 0; i < 10; i++) {
		srandChar[i] = rand() % 10;
	}
	std::string srandStr(srandChar);


	std::time(&rawtime);
		
	challengeStr = g_app->GetWorld()->GetTeam(teamId)->challengeStr;

	std::transform(challengeStr.begin(), challengeStr.end(), challengeStr.begin(), ::toupper);

	for(int i=0;i<5;i++)
	{
		memset(&buffer[0], '\0', sizeof(buffer));

		newTime = rawtime - (60 * i);
		timeinfo = std::localtime(&newTime);

		std::strftime(buffer, 80, "%Y%m%d%H%M", timeinfo);
		std::string time(buffer);

					
		activation = sha512(srandStr + time + challengeStr);

		activationShort = std::string(len * parts + parts - 1, '0');
		activationShort.clear();

		for (int i = 0; i < parts; ++i) 
		{
			for (int j = 0; j < len; ++j) 
			{
				activationShort.push_back(activation[len * i + j]);
			}
			activationShort.push_back('-');
		}
		activationShort.pop_back();

		std::transform(activationShort.begin(), activationShort.end(), activationShort.begin(), ::toupper);
		std::transform(challengeKeygen.begin(), challengeKeygen.end(), challengeKeygen.begin(), ::toupper);

		if (challengeKeygen == activationShort) {

			g_app->GetWorld()->GetTeam(teamId)->nuclearCodesAuthenticated = true;

			return true;
		}
	}
	return false;
}