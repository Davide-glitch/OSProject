#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>

#define MAX_CMD_LENGTH 256
#define MAX_PATH_LENGTH 512
#define COMMAND_FILE "command.tmp"
#define ARGS_FILE "args.tmp"
#define SCORE_CALCULATOR_EXEC "./score_calculator"
#define TREASURE_FILE_CHECK "treasures.dat" // Used by calculate_score to find hunts
#define END_OF_MONITOR_OUTPUT "---END_OF_MONITOR_OUTPUT---"


pid_t monitor_pid = -1;
int monitor_running = 0;
int waiting_for_monitor_end = 0;
int monitor_to_hub_pipe[2] = {-1, -1}; // Pipe for monitor to send results to hub

// Function prototypes
void start_monitor();
void list_hunts();
void list_treasures();
void view_treasure();
void stop_monitor();
void calculate_score(); // New function for Phase 3
void handle_child_exit(int sig);
void read_from_monitor_pipe();

// Set up signal handlers
void setup_signal_handlers() {
    struct sigaction sa;
    
    // Set up SIGCHLD handler
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_child_exit;
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP; // SA_RESTART to handle interrupted syscalls
    sigaction(SIGCHLD, &sa, NULL);
}

// Handle child process termination
void handle_child_exit(int sig) {
    int status;
    pid_t pid;
    
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (pid == monitor_pid) {
            if (WIFEXITED(status)) {
                printf("\nMonitor terminated with exit code: %d\n", WEXITSTATUS(status));
            } else if (WIFSIGNALED(status)) {
                printf("\nMonitor terminated by signal: %d\n", WTERMSIG(status));
            }
            
            monitor_running = 0;
            monitor_pid = -1;
            waiting_for_monitor_end = 0;
            if (monitor_to_hub_pipe[0] != -1) {
                close(monitor_to_hub_pipe[0]);
                monitor_to_hub_pipe[0] = -1;
            }
             if (monitor_to_hub_pipe[1] != -1) { // Should have been closed by monitor or hub start
                close(monitor_to_hub_pipe[1]);
                monitor_to_hub_pipe[1] = -1;
            }
            printf("> "); // Re-prompt
            fflush(stdout);
        }
        // Could handle other children here if calculate_score forks them without explicit wait
    }
}

// Start monitor process
void start_monitor() {
    if (monitor_running) {
        printf("Monitor is already running!\n");
        return;
    }

    if (pipe(monitor_to_hub_pipe) == -1) {
        perror("Failed to create pipe for monitor");
        return;
    }
    
    pid_t pid = fork();
    
    if (pid < 0) {
        perror("Failed to fork process for monitor");
        close(monitor_to_hub_pipe[0]);
        close(monitor_to_hub_pipe[1]);
        monitor_to_hub_pipe[0] = -1;
        monitor_to_hub_pipe[1] = -1;
        return;
    } else if (pid == 0) { // Child process (monitor)
        close(monitor_to_hub_pipe[0]); // Monitor only writes
        char pipe_fd_str[16];
        snprintf(pipe_fd_str, sizeof(pipe_fd_str), "%d", monitor_to_hub_pipe[1]);
        
        // Redirect monitor's stderr to /dev/null or a log file if desired
        // int dev_null_fd = open("/dev/null", O_WRONLY);
        // if(dev_null_fd != -1) {
        //     dup2(dev_null_fd, STDERR_FILENO);
        //     close(dev_null_fd);
        // }

        execl("./treasure_monitor", "treasure_monitor", pipe_fd_str, (char *)NULL);
        perror("Failed to execute treasure_monitor"); // This goes to original stderr
        close(monitor_to_hub_pipe[1]); // Close on exec fail
        exit(EXIT_FAILURE);
    } else { // Parent process (hub)
        close(monitor_to_hub_pipe[1]); // Hub only reads
        monitor_to_hub_pipe[1] = -1; 
        monitor_pid = pid;
        monitor_running = 1;
        printf("Monitor started with PID: %d. Reading on FD %d\n", monitor_pid, monitor_to_hub_pipe[0]);
    }
}

