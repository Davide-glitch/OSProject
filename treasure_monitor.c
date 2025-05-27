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
#define TREASURE_FILE "treasures.dat"
#define END_OF_MONITOR_OUTPUT "---END_OF_MONITOR_OUTPUT---"
#define MONITOR_STOP_DELAY 10

volatile sig_atomic_t should_stop = 0;
volatile sig_atomic_t command_received = 0;
int output_pipe_fd = -1;

void command_handler(int sig)
{
    command_received = 1;
}

void stop_handler(int sig)
{
    char stop_msg[256];
    snprintf(stop_msg, sizeof(stop_msg),
             "Monitor received stop signal, delaying exit for %d seconds...\n",
             MONITOR_STOP_DELAY);
    write(output_pipe_fd, stop_msg, strlen(stop_msg));
    write(output_pipe_fd, END_OF_MONITOR_OUTPUT, strlen(END_OF_MONITOR_OUTPUT));

    sleep(MONITOR_STOP_DELAY);
    should_stop = 1;
}

void setup_signal_handlers()
{
    struct sigaction sa_cmd, sa_stop;

    memset(&sa_cmd, 0, sizeof(sa_cmd));
    sa_cmd.sa_handler = command_handler;
    sa_cmd.sa_flags = SA_RESTART;
    sigaction(SIGUSR1, &sa_cmd, NULL);

    memset(&sa_stop, 0, sizeof(sa_stop));
    sa_stop.sa_handler = stop_handler;
    sa_stop.sa_flags = 0;
    sigaction(SIGUSR2, &sa_stop, NULL);
}

void send_output(const char *message)
{
    if (output_pipe_fd != -1)
    {
        write(output_pipe_fd, message, strlen(message));
    }
}

void send_end_marker()
{
    if (output_pipe_fd != -1)
    {
        write(output_pipe_fd, END_OF_MONITOR_OUTPUT, strlen(END_OF_MONITOR_OUTPUT));
    }
}

void list_hunts()
{
    DIR *dir;
    struct dirent *entry;
    char output[4096];
    int hunt_count = 0;

    dir = opendir(".");
    if (dir == NULL)
    {
        send_output("Error: Could not open current directory\n");
        send_end_marker();
        return;
    }

    send_output("Available hunts:\n");

    while ((entry = readdir(dir)) != NULL)
    {
        if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
        {
            char treasure_path[MAX_PATH_LENGTH];
            snprintf(treasure_path, sizeof(treasure_path), "%s/%s", entry->d_name, TREASURE_FILE);

            if (access(treasure_path, F_OK) == 0)
            {
                int fd = open(treasure_path, O_RDONLY);
                int treasure_count = 0;
                if (fd != -1)
                {
                    struct stat st;
                    if (fstat(fd, &st) == 0)
                    {
                        treasure_count = st.st_size / sizeof(struct {
                                             char id[32];
                                             char username[64];
                                             double latitude;
                                             double longitude;
                                             char clue[256];
                                             int value;
                                         });
                    }
                    close(fd);
                }

                snprintf(output, sizeof(output), "Hunt: %s (Treasures: %d)\n",
                         entry->d_name, treasure_count);
                send_output(output);
                hunt_count++;
            }
        }
    }
    closedir(dir);

    if (hunt_count == 0)
    {
        send_output("No hunts found.\n");
    }

    send_end_marker();
}

void list_treasures(const char *hunt_id)
{
    char treasure_path[MAX_PATH_LENGTH];
    snprintf(treasure_path, sizeof(treasure_path), "%s/%s", hunt_id, TREASURE_FILE);

    int fd = open(treasure_path, O_RDONLY);
    if (fd == -1)
    {
        char error_msg[512];
        snprintf(error_msg, sizeof(error_msg), "Error: Could not open treasure file for hunt '%s'\n", hunt_id);
        send_output(error_msg);
        send_end_marker();
        return;
    }

    typedef struct
    {
        char id[32];
        char username[64];
        double latitude;
        double longitude;
        char clue[256];
        int value;
    } Treasure;

    Treasure treasure;
    char output[1024];
    int treasure_count = 0;

    snprintf(output, sizeof(output), "Treasures in hunt '%s':\n", hunt_id);
    send_output(output);

    while (read(fd, &treasure, sizeof(Treasure)) == sizeof(Treasure))
    {
        snprintf(output, sizeof(output),
                 "ID: %s, User: %s, Location: (%.6f, %.6f), Value: %d, Clue: %s\n",
                 treasure.id, treasure.username, treasure.latitude, treasure.longitude,
                 treasure.value, treasure.clue);
        send_output(output);
        treasure_count++;
    }

    close(fd);

    if (treasure_count == 0)
    {
        send_output("No treasures found in this hunt.\n");
    }

    send_end_marker();
}

