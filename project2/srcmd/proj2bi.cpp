#include <string>
#include <string.h>
#include <iostream>
#include <unistd.h>
void write_login(int fd, char ipaddr[50], char port[10]) {
    int stdout_tmp;
    stdout_tmp = dup(STDOUT_FILENO);
    dup2(fd, STDOUT_FILENO);
    std::cout << "*** User '(no name)' entered from " << ipaddr << ":" << port << ". ***" << std::endl;
    dup2(stdout_tmp, STDOUT_FILENO);
    close(stdout_tmp);
}