#ifndef LOBBYSLOTHANDLER_H
#define LOBBYSLOTHANDLER_H

#include "raceInstance.hpp"

struct lobbySlotHandler{

	unsigned int playerIDs[4];  // IDs of the players waiting to race
	unsigned int playerStates[4];  // Whether or not the player is ready. 0 = no player, 1 = waiting, 2 = ready

	lobbySlotHandler();

	bool raceReady();
	raceInstance generateRace();

};

#endif
