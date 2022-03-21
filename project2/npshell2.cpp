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
#include <fcntl.h>
#include <string>
using namespace std;

void signalHandler(int signum){
    int status;
    while(waitpid(-1, &status, WNOHANG)>0);
}

class User_info {
public:
    int id;
    char nickname[21];
    char ip[50];
    char port[10];
    void show(void);
};
void User_info::show(void) {
    cout << id+1 << "\t" << nickname << "\t" << ip << ":" << port;
}


class God_info {
public:
    int id_table[FD_SETSIZE] = {0};

	std::vector<int> npipe_table[30];
	std::vector<int> npipe_clock[30];
	User_info user_info[FD_SETSIZE];
	int upipe[30][30][2] = {0};
	std::vector<string> ukey[30];
	std::vector<string> uval[30];

	std::vector<std::string> unikey;
	std::vector<std::string> unival;
    void clear(int id);
    void set_id(int sock_fd);
    int find_id(int sock_fd);
};
void God_info::clear(int id) {
    id_table[id] = 0;
    for (int i=0; i<npipe_table[id].size(); i++) {
        close(npipe_table[id][i]);
    }
    npipe_table[id].clear();
    npipe_clock[id].clear();
    User_info init;//
    user_info[id] = init;
    for (int i=0; i<30; i++) {
        if (upipe[i][id][0] != 0) {
            close(upipe[i][id][0]);
            close(upipe[i][id][1]);
            upipe[i][id][0] = 0;
            upipe[i][id][1] = 0;
        }
        if (upipe[id][i][0] != 0) {
            close(upipe[id][i][0]);
            close(upipe[id][i][1]);
            upipe[id][i][0] = 0;
            upipe[id][i][1] = 0;
        }
    }
    ukey[id].push_back("PATH");
    uval[id].push_back("bin:.");
}

void God_info::set_id(int sock_fd) {
    for (int i=0; i<FD_SETSIZE; i++) {
         if (id_table[i] == 0) {
             id_table[i] = sock_fd;
             break;
         }
    }
}

int God_info::find_id(int sock_fd) {
    int my_id;
    for (int i=0; i<FD_SETSIZE; i++) {
        if (id_table[i] == sock_fd) {
            my_id = i;
            break;
        }
    }
    return my_id;
}

