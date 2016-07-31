#include "lobbySlotHandler.h"

lobbySlotHandler::lobbySlotHandler(){
	playerIDs[0]	= 0; playerIDs[1]	= 0; playerIDs[2]	= 0; playerIDs[3]	= 0;
	playerStates[0] = 0; playerStates[1] = 0; playerStates[2] = 0; playerStates[3] = 0;
}

bool lobbySlotHandler::raceReady(){

	bool players = false;
	for(unsigned int d = 0; d < 4; d++){
		if(playerStates[d] == 1){  // If a player is still waiting (state = 1), the race is not ready
			return false;
		}else if(playerStates[d] == 2){  // If a player is ready (state = 1), there are players in the race who are ready to go
			players = true;
		}
	}
	return players;  // If everyone is ready and there is atleast one person, the race may start ready

}

raceInstance lobbySlotHandler::generateRace(){

	// Create a new raceInstance and add the players to it
	raceInstance newRace;
	newRace.raceEmpty = false;

	if(playerStates[0] == 2){
		newRace.playerIDs[0] = playerIDs[0];
		newRace.totalPlayers++;
	}
	if(playerStates[1] == 2){
		newRace.playerIDs[1] = playerIDs[1];
		newRace.totalPlayers++;
	}
	if(playerStates[2] == 2){
		newRace.playerIDs[2] = playerIDs[2];
		newRace.totalPlayers++;
	}
	if(playerStates[3] == 2){
		newRace.playerIDs[3] = playerIDs[3];
		newRace.totalPlayers++;
	}

	// Clear the lobby slots
	playerIDs[0] = 0;
	playerStates[0] = 0;
	playerIDs[1] = 0;
	playerStates[1] = 0;
	playerIDs[2] = 0;
	playerStates[2] = 0;
	playerIDs[3] = 0;
	playerStates[3] = 0;

	return newRace;

}
