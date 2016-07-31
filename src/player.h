#ifndef PLAYER_H
#define PLAYER_H

#include <string>

struct player{

	std::string user;
	float rank;
	unsigned int headNum;
	unsigned int bodyNum;
	unsigned int footNum;
	unsigned int speedPoints;
	unsigned int jumpPoints;
	unsigned int tractionPoints;

	unsigned int roomID;  // Stores the position + 1 of the race the player is doing in the currentRaces vector (0 = in the lobby)
	unsigned int raceMap;  // Stores which map the player is waiting to play or playing
	unsigned int raceSlot;  // Stores which slot in the race / lobby the player is in

	unsigned int x, y;
	unsigned int item;

	player();

	bool infoIsValid(const char buffer[2048]);

};

#endif
