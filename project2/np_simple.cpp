#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#define QLEN	32

int npshell();
int errexit(const char *format, ...);
int passiveTCP(const char *service, int qlen);
void signalHandler(int signum);

int main(int argc, char *argv[]) {
	struct	sockaddr_in fsin;	/* the from address of a client	*/
	char const *service = "12999";	/* service name or port number	*/
	int	msock, ssock;		/* master & slave sockets	*/
	unsigned int	alen;		/* from-address length		*/

	switch (argc) {
	case	1:
		break;
	case	2:
		service = argv[1];
		break;
	default:
		errexit("usage: TCPdaytimed [port]\n");
	}

	msock = passiveTCP(service, QLEN);
	signal(SIGCHLD, signalHandler);
	pid_t kidpid;

	while (1) {
		alen = sizeof(fsin);
		ssock = accept(msock, (struct sockaddr *)&fsin, &alen);
		if (ssock < 0)
			errexit("accept failed: %s\n", strerror(errno));

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
			dup2(ssock, STDOUT_FILENO);
			dup2(ssock, STDIN_FILENO);
			dup2(ssock, STDERR_FILENO);
			close(ssock);
			npshell();
		}
		else {
			// I am the parent.  Wait for the child.
			close(ssock);
		}
	}
}