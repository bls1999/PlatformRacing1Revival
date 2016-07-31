#include "socketServer.h"
#include <stdio.h>
#include <fstream>
#include <sstream>

int inet_pton(int af, const char *src, char *dst);
bool playerInfoIsValid(const char buffer[2048]);

socketServer::socketServer(){
	ip[0] = '\0';
	port = DEFAULT_PORT;
}

socketServer::~socketServer(){

	for(unsigned int d = 0; d < connectedSockets.size(); d++){
		closesocket(connectedSockets.at(d));
	}
	FD_ZERO(&socketSet);

	#ifdef _WIN32
		WSACleanup();
	#endif

}

void socketServer::reportError(const char *failedFunction, int errorCode){

	printf("\nSocket function %s has failed: %i\nSee here for more information:\nhttps://msdn.microsoft.com/en-us/library/windows/desktop/ms740668%%28v=vs.85%%29.aspx\n",
		   failedFunction, errorCode);

}

bool socketServer::loadConfig(const char *prgPath){

	char *cfgPath = (char*)prgPath;
	cfgPath[strrchr(cfgPath, '\\') - cfgPath + 1] = '\0';  // Removes program name (everything after the last backslash) from the path
	strcpy(cfgPath + strlen(cfgPath), "config.txt");  // Append "config.txt" to the end

	std::ifstream serverConfig(cfgPath);
	std::string line;

	if(serverConfig.is_open()){
		while(serverConfig.good()){

			getline(serverConfig, line);

			// Remove any comments from the line
			unsigned int commentPos = line.find("//");
			if(commentPos != std::string::npos){
				line.erase(commentPos);
			}

			if(line.length() >= 12 && line.substr(0, 5) == "ip = "){
				strcpy(ip, line.substr(5).c_str());

			}else if(line.length() >= 8 && line.substr(0, 7) == "port = "){
				std::istringstream(line.substr(7)) >> port;

			}else if(line.length() >= 8 && line.substr(0, 7) == "motd = "){
				motd = "^0`&#0;`" + line.substr(7) + "\n";

			}

		}

		serverConfig.close();
		printf("Config loaded.\n");
		return 1;

	}else{
		printf("Specified config path is invalid. ");
	}

	printf("No config loaded.\n");
	return 0;

}

