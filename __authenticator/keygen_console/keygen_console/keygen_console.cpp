#include "sha512.h"
#include <iostream>
#include <string>
#include <ctime>
#include <algorithm>

using namespace std;

int main()
{

	string challengeStr;

	cout << "Please enter your CHALLENGE CODE: ";
	getline(cin, challengeStr);

	time_t rawtime;
	tm timeinfo;
	char buffer[80];
	const int len = 5, parts = 2;

	srand(1);

	char srandChar[10];
	for (int i = 0; i < 10; i++) {
		srandChar[i] = rand() % 10;
	}
	string srandStr(srandChar);

	memset(&buffer[0], '\0', sizeof(buffer));

	time(&rawtime);
	localtime_s(&timeinfo ,&rawtime);

	strftime(buffer, 80, "%Y%m%d%H%M", &timeinfo);
	string time(buffer);

	transform(challengeStr.begin(), challengeStr.end(), challengeStr.begin(), ::toupper);


	string activation = sha512(srandStr + time + challengeStr);

	string activationShort = string(len * parts + parts - 1, '0');
	activationShort.clear();
	for (int i = 0; i < parts; ++i) {
		for (int j = 0; j < len; ++j) {
			activationShort.push_back(activation[len * i + j]);
		}
		activationShort.push_back('-');
	}
	activationShort.pop_back();
	transform(activationShort.begin(), activationShort.end(), activationShort.begin(), ::toupper);
	

	cout << endl << "This is your your ACTIVATION CODE: " << activationShort << endl;

	getchar();

	return 0;
}