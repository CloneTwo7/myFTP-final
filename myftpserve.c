#include "myftp.h"

#define BACKLOG 1

int servInit();
int establishDataSocket(int sock);
int isReadableFile(char *file);
int isDirectory(char *path);
void remoteExit(int sock);
void handleCommand(int sock, char *cmd);
void remoteGet(int sock, int datasock, char *file);
void remoteList(int sock, int datasock);
void remoteChangeDir(int sock, char *param);
void remotePut(int sock, int datasock, char *path);

int main(int argc, char **argv) {
	int sock = servInit();
	while(1) {
		int numread;
		char cmd[512];
		char ch;
		if((numread = read(sock, &ch, 1)) < 0) {
			perror("error reading from sock");
		}
		int tot = 0;
		while(ch != '\n') {
			cmd[tot] = ch;
			if((numread = read(sock, &ch, 1)) < 0) {
				perror("error reading from sock");
			}
			tot += numread;
		}
		cmd[tot] = '\n';
		cmd[tot+1] = '\0';
		handleCommand(sock, cmd);
	}
}

/*The following function is used to write an acknowledgement
 * to the clinet.*/
void sendAcknowledgement(int sock, char *ack) {
	if( write(sock, ack, strlen(ack)) < 0) {
		perror("Error sending acknowledgement\n");
	}

}


/*The following function handles all the packets sent to the server.
 * it comes equipped with D, L, Q, C, G, and P commands. Any other command
 * will send an acknolwedgement to the client stating that the packet
 * was not understood*/
void handleCommand(int sock, char *cmd) {
	int datasock;
	if(cmd[0] == 'D') {
		sendAcknowledgement(1,"Establishing Data Connection\n");
		datasock = establishDataSocket(sock);
	} 
	else if(cmd[0] == 'L') {
		sendAcknowledgement(1,"Performing Remote List\n");
		remoteList(sock, datasock);
	}
	else if(cmd[0] == 'Q') {
		sendAcknowledgement(1,"Received Exit Command\n");
		remoteExit(sock);
	}
	else if(cmd[0] == 'C') {
		sendAcknowledgement(1,"Received Change Dir Command\n");
		remoteChangeDir(sock, &cmd[1]);
	}
	else if(cmd[0] == 'G') {
		sendAcknowledgement(1,"Received Get Command\n");
		remoteGet(sock, datasock, &cmd[1]);
	}
	else if(cmd[0] == 'P') {
		sendAcknowledgement(1,"Received Put Command\n");
		remotePut(sock, datasock, &cmd[1]);
	}
	else {
		sendAcknowledgement(sock, "EUnknown Packet Symbol Sent");
	}
}

/*remotePut() reads from a datasocket and creates a file in the remote directory
 * with fileName. It then stores the data piped through the datasock into the file */
void remotePut(int sock, int datasock, char *file) {
        char *param = calloc(1, strlen(file)+1);
        strcpy(param, file);
        param[strlen(file)+1] = '\0';
        char *buffName = strtok(file, "/");
        char *fileName = buffName;
        while(buffName != NULL) {
        	fileName = buffName;
                buffName = strtok(NULL, "/");
        }
	fileName[strlen(fileName)-1] = '\0';
        if(isReadableFile(fileName)) { //If the file already exists in the remote repo, send err
		sendAcknowledgement(sock, "EFile Exists\n");
                return;
        }

	int fd = open(fileName, O_WRONLY | O_CREAT, 0744);
	if(fd == -1) { //If an error occurs creating a file, send error
		sendAcknowledgement(sock, "E");
		sendAcknowledgement(sock, strerror(errno));
		sendAcknowledgement(sock, "\n");
		return;
	}
	sendAcknowledgement(sock, "A\n");
	char buff;
	int numread = read(datasock, &buff, 1);
	if(numread < 0) {
		sendAcknowledgement(sock, "E");
                sendAcknowledgement(sock, strerror(errno));
                sendAcknowledgement(sock, "\n");
                return;
	}
	while(numread > 0) {
		if (write(fd, &buff, 1) < 0 ) {
                	sendAcknowledgement(sock, "E");
                	sendAcknowledgement(sock, strerror(errno));
                	sendAcknowledgement(sock, "\n");
                	return;
		}
                if ((numread = read(datasock, &buff, 1)) < 0 ) {
                        sendAcknowledgement(sock, "E");
                        sendAcknowledgement(sock, strerror(errno));
                        sendAcknowledgement(sock, "\n");
                        return;
                }
	}
	close(datasock);

}

/*remoteGet() verifies that the file sent exists in the remote repo
 * and then uses the datasock to pipe the contents over to the client*/
void remoteGet(int sock, int datasock, char *param) {
	char file[strlen(param)];
	strcpy(file, param);
	file[strlen(param)-1] = '\0';
	int fd = open(file, O_RDONLY);
	if(fd == -1) { //Error if the file cannot be opened
		sendAcknowledgement(sock, "E");
		sendAcknowledgement(sock, strerror(errno));
		sendAcknowledgement(sock,"\n");
		close(datasock);
		return;
	}
	if(!isReadableFile(file)) {
		sendAcknowledgement(sock, "E");
                sendAcknowledgement(sock, "File is Not Readable or Regular");
                sendAcknowledgement(sock,"\n");
		close(datasock);
                return;
	}
	sendAcknowledgement(sock, "A\n");
	char buff;
	int numread = read(fd, &buff, 1);
	while(numread > 0) {
		write(datasock, &buff, 1);
		numread = read(fd, &buff, 1);
	}
	close(datasock);
	close(fd);
}