int npshell(int sock_fd, God_info &god_info) {
    signal(SIGCHLD, signalHandler);
    
    // initialize
    // pipe
    int stdin_tmp, stdout_tmp, stderr_tmp;
    stdin_tmp = dup(STDIN_FILENO);
    stdout_tmp = dup(STDOUT_FILENO);
    stderr_tmp = dup(STDERR_FILENO);
    dup2(sock_fd, STDIN_FILENO);
    dup2(sock_fd, STDOUT_FILENO);
    dup2(sock_fd, STDERR_FILENO);

    // big table
    int my_id;
    my_id = god_info.find_id(sock_fd);

    // client environment
    clearenv();
    for (int j = 0; j < god_info.unikey.size(); j++) {
        setenv(god_info.unikey[j].c_str(), god_info.unival[j].c_str(), 10);
    }
    for (int j = 0; j < god_info.ukey[my_id].size(); j++) {
        setenv(god_info.ukey[my_id][j].c_str(), god_info.uval[my_id][j].c_str(), 10);
    }

    vector<int> pipe_table;
    vector<int> pipe_clock;
    pipe_table = god_info.npipe_table[my_id];
    pipe_clock = god_info.npipe_clock[my_id];

    // while (true) {
    for (int one_time = 0; one_time < 1; one_time++) {
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
                if (god_info.id_table[allid] != 0 && allid != my_id) {
                    dup2(god_info.id_table[allid], STDOUT_FILENO);
                    cout << "*** User '" << god_info.user_info[my_id].nickname << "' left. ***" << std::endl;
                }
            }
            dup2(stdin_tmp, STDIN_FILENO);
            dup2(stdout_tmp, STDOUT_FILENO);
            dup2(stderr_tmp, STDERR_FILENO);
            close(stdin_tmp);
            close(stdout_tmp);
            close(stderr_tmp);
            return 0;

        }
        else if ( tasks[0].size() != 0 && strcmp( tasks[0][0], "setenv" ) == 0 ) {
            char** argv = new char*[tasks[0].size()+1];
            for ( int k = 0; k < tasks[0].size(); k++ ) {
                argv[k] = tasks[0][k];
            }
            if (tasks[0].size() > 2) {
                setenv(argv[1], argv[2], 10);
                god_info.ukey[my_id].push_back(argv[1]);
                god_info.uval[my_id].push_back(argv[2]);
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
                if (god_info.id_table[allid] != 0) {
                    god_info.user_info[allid].show();
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
                if (swdig && atoi(argv[1]) < 31 && god_info.id_table[atoi(argv[1])-1] != 0 && atoi(argv[1])-1 != my_id) {
                    dup2(god_info.id_table[atoi(argv[1])-1], STDOUT_FILENO);
                    cout << "*** " << god_info.user_info[my_id].nickname << " told you ***: ";
                    for (int word = 0; word < strlen(my_cmd); word++) {
                        if (isdigit(my_cmd[word]) == 1) {
                            for (int words = word+1; words < strlen(my_cmd); words++) {
                                if (isdigit(my_cmd[words]) == 0) {
                                    cout << my_cmd+words+1 << endl;
                                    break;
                                }
                            }
                            break;
                        }
                    }
                    dup2(sock_fd, STDOUT_FILENO);
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
                for (int allid = 0; allid < 30; allid++) {
                    if (god_info.id_table[allid] != 0) {
                        dup2(god_info.id_table[allid], STDOUT_FILENO);
                        cout << "*** " << god_info.user_info[my_id].nickname << " yelled ***: ";
                        for (int word = 0; word < strlen(my_cmd); word++) {
                            if (my_cmd[word] == 'l') { 
                                cout << my_cmd+word+3 << endl;
                                break;
                            }
                        }
                        dup2(sock_fd, STDOUT_FILENO);
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
                    if (strcmp(god_info.user_info[allid].nickname, argv[1]) == 0) {
                        isname = 1;
                        break;
                    }
                }
                if (isname) {
                    cout << "*** User '" << argv[1] << "' already exists. ***" << endl;
                }
                else {
                    argv[1][strlen(argv[1])+1] = '\0';
                    strcpy(god_info.user_info[my_id].nickname, argv[1]);
                    for (int allid = 0; allid < 30; allid++) {
                        if (god_info.id_table[allid] != 0) {
                            dup2(god_info.id_table[allid], STDOUT_FILENO);
                            cout << "*** User from " << god_info.user_info[my_id].ip << ":" << god_info.user_info[my_id].port <<" is named '" << god_info.user_info[my_id].nickname << "'. ***" << endl;
                            dup2(sock_fd, STDOUT_FILENO);
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
                        if (user_in > 30 || god_info.id_table[user_in-1] == 0) {
                            cerr << "*** Error: user #" << user_in << " does not exist yet. ***" << endl;
                            user_in = 99;
                        }
                        else if (god_info.upipe[user_in-1][my_id][0] == 0) {
                            cerr << "*** Error: the pipe #" << user_in << "->#" << my_id+1 << " does not exist yet. ***" << endl;
                            user_in = 99;
                        }
                        else {
                            for (int allid = 0; allid < 30; allid++) {
                                if (god_info.id_table[allid] != 0) {
                                    dup2(god_info.id_table[allid], STDOUT_FILENO);
                                    cout << "*** " << god_info.user_info[my_id].nickname << " (#" << my_id+1 << ") just received from " << god_info.user_info[user_in-1].nickname << " (#" << user_in << ") by '" << my_cmd << "' ***" << endl;
                                    dup2(sock_fd, STDOUT_FILENO);
                                }
                            }
                        }
                    }
                    break;
                }
            }
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
                        if (user_out > 30 || god_info.id_table[user_out-1] == 0) {
                            cerr << "*** Error: user #" << user_out << " does not exist yet. ***" << endl;
                            user_out = 99;
                        }
                        else if (god_info.upipe[my_id][user_out-1][0] != 0) {
                            cerr << "*** Error: the pipe #" << my_id+1 << "->#" << user_out << " already exists. ***" << endl;
                            user_out = 99;
                        }
                        else {
                            for (int allid = 0; allid < 30; allid++) {
                                if (god_info.id_table[allid] != 0) {
                                    dup2(god_info.id_table[allid], STDOUT_FILENO);
                                    cout << "*** " << god_info.user_info[my_id].nickname << " (#" << my_id+1 << ") just piped '" << my_cmd << "' to " << god_info.user_info[user_out-1].nickname << " (#" << user_out << ") ***" << endl;
                                    dup2(sock_fd, STDOUT_FILENO);
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

                // user pipe
                int null_out;
                int null_in;
                if (user_out && j == pipenum) {
                    if (user_out != 99) {
                        if (pipe(god_info.upipe[my_id][user_out-1]) < 0) {
                            perror( "My error: can't create pipes" );
                        }
                    }
                    else {
                        null_out = open("/dev/null", O_RDWR);////////////////////////////
                    }
                }
                if (user_in == 99 && j == 0) null_in = open("/dev/null", O_RDONLY);////////////////////////////

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
                                dup2(god_info.upipe[user_in-1][my_id][0], STDIN_FILENO);
                                close(god_info.upipe[user_in-1][my_id][0]);
                                close(god_info.upipe[user_in-1][my_id][1]);
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
                            dup2(god_info.upipe[my_id][user_out-1][1], STDOUT_FILENO);
                            close(god_info.upipe[my_id][user_out-1][0]);
                            close(god_info.upipe[my_id][user_out-1][1]);
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
                    // client npipe
                    for (int allid = 0; allid < 30; allid++) {
                        if (allid != my_id) {
                            for (int allnp = 0; allnp < god_info.npipe_clock[allid].size(); allnp++) {
                                if (god_info.npipe_clock[allid][allnp] > 0) {
                                    close(god_info.npipe_table[allid][allnp*2]);
                                    close(god_info.npipe_table[allid][allnp*2+1]);
                                }
                            }
                        }
                    }
                    // user pipe
                    for (int rid = 0; rid < 30; rid++) {
                        for (int cid = 0; cid < 30; cid++) {
                            if (rid != my_id && cid != my_id) {
                                if (god_info.upipe[rid][cid][0] != 0) {
                                    close(god_info.upipe[rid][cid][0]);
                                    close(god_info.upipe[rid][cid][1]);
                                }
                            }
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
                        // close user pipe
                        if (user_in) {
                            if (user_in != 99) {
                                close(god_info.upipe[user_in-1][my_id][0]);
                                close(god_info.upipe[user_in-1][my_id][1]);
                                god_info.upipe[user_in-1][my_id][0] = 0;
                                god_info.upipe[user_in-1][my_id][1] = 0;
                            }
                            else {
                                close(null_in);
                            }
                        }
                    }
                    if (user_out && j == pipenum) {
                        if ( user_out == 99 ) close(null_out);
                        else {
                            close(god_info.upipe[my_id][user_out-1][0]);
                            close(god_info.upipe[my_id][user_out-1][1]);
                        }
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
    }
    god_info.npipe_table[my_id] = pipe_table;
    god_info.npipe_clock[my_id] = pipe_clock;

    // Show prompt.
    std::cout << "% " << flush;

    dup2(stdin_tmp, STDIN_FILENO);
    dup2(stdout_tmp, STDOUT_FILENO);
    dup2(stderr_tmp, STDERR_FILENO);
    close(stdin_tmp);
    close(stdout_tmp);
    close(stderr_tmp);

    return 1;
}