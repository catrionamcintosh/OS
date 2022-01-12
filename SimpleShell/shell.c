#include <stdio.h>
#include <unistd.h> 
#include <string.h> 
#include <stdlib.h> 
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdbool.h>

//GLOBAL VARIABLES
int running_pid; //Stores pid of foreground process
int *bgProcesses; //Stores pids of background processes
int MAX_BG_JOBS = 64;


//SIGNAL HANDLER
void sigIntHandler(int sig)
{
    if (running_pid > 0)
    {
        //printf("INT signal handler");
        kill(running_pid, SIGTERM);
    } 
    running_pid = 0;
}

// Removes background processes that have finished execution from list of
// processes running in the background
void clearCompletedBgJobs() {
    int status = 0;
    
    for (int i = 0; i < MAX_BG_JOBS; i++) {
        if (bgProcesses[i] != 0) {
            if (waitpid(bgProcesses[i], &status, WNOHANG) != 0) {
                if(WIFSIGNALED(status) | WIFEXITED(status) | WCOREDUMP(status) ) {
                    bgProcesses[i] = 0;
                }
            }
        }
    }

}

int getcmd(char *prompt, char *args[], int *background)
{
    int length, i = 0; 
    char *token, *loc; 
    char *line = NULL; 
    size_t linecap = 0;
    
    printf("%s", prompt);
    length = getline(&line, &linecap, stdin);
    
    if (length <= 0) {
        exit(-1); 
    }
    
    // Check if background is specified.. 
    if ((loc = index(line, '&')) != NULL) {
        *background = 1;
        *loc = ' '; 
    } else
        *background = 0;
    
    while ((token = strsep(&line, " \t\n")) != NULL) { 
        printf("%s\n", token);

        for (int j = 0; j < strlen(token); j++)
            if (token[j] <= 32) 
                token[j] = '\0';

        if (strlen(token) > 0) 
            args[i++] = token;
    }
    args[i+1] = NULL;
    return i; 
}

// Executes functionality of pwd
// buffer is a string of allocated size/memory
// size is the size of buffer
int pwdCommand(char *buffer, int size) {
    
    //getcwd() returns a string representing current directory
    //and stores string in buffer
    char *pwd = getcwd(buffer, size);
    
    //if getcwd() didn't work
    if(pwd == NULL){
        printf("unable to execute pwd");
        return -1;
    }
    
    return 1;  
}

// Executes functionality of cd
// args is string array including command and its arguments
// size is the number of strings in args array
void cdCommand(char **args, int size) {    
    if (size == 2){
        if (chdir(args[1]) != 0) {
            printf("cd: %s: No such file or directory\n", args[1]);
            return;
        }
    } else if (size > 2) {
        printf("cd: too many arguments\n");
        return;
    }  
    return;
}

// Executes functionality of jobs
void jobsCommand() {
    printf("Jobs running in the background\n");

    //prints each background process currently running stored in bgProcesses
    for(int i = 0; i < MAX_BG_JOBS; i++) {
        if (bgProcesses[i] != 0) {
            printf("Index: %d, PID: %d\n", i + 1, bgProcesses[i]);
        }
    }
}

// Executes functionality of fg
// index is the index passed to fg by user representing process to bring to foreground
void fgCommand(int index) {
    if (index > MAX_BG_JOBS || index < 1) {
        
        printf("No background job at this index");
        return;

    } else if(bgProcesses[index - 1] == 0) {
        
        printf("No background job at this index");
        return;

    } else {
       
        printf("Bringing process to foreground");
        running_pid = bgProcesses[index -1];
        waitpid(bgProcesses[index -1], NULL, 0);
        running_pid = 0;
        return;
    }
}