// Read output from monitor pipe
void read_from_monitor_pipe() {
    if (monitor_to_hub_pipe[0] == -1) {
        printf("Error: Monitor pipe not open for reading.\n");
        return;
    }
    char buffer[4096];
    ssize_t nbytes;
    int continue_reading = 1;

    printf("--- Monitor Output ---\n");
    while (continue_reading && (nbytes = read(monitor_to_hub_pipe[0], buffer, sizeof(buffer) - 1)) > 0) {
        buffer[nbytes] = '\0';
        char *delimiter_pos = strstr(buffer, END_OF_MONITOR_OUTPUT);
        if (delimiter_pos != NULL) {
            *delimiter_pos = '\0'; // Terminate string before delimiter
            printf("%s", buffer);
            continue_reading = 0; // Stop after finding delimiter
        } else {
            printf("%s", buffer);
        }
    }
    if (nbytes < 0 && errno != EINTR) { // EINTR is fine, loop will retry or signal handler sets prompt
        perror("Error reading from monitor pipe");
    } else if (nbytes == 0 && continue_reading) {
        // Pipe closed prematurely by monitor without sending delimiter
        printf("Monitor pipe closed unexpectedly.\n");
        // Consider this an error or monitor stopped state
        if (monitor_running) { // If hub thought it was running
             // This might be redundant if SIGCHLD handles it
            monitor_running = 0; 
            monitor_pid = -1;
            close(monitor_to_hub_pipe[0]);
            monitor_to_hub_pipe[0] = -1;
        }
    }
    printf("\n--- End of Monitor Output ---\n");
}

