#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <iostream>
#include <vector>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <fstream>
#include<fcntl.h>
using namespace std;

void signalHandler(int signum){
    int status;
    while(waitpid(-1, &status, WNOHANG)>0);
}

int main(void) {
    signal(SIGCHLD, signalHandler);
    setenv("PATH","bin:.",10);
    vector<int> pipe_table;
    vector<int> pipe_clock;
    while (true) {
        // Show prompt.
        cout << "% " << flush;
        char command[15001];
        string strbuffer;

        if (not getline(cin, strbuffer) ) return 0;
        strncpy(command, strbuffer.c_str(), strbuffer.length());
        command[strbuffer.length()]='\0';

        vector< vector<char*> > tasks;

        vector<char*> args;
        vector<char*> operp;

        char* prog = strtok( command, " " );
        char* tmp = prog;

        int isnp = 0;
        int nptmp = -1;
        pid_t kidpid;
        
        while ( tmp != NULL ) {
            if ( tmp[0] == '>' && strlen(tmp) == 1) {
                tasks.push_back( args );
                operp.push_back( tmp );
                args.clear();
            }
            else if ( tmp[0] == '|' ) {
                int dswitch = 0;
                int count = 0;
                for ( int d = 1; d < strlen(tmp); d++ ) {
                    if ( ( (int)tmp[d] > 48 && (int)tmp[d] < 58 ) || ( (int)tmp[d] > 47 && (int)tmp[d] < 58 && d != 1)) {
                        count += pow(10, strlen(tmp)-d-1) * ((int)tmp[d]-48);
                    }
                    else {
                        args.push_back( tmp );
                        dswitch++;
                        break;
                    }
                }
                if (not dswitch){
                    if (strlen(tmp) > 1) {
                        isnp = 1;
                        for (int pc = 0; pc < pipe_clock.size(); pc++) {
                            if (pipe_clock[pc] == count) {
                                nptmp = pc;
                                break;
                            }
                        }
                        if (nptmp == -1) {
                            pipe_clock.push_back(count);
                        }
                    }
                    tasks.push_back( args );
                    operp.push_back( tmp );
                    args.clear();
                }
            }
            else if ( tmp[0] == '!' ) {
                int dswitch = 0;
                int count = 0;
                for ( int d = 1; d < strlen(tmp); d++ ) {
                    if ( ( (int)tmp[d] > 48 && (int)tmp[d] < 58 ) || ( (int)tmp[d] > 47 && (int)tmp[d] < 58 && d != 1)) {
                        count += pow(10, strlen(tmp)-d-1) * ((int)tmp[d]-48);
                    }
                    else {
                        args.push_back( tmp );
                        dswitch++;
                        break;
                    }
                }
                if (not dswitch){
                    if (strlen(tmp) > 1) {
                        isnp = 1;
                        for (int pc = 0; pc < pipe_clock.size(); pc++) {
                            if (pipe_clock[pc] == count) {
                                nptmp = pc;
                                break;
                            }
                        }
                        if (nptmp == -1) {
                            pipe_clock.push_back(count);
                        }
                    }
                    tasks.push_back( args );
                    operp.push_back( tmp );
                    args.clear();
                }
            }
            else{
                args.push_back( tmp );
            }
            tmp = strtok( NULL, " " );
        }
        if (not isnp){
            tasks.push_back( args );
        }

        int pipenum = operp.size();
        int fin = 0;
        if (isnp && nptmp == -1) fin = 1;

        if (prog == NULL) {
            continue;
        }
        else if ( tasks[0].size() == 0 ) {
            if ( *prog == '>' && tasks[1].size() != 0) {
                // cerr << tasks[1][0] << endl;
                int fd = open(tasks[1][0],O_WRONLY|O_CREAT|O_TRUNC,0664);
                close(fd);
            }
            // for (int pc = 0; pc < pipe_clock.size(); pc++) {
            //     pipe_clock[pc] -= 1;
            // }
            // continue;
        }
        else if ( tasks[0].size() != 0 && (strcmp( tasks[0][0], "exit" ) == 0 || strcmp( tasks[0][0], "EOF" ) == 0) ) {
            return 0;
        }
        else if ( tasks[0].size() != 0 && strcmp( tasks[0][0], "setenv" ) == 0 ) {
            char** argv = new char*[tasks[0].size()+1];
            for ( int k = 0; k < tasks[0].size(); k++ ) {
                argv[k] = tasks[0][k];
            }
            if (tasks[0].size() > 2) {
                setenv(argv[1], argv[2], 10);
            }
            // for (int pc = 0; pc < pipe_clock.size(); pc++) {
            //     pipe_clock[pc] -= 1;
            // }
            // continue;
        }
        else if ( tasks[0].size() != 0 && strcmp( tasks[0][0], "printenv" ) == 0 ) {
            char** argv = new char*[tasks[0].size()+1];
            for ( int k = 0; k < tasks[0].size(); k++ ) {
                argv[k] = tasks[0][k];
            }
            if (tasks[0].size() > 1) {
                char *key = getenv(argv[1]);
                if (key != NULL) {
                    cout << getenv(argv[1]) << endl; 
                }
            }

            // for (int pc = 0; pc < pipe_clock.size(); pc++) {
            //     pipe_clock[pc] -= 1;
            // }
            // continue;
        }
        else {
            // Fork child
            int curpipes[2];
            int prepipes[2];
            for ( int j = 0; j < tasks.size(); j++ ) {
                char** argv = new char*[tasks[j].size()+1];
                for ( int k = 0; k < tasks[j].size(); k++ ) {
                    argv[k] = tasks[j][k];
                }
                argv[tasks[j].size()] = NULL;

                // Create pipe
                prepipes[0] = curpipes[0];
                prepipes[1] = curpipes[1];

                int fd_tmp[2];
                if (isnp && j == pipenum -1) {
                    if (nptmp == -1) {
                        if (pipe(fd_tmp) < 0) {
                            perror( "My error: can't create pipes" );
                        }
                        pipe_table.push_back(fd_tmp[0]);
                        pipe_table.push_back(fd_tmp[1]);
                    }
                    else {
                        fd_tmp[0] = pipe_table[nptmp*2];
                        fd_tmp[1] = pipe_table[nptmp*2+1];
                    }
                }
                else if (j != pipenum && pipenum) {
                    if (pipe(fd_tmp) < 0) {
                        perror( "My error: can't create pipes" );
                    }
                }
                curpipes[0] = fd_tmp[0];
                curpipes[1] = fd_tmp[1];

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
                    // Pipe process
                    // Dup
                    if (j != 0) {
                        dup2(prepipes[0], STDIN_FILENO);     
                        
                        close(prepipes[0]);
                        close(prepipes[1]);
                    }
                    else {
                        for (int pc = 0; pc < pipe_clock.size(); pc++) {
                            if (pipe_clock[pc] == 0) {
                                dup2(pipe_table[pc*2], STDIN_FILENO);
                                close(pipe_table[pc*2]);
                                close(pipe_table[pc*2+1]);
                                break;
                            }
                        }
                    }

                    if (j != pipenum && pipenum) {
                        if (operp[j][0] == '!') {
                            dup2(curpipes[1], STDERR_FILENO);
                        }
                        dup2(curpipes[1], STDOUT_FILENO);
                        close(curpipes[0]);
                        close(curpipes[1]);
                    }
                    
                    if (j == pipenum-1 && isnp) {
                        close(curpipes[0]);
                        close(curpipes[1]);
                    }

                    // Close pipe
                    for (int pc = 0; pc < pipe_clock.size()-fin; pc++) {
                        if (pipe_clock[pc] > 0) {
                            close(pipe_table[pc*2]);
                            close(pipe_table[pc*2+1]);
                        }
                    }                        

                    // Exec* 
                    if (j > 0 && operp[j-1][0] == '>') {
                        char buffer[20];
                        int num_bytes;
                        int fd = open(argv[0],O_WRONLY|O_CREAT|O_TRUNC,0664);
                        while ((num_bytes = read(fileno(stdin), buffer, 1)) > 0){
                                write(fd, buffer, num_bytes);
                        }
                        close(fd);
                        exit(0);
                    }
                    else {
                        execvp( argv[0], argv );
                        cerr << "Unknown command: [" << argv[0] << "].\n";
                        // perror(argv[0]);
                        exit(0);
                    }
                }
                else {
                    // I am the parent.  Wait for the child.
                    if (j != 0) {
                        close(prepipes[0]);
                        close(prepipes[1]);
                    }
                    else {
                        for (int pc = 0; pc < pipe_clock.size(); pc++) {
                            if (pipe_clock[pc] == 0) {
                                close(pipe_table[pc*2]);
                                close(pipe_table[pc*2+1]);
                                break;
                            }
                        }
                    }
                }
            }
        }
        for (int pc = 0; pc < pipe_clock.size(); pc++) {
            pipe_clock[pc] -= 1;
        }

        if (not isnp){
            if ( waitpid( kidpid, 0, 0 ) < 0 ) {
                // perror( "Internal error: cannot wait for child." );
                // return -1;
            } 
        }
    }
    return 0;
}