int main(void)
{
    //Initialize globals
    running_pid = 0;
    bgProcesses = (int *)calloc(MAX_BG_JOBS, sizeof(int));//TODO: Max number of jobs??
    
    char *args[20];
    int bg;

    //Bind signals
    if (signal(SIGINT, sigIntHandler) == SIG_ERR)
    {
        printf("Error SIGINT\n");
        exit(1);
    }

    if (signal(SIGTSTP, SIG_IGN) == SIG_ERR)
    {
        printf("Error SIGTSTP\n");
        exit(1);
    }

    //Begin command prompt
    while(1) {
        
        bg = 0;
        int cnt = getcmd("\n>> ", args, &bg);//get command
        if (cnt == 0) {
            continue;
        }
        args[cnt] = NULL;

        clearCompletedBgJobs();

        //Built in commands run in parent so don't require forked execution

        //Determine user input requires piped execution 
        // If "|" command entered by user toPipe = true
        bool toPipe = false;
        int m;           
        for (m = 0; m < cnt; m++) {
            if (strcmp(args[m], "|") == 0) {
                toPipe = true;
                break;
            }
        }
        
        //PIPED EXECUTION
        if (toPipe) {
            if (m == cnt -1) {
                printf("No command to pipe to\n");
                toPipe = false;
                free(args[0]);
                continue;
            } else {
                //Parse the input around the pipe
                char* firstCommand[m + 1];
                int n;
                for (n = 0; n < m; n++) {
                    firstCommand[n] = args[n]; 
                }
                firstCommand[n] = NULL;

                char* secondCommand[cnt - m + 1];
                int x = 0;
                for ( n = m + 1; n < cnt; n++) {
                    secondCommand[x] = args[n];
                    x++;
                }
                secondCommand[x] = NULL;

                // 0 is read end, 1 is write end
                int fds[2]; // file descriptors
                pipe(fds); //create pipe

                int CP1, CP2;

                // child process 1
                CP1 = fork();

                if (CP1 < 0) {
                    
                    printf("fork failed");
                    free(args[0]);
                    continue;

                } else if (CP1 == 0) {
                    
                    // Reassign stdin to read end of pipe.
                    close(0);
                    dup(fds[0]);
                    close(fds[1]);
                    close(fds[0]);
                    
                    // Execute second command
                    running_pid = CP1;
                    execvp(secondCommand[0], secondCommand);
                    running_pid = 0;
                
                } else  {
                    
                    // child proccess 2
                    CP2 = fork(); 

                    if (CP2 < 0) {
                        
                        printf("fork failed");        
                        free(args[0]);
                        continue;

                    } else if (CP2 == 0) {
                        
                        // Reassign stdout to write end of pipe.
                        close(1);
                        dup(fds[1]);
                        close(fds[0]);
                        close(fds[1]);
                        // Execute first command
                        execvp(firstCommand[0], firstCommand);
                    
                    // parent process
                    } else {
                        
                        close(fds[0]);
                        close(fds[1]);

                        waitpid(CP1, 0, 0);
                        waitpid(CP2, 0, 0);
                        
                        free(args[0]);
                        continue;
                        
                    }
                }
            }
        } //END OF PIPED EXECUTION

        //built in exit command
        if(strcmp(*args, "exit") == 0) {
            for (int i = 0; i < MAX_BG_JOBS; i++) {
                kill(bgProcesses[i], SIGKILL);
            }
            free(args[0]);
            free(bgProcesses);
            exit(0);
        }

        //built in pwd command
        if (strcmp(*args, "pwd") == 0) {

            if(args[1] != NULL) {
                printf("pwd does not take any arguments");
                free(args[0]);
                continue;
            }
            
            char *buffer = (char*) malloc(100*sizeof(char));
            
            if(pwdCommand(buffer, 100) == 1) {
                
                printf("%s\n", buffer);

            } else {
                
                printf("pwd: Error\n");
            }
            
            free(buffer);
            free(args[0]);
            continue; 
        }

        //built in cd command
        if (strcmp(*args, "cd") == 0)
        {
            cdCommand(args, cnt);        
            free(args[0]);
            continue;
        }
        
        //built in fg command
        if (strcmp(*args, "fg") == 0) {
            if (args[2] != NULL) {
                printf("Invalid number of arguments");
            } else if (args[1] != NULL) {
                fgCommand(atoi(args[1]));
            }
            free(args[0]);
            continue;
        }

        //Built in jobs command
        if(strcmp(*args, "jobs") == 0) {
            if(args[1] != NULL) {
                printf("Jobs command takes no arguments");       
                free(args[0]);
                continue;
            }
            jobsCommand();
            free(args[0]);
            continue;
        }
        
        
        pid_t pid = fork();
        if (pid < 0) {

        }

        if (pid == 0)//child process
        {
            //Output redirection
            if (cnt >= 3 && strcmp(args[cnt -2], ">") == 0) {
                close(1);
                if (open(args[cnt -1], O_RDWR|O_CREAT|O_APPEND|O_TRUNC, 0666) == -1) {
                    printf("file could not be opened");
                    free(args[0]);
                    continue;
                }
                args[cnt -1] = NULL;
                args[cnt -2] = NULL;
                cnt -= 2;
            }  

            // Background processes ignore SIGINT
            if(bg == 1) {
                signal(SIGINT, SIG_IGN);
            }

            execvp(*args, args);
            return 0;
        }
        else if (pid > 0) //Parent process
        {
            //built in commands don't run as background processes
            if (cnt >= 3 && strcmp(args[cnt -2], ">") == 0 && bg == 1) {
                bg = 0;
            }

            if (bg == 0)
            {
                running_pid = (int) pid;
                waitpid(pid, 0, 0);
                running_pid = 0;

                
            } else {
                // if process is to run in background, add it to list of
                // background processes
                for (int i = 0; i < MAX_BG_JOBS; i++) {
                    if(bgProcesses[i] == 0) {
                        bgProcesses[i] = pid;
                        break;
                    }
                }
            }
        }
        free(args[0]);
    }
    free(bgProcesses);
    return 0;
}