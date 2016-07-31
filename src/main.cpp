#include "socketServer.h"

int main(int argc, char *argv[]){

	socketServer host;
	if(!host.initServer(argc, (const char **)argv))  // Initialize the server
		return 1;

	while(true)
		host.handleConnections();

	return 0;

}
