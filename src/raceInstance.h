#ifndef RACEINSTANCE_H
#define RACEINSTANCE_H

struct raceInstance{

	bool raceEmpty;
	unsigned int playerIDs[4];
	unsigned int totalPlayers;
	unsigned int playersFinished;

	raceInstance();

	float calculateRank(unsigned int racerID, unsigned int raceMap);

};

#endif
