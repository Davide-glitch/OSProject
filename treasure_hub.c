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

pid_t monitor_pid = -1;
int monitor_running = 0;
int waiting_for_monitor_end = 0;

// Function prototypes
void start_monitor();
void list_hunts();
void list_treasures();
void view_treasure();
void stop_monitor();
void handle_child_exit(int sig);

// Set up signal handlers
void setup_signal_handlers() {
    struct sigaction sa;
    
    // Set up SIGCHLD handler
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_child_exit;
    sigaction(SIGCHLD, &sa, NULL);
}

// Handle child process termination
void handle_child_exit(int sig) {
    int status;
    pid_t pid;
    
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (pid == monitor_pid) {
            if (WIFEXITED(status)) {
                printf("\nMonitor terminated with exit code: %d\n", WEXITSTATUS(status)); // Added newline for clarity
            } else if (WIFSIGNALED(status)) {
                printf("\nMonitor terminated by signal: %d\n", WTERMSIG(status)); // Added newline for clarity
            }
            
            monitor_running = 0;
            monitor_pid = -1;
            waiting_for_monitor_end = 0;
            
            // Re-print the prompt after handling signal to show hub is ready
            printf("> ");
            fflush(stdout);
        }
    }
}

// Start monitor process
void start_monitor() {
    if (monitor_running) {
        printf("Monitor is already running!\n");
        return;
    }
    
    pid_t pid = fork();
    
    if (pid < 0) {
        perror("Failed to fork process");
        return;
    } else if (pid == 0) {
        // Child process (monitor)
        execl("./treasure_monitor", "treasure_monitor", NULL);
        perror("Failed to execute treasure_monitor");
        exit(EXIT_FAILURE);
    } else {
        // Parent process
        monitor_pid = pid;
        monitor_running = 1;
        printf("Monitor started with PID: %d\n", monitor_pid);
    }
}

// Send command to monitor
void send_command(const char* command, const char* args) {
    if (!monitor_running) {
        printf("Error: Monitor is not running. Use 'start_monitor' first.\n");
        return;
    }
    
    // Write command to file
    int cmd_fd = open(COMMAND_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (cmd_fd == -1) {
        perror("Failed to open command file");
        return;
    }
    write(cmd_fd, command, strlen(command));
    close(cmd_fd);
    
    // Write arguments to file if provided
    if (args != NULL) {
        int args_fd = open(ARGS_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (args_fd == -1) {
            perror("Failed to open arguments file");
            return;
        }
        write(args_fd, args, strlen(args));
        close(args_fd);
    }
    
    // Signal the monitor process
    if (kill(monitor_pid, SIGUSR1) == -1) {
        if (errno == ESRCH) {
            printf("Monitor process no longer exists. Resetting state.\n");
            monitor_running = 0;
            monitor_pid = -1;
            waiting_for_monitor_end = 0;
        }
        return;
    }
    
    // For commands that need output, wait a moment to let monitor process output
    if (strcmp(command, "stop_monitor") != 0) {
        usleep(100000);  // 100ms should be enough for monitor to process
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
    if (fgets(hunt_id, sizeof(hunt_id), stdin) == NULL) {
        return;
    }
    hunt_id[strcspn(hunt_id, "\n")] = 0; // Remove newline
    
    send_command("list_treasures", hunt_id);
}

// View a specific treasure
void view_treasure() {
    char input[MAX_CMD_LENGTH];
    char hunt_id[MAX_CMD_LENGTH];
    char treasure_id[MAX_CMD_LENGTH];
    
    printf("Enter hunt ID: ");
    if (fgets(hunt_id, sizeof(hunt_id), stdin) == NULL) {
        return;
    }
    hunt_id[strcspn(hunt_id, "\n")] = 0; // Remove newline
    
    printf("Enter treasure ID: ");
    if (fgets(treasure_id, sizeof(treasure_id), stdin) == NULL) {
        return;
    }
    treasure_id[strcspn(treasure_id, "\n")] = 0; // Remove newline
    
    // Combine hunt_id and treasure_id for the args
    snprintf(input, sizeof(input), "%s %s", hunt_id, treasure_id);
    send_command("view_treasure", input);
}

// Stop the monitor - simplified to just send command and return to prompt
void stop_monitor() {
    if (!monitor_running) {
        printf("Monitor is not running.\n");
        return;
    }
    
    send_command("stop_monitor", NULL);
    printf("Stop command sent to monitor. Returning to prompt.\n");
}

// Main function - interactive loop
int main() {
    char command[MAX_CMD_LENGTH];
    
    setup_signal_handlers();
    
    printf("Treasure Hub - Interactive Interface\n");
    printf("Available commands: start_monitor, list_hunts, list_treasures, view_treasure, stop_monitor, exit\n");
    
    while (1) {
        printf("> ");
        fflush(stdout); // Ensure prompt is shown before reading input

        // Clear errno before calling fgets, as it might not be set on success
        errno = 0; 
        if (fgets(command, sizeof(command), stdin) == NULL) {
            // Check if fgets failed due to EOF or a real error, or just interruption
            if (feof(stdin)) {
                printf("\nEOF detected. ");
                if (monitor_running) {
                     printf("Monitor is still running. Please use 'stop_monitor' first.\n");
                     clearerr(stdin); // Clear EOF state
                     continue; // Go back to prompt
                } else {
                    printf("Exiting Treasure Hub.\n");
                    break; // Exit cleanly on EOF if monitor is stopped
                }
            } else if (errno == EINTR) {
                 // Interrupted by a signal (expected behavior for SIGCHLD).
                 // Clear any potential error state and loop again to re-read.
                 clearerr(stdin);
                 continue; // Silently continue without printing the error
            } else {
                 // A genuine read error occurred (not EOF, not EINTR)
                 perror("\nError reading command"); 
                 if (monitor_running) {
                     printf("Monitor is still running. Please use 'stop_monitor' first.\n");
                 }
                 // Decide whether to exit or try again. Let's try continuing.
                 clearerr(stdin);
                 continue;
            }
        }
        
        // Remove newline character
        command[strcspn(command, "\n")] = 0;
        
        // --- Command processing logic (no changes needed here) ---
        if (strcmp(command, "start_monitor") == 0) {
            start_monitor();
        } else if (strcmp(command, "list_hunts") == 0) {
            list_hunts();
        } else if (strcmp(command, "list_treasures") == 0) {
            list_treasures();
        } else if (strcmp(command, "view_treasure") == 0) {
            view_treasure();
        } else if (strcmp(command, "stop_monitor") == 0) {
            stop_monitor();
        } else if (strcmp(command, "exit") == 0) {
            if (monitor_running) {
                printf("Error: Monitor is still running. Please use 'stop_monitor' first.\n");
            } else {
                printf("Exiting Treasure Hub.\n");
                break;
            }
        } else if (strcmp(command, "") == 0) {
            // Empty command, do nothing
        } else {
            printf("Unknown command: %s\n", command);
        }
        // --- End of command processing ---
    }
    
    // Clean up temporary files
    unlink(COMMAND_FILE);
    unlink(ARGS_FILE);
    
    return 0;
}
