#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <iostream>
#include <vector>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <fstream>
#include <fcntl.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <string>
using namespace std;

#define	MAX_Client	30 
#define	FIFO_file	"./user_pipe/" 
#define	shmkey      12092
#define	semname		"semaphorename"
#define TEXT_SZ 2048

int my_id;

struct Client_info {
	int kidpid[30] = {0};
    int id[30] = {0};
	int id_sock[30] = {0};
	char nickname[30][21];
	char ip[30][50];
	char port[30][10];
    int mode[30] = {0};
    char message[30][15200];
    int upipe[30][30];
};
void show(int id, struct Client_info *client_info) {
    cout << client_info->id[id] << "\t" << client_info->nickname[id] << "\t" << client_info->ip[id] << ":" << client_info->port[id];
}
int set_id(struct Client_info *&client_info) {
    int id;
    for (int i=0; i<MAX_Client; i++) {
         if (client_info->id[i] == 0) {
             client_info->id[i] = i+1;
             id = i;
             break;
         }
    }
    return id;
}

// shared memory
struct Client_info * shmset(int &shmid, void *&shared_memory, bool create = false) {
    if (create) {
        shmid = shmget((key_t)shmkey,sizeof(struct Client_info),0666|IPC_CREAT);
    }
    else {
        shmid = shmget((key_t)shmkey,sizeof(struct Client_info),0666);
    }
    if (shmid == -1) std::cerr << "shmget failed\n";
   
    shared_memory = shmat(shmid,(void *) 0,0);
    if (shared_memory == (void *)-1) std::cerr << "shmat failed\n";

    return (struct Client_info *)shared_memory;
}

struct Client_info *client_info;
int shmid;
void *shared_memory=(void *) 0;

sem_t *semory;

void signalHandler(int signum){
    int status;
    while(waitpid(-1, &status, WNOHANG)>0);
}
void exit_handler(int signum){
    for (int allid = 0; allid < 30; allid++) {
        char semName[16];
        strcpy(semName, semname);
        strcat(semName, to_string(allid+1).c_str());
        sem_unlink(semName);
    }
	if (shmdt(shared_memory)<0) std::cerr << "detach error";
	if (shmctl(shmid, IPC_RMID, NULL)<0) std::cerr << "control error";
    int status;
    while(waitpid(-1, &status, WNOHANG)>0);
	write(1, "\n", 1);
	exit(0);
}
void client_handler(int signum){
    char semName[16];
    strcpy(semName, semname);
    strcat(semName, to_string(my_id+1).c_str());
    while ((semory = sem_open(semName, O_CREAT , 0777, 1)) == SEM_FAILED);
    if (client_info->mode[my_id]) {
        int fifo_read_test;
        char fifoname[20];
        strcpy(fifoname, FIFO_file);
        strcat(fifoname, to_string(client_info->mode[my_id]).c_str());
        strcat(fifoname, "to");
        strcat(fifoname, to_string(my_id+1).c_str());
        fifo_read_test = open(fifoname, O_RDONLY);
        client_info->upipe[client_info->mode[my_id]-1][my_id] = fifo_read_test;
        sem_post(semory);
    }
    else {
        cout << client_info->message[my_id] << endl;
        sem_post(semory);
    }
}

void client_clear(int id) {
    char semName[16];
    strcpy(semName, semname);
    strcat(semName, to_string(id+1).c_str());
    while ((semory = sem_open(semName, O_CREAT , 0777, 1)) == SEM_FAILED);
    sem_wait(semory);
    client_info->id_sock[id] = 0;
    client_info->kidpid[id] = 0;
    client_info->id[id] = 0;
    for (int i=0; i<30; i++) {
        if (client_info->upipe[i][id] != 0) {
            close(client_info->upipe[i][id]);
            client_info->upipe[i][id] = 0;
        }
        if (client_info->upipe[id][i] != 0) {
            close(client_info->upipe[id][i]);
            client_info->upipe[id][i] = 0;
        }
    }
    sem_post(semory);
}

