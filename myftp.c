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

/*The main function establishes a main connection with the
 * server and then loops through the cycle of reading the users'
 * input and sending it to the handleInput function*/
int main(int argc, char **argv) {
	int sock = client_init(argv[1]);
	while(1) {
		if(write(1, "MFTP> ", strlen("MFTP> ")) <0) {
			perror("Writing prompt");
		}
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

/*The following command sends a request to the server and builds
 * a string for the acknowledgement or the error received*/
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

/*The following parses and handles all possible input by the user*/
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
	} else {
		if(write(1, "Command Not Recognized\n", strlen("Command Not Recognized\n")) < 0) {
			perror("Writing command error");
		}
	}

}

/*remotePut() will establish a data connection with the server
 * and then read the contents of a file and send the
 * data to the server.*/
void remotePut(int sock, char *file, char *clientArg) {
	int fd = open(file, O_RDONLY);
	if(fd == -1) { //Verify the file exists locally
		perror("");
		return;
	}
	if(!isReadableFile(file)) {
		if (write(1, "Not Readable File\n", strlen("Not Readable File\n")) <0) {
			perror("writing error");
			return;
		}
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

/*the buildRequest function takes in a character (representing the command
 * sent to the server) and the parameter of the command. This will then
 * build an adequate string that can be written to the server and understood.
 * the resulting string must be freed using free() whenever it is done being
 * used*/
char *buildRequest(char req, char *param) {
	char * request = calloc(1, strlen(param) +3);
	request[0] = req;
	strcpy(&request[1], param);
	request[strlen(param)+1] = '\n';
	request[strlen(param)+2] = '\0';
	return request;
}


/*the remoteShow function will be passed the server's socket, file targeted, and
 * the client's argument. This will then create a datasocket with the server
 * and the following information will be piped to the client being displayed 20
 * lines at a time*/
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

/*Simple write to standard out function used to streamline writing errors*/
void printErr(char *err) {
	if(write(1, err, strlen(err)+1) < 0) {
		perror("Error Writing Error");
	}
}

/*the remoteGet function will establish a dataconnection with
 * the server and write the contents sent by the server into a new
 * file created based on the file argument sent by the function call*/
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

/*this function call will cause the server to change to whatever
 * path was sent to it. It simply requests the 'C' command from
 * the server given the path variable sent to it*/
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

/*This requests the 'L' command from the server. The function
 * then takes the information sent by the server and displays it
 * 20 lines at a time*/
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

/*simple function used to tell if a given path leads to a readable directory*/
int isDirectory(char *path) {
	struct stat sstat, *pstat = &sstat;
	if(!strcmp(path, ".") || !strcmp(path, "..")) return (0);
	if(lstat(path, pstat) == 0) { 
		return ((S_ISDIR(pstat->st_mode)) ? 1 : 0 );
	}
	return(0);
}

/*simple function used to tell if a given path leads to a readable file*/
int isReadableFile(char *path) {
	struct stat sstat, *pstat = &sstat;
	if(lstat(path, pstat) == 0 && (S_ISREG(pstat->st_mode)) && !(isDirectory(path))) {
		return (pstat->st_mode & S_IRUSR);
	}
	return (0);
}

/*simple function used to call chdir on the client to the given path*/
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

/*localList() displays the contents following the "ls -l" style
 * the contents are then piped into "more -20" to display 20
 * lines at a time*/
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
                if(execlp("ls", "ls", "-l","-a", NULL) == -1) {
                        perror("exec(ls) failed");
                }
                exit(1);
        }
}

/*This sends the 'Q' command to the server. After receiving an acknloweldegment
 * the client will exit*/
void remoteExit(int sock) {
	char *ack = sendPortRequest(sock, "Q\n");
	if(ack[0] != 'A') {
		if(write(1, ack+1, strlen(ack+1)) < 0) {
			perror("Error writing error");
		}
	}
	free(ack);
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
