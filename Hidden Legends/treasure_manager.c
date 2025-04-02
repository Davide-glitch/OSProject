#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>

#define MAX_CLUE_LENGTH 256
#define MAX_USERNAME_LENGTH 64
#define MAX_ID_LENGTH 32
#define MAX_PATH_LENGTH 512
#define TREASURE_FILE "treasures.dat"
#define LOG_FILE "logged_hunt"

// Define treasure structure
typedef struct {
    char id[MAX_ID_LENGTH];
    char username[MAX_USERNAME_LENGTH];
    double latitude;
    double longitude;
    char clue[MAX_CLUE_LENGTH];
    int value;
} Treasure;

// Function prototypes
void add_treasure(const char *hunt_id);
void list_treasures(const char *hunt_id);
void view_treasure(const char *hunt_id, const char *treasure_id);
void remove_treasure(const char *hunt_id, const char *treasure_id);
void remove_hunt(const char *hunt_id);
void log_operation(const char *hunt_id, const char *operation);
void create_symlink(const char *hunt_id);
int ensure_hunt_directory(const char *hunt_id);

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: treasure_manager <operation> [arguments]\n");
        printf("Operations:\n");
        printf("  --add <hunt_id>\n");
        printf("  --list <hunt_id>\n");
        printf("  --view <hunt_id> <treasure_id>\n");
        printf("  --remove_treasure <hunt_id> <treasure_id>\n");
        printf("  --remove_hunt <hunt_id>\n");
        return 1;
    }

    if (strcmp(argv[1], "--add") == 0) {
        if (argc != 3) {
            printf("Usage: treasure_manager --add <hunt_id>\n");
            return 1;
        }
        add_treasure(argv[2]);
    } else if (strcmp(argv[1], "--list") == 0) {
        if (argc != 3) {
            printf("Usage: treasure_manager --list <hunt_id>\n");
            return 1;
        }
        list_treasures(argv[2]);
    } else if (strcmp(argv[1], "--view") == 0) {
        if (argc != 4) {
            printf("Usage: treasure_manager --view <hunt_id> <treasure_id>\n");
            return 1;
        }
        view_treasure(argv[2], argv[3]);
    } else if (strcmp(argv[1], "--remove_treasure") == 0) {
        if (argc != 4) {
            printf("Usage: treasure_manager --remove_treasure <hunt_id> <treasure_id>\n");
            return 1;
        }
        remove_treasure(argv[2], argv[3]);
    } else if (strcmp(argv[1], "--remove_hunt") == 0) {
        if (argc != 3) {
            printf("Usage: treasure_manager --remove_hunt <hunt_id>\n");
            return 1;
        }
        remove_hunt(argv[2]);
    } else {
        printf("Unknown operation: %s\n", argv[1]);
        return 1;
    }

    return 0;
}

// Create hunt directory if it doesn't exist
int ensure_hunt_directory(const char *hunt_id) {
    struct stat st = {0};
    if (stat(hunt_id, &st) == -1) {
        if (mkdir(hunt_id, 0755) != 0) {
            perror("Failed to create hunt directory");
            return 0;
        }
    }
    return 1;
}

// Log operation to the log file
void log_operation(const char *hunt_id, const char *operation) {
    char log_path[MAX_PATH_LENGTH];
    snprintf(log_path, MAX_PATH_LENGTH, "%s/%s", hunt_id, LOG_FILE);
    
    int fd = open(log_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd == -1) {
        perror("Failed to open log file");
        return;
    }
    
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);
    
    char log_entry[512];
    snprintf(log_entry, sizeof(log_entry), "[%s] %s\n", timestamp, operation);
    
    write(fd, log_entry, strlen(log_entry));
    close(fd);
    
    // Create or update symlink
    create_symlink(hunt_id);
}

// Create symbolic link for log file
void create_symlink(const char *hunt_id) {
    char log_path[MAX_PATH_LENGTH];
    char link_path[MAX_PATH_LENGTH];
    
    snprintf(log_path, MAX_PATH_LENGTH, "%s/%s", hunt_id, LOG_FILE);
    snprintf(link_path, MAX_PATH_LENGTH, "%s-%s", LOG_FILE, hunt_id);
    
    // Remove existing symlink if it exists
    unlink(link_path);
    
    if (symlink(log_path, link_path) != 0) {
        perror("Failed to create symlink");
    }
}

// Add a new treasure to the hunt
void add_treasure(const char *hunt_id) {
    if (!ensure_hunt_directory(hunt_id)) {
        return;
    }
    
    char treasure_path[MAX_PATH_LENGTH];
    snprintf(treasure_path, MAX_PATH_LENGTH, "%s/%s", hunt_id, TREASURE_FILE);
    
    Treasure new_treasure;
    
    printf("Enter treasure ID: ");
    scanf("%31s", new_treasure.id);
    
    printf("Enter username: ");
    scanf("%63s", new_treasure.username);
    
    printf("Enter latitude: ");
    scanf("%lf", &new_treasure.latitude);
    
    printf("Enter longitude: ");
    scanf("%lf", &new_treasure.longitude);
    
    printf("Enter clue: ");
    // Clear input buffer
    while (getchar() != '\n');
    fgets(new_treasure.clue, MAX_CLUE_LENGTH, stdin);
    // Remove newline character
    new_treasure.clue[strcspn(new_treasure.clue, "\n")] = '\0';
    
    printf("Enter value: ");
    scanf("%d", &new_treasure.value);
    
    int fd = open(treasure_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd == -1) {
        perror("Failed to open treasure file");
        return;
    }
    
    if (write(fd, &new_treasure, sizeof(Treasure)) != sizeof(Treasure)) {
        perror("Failed to write treasure data");
        close(fd);
        return;
    }
    
    close(fd);
    
    char operation[256];
    snprintf(operation, sizeof(operation), "Added treasure %s by user %s", new_treasure.id, new_treasure.username);
    log_operation(hunt_id, operation);
    
    printf("Treasure added successfully.\n");
}

