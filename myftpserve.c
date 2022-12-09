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
		printf("PRCS %d: Received Command %s\n",getpid(), cmd);
		handleCommand(sock, cmd);
	}
}

void sendAcknowledgement(int sock, char *ack) {
	printf("PRCS %d: Sending ack %s\n", getpid(), ack);
	if( write(sock, ack, strlen(ack)) < 0) {
		perror("Error sending acknowledgement\n");
	}

}


void handleCommand(int sock, char *cmd) {
	int datasock;
	printf("PRCS %d: Handling command %s\n", getpid(), cmd);
	if(cmd[0] == 'D') {
		printf("PRCS %d: Handling command %s\n", getpid(), cmd);
		datasock = establishDataSocket(sock);
		printf("PRCS %d: datasock %d\n",getpid(), datasock);
	} 
	else if(cmd[0] == 'L') {
		remoteList(sock, datasock);
	}
	else if(cmd[0] == 'Q') {
		remoteExit(sock);
	}
	else if(cmd[0] == 'C') {
		remoteChangeDir(sock, &cmd[1]);
	}
	else if(cmd[0] == 'G') {
		remoteGet(sock, datasock, &cmd[1]);
	}
	else if(cmd[0] == 'P') {
		remotePut(sock, datasock, &cmd[1]);
	}
	else {
		exit(1);
	}
}

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
	printf("PRCS %d: Checking if file '%s' exists\n", getpid(), fileName);
        if(isReadableFile(fileName)) {
		sendAcknowledgement(sock, "EFile Exists\n");
                return;
        }

	int fd = open(fileName, O_WRONLY | O_CREAT, 0744);
	if(fd == -1) {
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

void remoteGet(int sock, int datasock, char *param) {
	char file[strlen(param)];
	strcpy(file, param);
	file[strlen(param)-1] = '\0';
	int fd = open(file, O_RDONLY);
	printf("PRCS %d: Open returned %d for file '%s'\n", getpid(), fd, file);
	if(fd == -1) {
		sendAcknowledgement(sock, "E");
		sendAcknowledgement(sock, strerror(errno));
		sendAcknowledgement(sock,"\n");
		return;
	} else {
		sendAcknowledgement(sock, "A\n");
	}
	char buff;
	int numread = read(fd, &buff, 1);
	while(numread > 0) {
		write(datasock, &buff, 1);
		numread = read(fd, &buff, 1);
	}
	close(datasock);
	printf("PRCS %d: Finished reading from file '%s'\n", getpid(), file);
	close(fd);
}

void remoteChangeDir(int sock, char *param) {
	printf("String length param:  %ld \n", strlen(param));
	char path[strlen(param)];
	strcpy(path, param);
	path[strlen(param)-1] = '\0';
	printf("PRCS %d: Changing to directory \'%s\'\n", getpid(),path);
	if(chdir(path) == 0) {
		sendAcknowledgement(sock, "A\n");
	} else {
		sendAcknowledgement(sock, "E");
		sendAcknowledgement(sock, strerror(errno));
		sendAcknowledgement(sock, "\n");
	}
	
}

void remoteExit(int sock) {
	if(write(1, "Received Command to Exit\n", strlen("Received Command to Exit\n")) < 0) {
		perror("Error on Exit");
	}
	sendAcknowledgement(sock, "A\n");
	close(sock);
	exit(0);
}

void remoteList(int sock, int datasock) {
	if(fork()) {
		printf("PRCS %d: Waiting for child\n", getpid());
		sendAcknowledgement(sock, "A\n");
		wait(NULL);
		printf("PRCS %d: closing data sock\n", getpid());
		close(datasock);
	} 
	else{
		printf("PRCS %d: Executing ls command\n", getpid());
        	close(1);
        	dup(datasock);
        	close(datasock);
        	if(execlp("ls", "ls", "-l", NULL) == -1) {
			sendAcknowledgement(datasock, "E");
			sendAcknowledgement(datasock, strerror(errno));
			sendAcknowledgement(datasock, "\n");
        	}
        	exit(1);
	}
}

int isDirectory(char *path) {
        struct stat sstat, *pstat = &sstat;
        if(!strcmp(path, ".") || !strcmp(path, "..")) return (0);
        if(lstat(path, pstat) == 0) {
                return ((S_ISDIR(pstat->st_mode)) ? 1 : 0 );
        }
        return(0);
}

int isReadableFile(char *path) {
        struct stat sstat, *pstat = &sstat;
        if(lstat(path, pstat) == 0 && (S_ISREG(pstat->st_mode)) && !(isDirectory(path))) {
                return (pstat->st_mode & S_IRUSR);
        }
        return (0);
}

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
        printf("Connection Received\n");
        return cfd;
}

int establishDataSocket(int sock) {
	printf("PRCS %d: Creating Data Socket\n", getpid());
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
	printf("PRCS %d: Data Connection Established at FD %d \n", getpid(), datacfd);
        return datacfd;
}
