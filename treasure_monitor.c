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
#include <time.h>

#define MAX_CMD_LENGTH 256
#define MAX_PATH_LENGTH 512
#define COMMAND_FILE "command.tmp"
#define ARGS_FILE "args.tmp"
#define TREASURE_FILE "treasures.dat"
#define END_OF_MONITOR_OUTPUT "---END_OF_MONITOR_OUTPUT---"

#define MAX_CLUE_LENGTH 256
#define MAX_USERNAME_LENGTH 64
#define MAX_ID_LENGTH 32

typedef struct
{
    char id[MAX_ID_LENGTH];
    char username[MAX_USERNAME_LENGTH];
    double latitude;
    double longitude;
    char clue[MAX_CLUE_LENGTH];
    int value;
} Treasure;

volatile sig_atomic_t command_received = 0;
volatile sig_atomic_t running = 1;
int output_pipe_fd = -1;

void handle_command_signal(int sig)
{
    command_received = 1;
}

void handle_termination_signal(int sig)
{
    running = 0;
}

void setup_signal_handlers()
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_command_signal;
    sigaction(SIGUSR1, &sa, NULL);

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_termination_signal;
    sigaction(SIGTERM, &sa, NULL);

    sigaction(SIGINT, &sa, NULL);
}

int count_treasures(const char *hunt_id)
{
    char treasure_path[MAX_PATH_LENGTH];
    snprintf(treasure_path, MAX_PATH_LENGTH, "%s/%s", hunt_id, TREASURE_FILE);

    int fd = open(treasure_path, O_RDONLY);
    if (fd == -1)
    {
        return 0;
    }

    Treasure treasure;
    int count = 0;

    while (read(fd, &treasure, sizeof(Treasure)) == sizeof(Treasure))
    {
        count++;
    }

    close(fd);
    return count;
}

void list_hunts()
{
    DIR *dir;
    struct dirent *entry;
    char buffer[MAX_PATH_LENGTH * 2];

    dir = opendir(".");
    if (dir == NULL)
    {
        dprintf(output_pipe_fd, "Error: Failed to open current directory.\n");
        return;
    }

    dprintf(output_pipe_fd, "\nAvailable Hunts:\n");
    dprintf(output_pipe_fd, "-------------------------------\n");

    int hunt_count = 0;

    while ((entry = readdir(dir)) != NULL)
    {
        if (entry->d_type == DT_DIR &&
            strcmp(entry->d_name, ".") != 0 &&
            strcmp(entry->d_name, "..") != 0)
        {

            char path[MAX_PATH_LENGTH];
            snprintf(path, MAX_PATH_LENGTH, "%s/%s", entry->d_name, TREASURE_FILE);

            struct stat st;
            if (stat(path, &st) == 0)
            {
                int count = count_treasures(entry->d_name);
                snprintf(buffer, sizeof(buffer), "Hunt: %s (Treasures: %d)\n", entry->d_name, count);
                dprintf(output_pipe_fd, "%s", buffer);
                hunt_count++;
            }
        }
    }
    closedir(dir);

    if (hunt_count == 0)
    {
        dprintf(output_pipe_fd, "No hunts found.\n");
    }
    else
    {
        snprintf(buffer, sizeof(buffer), "\nTotal hunts: %d\n", hunt_count);
        dprintf(output_pipe_fd, "%s", buffer);
    }
}

void list_treasures(const char *hunt_id)
{
    char treasure_path[MAX_PATH_LENGTH];
    char buffer[1024];
    snprintf(treasure_path, MAX_PATH_LENGTH, "%s/%s", hunt_id, TREASURE_FILE);

    struct stat file_stat;
    if (stat(treasure_path, &file_stat) == -1)
    {
        if (errno == ENOENT)
        {
            snprintf(buffer, sizeof(buffer), "Hunt '%s' does not exist or has no treasures.\n", hunt_id);
        }
        else
        {
            snprintf(buffer, sizeof(buffer), "Failed to get file information for hunt '%s': %s\n", hunt_id, strerror(errno));
        }
        dprintf(output_pipe_fd, "%s", buffer);
        return;
    }

    snprintf(buffer, sizeof(buffer), "\nHunt: %s\nTotal file size: %ld bytes\n", hunt_id, (long)file_stat.st_size);
    dprintf(output_pipe_fd, "%s", buffer);

    char time_str[100];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&file_stat.st_mtime));
    snprintf(buffer, sizeof(buffer), "Last modified: %s\n\n", time_str);
    dprintf(output_pipe_fd, "%s", buffer);

    int fd = open(treasure_path, O_RDONLY);
    if (fd == -1)
    {
        snprintf(buffer, sizeof(buffer), "Failed to open treasure file for hunt '%s': %s\n", hunt_id, strerror(errno));
        dprintf(output_pipe_fd, "%s", buffer);
        return;
    }

    Treasure treasure;
    int count = 0;

    snprintf(buffer, sizeof(buffer), "Treasures in hunt %s:\n-----------------------------------------\n", hunt_id);
    dprintf(output_pipe_fd, "%s", buffer);

    while (read(fd, &treasure, sizeof(Treasure)) == sizeof(Treasure))
    {
        snprintf(buffer, sizeof(buffer), "ID: %s\nUser: %s\nGPS: (%.6f, %.6f)\nValue: %d\n-----------------------------------------\n",
                 treasure.id, treasure.username, treasure.latitude, treasure.longitude, treasure.value);
        dprintf(output_pipe_fd, "%s", buffer);
        count++;
    }
    close(fd);

    if (count == 0)
    {
        dprintf(output_pipe_fd, "No treasures found.\n");
    }
    else
    {
        snprintf(buffer, sizeof(buffer), "Total treasures: %d\n", count);
        dprintf(output_pipe_fd, "%s", buffer);
    }
}

