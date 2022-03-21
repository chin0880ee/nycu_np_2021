/* TCPmechod.c - main, echo */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <iostream>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <arpa/inet.h>
#include <vector>
#include <string>
#define	QLEN		  32	/* maximum connection queue length	*/
#define	BUFSIZE		4096

int		errexit(const char *format, ...);
int		passiveTCP(const char *service, int qlen);
int		echo(int fd);

class User_info {
public:
    int id;
    char nickname[21];
    char ip[50];
    char port[10];
    void show(void);
};

class God_info {
public:
    int id_table[FD_SETSIZE] = {0};

	std::vector<int> npipe_table[30];
	std::vector<int> npipe_clock[30];
	User_info user_info[FD_SETSIZE];
	int upipe[30][30][2] = {0};
	std::vector<std::string> ukey[30];
	std::vector<std::string> uval[30];

	std::vector<std::string> unikey;
	std::vector<std::string> unival;
    void clear(int id);
    void set_id(int sock_fd);
    int find_id(int sock_fd);
};

int		npshell(int fd, God_info &god_info);
void 	write_login(int fd, char ipaddr[50], char port[10]);
/*------------------------------------------------------------------------
 * main - Concurrent TCP server for ECHO service
 *------------------------------------------------------------------------
 */
int
main(int argc, char *argv[], char **envp)
{	
	char const *service = "12998";	/* service name or port number	*/
	struct sockaddr_in fsin;	/* the from address of a client	*/
	int	msock;			/* master server socket		*/
	fd_set	rfds;			/* read file descriptor set	*/
	fd_set	afds;			/* active file descriptor set	*/
	unsigned int	alen;		/* from-address length		*/
	int	fd, nfds;

	// initialize big table
	God_info god_info;
	for (int j=0; j<30; j++) {
		god_info.clear(j);
	}

	for (char **env = envp; *env != 0; env++) {
		char *thisEnv = *env;

		for (int word = 0; word < strlen(thisEnv); word++) {
			if (thisEnv[word] == '=') {
				char unikey[word+1];
				char unival[strlen(thisEnv)-word+1];
				memcpy(unikey,&thisEnv[0], word);
				unikey[word] = '\0';
				memcpy(unival,&thisEnv[word+1], strlen(thisEnv)-word);
				unival[strlen(thisEnv)-word] = '\0';
				god_info.unikey.push_back(unikey);
				god_info.unival.push_back(unival);
				break;
			}
		}
		
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

	nfds = FD_SETSIZE;//getdtablesize();
	
	FD_ZERO(&afds);
	FD_SET(msock, &afds);

	while (1) {
		memcpy(&rfds, &afds, sizeof(rfds));
		while (select(nfds, &rfds, (fd_set *)0, (fd_set *)0,
				(struct timeval *)0) < 0)
			std::cerr << "my select error--" << strerror(errno) << std::endl;
			// errexit("select: %s\n", strerror(errno));

		if (FD_ISSET(msock, &rfds)) {
			int	ssock;

			alen = sizeof(fsin);
			ssock = accept(msock, (struct sockaddr *)&fsin,
				&alen);
			if (ssock < 0)
				errexit("accept: %s\n",
					strerror(errno));

			char ipaddr[50];
			inet_ntop(AF_INET, &fsin.sin_addr, ipaddr, sizeof(struct sockaddr));
			ipaddr[strlen(ipaddr)+1] = '\0';

			char *port;
			std::string port_tmp = std::to_string(ntohs(fsin.sin_port));
			port = (char*)port_tmp.c_str();
			port[strlen(port)+1] = '\0';

			god_info.set_id(ssock);

			// User_info user;
			int my_id;
			my_id = god_info.find_id(ssock);
			god_info.user_info[my_id].id = my_id;
			strcpy(god_info.user_info[my_id].nickname, "(no name)\0");
			strcpy(god_info.user_info[my_id].ip, ipaddr);
			strcpy(god_info.user_info[my_id].port, port);

			if (my_id < 30) {
				// Welcome
				int stdout_tmp;
				stdout_tmp = dup(STDOUT_FILENO);
				dup2(ssock, STDOUT_FILENO);
				std::cout << "****************************************" << std::endl;
				std::cout << "** Welcome to the information server. **" << std::endl;
				std::cout << "****************************************" << std::endl;
				dup2(stdout_tmp, STDOUT_FILENO);
				close(stdout_tmp);

				for (int allid = 0; allid < 30; allid++) {
					if (god_info.id_table[allid] != 0) {
						write_login(god_info.id_table[allid], ipaddr, port);
					}
				}
				write(ssock, "% ", 2);

				FD_SET(ssock, &afds);
			}


		}
		for (fd=0; fd<nfds; ++fd) {
			if (fd != msock && FD_ISSET(fd, &rfds)) {
				if (npshell(fd, god_info) == 0) {
					// logout
					int close_id;
					close_id = god_info.find_id(fd);

					// logout clear
					(void) close(fd);
					god_info.clear(close_id);
					FD_CLR(fd, &afds);
					
					// login new user
					int preid = 0;
					for (int allid = 30; allid < FD_SETSIZE; allid++) {
						if (god_info.id_table[allid] != 0) {
							preid = allid;
							break;
						}
					}
					if (preid) {
						god_info.set_id(god_info.id_table[preid]);
						int new_id;
						new_id = god_info.find_id(god_info.id_table[preid]);
						god_info.user_info[new_id] = god_info.user_info[preid];
						god_info.user_info[new_id].id = new_id;
						// strcpy(god_info.user_info[new_id].nickname, "(no name)");
						// strcpy(god_info.user_info[new_id].ip, god_info.user_info[preid].ip);
						// strcpy(god_info.user_info[new_id].port, god_info.user_info[preid].port);

						// Welcome
						int stdout_tmp;
						stdout_tmp = dup(STDOUT_FILENO);
						dup2(god_info.id_table[new_id], STDOUT_FILENO);
						std::cout << "****************************************" << std::endl;
						std::cout << "** Welcome to the information server. **" << std::endl;
						std::cout << "****************************************" << std::endl;
						dup2(stdout_tmp, STDOUT_FILENO);
						close(stdout_tmp);

						for (int allid = 0; allid < 30; allid++) {
							if (god_info.id_table[allid] != 0) {
								write_login(god_info.id_table[allid], god_info.user_info[new_id].ip, god_info.user_info[new_id].port);
							}
						}
						write(god_info.id_table[new_id], "% ", 2);

						FD_SET(god_info.id_table[new_id], &afds);
						
						god_info.id_table[preid] = 0;
						User_info init;//
						god_info.user_info[preid] = init;

						for (int allid = 30; allid < FD_SETSIZE; allid++) {
							if (god_info.id_table[allid] != 0) {
								god_info.id_table[allid-1] = god_info.id_table[allid];
								god_info.user_info[allid-1] = god_info.user_info[allid];
								god_info.user_info[allid-1].id = allid;
								god_info.id_table[allid] = 0;
								User_info inits;//
								god_info.user_info[allid] = inits;
							}
						}
					}
				}
			}
		}
	}
}