void view_treasure(const char *hunt_id, const char *treasure_id)
{
    char treasure_path[MAX_PATH_LENGTH];
    snprintf(treasure_path, sizeof(treasure_path), "%s/%s", hunt_id, TREASURE_FILE);

    int fd = open(treasure_path, O_RDONLY);
    if (fd == -1)
    {
        char error_msg[512];
        snprintf(error_msg, sizeof(error_msg), "Error: Could not open treasure file for hunt '%s'\n", hunt_id);
        send_output(error_msg);
        send_end_marker();
        return;
    }

    typedef struct
    {
        char id[32];
        char username[64];
        double latitude;
        double longitude;
        char clue[256];
        int value;
    } Treasure;

    Treasure treasure;
    char output[1024];
    int found = 0;

    while (read(fd, &treasure, sizeof(Treasure)) == sizeof(Treasure))
    {
        if (strcmp(treasure.id, treasure_id) == 0)
        {
            snprintf(output, sizeof(output),
                     "Treasure Details:\nID: %s\nUser: %s\nLocation: (%.6f, %.6f)\nValue: %d\nClue: %s\n",
                     treasure.id, treasure.username, treasure.latitude, treasure.longitude,
                     treasure.value, treasure.clue);
            send_output(output);
            found = 1;
            break;
        }
    }

    close(fd);

    if (!found)
    {
        snprintf(output, sizeof(output), "Treasure with ID '%s' not found in hunt '%s'\n", treasure_id, hunt_id);
        send_output(output);
    }

    send_end_marker();
}

void process_command()
{
    char command[MAX_CMD_LENGTH] = {0};
    char args[MAX_CMD_LENGTH * 2] = {0};

    int cmd_fd = open(COMMAND_FILE, O_RDONLY);
    if (cmd_fd != -1)
    {
        read(cmd_fd, command, sizeof(command) - 1);
        close(cmd_fd);
    }

    int args_fd = open(ARGS_FILE, O_RDONLY);
    if (args_fd != -1)
    {
        read(args_fd, args, sizeof(args) - 1);
        close(args_fd);
    }

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
        char *space = strchr(args, ' ');
        if (space)
        {
            *space = '\0';
            view_treasure(args, space + 1);
        }
        else
        {
            send_output("Error: Invalid arguments for view_treasure\n");
            send_end_marker();
        }
    }
    else if (strcmp(command, "stop_monitor") == 0)
    {
        return;
    }
    else
    {
        char error_msg[512];
        snprintf(error_msg, sizeof(error_msg), "Unknown command: %s\n", command);
        send_output(error_msg);
        send_end_marker();
    }
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <pipe_fd>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    output_pipe_fd = atoi(argv[1]);
    setup_signal_handlers();

    char start_msg[256];
    snprintf(start_msg, sizeof(start_msg), "Monitor process started with PID: %d\n", getpid());
    send_output(start_msg);
    send_end_marker();

    while (!should_stop)
    {
        if (command_received)
        {
            command_received = 0;
            process_command();
        }
        pause();
    }

    char exit_msg[256];
    snprintf(exit_msg, sizeof(exit_msg), "Monitor process exiting after %d second delay.\n", MONITOR_STOP_DELAY);
    send_output(exit_msg);
    send_end_marker();

    if (output_pipe_fd != -1)
    {
        close(output_pipe_fd);
    }

    exit(0);
}
