#include "myftp.h"

char *sendPortRequest(int sock, char *cmd);
void handleInput(char *input, int sock, char *clientArg);
void remotePut(int sock, char *file, char* clientArg);
void remoteShow(int sock, char *file, char *clientArg);
void remoteGet(int sock, char *file, char *clientArg);
void remoteChangeDir(int sock, char *path);
void remoteList(int sock, char *clientArg);
void localChangeDir(char *param);
void localList();
void remoteExit(int sock);
int client_init(char *clientArg);
int establishDataSocket(char *clientArg, char *port);
int isReadableFile(char *path);
int isDirectory(char *path);
char *buildRequest(char req, char *param);
void printErr(char *err);

int main(int argc, char **argv) {
	int sock = client_init(argv[1]);
	while(1) {
		int numread = 0;
		char input[512];
		char ch;
		if( (numread = read(0, &ch, 1)) < 0) {
			perror("Error reading from stdin");
		}
		int totread = 0;
		while(ch!='\n') {
			input[totread] = ch;
			if((numread = read(0, &ch, 1))< 0) {
				perror("Error reading from stdin");
			}
			totread += numread;
		}
		input[totread] = '\0';
		handleInput(input, sock, argv[1]);
	}
}

char *sendPortRequest(int sock, char *cmd) {
	if(write(sock, cmd, strlen(cmd)) < 0) {
		perror("Error sending request");
	}
	int numread = 0;
	char ch;
	char *resp = calloc(1, sizeof(char)*512);
	if( (numread = read(sock, &ch, 1)) < 0) {
		perror("Error reading from server");
	}
	int totread = 0;
	while(ch!='\n') {
		resp[totread] = ch;
		if((numread = read(sock, &ch, 1)) < 0) {
			perror("Error reading from stdin");
		}
		totread += numread;
	}
	resp[totread] = '\0';
	return resp;
}

void handleInput(char *input, int sock, char *clientArg) {
	char *cmd = strtok(input, " ");
	char *param;
	if(cmd != NULL) {
		param = strtok(NULL, " ");
	}
	if(strcmp(cmd, "ls") == 0) {
		if(fork()) {
			wait(NULL);
		} else {
			localList();
		}
	}
	else if(strcmp(cmd, "cd") == 0) {
		localChangeDir(param);
	}
	else if(strcmp(cmd, "exit") == 0) {
		remoteExit(sock);
	}
	else if(strcmp(cmd, "rls") == 0) {
		remoteList(sock, clientArg);
	}
	else if(strcmp(cmd, "rcd") == 0) {
		remoteChangeDir(sock, param);
	}
	else if(strcmp(cmd, "get") == 0) {
		remoteGet(sock, param, clientArg);
	}
	else if(strcmp(cmd, "show") == 0) {
		remoteShow(sock, param, clientArg);
	}
	else if(strcmp(cmd, "put") == 0) {
		remotePut(sock, param, clientArg);
	}

}

void remotePut(int sock, char *file, char *clientArg) {
	int fd = open(file, O_RDONLY);
	if(fd == -1) {
		perror("");
		return;
	}
	char *resp = sendPortRequest(sock, "D\n");
	if(resp[0] == 'A') {
		char *port = resp+1;
		int datasock = establishDataSocket(clientArg, port);
		char *req = buildRequest('P', file);
		char *ack = sendPortRequest(sock, req);
		free(req);
		if(ack[0] == 'E') {
			printErr(ack+1);
			close(datasock);
			return;
		}
		char buff;
		int numread = read(fd, &buff, 1);
		while(numread > 0) {
			write(datasock, &buff, 1);
			numread = read(fd, &buff, 1);
		}
		free(ack);
		close(datasock);
	} else if(resp[0] == 'E') {
		printErr(resp+1);
	}
	free(resp);
	close(fd);
}

char *buildRequest(char req, char *param) {
	char * request = calloc(1, strlen(param) +3);
	request[0] = req;
	strcpy(&request[1], param);
	request[strlen(param)+1] = '\n';
	request[strlen(param)+2] = '\0';
	return request;
}


void remoteShow(int sock, char *file, char* clientArg) {
        char *resp = sendPortRequest(sock, "D\n");
        if(resp[0] == 'A') {
                int datasock = establishDataSocket(clientArg, resp+1);
		char *request = buildRequest('G', file);
		char *ack = sendPortRequest(sock, request);
		free(request);
                if(ack[0] == 'A') {
                        if(fork()) {
                                wait(NULL);
                                close(datasock);
                        } else {
                                close(0); dup(datasock); close(datasock);
                                if(execlp("more","more","-20", NULL) == -1) {
                                        perror("exec more failed\n");
                                }
                        }
                } else if (ack[0] == 'E') {
			printErr(ack+1);
                }
		free(ack);
        } else if(resp[0] == 'E') {
		printErr(resp+1);
        } else {
                write(1, resp, strlen(resp));
        }
	free(resp);
}

void printErr(char *err) {
	if(write(1, err, strlen(err)+1) < 0) {
		perror("Error Writing Error");
	}
}