// Send command to monitor
void send_command(const char* command, const char* args) {
    if (!monitor_running) {
        printf("Error: Monitor is not running. Use 'start_monitor' first.\n");
        return;
    }
    
    int cmd_fd = open(COMMAND_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (cmd_fd == -1) {
        perror("Failed to open command file");
        return;
    }
    write(cmd_fd, command, strlen(command));
    close(cmd_fd);
    
    if (args != NULL && strlen(args) > 0) {
        int args_fd = open(ARGS_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (args_fd == -1) {
            perror("Failed to open arguments file");
            return;
        }
        write(args_fd, args, strlen(args));
        close(args_fd);
    } else {
        // Ensure args file is empty if no args
        int args_fd = open(ARGS_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (args_fd != -1) close(args_fd); else perror("Failed to truncate/create args file");
    }
    
    if (kill(monitor_pid, SIGUSR1) == -1) {
        perror("Failed to signal monitor process");
        if (errno == ESRCH) { // Monitor died
            monitor_running = 0;
            monitor_pid = -1;
            if(monitor_to_hub_pipe[0] != -1) {close(monitor_to_hub_pipe[0]); monitor_to_hub_pipe[0] = -1;}
        }
        return;
    }
    
    // For commands that expect output, read from the pipe
    if (strcmp(command, "stop_monitor") != 0) {
        read_from_monitor_pipe();
    }
}

// List all hunts
void list_hunts() {
    send_command("list_hunts", NULL);
}

// List all treasures in a hunt
void list_treasures() {
    char hunt_id[MAX_CMD_LENGTH];
    printf("Enter hunt ID: ");
    if (fgets(hunt_id, sizeof(hunt_id), stdin) == NULL) return;
    hunt_id[strcspn(hunt_id, "\n")] = 0; 
    send_command("list_treasures", hunt_id);
}

// View a specific treasure
void view_treasure() {
    char input[MAX_CMD_LENGTH * 2];
    char hunt_id[MAX_CMD_LENGTH];
    char treasure_id[MAX_CMD_LENGTH];
    
    printf("Enter hunt ID: ");
    if (fgets(hunt_id, sizeof(hunt_id), stdin) == NULL) return;
    hunt_id[strcspn(hunt_id, "\n")] = 0; 
    
    printf("Enter treasure ID: ");
    if (fgets(treasure_id, sizeof(treasure_id), stdin) == NULL) return;
    treasure_id[strcspn(treasure_id, "\n")] = 0; 
    
    snprintf(input, sizeof(input), "%s %s", hunt_id, treasure_id);
    send_command("view_treasure", input);
}

void stop_monitor() {
    if (!monitor_running) {
        printf("Monitor is not running.\n");
        return;
    }
    send_command("stop_monitor", NULL); // Monitor will close its pipe end upon exit
    printf("Stop command sent to monitor. Returning to prompt.\n");
    // SIGCHLD handler will eventually clean up monitor_running and pipe FD
}

void calculate_score() {
    DIR *dir;
    struct dirent *entry;
    char current_dir_path[MAX_PATH_LENGTH];

    if (getcwd(current_dir_path, sizeof(current_dir_path)) == NULL) {
        perror("Failed to get current working directory");
        return;
    }

    dir = opendir("."); // Open current directory
    if (dir == NULL) {
        perror("Failed to open current directory to list hunts");
        return;
    }

    printf("\nCalculating scores for all hunts...\n");
    int hunts_found = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            // Check if this directory is a hunt (e.g., contains treasures.dat)
            char treasures_dat_path[MAX_PATH_LENGTH];
            snprintf(treasures_dat_path, sizeof(treasures_dat_path), "%s/%s", entry->d_name, TREASURE_FILE_CHECK);
            struct stat st_check;
            if (stat(treasures_dat_path, &st_check) == 0 && S_ISREG(st_check.st_mode)) {
                // This is a hunt directory
                hunts_found++;
                printf("\n--- Scores for Hunt: %s ---\n", entry->d_name);

                int score_pipe[2];
                if (pipe(score_pipe) == -1) {
                    perror("Failed to create pipe for score calculator");
                    continue;
                }

                pid_t child_pid = fork();
                if (child_pid == -1) {
                    perror("Failed to fork for score calculator");
                    close(score_pipe[0]);
                    close(score_pipe[1]);
                    continue;
                }

                if (child_pid == 0) { // Child process
                    close(score_pipe[0]); // Child writes, doesn't read
                    dup2(score_pipe[1], STDOUT_FILENO); // Redirect stdout to pipe
                    close(score_pipe[1]); // Close original write end

                    // Construct full path to score_calculator if it's not in PATH
                    // For simplicity, assume it's in current dir or PATH
                    execl(SCORE_CALCULATOR_EXEC, SCORE_CALCULATOR_EXEC, entry->d_name, (char *)NULL);
                    perror("Failed to execute score_calculator");
                    exit(EXIT_FAILURE);
                } else { // Parent process
                    close(score_pipe[1]); // Parent reads, doesn't write
                    
                    char buffer[4096];
                    ssize_t nbytes;
                    while ((nbytes = read(score_pipe[0], buffer, sizeof(buffer) - 1)) > 0) {
                        buffer[nbytes] = '\0';
                        printf("%s", buffer);
                    }
                    if (nbytes < 0 && errno != EINTR) {
                        perror("Error reading from score_calculator pipe");
                    }
                    close(score_pipe[0]);
                    waitpid(child_pid, NULL, 0); // Wait for this score_calculator to finish
                    printf("--- End of Scores for Hunt: %s ---\n", entry->d_name);
                }
            }
        }
    }
    closedir(dir);
    if (hunts_found == 0) {
        printf("No hunts found to calculate scores for.\n");
    }
    printf("\nScore calculation complete.\n");
}

// Main function - interactive loop
int main() {
    char command[MAX_CMD_LENGTH];
    setup_signal_handlers();
    
    printf("Treasure Hub - Interactive Interface\n");
    printf("Available commands: start_monitor, list_hunts, list_treasures, view_treasure, calculate_score, stop_monitor, exit\n");
    
    while (1) {
        printf("> ");
        fflush(stdout);
        errno = 0;
        if (fgets(command, sizeof(command), stdin) == NULL) {
            if (feof(stdin)) {
                // ... (existing EOF handling, simplified for brevity)
                if (monitor_running) { printf("\nMonitor running. Use 'stop_monitor'.\n> "); clearerr(stdin); continue;}
                printf("\nExiting Treasure Hub.\n"); break;
            } else if (errno == EINTR) {
                clearerr(stdin); continue;
            } else {
                // ... (existing error handling, simplified for brevity)
                perror("\nError reading command"); if (monitor_running) printf("Monitor running.\n");
                clearerr(stdin); continue;
            }
        }
        command[strcspn(command, "\n")] = 0;
        
        if (strcmp(command, "start_monitor") == 0) start_monitor();
        else if (strcmp(command, "list_hunts") == 0) list_hunts();
        else if (strcmp(command, "list_treasures") == 0) list_treasures();
        else if (strcmp(command, "view_treasure") == 0) view_treasure();
        else if (strcmp(command, "calculate_score") == 0) calculate_score(); // New command
        else if (strcmp(command, "stop_monitor") == 0) stop_monitor();
        else if (strcmp(command, "exit") == 0) {
            if (monitor_running) printf("Error: Monitor is still running. Please use 'stop_monitor' first.\n");
            else { printf("Exiting Treasure Hub.\n"); break; }
        } else if (strcmp(command, "") == 0) { /* Do nothing */ }
        else printf("Unknown command: %s\n", command);
    }
    
    if (monitor_running && monitor_pid != -1) { // Ensure monitor is stopped if hub exits unexpectedly
        kill(monitor_pid, SIGTERM);
        waitpid(monitor_pid, NULL, 0);
    }
    if (monitor_to_hub_pipe[0] != -1) close(monitor_to_hub_pipe[0]);
    if (monitor_to_hub_pipe[1] != -1) close(monitor_to_hub_pipe[1]); // Should be closed already
    unlink(COMMAND_FILE);
    unlink(ARGS_FILE);
    return 0;
}
