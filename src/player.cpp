#include "player.hpp"
#include <sstream>

player::player(){

	user = "undefined";
	rank = 0.f;
	headNum = 1;
	bodyNum = 1;
	footNum = 1;
	speedPoints = 0;
	jumpPoints = 0;
	tractionPoints = 0;

	roomID = 0;
	raceMap = 0;
	raceSlot = 0;

}

bool player::infoIsValid(const char buffer[2048]){

	// Player data is sent in the following format:
	// nid`name`rank`head`body`foot`speed`jump`traction

	// Parses the player data buffer backwards so we know exactly where
	// the username starts and ends, even if it contains a grave accent
	std::string playerInfo = buffer;
	unsigned int lastGrave;
	unsigned int grave = playerInfo.length();
	for(unsigned int d = 0; d <= 7; d++){
		lastGrave = grave - 1;
		grave = playerInfo.rfind("`", lastGrave - 1);
		switch(d){
			case(0):
				std::istringstream(playerInfo.substr(grave + 1)) >> tractionPoints;
			break;
			case(1):
				std::istringstream(playerInfo.substr(grave + 1, lastGrave - grave)) >> jumpPoints;
			break;
			case(2):
				std::istringstream(playerInfo.substr(grave + 1, lastGrave - grave)) >> speedPoints;
			break;
			case(3):
				std::istringstream(playerInfo.substr(grave + 1, lastGrave - grave)) >> footNum;
			break;
			case(4):
				std::istringstream(playerInfo.substr(grave + 1, lastGrave - grave)) >> bodyNum;
			break;
			case(5):
				std::istringstream(playerInfo.substr(grave + 1, lastGrave - grave)) >> headNum;
			break;
			case(6):
				std::istringstream(playerInfo.substr(grave + 1, lastGrave - grave)) >> rank;
				user = playerInfo.substr(1, grave - 1);
			break;
		}
	}

	// EXTREMELY convoluted if statement to validate the player data
	if(user.length() > 0 && user.find("<") == std::string::npos && user.find("&#0;") == std::string::npos && user.find("`") == std::string::npos &&
	headNum >= 1 && headNum <= 11 && bodyNum >= 1 && bodyNum <= 11 && footNum >= 1 && footNum <= 11 &&
	speedPoints + jumpPoints + tractionPoints <= 150 && speedPoints <= 100 && jumpPoints <= 100 && tractionPoints <= 100){
		return true;
	}

	return false;

}