void remoteGet(int sock, char *file, char *clientArg) {
	char *param = calloc(1, strlen(file)+1);
	strcpy(param, file);
	param[strlen(file)+1] = '\0';
	char *buffName = strtok(file, "/");
	char *fileName = buffName;
	while(buffName != NULL) {
		fileName = buffName;
		buffName = strtok(NULL, "/");
	}

	if(isReadableFile(fileName)) {
		printErr("File Exists\n");
		return;
	}

	char *request = buildRequest('D', "");
	char *ack = sendPortRequest(sock, request);
	if(ack[0] == 'E') {
		printErr(ack+1);
		return;
	}
	free(request);
	int datasock = establishDataSocket(clientArg, ack+1);
	free(ack);
	request  = buildRequest('G', param);
	ack = sendPortRequest(sock, request);
	if(ack[0] == 'E') {
		printErr(ack+1);
		return;
	}
	printf("PRCS %d: Received ack '%s'\n", getpid(), ack);

	int fd = open(fileName, O_WRONLY | O_CREAT, 0744);
	if(fd == -1) {
		perror("");
		return;
	}
        char buff;
        int numread = read(datasock, &buff, 1);
        while(numread > 0) {
        	write(fd, &buff, 1);
                numread = read(datasock, &buff, 1);
        }
	free(request);
	free(ack);
       	close(datasock);
	free(param);
}

void remoteChangeDir(int sock, char *path) {
	char *request = buildRequest('C', path);
	char *ak = sendPortRequest(sock, request);
	if (ak[0] == 'E') {
		printErr(ak+1);
		return;
	}
	free(ak);
	free(request);
}

void remoteList(int sock, char *clientArg) {
	char *resp = sendPortRequest(sock, "D\n");
	if(resp[0] == 'A') {
                int datasock = establishDataSocket(clientArg, resp+1);
                char *request = buildRequest('L', "");
                char *ack = sendPortRequest(sock, request);
                free(request);
		if(ack[0] == 'A') {
			if(fork()) {
				wait(NULL);
				close(datasock);
			} else {
				close(0); dup(datasock); close(datasock);
				if(execlp("more","more","-20", NULL) == -1) {
					perror("exec more failed\n");
				}
			}
		} else if (ack[0] == 'E') {
			printErr(ack+1);
			return;
		}
		free(ack);
	} else if(resp[0] == 'E') {
		printErr(resp+1);
		return;
	} else {
		write(1, resp, strlen(resp));
	}
	free(resp);
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

void localChangeDir(char *path) {
        if(chdir(path)) {
		if(write(1, strerror(errno), strlen(strerror(errno)))<0) {
			perror("writing error message\n");
		}
		if(write(1, "\n", strlen("\n")) < 0) {
			perror("writing new line");
		}
        }
}

void localList() {
        int fd[2];
        int rdr, wrt;
        if(pipe(fd) < 0) {
                perror("Pipe Broken");
        }

        rdr = fd[0]; wrt = fd[1];

        if(fork()) {
                wait(NULL);
                close(wrt);
                close(0); dup(rdr); close(rdr);
                if(execlp("more", "more", "-20", NULL) == -1) {
                        perror("exec failed");
                }
        }

        else {
                close(rdr);
                close(1); dup(wrt); close(wrt);
                if(execlp("ls", "ls", "-l", NULL) == -1) {
                        perror("exec(ls) failed");
                }
                exit(1);
        }
}

void remoteExit(int sock) {
	if(write(sock, "Q\n", strlen("Q\n")) < 0) {
		perror("Error sending Q");
	}
	close(sock);
	exit(0);
}

/*client_init() generates the initial socket that is used to communicate
 * between the client and the server*/
int client_init(char* clientArg) {
        int socketfd;
        struct addrinfo hints, *actualdata;
        memset(&hints, 0, sizeof(hints));
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_family = AF_INET;
        int err;
        if((err = getaddrinfo(clientArg, DEF_PORT , &hints, &actualdata))!= 0) {                
		fprintf(stderr, "Error: %s\n", gai_strerror(err));
                exit(1);
        }
        socketfd = socket(actualdata -> ai_family, actualdata -> ai_socktype, 0);
        if(socketfd == -1) {
                perror("Error");
                exit(1);
        }
        if( connect(socketfd, actualdata -> ai_addr, actualdata -> ai_addrlen) < 0) {
                perror("Error");
                exit(1);
        }
	freeaddrinfo(actualdata);
        fflush(stdout);
        return (socketfd);
}

/*establishDataSocket() establishes a connection for transporting data between
 * the client and the server*/
int establishDataSocket(char *clientArg, char *port) {
        int socketfd;
        struct addrinfo hints, *actualdata;
        memset(&hints, 0, sizeof(hints));
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_family = AF_INET;
        int err;
        if((err = getaddrinfo(clientArg, port, &hints, &actualdata))!= 0) {
                fprintf(stderr, "Error: %s\n", gai_strerror(err));
                exit(1);
        }
        socketfd = socket(actualdata -> ai_family, actualdata -> ai_socktype, 0);
        if(socketfd == -1) {
                perror("Socket Err");
                exit(1);
        }
        if( connect(socketfd, actualdata -> ai_addr, actualdata -> ai_addrlen) < 0) {
                perror("Connect Err");
                exit(1);
        }
	freeaddrinfo(actualdata);
        fflush(stdout);
        return (socketfd);
}
