#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>  // Added this include for strftime() and localtime() functions

#define MAX_CMD_LENGTH 256
#define MAX_PATH_LENGTH 512
#define COMMAND_FILE "command.tmp"
#define ARGS_FILE "args.tmp"
#define TREASURE_FILE "treasures.dat"

// Define treasure structure (same as in treasure_manager.c)
#define MAX_CLUE_LENGTH 256
#define MAX_USERNAME_LENGTH 64
#define MAX_ID_LENGTH 32

typedef struct {
    char id[MAX_ID_LENGTH];
    char username[MAX_USERNAME_LENGTH];
    double latitude;
    double longitude;
    char clue[MAX_CLUE_LENGTH];
    int value;
} Treasure;

volatile sig_atomic_t command_received = 0;
volatile sig_atomic_t running = 1;

void handle_command_signal(int sig) {
    command_received = 1;
}

void handle_termination_signal(int sig) {
    running = 0;
}

void setup_signal_handlers() {
    struct sigaction sa;
    
    // Set up SIGUSR1 handler
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_command_signal;
    sigaction(SIGUSR1, &sa, NULL);
    
    // Set up SIGTERM handler
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_termination_signal;
    sigaction(SIGTERM, &sa, NULL);
    
    // Set up SIGINT handler
    sigaction(SIGINT, &sa, NULL);
}

// Count treasures in a hunt
int count_treasures(const char *hunt_id) {
    char treasure_path[MAX_PATH_LENGTH];
    snprintf(treasure_path, MAX_PATH_LENGTH, "%s/%s", hunt_id, TREASURE_FILE);
    
    int fd = open(treasure_path, O_RDONLY);
    if (fd == -1) {
        return 0;
    }
    
    Treasure treasure;
    int count = 0;
    
    while (read(fd, &treasure, sizeof(Treasure)) == sizeof(Treasure)) {
        count++;
    }
    
    close(fd);
    return count;
}

// List all hunts with treasure counts
void list_hunts() {
    DIR *dir;
    struct dirent *entry;
    
    dir = opendir(".");
    if (dir == NULL) {
        perror("Failed to open current directory");
        return;
    }
    
    printf("\nAvailable Hunts:\n");
    printf("-------------------------------\n");
    
    int hunt_count = 0;
    
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR && 
            strcmp(entry->d_name, ".") != 0 && 
            strcmp(entry->d_name, "..") != 0) {
            
            // Check if this directory contains a treasures.dat file
            char path[MAX_PATH_LENGTH];
            snprintf(path, MAX_PATH_LENGTH, "%s/%s", entry->d_name, TREASURE_FILE);
            
            struct stat st;
            if (stat(path, &st) == 0) {
                int count = count_treasures(entry->d_name);
                printf("Hunt: %s (Treasures: %d)\n", entry->d_name, count);
                hunt_count++;
            }
        }
    }
    
    closedir(dir);
    
    if (hunt_count == 0) {
        printf("No hunts found.\n");
    } else {
        printf("\nTotal hunts: %d\n", hunt_count);
    }
}

// List treasures in a hunt (similar to treasure_manager's list_treasures)
void list_treasures(const char *hunt_id) {
    char treasure_path[MAX_PATH_LENGTH];
    snprintf(treasure_path, MAX_PATH_LENGTH, "%s/%s", hunt_id, TREASURE_FILE);
    
    // Get file info
    struct stat file_stat;
    if (stat(treasure_path, &file_stat) == -1) {
        if (errno == ENOENT) {
            printf("Hunt '%s' does not exist or has no treasures.\n", hunt_id);
        } else {
            perror("Failed to get file information");
        }
        return;
    }
    
    // Display hunt info
    printf("\nHunt: %s\n", hunt_id);
    printf("Total file size: %ld bytes\n", (long)file_stat.st_size);
    
    // Format and display last modification time
    char time_str[100];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&file_stat.st_mtime));
    printf("Last modified: %s\n\n", time_str);
    
    int fd = open(treasure_path, O_RDONLY);
    if (fd == -1) {
        perror("Failed to open treasure file");
        return;
    }
    
    Treasure treasure;
    int count = 0;
    
    printf("Treasures in hunt %s:\n", hunt_id);
    printf("-----------------------------------------\n");
    
    while (read(fd, &treasure, sizeof(Treasure)) == sizeof(Treasure)) {
        printf("ID: %s\n", treasure.id);
        printf("User: %s\n", treasure.username);
        printf("GPS: (%.6f, %.6f)\n", treasure.latitude, treasure.longitude);
        printf("Value: %d\n", treasure.value);
        printf("-----------------------------------------\n");
        count++;
    }
    
    close(fd);
    
    if (count == 0) {
        printf("No treasures found.\n");
    } else {
        printf("Total treasures: %d\n", count);
    }
}

