//lib
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>


//max length command
#define MAX_LINE 80   
int main(void) {
    //input
    char input[MAX_LINE];

    //arguments
    char *args[MAX_LINE/2 + 1];  

    //history
    char history[MAX_LINE] = "";

    //state of the program
    int should_run = 1;        

    while (should_run) {
        // print prompt and flush output
        printf("osh> ");
        fflush(stdout);

        // read the command line input
        if (fgets(input, MAX_LINE, stdin) == NULL) {
            //if fgets failed, throw exception
            perror("fgets failed");
            exit(1);
        }
        // remove \n 
        input[strcspn(input, "\n")] = '\0';

        // If the input is empty, continue
        if (strlen(input) == 0) {
            continue;
        }

        // if the command is "exit", terminate the shell.
        if (strcmp(input, "exit") == 0) {
            should_run = 0;
            continue;
        }

        // if the user wants to execute the last command with "!!"
        if (strcmp(input, "!!") == 0) {
            if (strlen(history) == 0) {  
                //no history ==> first command
                printf("No commands in history.\n");
                continue;
            } else {
                // Echo the command before executing
                printf("%s\n", history);
                strcpy(input, history);
            }
        } else {
            // Update history with the current command
            strcpy(history, input);
        }

        // divide the input string into args array
        int token_count = 0;
        char *token = strtok(input, " ");
        while (token != NULL) {
            args[token_count++] = token;
            token = strtok(NULL, " ");
        }
        args[token_count] = NULL;

        // check if user want to wait for child to finished executing or not
        int background = 0;
        if (token_count > 0 && strcmp(args[token_count - 1], "&") == 0) { //if the last arg is &
            background = 1;

            //delete &
            args[token_count - 1] = NULL;
            token_count--;
        }

        // Look for a pipe symbol in the arguments
        // type ABCD | EFGH
        int pipe_index = -1;
        for (int i = 0; i < token_count; i++) {
            if (strcmp(args[i], "|") == 0) {
                //get pipe index
                pipe_index = i;
             break;
            }
        }

        // If a pipe is detected, handle the pipe case
        if (pipe_index != -1) {
            // Split the arguments into two commands around the pipe
            args[pipe_index] = NULL;
            char **args1 = args;             // ABCD --> the first argument
            char **args2 = &args[pipe_index + 1];  // EFGH --> the second


            //create an array for 2 heads of pipe: 
            // f[0] for read and f[1] for write
            int fd[2];

            //create pipt
            if (pipe(fd) < 0) {
                //if create pipe faile
                perror("pipe failed");
                exit(1);
            }

            // First child: executes the command ABCD
            pid_t pid1 = fork();
            if (pid1 < 0) {
                perror("fork failed");
                exit(1);
            } else if (pid1 == 0) {
                // child process
                // stdout of command will be transfer to writing head of pipe
                dup2(fd[1], STDOUT_FILENO);
                //close read
                close(fd[0]);
                //close write
                close(fd[1]);
                //execute
                execvp(args1[0], args1);
                perror("execvp failed");
                exit(1);
            }
           // Second child: executes the command on the right side of the pipe
            pid_t pid2 = fork();
            if (pid2 < 0) {
                perror("fork failed");
                exit(1);
            } else if (pid2 == 0) {
                // Redirect stdin to the read end of the pipe
                dup2(fd[0], STDIN_FILENO);
                close(fd[1]);
                close(fd[0]);
                execvp(args2[0], args2);
                perror("execvp failed");
                exit(1);
            }
            // Parent closes both ends of the pipe
            close(fd[0]);
            close(fd[1]);

            // Wait for both children to complete
            waitpid(pid1, NULL, 0);
            waitpid(pid2, NULL, 0);
        } else {
            // Check for redirection symbols: ">" for output and "<" for input
            int redirect_index = -1;
            int redirect_type = 0; // 1 for >, 2 for <
            for (int i = 0; i < token_count; i++) {
                if (strcmp(args[i], ">") == 0) {
                    redirect_index = i;
                    redirect_type = 1;
                    break;
                }
                if (strcmp(args[i], "<") == 0) {
                    redirect_index = i;
                    redirect_type = 2;
               break;
                }
            }

            // If redirection is found, handle it
            if (redirect_index != -1) {
                // command (>|<) filename
                // cut filename
                char *file = args[redirect_index + 1];
                
                // command NULL filename ==> only execute command
                args[redirect_index] = NULL; 

                pid_t pid = fork();
                if (pid < 0) {
                    perror("fork failed");
                    exit(1);
                } else if (pid == 0) {
                    if (redirect_type == 1) { // Output redirection: command > file
                        //open file to write, create file or truncate
                        int fd = open(file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                        if (fd < 0) {
                            perror("open failed");
                            exit(1);
                        }
                        dup2(fd, STDOUT_FILENO);
                        close(fd);
                    } else if (redirect_type == 2) { // Input redirection: command < file
                        int fd = open(file, O_RDONLY);
                        if (fd < 0) {
                            perror("open failed");
                            exit(1);
                   }
                        dup2(fd, STDIN_FILENO);
                        close(fd);
                    }
                    execvp(args[0], args);
                    perror("execvp failed");
                    exit(1);
                }
                if (!background) {
                    waitpid(pid, NULL, 0);
                }
            } else {
                // No pipe or redirection; execute the command normally.
                pid_t pid = fork();
                if (pid < 0) {
                    perror("fork failed");
                    exit(1);
                } else if (pid == 0) {
                    execvp(args[0], args);
                    perror("execvp failed");
                    exit(1);
                }
                if (!background) {
                    waitpid(pid, NULL, 0);
                }
            } 
        }
    }
    return 0;
}