// List all treasures in the hunt
void list_treasures(const char *hunt_id) {
    char treasure_path[MAX_PATH_LENGTH];
    snprintf(treasure_path, MAX_PATH_LENGTH, "%s/%s", hunt_id, TREASURE_FILE);
    
    // Get file info
    struct stat file_stat;
    if (stat(treasure_path, &file_stat) == -1) {
        perror("Failed to get file information");
        return;
    }
    
    // Display hunt info
    printf("Hunt: %s\n", hunt_id);
    printf("Total file size: %ld bytes\n", (long)file_stat.st_size);
    
    // Format and display last modification time
    char time_str[100];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&file_stat.st_mtime));
    printf("Last modified: %s\n\n", time_str);
    
    int fd = open(treasure_path, O_RDONLY);
    if (fd == -1) {
        if (errno == ENOENT) {
            printf("No treasures found in hunt %s.\n", hunt_id);
        } else {
            perror("Failed to open treasure file");
        }
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
    
    char operation[100];
    snprintf(operation, sizeof(operation), "Listed all treasures in hunt %s", hunt_id);
    log_operation(hunt_id, operation);
}

// View details of a specific treasure
void view_treasure(const char *hunt_id, const char *treasure_id) {
    char treasure_path[MAX_PATH_LENGTH];
    snprintf(treasure_path, MAX_PATH_LENGTH, "%s/%s", hunt_id, TREASURE_FILE);
    
    int fd = open(treasure_path, O_RDONLY);
    if (fd == -1) {
        perror("Failed to open treasure file");
        return;
    }
    
    Treasure treasure;
    int found = 0;
    
    while (read(fd, &treasure, sizeof(Treasure)) == sizeof(Treasure)) {
        if (strcmp(treasure.id, treasure_id) == 0) {
            printf("Treasure Details:\n");
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
        printf("Treasure %s not found in hunt %s.\n", treasure_id, hunt_id);
    }
    
    char operation[200];
    snprintf(operation, sizeof(operation), "Viewed treasure %s in hunt %s", treasure_id, hunt_id);
    log_operation(hunt_id, operation);
}

// Remove a treasure from the hunt
void remove_treasure(const char *hunt_id, const char *treasure_id) {
    char treasure_path[MAX_PATH_LENGTH];
    char temp_path[MAX_PATH_LENGTH];
    snprintf(treasure_path, MAX_PATH_LENGTH, "%s/%s", hunt_id, TREASURE_FILE);
    snprintf(temp_path, MAX_PATH_LENGTH, "%s/%s.tmp", hunt_id, TREASURE_FILE);
    
    int src_fd = open(treasure_path, O_RDONLY);
    if (src_fd == -1) {
        perror("Failed to open treasure file");
        return;
    }
    
    int dst_fd = open(temp_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dst_fd == -1) {
        perror("Failed to create temporary file");
        close(src_fd);
        return;
    }
    
    Treasure treasure;
    int found = 0;
    
    while (read(src_fd, &treasure, sizeof(Treasure)) == sizeof(Treasure)) {
        if (strcmp(treasure.id, treasure_id) == 0) {
            found = 1;
            continue;  // Skip this treasure
        }
        
        if (write(dst_fd, &treasure, sizeof(Treasure)) != sizeof(Treasure)) {
            perror("Failed to write to temporary file");
            close(src_fd);
            close(dst_fd);
            unlink(temp_path);
            return;
        }
    }
    
    close(src_fd);
    close(dst_fd);
    
    if (!found) {
        printf("Treasure %s not found in hunt %s.\n", treasure_id, hunt_id);
        unlink(temp_path);
        return;
    }
    
    if (rename(temp_path, treasure_path) != 0) {
        perror("Failed to update treasure file");
        unlink(temp_path);
        return;
    }
    
    char operation[200];
    snprintf(operation, sizeof(operation), "Removed treasure %s from hunt %s", treasure_id, hunt_id);
    log_operation(hunt_id, operation);
    
    printf("Treasure %s removed from hunt %s.\n", treasure_id, hunt_id);
}

// Remove an entire hunt
void remove_hunt(const char *hunt_id) {
    char treasure_path[MAX_PATH_LENGTH];
    char log_path[MAX_PATH_LENGTH];
    char link_path[MAX_PATH_LENGTH];
    
    snprintf(treasure_path, MAX_PATH_LENGTH, "%s/%s", hunt_id, TREASURE_FILE);
    snprintf(log_path, MAX_PATH_LENGTH, "%s/%s", hunt_id, LOG_FILE);
    snprintf(link_path, MAX_PATH_LENGTH, "%s-%s", LOG_FILE, hunt_id);
    
    // Log the operation before removing the hunt
    char operation[100];
    snprintf(operation, sizeof(operation), "Removed hunt %s", hunt_id);
    log_operation(hunt_id, operation);
    
    // Remove files and directory
    unlink(treasure_path);
    unlink(log_path);
    
    if (rmdir(hunt_id) != 0) {
        perror("Failed to remove hunt directory");
    } else {
        printf("Hunt %s successfully removed.\n", hunt_id);
    }
    
    // Remove symlink
    unlink(link_path);
}