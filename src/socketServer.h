#ifndef SOCKETSERVER_H
#define SOCKETSERVER_H

#ifdef _WIN32
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#define WINSOCK_VERSION MAKEWORD(2, 2)
#else
	#include <sys/socket.h>
	#include <arpa/inet.h>
	#include <netdb.h>
	#include <unistd.h>
	#define INVALID_SOCKET -1
	#define SOCKET_ERROR -1
	typedef int SOCKET;
#endif

#define DEFAULT_PORT 7249

#include <vector>
#include "player.h"
#include "lobbySlotHandler.h"

struct socketServer{

	char ip[15];
	uint16_t port;
	SOCKET masterSocket;	 // Host's socket object
	FD_SET socketSet;		 // Set of connected sockets for select()
	std::vector<SOCKET> connectedSockets;
	int recvBytes;			 // Length of the last buffer recieved
	char lastBuffer[2048];	 // Last buffer ("message") received from a client. Buffers are capped at 2,048 bytes (which is way more then you'll need here)
	std::string motd;

	std::vector<player> playerData;
	std::vector<std::string> lastMessages;  // Last 20 chat messages
	lobbySlotHandler lobbyMaps[8];
	std::vector<raceInstance> currentRaces;

	socketServer();
	~socketServer();

	void reportError(const char *failedFunction, int errorCode);
	bool loadConfig(const char *prgPath);
	bool initServer(const int argc, const char *argv[]);
	void handleConnections();
	void handleBuffer(unsigned int senderID);
	void storeChatMessage(std::string chatMessageBuffer);
	void startRace(unsigned int raceMap);
	void leaveRace(unsigned int socketNum);
	void disconnectSocket(unsigned int socketID);

};

#endif