// View details of a specific treasure
void view_treasure(const char *hunt_id, const char *treasure_id) {
    char treasure_path[MAX_PATH_LENGTH];
    snprintf(treasure_path, MAX_PATH_LENGTH, "%s/%s", hunt_id, TREASURE_FILE);
    
    int fd = open(treasure_path, O_RDONLY);
    if (fd == -1) {
        if (errno == ENOENT) {
            printf("Hunt '%s' does not exist.\n", hunt_id);
        } else {
            perror("Failed to open treasure file");
        }
        return;
    }
    
    Treasure treasure;
    int found = 0;
    
    while (read(fd, &treasure, sizeof(Treasure)) == sizeof(Treasure)) {
        if (strcmp(treasure.id, treasure_id) == 0) {
            printf("\nTreasure Details:\n");
            printf("ID: %s\n", treasure.id);
            printf("User: %s\n", treasure.username);
            printf("GPS Coordinates: (%.6f, %.6f)\n", treasure.latitude, treasure.longitude);
            printf("Clue: %s\n", treasure.clue);
            printf("Value: %d\n", treasure.value);
            found = 1;
            break;
        }
    }
    
    close(fd);
    
    if (!found) {
        printf("Treasure '%s' not found in hunt '%s'.\n", treasure_id, hunt_id);
    }
}

// Process commands received from treasure_hub
void process_command() {
    char command[MAX_CMD_LENGTH] = {0};
    char args[MAX_CMD_LENGTH] = {0};
    
    // Read command
    int cmd_fd = open(COMMAND_FILE, O_RDONLY);
    if (cmd_fd == -1) {
        perror("Failed to open command file");
        return;
    }
    
    read(cmd_fd, command, sizeof(command) - 1);
    close(cmd_fd);
    
    // Read arguments if needed
    int args_fd = open(ARGS_FILE, O_RDONLY);
    if (args_fd != -1) {
        read(args_fd, args, sizeof(args) - 1);
        close(args_fd);
    }
    
    printf("\n[Monitor] Received command: %s\n", command);
    
    if (strcmp(command, "list_hunts") == 0) {
        list_hunts();
    } else if (strcmp(command, "list_treasures") == 0) {
        list_treasures(args);
    } else if (strcmp(command, "view_treasure") == 0) {
        // Split args into hunt_id and treasure_id
        char hunt_id[MAX_ID_LENGTH];
        char treasure_id[MAX_ID_LENGTH];
        sscanf(args, "%s %s", hunt_id, treasure_id);
        view_treasure(hunt_id, treasure_id);
    } else if (strcmp(command, "stop_monitor") == 0) {
        printf("[Monitor] Shutting down...\n");
        running = 0;
    }
}

int main() {
    printf("[Monitor] Started (PID: %d)\n", getpid());
    
    setup_signal_handlers();
    
    // Main monitor loop
    while (running) {
        if (command_received) {
            process_command();
            command_received = 0;
        }
        usleep(100000);  // Sleep 100ms to avoid CPU hogging
    }
    
    // Cleanup before exiting
    printf("[Monitor] Cleaning up and terminating.\n");
    // No delay anymore to improve responsiveness
    
    return 0;
}