/*remoteChangeDir() takes in a path and utilizes chdir() to change
 * the current positioning of the remote repos' spot in the file system*/
void remoteChangeDir(int sock, char *param) {
	char path[strlen(param)];
	strcpy(path, param);
	path[strlen(param)-1] = '\0';
	if(chdir(path) == 0) { //Change Dir completed successfully
		sendAcknowledgement(sock, "A\n");
		return;
	}
	sendAcknowledgement(sock, "E");
	sendAcknowledgement(sock, strerror(errno));
	sendAcknowledgement(sock, "\n");
}

/*After receiving the 'Q' command, the server will
 * send an acknowledgement to the client that it should exit
 * afterwards, it will close the open socket and exit with 0*/
void remoteExit(int sock) {
	if(write(1, "Received Command to Exit\n", strlen("Received Command to Exit\n")) < 0) {
		perror("Error on Exit");
	}
	sendAcknowledgement(sock, "A\n");
	close(sock);
	exit(0);
}

/*remoteList() will fork and exec "ls -l" and pipe the contents 
 * over the datasocket*/
void remoteList(int sock, int datasock) {
	if(fork()) {
		sendAcknowledgement(sock, "A\n");
		wait(NULL);
		close(datasock);
	} 
	else{
        	close(1);
        	dup(datasock);
        	close(datasock);
        	if(execlp("ls", "ls", "-l","-a",NULL) == -1) {
			sendAcknowledgement(datasock, "E");
			sendAcknowledgement(datasock, strerror(errno));
			sendAcknowledgement(datasock, "\n");
        	}
        	exit(1);
	}
}

/*simple function to test whether path is a readable directory*/
int isDirectory(char *path) {
        struct stat area, *s = &area;
	if(S_ISDIR(s -> st_mode)) {
		return (1);
	}
	return (0);
}

/*simple function to thest whether path is a readable file*/
int isReadableFile(char *path) {
        struct stat sstat, *pstat = &sstat;
        if(lstat(path, pstat) == 0 && (S_ISREG(pstat->st_mode)) && !(isDirectory(path))) {
                return (pstat->st_mode & S_IRUSR);
        }
        return (0);
}

/*servInit() initializes a server on DEF_SERV_PORT. Then it will return the socket associated with
 * the connection*/
int servInit() {
        struct sockaddr_in servAddr;
        /*make the socket*/
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) {
                perror("Error");
                exit(1);
        }

        /*bind the socket*/
        memset(&servAddr, 0, sizeof(servAddr));
        servAddr.sin_family = AF_INET;
        servAddr.sin_port = htons(DEF_SERV_PORT);
        servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
        if(bind(sock, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0) {
                perror("Error");
                exit(1);
        }
        /*listen and accept connections*/
        listen(sock, BACKLOG);
        int cfd;
        socklen_t length = sizeof(struct sockaddr_in);
        struct sockaddr_in clientAddr;
        char hostName[NI_MAXHOST];
        int count = 0;
        while(1) {
                count++;
                if((cfd = accept(sock, (struct sockaddr *) &clientAddr, &length)) < 0) {
                        perror("Error");
                        exit(1);
                }
                if(!fork()) {
                        int hostEntry = getnameinfo((struct sockaddr*) &clientAddr,
                                        sizeof(clientAddr), hostName, sizeof(hostName),
                                        NULL, 0, NI_NUMERICSERV);
                        if(hostEntry != 0) {
                                fprintf(stderr, "ERROR: %s\n", gai_strerror(hostEntry));
                                exit(1);
                        }
                        break;
                }
        }
        return cfd;
}
/*Establishes a datasocket used to pipe data to and from the server*/
int establishDataSocket(int sock) {
	struct sockaddr_in servAddr;
	/*make socket*/
	int datasock = socket(AF_INET, SOCK_STREAM, 0);
	if(setsockopt(datasock, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) {
		perror("SetSockOpt Error");
		exit(1);
	}

	/*bind the socket*/
        memset(&servAddr, 0, sizeof(servAddr));
        servAddr.sin_family = AF_INET;
        servAddr.sin_port = htons(0);
        servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
        if(bind(datasock, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0) {
                perror("Error");
                exit(1);
        }
        int size = sizeof(servAddr);
        if(getsockname(datasock, (struct sockaddr *) &servAddr, &size) < 0) {
                perror("getsockname()");
                exit(1);
        }

	/*send acknowledgement*/
	int portnum = ntohs(servAddr.sin_port);
	sendAcknowledgement(sock, "A");
	char *portstr = calloc(1,sizeof(char)*512);
	sprintf(portstr, "%d", portnum);
	sendAcknowledgement(sock, portstr);
	sendAcknowledgement(sock, "\n");
	free(portstr);

	/*listen and accept connections*/
        listen(datasock, BACKLOG);
        int datacfd;
        socklen_t length = sizeof(struct sockaddr_in);
        struct sockaddr_in clientAddr;
        char hostName[NI_MAXHOST];
        int count = 0;
        if((datacfd = accept(datasock, (struct sockaddr *) &clientAddr, &length)) < 0) {
		sendAcknowledgement(sock, "E");
		sendAcknowledgement(sock, strerror(errno));
		sendAcknowledgement(sock, "\n");
        }
        return datacfd;
}