bool socketServer::initServer(const int argc, const char *argv[]){

	/* Load server config */
	if(argc > 0){
		loadConfig(argv[0]);
	}


	/* Specify the version of Winsock to use and initialize it */
	#ifdef _WIN32
		WSADATA wsaData;
		int initError = WSAStartup(WINSOCK_VERSION, &wsaData);

		if(initError != 0){  // If Winsock did not initialize correctly, abort
			reportError("WSAStartup()", initError);
			return 0;
		}
	#endif


	/* Create a socket prototype for the master socket */
	/*
	   socket(address family, type, protocol)
	   address family = AF_UNSPEC, which can be either IPv4 or IPv6
	   type = SOCK_STREAM, which uses TCP
	   protocol = IPPROTO_TCP, specifies to use TCP
	*/
	masterSocket = socket(AF_UNSPEC, SOCK_STREAM, IPPROTO_TCP);

	if(masterSocket == INVALID_SOCKET){  // If socket() failed, abort
		reportError("socket()", WSAGetLastError());
		WSACleanup();
		return 0;
	}


	/* Bind the master socket to the host address */
	WSAPROTOCOL_INFO protocolInfo;
	WSADuplicateSocket(masterSocket, GetCurrentProcessId(), &protocolInfo);  // Retrieve details about the master socket (we want the address family being used)

	sockaddr_in serverAddress;
	serverAddress.sin_family = protocolInfo.iAddressFamily;
	if(strlen(ip) > 0){  // If the IP has been specified, convert it from a string to the in_addr format for sockaddr_in
		inet_pton(protocolInfo.iAddressFamily, ip, (char*)&(serverAddress.sin_addr));
	}else{	  // Otherwise use all available addresses
		serverAddress.sin_addr.s_addr = INADDR_ANY;
	}
	serverAddress.sin_port = htons(port);  // htons() converts the port from big-endian to little-endian for sockaddr_in

	if(bind(masterSocket, (sockaddr*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR){  // If bind() failed, abort
		reportError("bind()", WSAGetLastError());
		WSACleanup();
		return 0;
	}


	/* Change the master socket's state to "listen" so it will start listening for incoming connections from sockets */
	if(listen(masterSocket, SOMAXCONN) == SOCKET_ERROR){  // SOMAXCONN = automatically choose maximum number of pending connections, different across systems
		reportError("listen()", WSAGetLastError());
		WSACleanup();
		return 0;
	}


	printf("Server is up!\n\n");
	return 1;

}

void socketServer::handleConnections(){

	/* Empties and refills socketSet, as select() may have modified it */
	FD_ZERO(&socketSet);
	FD_SET(masterSocket, &socketSet);
	for(unsigned int d = 0; d < connectedSockets.size(); d++){
		FD_SET(connectedSockets.at(d), &socketSet);
	}


	// Checks which sockets have changed state, and removes the ones that haven't from socketSet
	int changedSockets = select(0, &socketSet, NULL, NULL, NULL);

	if(changedSockets != SOCKET_ERROR){  // If select() did not return SOCKET_ERROR (-1), all is fine

		if(changedSockets > 0){  // Only continue if there are sockets that have changed state

			/* If the master socket has changed state, there is an incoming connection. Accept the connection if the socket is valid */
			if(FD_ISSET(masterSocket, &socketSet)){

				SOCKET clientSocket = accept(masterSocket, NULL, NULL);

				if(clientSocket != INVALID_SOCKET){
					FD_SET(clientSocket, &socketSet);
					connectedSockets.push_back(clientSocket);
					printf("Accepted connection from socket #%i.\n", clientSocket);
				}else{
					reportError("accept()", WSAGetLastError());
				}

			}

			/* Receive data from and send data to connected sockets */
			for(unsigned int d = 0; d < connectedSockets.size(); d++){  // Loop through each connected socket

				if(FD_ISSET(connectedSockets.at(d), &socketSet)){  // Check if the socket actually has changed state

					// Receives up to 2,048 bytes of data from a client socket and stores it in lastBuffer
					recvBytes = recv(connectedSockets.at(d), lastBuffer, 2048, 0);

					if(recvBytes == -1){  // Error encountered, disconnect problematic socket

						reportError("recv()", WSAGetLastError());
						printf("Closing connection with socket #%i.\n\n", connectedSockets.at(d));
						disconnectSocket(d);
						d--;

					}else if(recvBytes == 0){  // If the buffer is empty, the connection has closed

						printf("Socket #%i has disconnected, closing connection.\n", connectedSockets.at(d));
						disconnectSocket(d);
						d--;

					}else{

						handleBuffer(d);  // Do something with the received data

					}

				}

			}

		}

	}else{

		reportError("select()", WSAGetLastError());

	}

}

void socketServer::handleBuffer(unsigned int senderNum){

	if(lastBuffer[0] == 'n'){  // Client connected or changed player data

		player newPlayer;
		if(newPlayer.infoIsValid(lastBuffer)){  // Validate player data

			if(senderNum == playerData.size()){  // If the player is new, add them to the playerData vector

				playerData.push_back(newPlayer);

				std::ostringstream ss; ss << "i" << connectedSockets.at(senderNum);
				if(send(connectedSockets.at(senderNum), ss.str().c_str(), ss.str().length() + 1, 0) < 0){  // Acknowledge connection and return player ID
					reportError("send()", WSAGetLastError());
				}

			}else{
				if(playerData.at(senderNum).rank == newPlayer.rank){  // Make sure the player's rank has not changed

					playerData.at(senderNum) = newPlayer;  // If all is good, update the player's information

					// Generate a player data buffer using the new information provided
					std::ostringstream ss;
					ss << "p" << connectedSockets.at(senderNum) << "`" << playerData.at(senderNum).user << "`" << playerData.at(senderNum).rank
					   << "`" << playerData.at(senderNum).headNum << "`" << playerData.at(senderNum).bodyNum << "`" << playerData.at(senderNum).footNum
					   << "`" << playerData.at(senderNum).speedPoints << "`" << playerData.at(senderNum).jumpPoints << "`" << playerData.at(senderNum).tractionPoints;

					// Send the new player data to all clients who aren't racing
					for(unsigned int d = 0; d < playerData.size(); d++){
						if(playerData.at(d).roomID == 0){
							if(send(connectedSockets.at(d), ss.str().c_str(), ss.str().length() + 1, 0) < 0){
								reportError("send()", WSAGetLastError());
							}
						}
					}

				}else{  // If it has changed without the server's knowledge, disconnect them (not really a good solution)

					printf("Socket #%i has sent suspicious player data, closing connection.\n", connectedSockets.at(senderNum));
					disconnectSocket(senderNum);

				}
			}

		}else{  // If the player data isn't valid, disconnect them

			printf("Socket #%i has sent suspicious player data, closing connection.\n", connectedSockets.at(senderNum));
			disconnectSocket(senderNum);

		}

	}else if(senderNum < playerData.size()){  // Make sure the socket has registered valid player data

		if(lastBuffer[0] == 'o'){  // Someone has joined the lobby

			if(playerData.at(senderNum).roomID != 0){  // If the player has finished a singleplayer race, call leaveRace()
				leaveRace(senderNum);
			}

			// Generate the sender's player data buffer
			std::ostringstream ss;
			ss << "p" << connectedSockets.at(senderNum) << "`" << playerData.at(senderNum).user << "`" << playerData.at(senderNum).rank
			   << "`" << playerData.at(senderNum).headNum << "`" << playerData.at(senderNum).bodyNum << "`" << playerData.at(senderNum).footNum
			   << "`" << playerData.at(senderNum).speedPoints << "`" << playerData.at(senderNum).jumpPoints << "`" << playerData.at(senderNum).tractionPoints;
			std::string senderData = ss.str();

			/* Sends the requestor's information to the other clients and the other clients' information to the requestor */
			for(unsigned int d = 0; d < playerData.size(); d++){

				/* Send the requestor's information to player d */
				if(send(connectedSockets.at(d), senderData.c_str(), senderData.length() + 1, 0) < 0){
					reportError("send()", WSAGetLastError());
				}

				if(d != senderNum){

					/* Send player d's information to the requestor */
					// Generate player d's player data buffer
					ss.str(std::string());  // Clear stringstream for next usage
					ss << "p" << connectedSockets.at(d) << "`" << playerData.at(d).user << "`" << playerData.at(d).rank
					   << "`" << playerData.at(d).headNum << "`" << playerData.at(d).bodyNum << "`" << playerData.at(d).footNum
					   << "`" << playerData.at(d).speedPoints << "`" << playerData.at(d).jumpPoints << "`" << playerData.at(d).tractionPoints;

					// Send it to the requestor
					if(send(connectedSockets.at(senderNum), ss.str().c_str(), ss.str().length() + 1, 0) < 0){
						reportError("send()", WSAGetLastError());
					}

					// Check if player d is waiting for a race to start
					if(playerData.at(d).roomID == 0 && playerData.at(d).raceMap != 0 && playerData.at(d).raceSlot != 0){

						/* Tell the requestor which slot of which race player d is in */
						ss.str(std::string());  // Clear stringstream for next usage
						ss << "j" << playerData.at(d).raceMap << "`" << playerData.at(d).raceSlot << "`" << connectedSockets.at(d);
						if(send(connectedSockets.at(senderNum), ss.str().c_str(), ss.str().length() + 1, 0) < 0){
							reportError("send()", WSAGetLastError());
						}

						/* If player d is ready, tell the requestor that too */
						if(lobbyMaps[playerData.at(d).raceMap - 1].playerStates[playerData.at(d).raceSlot - 1] == 2){
							ss.str(std::string());  // Clear stringstream for next usage
							ss << "r" << connectedSockets.at(d);
							if(send(connectedSockets.at(senderNum), ss.str().c_str(), ss.str().length() + 1, 0) < 0){
								reportError("send()", WSAGetLastError());
							}
						}

					}

				}

			}

			// Send the player the current MotD
			if(send(connectedSockets.at(senderNum), motd.c_str(), motd.length() + 1, 0) < 0){
				reportError("send()", WSAGetLastError());
			}

			// Send the player the last 20 chat messages
			for(unsigned int d = 0; d < lastMessages.size(); d++){
				if(send(connectedSockets.at(senderNum), lastMessages.at(d).c_str(), lastMessages.at(d).length() + 1, 0) < 0){
					reportError("send()", WSAGetLastError());
				}
			}

		}else if(lastBuffer[0] == '^'){  // Chat message

			// Generate a chat message buffer to send to the other players
			std::string chatMessageBuffer = lastBuffer;
			std::ostringstream ss; ss << connectedSockets.at(senderNum);
			chatMessageBuffer.insert(1, ss.str() + "`" + playerData.at(senderNum).user + "`");

			if(lastMessages.size() == 20){
				lastMessages.erase(lastMessages.begin());  // If 20 chat messages are being stored, discard the first
			}
			lastMessages.push_back(chatMessageBuffer);  // Store chat message (max 20)

			for(unsigned int d = 0; d < playerData.size(); d++){  // Send the chat message to all clients who aren't racing
				if(playerData.at(d).roomID == playerData.at(senderNum).roomID){  // Make sure the client is in the same "room" as the player
					if(send(connectedSockets.at(d), chatMessageBuffer.c_str(), chatMessageBuffer.length() + 1, 0) < 0){
						reportError("send()", WSAGetLastError());
					}
				}
			}

			/* Print chat message in terminal */
			ss.str(std::string());  // Clear stringstream for next usage
			ss << playerData.at(senderNum).user << "(#" << connectedSockets.at(senderNum) << "): " << (lastBuffer + 1);
			printf("%s\n", ss.str().c_str());

		}else if(lastBuffer[0] == 'j'){  // Joining or leaving a race slot

			unsigned int grave = strchr(lastBuffer, '`') - lastBuffer;
			std::string raceMapStr = lastBuffer;
			std::string raceSlotStr = raceMapStr.substr(grave + 1, raceMapStr.length());
			raceMapStr = raceMapStr.substr(1, raceMapStr.length() - grave);

			unsigned int raceMap = 0;
			std::istringstream(raceMapStr) >> raceMap;
			unsigned int raceSlot = 0;
			std::istringstream(raceSlotStr) >> raceSlot;
			int raceStart = 0;

			if(raceMap > 0 && raceMap < 9 && raceSlot > 0 && raceSlot < 5){  // The player is joining or switching a race slot

				if(lobbyMaps[raceMap - 1].playerIDs[raceSlot - 1] == 0){

					float minRank = 300.f;
					switch(raceMap){
						case 1:
							minRank = 0.f;	// Newbieland (0)
						break;
						case 2:
							minRank = 0.1f;   // Buto (0.1)
						break;
						case 3:
							minRank = 3.f;	// Pyramids (3)
						break;
						case 4:
							minRank = 10.f;   // Robocity (10)
						break;
						case 5:
							minRank = 20.f;   // Assembly (20)
						break;
						case 6:
							minRank = 50.f;   // Infernal Hop (50)
						break;
						case 7:
							minRank = 150.f;  // Going down (150)
						break;
						case 8:
							minRank = 300.f;  // Slip (300)
						break;
					}

					if(playerData.at(senderNum).rank >= minRank){  // Make sure the player is on a high enough rank to join

						// If raceMap and raceSlot are greater than 0, the player is switching to another a race slot
						if(playerData.at(senderNum).raceMap > 0 && playerData.at(senderNum).raceSlot > 0){

							lobbyMaps[playerData.at(senderNum).raceMap - 1].playerIDs[playerData.at(senderNum).raceSlot - 1] = 0;
							lobbyMaps[playerData.at(senderNum).raceMap - 1].playerStates[playerData.at(senderNum).raceSlot - 1] = 0;
							if(lobbyMaps[playerData.at(senderNum).raceMap - 1].raceReady()){
								raceStart = playerData.at(senderNum).raceMap;
							}

						}

						playerData.at(senderNum).raceMap = raceMap;
						playerData.at(senderNum).raceSlot = raceSlot;
						lobbyMaps[raceMap - 1].playerIDs[raceSlot - 1] = connectedSockets.at(senderNum);
						lobbyMaps[raceMap - 1].playerStates[raceSlot - 1] = 1;

						// Notify all clients who aren't racing that the player is joining or switching a race slot
						std::ostringstream ss; ss << lastBuffer << "`" << connectedSockets.at(senderNum);
						for(unsigned int d = 0; d < playerData.size(); d++){
							if(playerData.at(d).roomID == 0){
								if(send(connectedSockets.at(d), ss.str().c_str(), ss.str().length() + 1, 0) < 0){
									reportError("send()", WSAGetLastError());
								}
							}
						}

					}

				}

			}else{  // The player is leaving a race slot or is not in one

				if(lobbyMaps[playerData.at(senderNum).raceMap - 1].playerIDs[playerData.at(senderNum).raceSlot - 1] == connectedSockets.at(senderNum)){

					lobbyMaps[playerData.at(senderNum).raceMap - 1].playerIDs[playerData.at(senderNum).raceSlot - 1] = 0;
					lobbyMaps[playerData.at(senderNum).raceMap - 1].playerStates[playerData.at(senderNum).raceSlot - 1] = 0;
					if(lobbyMaps[playerData.at(senderNum).raceMap - 1].raceReady()){
						raceStart = playerData.at(senderNum).raceMap;
					}

					playerData.at(senderNum).raceMap = 0;
					playerData.at(senderNum).raceSlot = 0;

					// Notify all clients who aren't racing that the player is leaving a race slot
					std::ostringstream ss; ss << "jnone`none`" << connectedSockets.at(senderNum);
					for(unsigned int d = 0; d < playerData.size(); d++){
						if(playerData.at(d).roomID == 0){
							if(send(connectedSockets.at(d), ss.str().c_str(), ss.str().length() + 1, 0) < 0){
								reportError("send()", WSAGetLastError());
							}
						}
					}

				}

			}

			if(raceStart > 0){
				startRace(raceStart);
			}

		}else if(lastBuffer[0] == 'r'){  // Player has readied up

			// If the player is waiting to race
			if(playerData.at(senderNum).roomID == 0 && playerData.at(senderNum).raceMap != 0 && playerData.at(senderNum).raceSlot != 0){

				lobbyMaps[playerData.at(senderNum).raceMap - 1].playerStates[playerData.at(senderNum).raceSlot - 1] = 2;  // Set the player's state to ready

				std::ostringstream ss; ss << "r" << connectedSockets.at(senderNum);
				for(unsigned int d = 0; d < playerData.size(); d++){  // Notify all clients who aren't racing that the player has readied themselves
					if(playerData.at(d).roomID == 0){
						if(send(connectedSockets.at(d), ss.str().c_str(), ss.str().length() + 1, 0) < 0){
							reportError("send()", WSAGetLastError());
						}
					}
				}

				if(lobbyMaps[playerData.at(senderNum).raceMap - 1].raceReady()){  // If everyone is ready, start the race
					startRace(playerData.at(senderNum).raceMap);
				}

			}

		}else if(lastBuffer[0] == '#'){  // Race information has been sent

			if(lastBuffer[1] == 'q'){  // Sent once every second

				std::string newMessage = lastBuffer;
				newMessage.erase(newMessage.begin());  // Remove the hash from the beginning of the buffer before relaying it

				for(unsigned int d = 0; d < 4; d++){  // Relay the buffer to every other player in the race
					if(currentRaces.at(playerData.at(senderNum).roomID - 1).playerIDs[d] != connectedSockets.at(senderNum) &&
					   currentRaces.at(playerData.at(senderNum).roomID - 1).playerIDs[d] != 0){

						if(send(currentRaces.at(playerData.at(senderNum).roomID - 1).playerIDs[d], newMessage.c_str(), newMessage.length() + 1, 0) < 0){
							reportError("send()", WSAGetLastError());
						}

					}
				}

			}else if(lastBuffer[1] == 't'){  // Player has pressed or released a valid input key (up, down, left, right and spacebar)

				std::string newMessage = lastBuffer;
				newMessage.erase(newMessage.begin());  // Remove the hash from the beginning of the buffer before relaying it

				for(unsigned int d = 0; d < 4; d++){  // Relay the buffer to every other player in the race
					if(currentRaces.at(playerData.at(senderNum).roomID - 1).playerIDs[d] != connectedSockets.at(senderNum) &&
					   currentRaces.at(playerData.at(senderNum).roomID - 1).playerIDs[d] != 0){

						if(send(currentRaces.at(playerData.at(senderNum).roomID - 1).playerIDs[d], newMessage.c_str(), newMessage.length() + 1, 0) < 0){
							reportError("send()", WSAGetLastError());
						}

					}
				}

			}else if(lastBuffer[1] == 'k'){  // Player has obtained an item

				std::string newMessage = lastBuffer;
				newMessage.erase(newMessage.begin());  // Remove the hash from the beginning of the buffer before relaying it

				for(unsigned int d = 0; d < 4; d++){  // Relay the buffer to every other player in the race
					if(currentRaces.at(playerData.at(senderNum).roomID - 1).playerIDs[d] != connectedSockets.at(senderNum) &&
					   currentRaces.at(playerData.at(senderNum).roomID - 1).playerIDs[d] != 0){

						if(send(currentRaces.at(playerData.at(senderNum).roomID - 1).playerIDs[d], newMessage.c_str(), newMessage.length() + 1, 0) < 0){
							reportError("send()", WSAGetLastError());
						}

					}
				}

			}else if(lastBuffer[1] == 's'){  // Player has left the race

				leaveRace(senderNum);

			}else{
				printf("Unable to interpret data sent by socket #%i: %s\n", connectedSockets.at(senderNum), lastBuffer);
			}

		}else if(lastBuffer[0] == '%' && lastBuffer[1] == 'f'){  // Player has finished a race and is sending their time

			std::string newMessage = lastBuffer;
			newMessage.erase(newMessage.begin());  // Remove the percent sign from the beginning of the buffer before relaying it

			for(unsigned int d = 0; d < 4; d++){  // Relay the buffer to all players in the race
				if(currentRaces.at(playerData.at(senderNum).roomID - 1).playerIDs[d] != 0){

					if(send(currentRaces.at(playerData.at(senderNum).roomID - 1).playerIDs[d], newMessage.c_str(), newMessage.length() + 1, 0) < 0){
						reportError("send()", WSAGetLastError());
					}

				}
			}

		}else if(lastBuffer[0] == 'b'){  // Player has finished a race and is requesting a rank update

			if(playerData.at(senderNum).roomID != 0){

				// A VERY long line that just calculates the player's new rank
				playerData.at(senderNum).rank += currentRaces.at(playerData.at(senderNum).roomID - 1).calculateRank(connectedSockets.at(senderNum), playerData.at(senderNum).raceMap);

				// Send the updated player data to all connected clients who aren't racing
				std::ostringstream ss;
				ss << "p" << connectedSockets.at(senderNum) << "`" << playerData.at(senderNum).user << "`" << playerData.at(senderNum).rank
				   << "`" << playerData.at(senderNum).headNum << "`" << playerData.at(senderNum).bodyNum << "`" << playerData.at(senderNum).footNum
				   << "`" << playerData.at(senderNum).speedPoints << "`" << playerData.at(senderNum).jumpPoints << "`" << playerData.at(senderNum).tractionPoints;
				for(unsigned int d = 0; d < playerData.size(); d++){
					if(playerData.at(d).roomID == 0 || connectedSockets.at(d) == connectedSockets.at(senderNum)){
						if(send(connectedSockets.at(d), ss.str().c_str(), ss.str().length() + 1, 0) < 0){
							reportError("send()", WSAGetLastError());
						}
					}
				}

			}

		}else if(lastBuffer[0] != 'a'){  // (a is sent every second, presumably to keep the connection alive)
			printf("Unable to interpret data sent by socket #%i: %s\n", connectedSockets.at(senderNum), lastBuffer);
		}

	}else{
		printf("Socket #%i is trying to make requests before sending player data, closing connection.\n", connectedSockets.at(senderNum));
		disconnectSocket(senderNum);
	}

}

void socketServer::startRace(unsigned int raceMap){

	unsigned int raceCreated = 0;
	for(unsigned int d = 0; d < currentRaces.size(); d++){  // Find a race that is empty and replace it
		if(currentRaces.at(d).raceEmpty){
			currentRaces.at(d) = lobbyMaps[raceMap - 1].generateRace();  // Generate a new raceInstance in the place of an old one
			raceCreated = d + 1;
			d = currentRaces.size();  // Exit loop
		}
	}
	if(raceCreated == 0){  // If there are no empty races, generate a new one
		currentRaces.push_back(lobbyMaps[raceMap - 1].generateRace());
		raceCreated = currentRaces.size();
	}


	std::ostringstream ss; ss << "m" << raceMap;  // Message for new racers
	std::ostringstream ss2; ss2 << "z" << raceMap;  // Message for players in the lobby (tells them to clear the slots for this race)

	for(unsigned int d = 0; d < playerData.size(); d++){
		if(playerData.at(d).roomID == 0){  // Make sure the player is not racing
			if(playerData.at(d).raceMap == raceMap){  // Check if the player is joining a race
				playerData.at(d).roomID = raceCreated;
				if(send(connectedSockets.at(d), ss.str().c_str(), ss.str().length() + 1, 0) < 0){
					reportError("send()", WSAGetLastError());
				}
			}else{  // If the player is not racing, tell them to clear the slots for race raceMap
				if(send(connectedSockets.at(d), ss2.str().c_str(), ss2.str().length() + 1, 0) < 0){
					reportError("send()", WSAGetLastError());
				}
			}
		}
	}

}

void socketServer::leaveRace(unsigned int socketNum){

	unsigned int raceID = playerData.at(socketNum).roomID;
	playerData.at(socketNum).roomID = 0;
	playerData.at(socketNum).raceMap = 0;
	playerData.at(socketNum).raceSlot = 0;

	std::ostringstream ss; ss << "s" << connectedSockets.at(socketNum);
	bool nowEmpty = true;
	for(unsigned int d = 0; d < 4; d++){  // Loop through each player in the race
		if(currentRaces.at(raceID - 1).playerIDs[d] == connectedSockets.at(socketNum)){  // If this is the slot the player was in, clear it

			currentRaces.at(raceID - 1).playerIDs[d] = 0;

		}else if(currentRaces.at(raceID - 1).playerIDs[d] != 0){  // If someone else is still racing, tell them the player left

			nowEmpty = false;
			if(send(currentRaces.at(raceID - 1).playerIDs[d], ss.str().c_str(), ss.str().length() + 1, 0) < 0){
				reportError("send()", WSAGetLastError());
			}

		}
	}
	currentRaces.at(raceID - 1).raceEmpty = nowEmpty;

	if(nowEmpty){  // If the race is empty, check for other empty races at the end of the currentRaces vector and destroy them

		for(unsigned int d = currentRaces.size(); d > 0; d--){
			if(currentRaces.back().raceEmpty){
				currentRaces.pop_back();
			}else{
				d = 0;
			}
		}

	}

}

void socketServer::disconnectSocket(unsigned int socketNum){
	closesocket(connectedSockets.at(socketNum));
	FD_CLR(connectedSockets.at(socketNum), &socketSet);

	if(socketNum < playerData.size()){  // If the socket had registered player data, clean up and tell the other clients they disconnected

		if(playerData.at(socketNum).raceMap != 0 && playerData.at(socketNum).raceSlot != 0){

			if(playerData.at(socketNum).roomID == 0){  // If the player was in a race slot, remove them from it

				lobbyMaps[playerData.at(socketNum).raceMap - 1].playerIDs[playerData.at(socketNum).raceSlot - 1] = 0;
				lobbyMaps[playerData.at(socketNum).raceMap - 1].playerStates[playerData.at(socketNum).raceSlot - 1] = 0;
				if(lobbyMaps[playerData.at(socketNum).raceMap - 1].raceReady()){
					startRace(playerData.at(socketNum).raceMap);
				}

			}else{  // If the player was in a race, notify the other racers

				leaveRace(socketNum);

			}

		}

		std::ostringstream ss; ss << "d" << connectedSockets.at(socketNum);
		for(unsigned int d = 0; d < playerData.size(); d++){  // Notify all other clients that the player has disconnected
			if(d != socketNum){
				if(send(connectedSockets.at(d), ss.str().c_str(), ss.str().length() + 1, 0) < 0){
					reportError("send()", WSAGetLastError());
				}
			}
		}

		playerData.erase(playerData.begin() + socketNum);

	}
	connectedSockets.erase(connectedSockets.begin() + socketNum);
}