void view_treasure(const char *hunt_id, const char *treasure_id)
{
    char treasure_path[MAX_PATH_LENGTH];
    char buffer[1024];
    snprintf(treasure_path, MAX_PATH_LENGTH, "%s/%s", hunt_id, TREASURE_FILE);

    int fd = open(treasure_path, O_RDONLY);
    if (fd == -1)
    {
        if (errno == ENOENT)
        {
            snprintf(buffer, sizeof(buffer), "Hunt '%s' does not exist.\n", hunt_id);
        }
        else
        {
            snprintf(buffer, sizeof(buffer), "Failed to open treasure file for hunt '%s': %s\n", hunt_id, strerror(errno));
        }
        dprintf(output_pipe_fd, "%s", buffer);
        return;
    }

    Treasure treasure;
    int found = 0;

    while (read(fd, &treasure, sizeof(Treasure)) == sizeof(Treasure))
    {
        if (strcmp(treasure.id, treasure_id) == 0)
        {
            snprintf(buffer, sizeof(buffer), "\nTreasure Details:\nID: %s\nUser: %s\nGPS Coordinates: (%.6f, %.6f)\nClue: %s\nValue: %d\n",
                     treasure.id, treasure.username, treasure.latitude, treasure.longitude, treasure.clue, treasure.value);
            dprintf(output_pipe_fd, "%s", buffer);
            found = 1;
            break;
        }
    }
    close(fd);

    if (!found)
    {
        snprintf(buffer, sizeof(buffer), "Treasure '%s' not found in hunt '%s'.\n", treasure_id, hunt_id);
        dprintf(output_pipe_fd, "%s", buffer);
    }
}

void process_command()
{
    char command[MAX_CMD_LENGTH] = {0};
    char args[MAX_CMD_LENGTH] = {0};

    int cmd_fd = open(COMMAND_FILE, O_RDONLY);
    if (cmd_fd == -1)
    {
        fprintf(stderr, "[Monitor] Error: Failed to open command file: %s\n", strerror(errno));
        return;
    }
    read(cmd_fd, command, sizeof(command) - 1);
    close(cmd_fd);

    int args_fd = open(ARGS_FILE, O_RDONLY);
    if (args_fd != -1)
    {
        read(args_fd, args, sizeof(args) - 1);
        close(args_fd);
    }

    fprintf(stderr, "[Monitor] Received command: %s, Args: %s\n", command, args[0] ? args : "(none)");

    if (strcmp(command, "list_hunts") == 0)
    {
        list_hunts();
    }
    else if (strcmp(command, "list_treasures") == 0)
    {
        list_treasures(args);
    }
    else if (strcmp(command, "view_treasure") == 0)
    {
        char hunt_id[MAX_ID_LENGTH];
        char treasure_id[MAX_ID_LENGTH];
        if (sscanf(args, "%s %s", hunt_id, treasure_id) == 2)
        {
            view_treasure(hunt_id, treasure_id);
        }
        else
        {
            dprintf(output_pipe_fd, "Error: Invalid arguments for view_treasure.\n");
        }
    }
    else if (strcmp(command, "stop_monitor") == 0)
    {
        fprintf(stderr, "[Monitor] Shutting down...\n");
        running = 0;
    }
    else
    {
        dprintf(output_pipe_fd, "Error: Unknown command '%s' received by monitor.\n", command);
    }
    dprintf(output_pipe_fd, "%s\n", END_OF_MONITOR_OUTPUT);
    fsync(output_pipe_fd);
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "[Monitor] Error: Output pipe FD not provided.\n");
        return EXIT_FAILURE;
    }
    output_pipe_fd = atoi(argv[1]);
    if (output_pipe_fd <= 0)
    {
        fprintf(stderr, "[Monitor] Error: Invalid output pipe FD '%s'.\n", argv[1]);
        return EXIT_FAILURE;
    }

    fprintf(stderr, "[Monitor] Started (PID: %d), outputting to FD %d\n", getpid(), output_pipe_fd);

    setup_signal_handlers();

    while (running)
    {
        if (command_received)
        {
            process_command();
            command_received = 0;
        }
        usleep(100000);
    }

    fprintf(stderr, "[Monitor] Cleaning up and terminating.\n");
    if (output_pipe_fd != -1)
    {
        close(output_pipe_fd);
    }
    return 0;
}
