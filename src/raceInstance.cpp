#include "raceInstance.h"

raceInstance::raceInstance(){
	raceEmpty = true;
	playerIDs[0] = 0;
	playerIDs[1] = 0;
	playerIDs[2] = 0;
	playerIDs[3] = 0;
	totalPlayers = 0;
	playersFinished = 0;
}

float raceInstance::calculateRank(unsigned int racerID, unsigned int raceMap){

	playersFinished++;

	unsigned int rankMultiplier = 0;
	switch(raceMap){
		case 1:
			rankMultiplier = 1;  // Newbieland (x1)
		break;
		case 2:
			rankMultiplier = 3;  // Buto (x3)
		break;
		case 3:
			rankMultiplier = 5;  // Pyramids (x5)
		break;
		case 4:
			rankMultiplier = 2;  // Robocity (x2)
		break;
		case 5:
			rankMultiplier = 5;  // Assembly (x5)
		break;
		case 6:
			rankMultiplier = 8;  // Infernal Hop (x8)
		break;
		case 7:
			rankMultiplier = 3;  // Going Down (x3)
		break;
		case 8:
			rankMultiplier = 7;  // Slip (x7)
		break;
	}

	return totalPlayers / (float)(2 << (playersFinished - 1)) * rankMultiplier;  // Ranking algorithm from PR1

}