int npshell(void) {
    signal(SIGCHLD, signalHandler);
    
    // initialize
    setenv("PATH","bin:.",10);
    // pipe
    int sock_fd;
    sock_fd = client_info->id_sock[my_id];// change to main
    dup2(sock_fd, STDIN_FILENO);
    dup2(sock_fd, STDOUT_FILENO);
    dup2(sock_fd, STDERR_FILENO);
    close(sock_fd);
    
    vector<int> pipe_table;
    vector<int> pipe_clock;

    while (true) {
    // for (int one_time = 0; one_time < 1; one_time++) {
        char command[15001];
        string strbuffer;

        if (not getline(cin, strbuffer) ) return 0;
        strncpy(command, strbuffer.c_str(), strbuffer.length());
        command[strbuffer.length()]='\0';
        if (command[strbuffer.length()-1] == '\r'){
            command[strbuffer.length()-1]='\0';
        }
        
        char my_cmd[strbuffer.length()];
        strcpy(my_cmd, command);

        vector< vector<char*> > tasks;

        vector<char*> args;
        vector<char*> operp;

        char* prog = strtok( command, " " );
        char* tmp = prog;
        
        int isnp = 0;
        int nptmp = -1;
        int user_in = 0;
        int user_out = 0;
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
            // Show prompt.
            cout << "% " << flush;
            continue;
        }
        else if ( tasks[0].size() == 0 ) {
            if ( *prog == '>' && tasks[1].size() != 0) {
                int fd = open(tasks[1][0],O_WRONLY|O_CREAT|O_TRUNC,0664);
                close(fd);
            }
        }
        else if ( tasks[0].size() != 0 && (strcmp( tasks[0][0], "exit" ) == 0 || strcmp( tasks[0][0], "EOF" ) == 0) ) {
            for (int allid = 0; allid < 30; allid++) {
                char semName[16];
                strcpy(semName, semname);
                strcat(semName, to_string(allid+1).c_str());
                while ((semory = sem_open(semName, O_CREAT , 0777, 1)) == SEM_FAILED);
                sem_wait(semory);
                if (client_info->kidpid[allid] != 0 && allid != my_id) {
                    client_info->mode[allid] = 0;
                    strcpy(client_info->message[allid], "*** User '");
                    strcat(client_info->message[allid], client_info->nickname[my_id]);
                    strcat(client_info->message[allid], "' left. ***");
                    kill(client_info->kidpid[allid], SIGUSR1);
                }
                else {
                   sem_post(semory); 
                }
            }

            // clear /////////////////////
            client_clear(my_id);
            exit(0);

        }
        else if ( tasks[0].size() != 0 && strcmp( tasks[0][0], "setenv" ) == 0 ) {
            char** argv = new char*[tasks[0].size()+1];
            for ( int k = 0; k < tasks[0].size(); k++ ) {
                argv[k] = tasks[0][k];
            }
            if (tasks[0].size() > 2) {
                setenv(argv[1], argv[2], 10);
            }
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
        }
        else if ( tasks[0].size() != 0 && strcmp( tasks[0][0], "who" ) == 0 ) {
            cout << "<ID>\t<nickname>\t<IP:port>\t<indicate me>" << endl;
            for (int allid = 0; allid < 30; allid++) {
                if (client_info->id[allid] != 0) {
                    show(allid, client_info);
                    if (allid == my_id) cout << "\t<-me" << endl;
                    else cout << endl;
                }
            }

        }
        else if ( tasks[0].size() != 0 && strcmp( tasks[0][0], "tell" ) == 0 ) {
            char** argv = new char*[tasks[0].size()+1];
            for ( int k = 0; k < tasks[0].size(); k++ ) {
                argv[k] = tasks[0][k];
            }
            if (tasks[0].size() > 1) {
                int swdig = 1;
                for (int isdig = 0; isdig < strlen(argv[1]); isdig++) {
                    if ( not isdigit(argv[1][isdig]) ) {
                        swdig = 0;
                        break;
                    }
                }
                if (swdig && atoi(argv[1]) < 31 && client_info->id[atoi(argv[1])-1] != 0 && atoi(argv[1])-1 != my_id) {
                    int cshift;
                    for (int word = 0; word < strlen(my_cmd); word++) {
                        if (isdigit(my_cmd[word]) == 1) {
                            for (int words = word+1; words < strlen(my_cmd); words++) {
                                if (isdigit(my_cmd[words]) == 0) {
                                    cshift = words+1;
                                    break;
                                }
                            }
                            break;
                        }
                    }
					char semName[16];
					strcpy(semName, semname);
					strcat(semName, to_string(atoi(argv[1])).c_str());
					while ((semory = sem_open(semName, O_CREAT , 0777, 1)) == SEM_FAILED);
                    sem_wait(semory);
                    client_info->mode[atoi(argv[1])-1] = 0;
                    strcpy(client_info->message[atoi(argv[1])-1], "*** ");
                    strcat(client_info->message[atoi(argv[1])-1], client_info->nickname[my_id]);
                    strcat(client_info->message[atoi(argv[1])-1], " told you ***: ");
                    strcat(client_info->message[atoi(argv[1])-1], my_cmd+cshift);
                    kill(client_info->kidpid[atoi(argv[1])-1], SIGUSR1);
                }
                // else if (swdig && atoi(argv[1]) < 31 && god_info.id_table[atoi(argv[1])-1] != 0 && atoi(argv[1])-1 == my_id) {
                //     cout << "*** " << god_info.user_info[my_id].nickname << " told you ***:";
                //     for (int word = 2; word < tasks[0].size(); word++) {
                //         cout << " " <<argv[word];
                //     }
                //     cout << endl;
                // }
                else {
                    cerr << "*** Error: user #" << argv[1] << " does not exist yet. ***" << endl;
                }
            }
        }
        else if ( tasks[0].size() != 0 && strcmp( tasks[0][0], "yell" ) == 0 ) {
            char** argv = new char*[tasks[0].size()+1];
            for ( int k = 0; k < tasks[0].size(); k++ ) {
                argv[k] = tasks[0][k];
            }
            if (tasks[0].size() > 0) {
                int cshift;
                for (int allid = 0; allid < 30; allid++) {
                    if (client_info->kidpid[allid] != 0) {
                        for (int word = 0; word < strlen(my_cmd); word++) {
                            if (my_cmd[word] == 'l') {
                                cshift = word+3;
                                break;
                            }
                        }
                    }
                }
                cout << "*** " << client_info->nickname[my_id] << " yelled ***: " << my_cmd+cshift << endl;
                for (int allid = 0; allid < 30; allid++) {
					char semName[16];
					strcpy(semName, semname);
					strcat(semName, to_string(allid+1).c_str());
					while ((semory = sem_open(semName, O_CREAT , 0777, 1)) == SEM_FAILED);
					sem_wait(semory);
                    if (client_info->kidpid[allid] != 0 && allid != my_id) {
                        client_info->mode[allid] = 0;
                        strcpy(client_info->message[allid], "*** ");
                        strcat(client_info->message[allid], client_info->nickname[my_id]);
                        strcat(client_info->message[allid], " yelled ***: ");
                        strcat(client_info->message[allid], my_cmd+cshift);
                        kill(client_info->kidpid[allid], SIGUSR1);
                    }
                    else {
                        sem_post(semory);
                    }
                }
            }
        }
        else if ( tasks[0].size() != 0 && strcmp( tasks[0][0], "name" ) == 0 ) {
            char** argv = new char*[tasks[0].size()+1];
            for ( int k = 0; k < tasks[0].size(); k++ ) {
                argv[k] = tasks[0][k];
            }
            if (tasks[0].size() > 1) {
                int isname = 0;
                for (int allid = 0; allid < 30; allid++) {
                    char semName[16];
                    strcpy(semName, semname);
                    strcat(semName, to_string(allid+1).c_str());
                    while ((semory = sem_open(semName, O_CREAT , 0777, 1)) == SEM_FAILED);
                    sem_wait(semory);
                    if (strcmp(client_info->nickname[allid], argv[1]) == 0) {
                        isname = 1;
                        sem_post(semory);
                        break;
                    }
                    sem_post(semory);
                }
                if (isname) {
                    cout << "*** User '" << argv[1] << "' already exists. ***" << endl;
                }
                else {
                    argv[1][strlen(argv[1])+1] = '\0';
                    strcpy(client_info->nickname[my_id], argv[1]);
                    cout << "*** User from " << client_info->ip[my_id] << ":" << client_info->port[my_id] << " is named '" << client_info->nickname[my_id] << "'. ***" << endl;
                    for (int allid = 0; allid < 30; allid++) {
                        char semName[16];
                        strcpy(semName, semname);
                        strcat(semName, to_string(allid+1).c_str());
                        while ((semory = sem_open(semName, O_CREAT , 0777, 1)) == SEM_FAILED);
                        sem_wait(semory);
                        if (client_info->kidpid[allid] != 0 && allid != my_id) {
                            client_info->mode[allid] = 0;
                            strcpy(client_info->message[allid], "*** User from ");
                            strcat(client_info->message[allid], client_info->ip[my_id]);
                            strcat(client_info->message[allid], ":");
                            strcat(client_info->message[allid], client_info->port[my_id]);
                            strcat(client_info->message[allid], " is named '");
                            strcat(client_info->message[allid], client_info->nickname[my_id]);
                            strcat(client_info->message[allid], "'. ***");
                            kill(client_info->kidpid[allid], SIGUSR1);
                        }
                        else {
                            sem_post(semory);
                        }
                    }
                }
            }
        }
        else {
            // execvp
            int curpipes[2];
            int prepipes[2];

            // user pipe parser
            int userpipeinsock;
            for ( int j = 0; j < tasks[0].size(); j++ ) {
                if (tasks[0][j][0] == '<') {
                    int swdig = 1;
                    for (int isdig = 1; isdig < strlen(tasks[0][j]); isdig++) {
                        if ( not isdigit(tasks[0][j][isdig]) ) {
                            swdig = 0;
                            break;
                        }
                    }
                    if (swdig) {
                        user_in = atoi(tasks[0][j]+1);
                        char semName[16];
                        strcpy(semName, semname);
                        strcat(semName, to_string(user_in).c_str());
                        while ((semory = sem_open(semName, O_CREAT , 0777, 1)) == SEM_FAILED);
                        sem_wait(semory);
                        if (user_in > 30 || client_info->kidpid[user_in-1] == 0) {
                            cerr << "*** Error: user #" << user_in << " does not exist yet. ***" << endl;
                            user_in = 99;
                            sem_post(semory);
                        }
                        else if (client_info->upipe[user_in-1][my_id] == 0) {
                            cerr << "*** Error: the pipe #" << user_in << "->#" << my_id+1 << " does not exist yet. ***" << endl;
                            user_in = 99;
                            sem_post(semory);
                        }
                        else {
                            cout << "*** " << client_info->nickname[my_id] << " (#" << my_id+1 << ") just received from " << client_info->nickname[user_in-1] << " (#" << user_in << ") by '" << my_cmd << "' ***" << endl;
                            userpipeinsock = client_info->upipe[user_in-1][my_id];
                            sem_post(semory);
                            for (int allid = 0; allid < 30; allid++) {
                                char semName[16];
                                strcpy(semName, semname);
                                strcat(semName, to_string(allid+1).c_str());
                                while ((semory = sem_open(semName, O_CREAT , 0777, 1)) == SEM_FAILED);
                                sem_wait(semory);
                                if (client_info->kidpid[allid] != 0 && allid != my_id) {
                                    client_info->mode[allid] = 0;
                                    strcpy(client_info->message[allid], "*** ");
                                    strcat(client_info->message[allid], client_info->nickname[my_id]);
                                    strcat(client_info->message[allid], " (#");
                                    strcat(client_info->message[allid], to_string(my_id+1).c_str());
                                    strcat(client_info->message[allid], ") just received from ");
                                    strcat(client_info->message[allid], client_info->nickname[user_in-1]);
                                    strcat(client_info->message[allid], " (#");
                                    strcat(client_info->message[allid], to_string(user_in).c_str());
                                    strcat(client_info->message[allid], ") by '");
                                    strcat(client_info->message[allid], my_cmd);
                                    strcat(client_info->message[allid], "' ***");
                                    kill(client_info->kidpid[allid], SIGUSR1);
                                }
                                else {
                                    sem_post(semory);
                                }
                            }
                        }
                    }
                    break;
                }
            }
            int fifo_write_test;
            for ( int j = 0; j < tasks[tasks.size()-1].size(); j++ ) {
                if (tasks[tasks.size()-1][j][0] == '>') {
                    int swdig = 1;
                    for (int isdig = 1; isdig < strlen(tasks[tasks.size()-1][j]); isdig++) {
                        if ( not isdigit(tasks[tasks.size()-1][j][isdig]) ) {
                            swdig = 0;
                            break;
                        }
                    }
                    if (swdig) {
                        user_out = atoi(tasks[tasks.size()-1][j]+1);
                        char semName[16];
                        strcpy(semName, semname);
                        strcat(semName, to_string(user_out).c_str());
                        while ((semory = sem_open(semName, O_CREAT , 0777, 1)) == SEM_FAILED);
                        sem_wait(semory);
                        if (user_out > 30 || client_info->id_sock[user_out-1] == 0) {
                            cerr << "*** Error: user #" << user_out << " does not exist yet. ***" << endl;
                            user_out = 99;
                            sem_post(semory);
                        }
                        else if (client_info->upipe[my_id][user_out-1] != 0) {
                            cerr << "*** Error: the pipe #" << my_id+1 << "->#" << user_out << " already exists. ***" << endl;
                            user_out = 99;
                            sem_post(semory);
                        }
                        else {
                            cout << "*** " << client_info->nickname[my_id] << " (#" << my_id+1 << ") just piped '" << my_cmd << "' to " << client_info->nickname[user_out-1] << " (#" << user_out << ") ***" << endl;
                            client_info->mode[user_out-1] = my_id+1;
                            char fifoname[20];
                            strcpy(fifoname, FIFO_file);
                            strcat(fifoname, to_string(my_id+1).c_str());
                            strcat(fifoname, "to");
                            strcat(fifoname, to_string(user_out).c_str());
                            unlink(fifoname);
                            mkfifo(fifoname, 0666);
                            kill(client_info->kidpid[user_out-1], SIGUSR1);
                            fifo_write_test = open(fifoname, O_WRONLY);
                            for (int allid = 0; allid < 30; allid++) {
                                char semName[16];
                                strcpy(semName, semname);
                                strcat(semName, to_string(allid+1).c_str());
                                while ((semory = sem_open(semName, O_CREAT , 0777, 1)) == SEM_FAILED);
                                sem_wait(semory);
                                if (client_info->kidpid[allid] != 0 && allid != my_id) {
                                    client_info->mode[allid] = 0;
                                    strcpy(client_info->message[allid], "*** ");
                                    strcat(client_info->message[allid], client_info->nickname[my_id]);
                                    strcat(client_info->message[allid], " (#");
                                    strcat(client_info->message[allid], to_string(my_id+1).c_str());
                                    strcat(client_info->message[allid], ") just piped '");
                                    strcat(client_info->message[allid], my_cmd);
                                    strcat(client_info->message[allid], "' to ");
                                    strcat(client_info->message[allid], client_info->nickname[user_out-1]);
                                    strcat(client_info->message[allid], " (#");
                                    strcat(client_info->message[allid], to_string(user_out).c_str());
                                    strcat(client_info->message[allid], ") ***");          
                                    kill(client_info->kidpid[allid], SIGUSR1);
                                }
                                else {
                                    sem_post(semory);
                                }
                            }
                        }
                    }
                    break;
                }
            }
            if (user_in) tasks[0].pop_back();
            if (user_out) tasks[tasks.size()-1].pop_back();


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

                // user pipe null
                int null_out;
                int null_in;
                if (user_out == 99 && j == pipenum) null_out = open("/dev/null", O_RDWR);
                if (user_in == 99 && j == 0) null_in = open("/dev/null", O_RDONLY);
                
                // Fork child
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
                        // dup user pipe in
                        if (user_in) {
                            if (user_in != 99) {
                                dup2(userpipeinsock, STDIN_FILENO);
                                close(userpipeinsock);
                            }
                            else {
                                dup2(null_in, STDIN_FILENO);
                                close(null_in);
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

                    // dup user pipe out
                    if (user_out && j == pipenum) {
                        if (user_out != 99) {
                            dup2(fifo_write_test, STDOUT_FILENO);
                            close(fifo_write_test);
                        }
                        else {
                            dup2(null_out, STDOUT_FILENO);
                            close(null_out);
                        }
                    }

                    // Close pipe
                    // self number pipe
                    if (j == pipenum-1 && isnp) {
                        close(curpipes[0]);
                        close(curpipes[1]);
                    }
                    // number pipe
                    for (int pc = 0; pc < pipe_clock.size()-fin; pc++) {
                        if (pipe_clock[pc] > 0) {
                            close(pipe_table[pc*2]);
                            close(pipe_table[pc*2+1]);
                        }
                    }
                    // // user pipe
                    // for (int rid = 0; rid < 30; rid++) {
                    //     for (int cid = 0; cid < 30; cid++) {
                    //         if (rid != my_id && cid != my_id) {
                    //             if (god_info.upipe[rid][cid][0] != 0) {
                    //                 close(god_info.upipe[rid][cid][0]);
                    //                 close(god_info.upipe[rid][cid][1]);
                    //             }
                    //         }
                    //     }
                    // }

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
                        // close user pipe
                        if (user_in) {
                            if (user_in != 99) {
                                char semName[16];
                                strcpy(semName, semname);
                                strcat(semName, to_string(user_in).c_str());
                                while ((semory = sem_open(semName, O_CREAT , 0777, 1)) == SEM_FAILED);
                                sem_wait(semory);
                                close(client_info->upipe[user_in-1][my_id]);
                                client_info->upipe[user_in-1][my_id] = 0;
                                sem_post(semory);
                            }
                            else {
                                close(null_in);
                            }
                        }
                    }
                    if (user_out && j == pipenum) {
                        if ( user_out == 99 ) close(null_out);
                        else close(fifo_write_test);
                    } 
                }
            }
        }
        for (int pc = 0; pc < pipe_clock.size(); pc++) {
            pipe_clock[pc] -= 1;
        }

        if (not isnp && not user_out){
            if ( waitpid( kidpid, 0, 0 ) < 0 ) {
                // perror( "Internal error: cannot wait for child." );
                // return -1;
            } 
        }

        // Show prompt.
        std::cout << "% " << flush;  
    }
    return 0;
}