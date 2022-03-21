/* concurrent connection-oriented - main */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <signal.h>
#include <iostream>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <arpa/inet.h>
// #include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>
#include "npshell.cpp"

#include <vector>
#include <string>
#define	QLEN		  32	/* maximum connection queue length	*/
#define	BUFSIZE		4096

int		errexit(const char *format, ...);
int		passiveTCP(const char *service, int qlen);
int		echo(int fd);

void 	write_login(int fd, char ipaddr[50], char port[10]);

int
main(int argc, char *argv[], char **envp)
{	
	char const *service = "12997";	/* service name or port number	*/
	struct sockaddr_in fsin;	/* the from address of a client	*/
	int	msock, ssock;		/* master & slave sockets	*/
	unsigned int	alen;		/* from-address length		*/
	
	client_info = shmset(shmid, shared_memory, true);
	// semaphore
	for (int allid = 0; allid < 30; allid++) {
		char semName[16];
		strcpy(semName, semname);
		strcat(semName, to_string(allid+1).c_str());
		if (sem_open(semName, O_CREAT | O_EXCL, 0777, 1) == SEM_FAILED)
			errexit("sem_open failed: %s\n", strerror(errno));
	}

	// initialize big table
	for (int j=0; j<30; j++) {
		client_clear(j);
	}

	switch (argc) {
	case	1:
		break;
	case	2:
		service = argv[1];
		break;
	default:
		errexit("usage: TCPmechod [port]\n");
	}

	msock = passiveTCP(service, QLEN);
	signal(SIGCHLD, signalHandler);
	signal(SIGINT, exit_handler);
	signal(SIGUSR1, client_handler);
	pid_t kidpid;

	while (1) {
		alen = sizeof(fsin);
		ssock = accept(msock, (struct sockaddr *)&fsin, &alen);
		if (ssock < 0)
			errexit("accept failed: %s\n", strerror(errno));

		char ipaddr[50];
		inet_ntop(AF_INET, &fsin.sin_addr, ipaddr, sizeof(struct sockaddr));
		ipaddr[strlen(ipaddr)+1] = '\0';

		char *port;
		std::string port_tmp = std::to_string(ntohs(fsin.sin_port));
		port = (char*)port_tmp.c_str();
		port[strlen(port)+1] = '\0';

		// client_info;
		my_id = set_id(client_info);
		client_info->id_sock[my_id] = ssock;
		strcpy(client_info->nickname[my_id], "(no name)\0");
		strcpy(client_info->ip[my_id], ipaddr);
		strcpy(client_info->port[my_id], port);

		kidpid = fork();
		while ( kidpid < 0 ) {
			kidpid = fork();
		}
		if ( kidpid < 0 ) {
			perror( "Internal error: cannot fork." );
			return -1;
		}
		else if ( kidpid == 0 ) {
			// I am the child.
			close(msock);


			if (my_id < 30) {
				// Welcome
				int stdout_tmp;
				stdout_tmp = dup(STDOUT_FILENO);
				dup2(ssock, STDOUT_FILENO);
				std::cout << "****************************************" << std::endl;
				std::cout << "** Welcome to the information server. **" << std::endl;
				std::cout << "****************************************" << std::endl;
				std::cout << "*** User '(no name)' entered from " << ipaddr << ":" << port << ". ***" << std::endl;
				dup2(stdout_tmp, STDOUT_FILENO);
				close(stdout_tmp);

				for (int allid = 0; allid < 30; allid++) {
					char semName[16];
					strcpy(semName, semname);
					strcat(semName, to_string(allid+1).c_str());
					while ((semory = sem_open(semName, O_CREAT , 0777, 1)) == SEM_FAILED);
					sem_wait(semory);
					if (client_info->kidpid[allid] != 0 && allid != my_id) {
						client_info->mode[allid] = 0;
						strcpy(client_info->message[allid], "*** User '(no name)' entered from ");
						strcat(client_info->message[allid], ipaddr);
						strcat(client_info->message[allid], ":");
						strcat(client_info->message[allid], port);
						strcat(client_info->message[allid], ". ***");
						kill(client_info->kidpid[allid], SIGUSR1);
					}
					else {
						sem_post(semory);
					}
				}

				write(ssock, "% ", 2);

				npshell();
			}
		}
		else {
			close(ssock);
			client_info->kidpid[my_id] = kidpid;
		}
	}
}
