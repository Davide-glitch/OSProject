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
#define TREASURE_FILE_CHECK "treasures.dat"
#define END_OF_MONITOR_OUTPUT "---END_OF_MONITOR_OUTPUT---"

volatile sig_atomic_t monitor_stopping = 0;
volatile sig_atomic_t monitor_running = 0;
volatile sig_atomic_t waiting_for_monitor_end = 0;

pid_t monitor_pid = -1;
int monitor_to_hub_pipe[2] = {-1, -1};

#define MONITOR_STOP_DELAY 20

void start_monitor();
void list_hunts();
void list_treasures();
void view_treasure();
void stop_monitor();
void calculate_score();
void handle_child_exit(int sig);
void read_from_monitor_pipe();

void sigchld_handler(int sig)
{
    int status;
    pid_t pid = waitpid(monitor_pid, &status, WNOHANG);
    if (pid == monitor_pid)
    {
        printf("\nMonitor process terminated with status: %d\n", WEXITSTATUS(status));
        monitor_running = 0;
        monitor_stopping = 0;
        monitor_pid = -1;
        waiting_for_monitor_end = 0;
        if (monitor_to_hub_pipe[0] != -1)
        {
            close(monitor_to_hub_pipe[0]);
            monitor_to_hub_pipe[0] = -1;
        }
        if (monitor_to_hub_pipe[1] != -1)
        {
            close(monitor_to_hub_pipe[1]);
            monitor_to_hub_pipe[1] = -1;
        }
        printf("> ");
        fflush(stdout);
    }
}

void setup_signal_handlers()
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigchld_handler;
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);
}

void start_monitor()
{
    if (monitor_running)
    {
        printf("Monitor is already running!\n");
        return;
    }

    if (pipe(monitor_to_hub_pipe) == -1)
    {
        perror("Failed to create pipe for monitor");
        return;
    }

    pid_t pid = fork();

    if (pid < 0)
    {
        perror("Failed to fork process for monitor");
        close(monitor_to_hub_pipe[0]);
        close(monitor_to_hub_pipe[1]);
        monitor_to_hub_pipe[0] = -1;
        monitor_to_hub_pipe[1] = -1;
        return;
    }
    else if (pid == 0)
    {
        close(monitor_to_hub_pipe[0]);
        char pipe_fd_str[16];
        snprintf(pipe_fd_str, sizeof(pipe_fd_str), "%d", monitor_to_hub_pipe[1]);

        execl("./treasure_monitor", "treasure_monitor", pipe_fd_str, (char *)NULL);
        perror("Failed to execute treasure_monitor");
        close(monitor_to_hub_pipe[1]);
        exit(EXIT_FAILURE);
    }
    else
    {
        close(monitor_to_hub_pipe[1]);
        monitor_to_hub_pipe[1] = -1;
        monitor_pid = pid;
        monitor_running = 1;
        printf("Monitor started with PID: %d\n", monitor_pid);

        char buffer[4096];
        ssize_t nbytes;
        while ((nbytes = read(monitor_to_hub_pipe[0], buffer, sizeof(buffer) - 1)) > 0)
        {
            buffer[nbytes] = '\0';
            char *delimiter_pos = strstr(buffer, END_OF_MONITOR_OUTPUT);
            if (delimiter_pos != NULL)
            {
                break;
            }
        }
    }
}

void read_from_monitor_pipe()
{
    if (monitor_to_hub_pipe[0] == -1)
    {
        printf("Error: Monitor pipe not open for reading.\n");
        return;
    }
    char buffer[4096];
    ssize_t nbytes;
    int continue_reading = 1;

    printf("--- Monitor Output ---\n");
    while (continue_reading && (nbytes = read(monitor_to_hub_pipe[0], buffer, sizeof(buffer) - 1)) > 0)
    {
        buffer[nbytes] = '\0';
        char *delimiter_pos = strstr(buffer, END_OF_MONITOR_OUTPUT);
        if (delimiter_pos != NULL)
        {
            *delimiter_pos = '\0';
            printf("%s", buffer);
            continue_reading = 0;
        }
        else
        {
            printf("%s", buffer);
        }
    }
    if (nbytes < 0 && errno != EINTR)
    {
        perror("Error reading from monitor pipe");
    }
    else if (nbytes == 0 && continue_reading)
    {
        printf("Monitor pipe closed unexpectedly.\n");
        if (monitor_running)
        {
            monitor_running = 0;
            monitor_pid = -1;
            close(monitor_to_hub_pipe[0]);
            monitor_to_hub_pipe[0] = -1;
        }
    }
    printf("\n--- End of Monitor Output ---\n");
}

void send_command(const char *command, const char *args)
{
    if (!monitor_running)
    {
        printf("Error: Monitor is not running. Use 'start_monitor' first.\n");
        return;
    }

    if (monitor_stopping)
    {
        printf("Error: Monitor is stopping. Please wait until it terminates.\n");
        return;
    }

    int cmd_fd = open(COMMAND_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (cmd_fd == -1)
    {
        perror("Failed to open command file");
        return;
    }
    write(cmd_fd, command, strlen(command));
    close(cmd_fd);

    if (args != NULL && strlen(args) > 0)
    {
        int args_fd = open(ARGS_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (args_fd == -1)
        {
            perror("Failed to open arguments file");
            return;
        }
        write(args_fd, args, strlen(args));
        close(args_fd);
    }
    else
    {
        int args_fd = open(ARGS_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (args_fd != -1)
            close(args_fd);
        else
            perror("Failed to truncate/create args file");
    }

    if (kill(monitor_pid, SIGUSR1) == -1)
    {
        perror("Failed to signal monitor process");
        if (errno == ESRCH)
        {
            monitor_running = 0;
            monitor_pid = -1;
            if (monitor_to_hub_pipe[0] != -1)
            {
                close(monitor_to_hub_pipe[0]);
                monitor_to_hub_pipe[0] = -1;
            }
        }
        return;
    }

    if (strcmp(command, "stop_monitor") != 0)
    {
        read_from_monitor_pipe();
    }
}

void list_hunts()
{
    send_command("list_hunts", NULL);
}

void list_treasures()
{
    char hunt_id[MAX_CMD_LENGTH];
    printf("Enter hunt ID: ");
    if (fgets(hunt_id, sizeof(hunt_id), stdin) == NULL)
        return;
    hunt_id[strcspn(hunt_id, "\n")] = 0;
    send_command("list_treasures", hunt_id);
}

void view_treasure()
{
    char input[MAX_CMD_LENGTH * 2];
    char hunt_id[MAX_CMD_LENGTH];
    char treasure_id[MAX_CMD_LENGTH];

    printf("Enter hunt ID: ");
    if (fgets(hunt_id, sizeof(hunt_id), stdin) == NULL)
        return;
    hunt_id[strcspn(hunt_id, "\n")] = 0;

    printf("Enter treasure ID: ");
    if (fgets(treasure_id, sizeof(treasure_id), stdin) == NULL)
        return;
    treasure_id[strcspn(treasure_id, "\n")] = 0;

    snprintf(input, sizeof(input), "%s %s", hunt_id, treasure_id);
    send_command("view_treasure", input);
}

void stop_monitor()
{
    if (!monitor_running)
    {
        printf("Error: No monitor is running\n");
        return;
    }
    if (monitor_stopping)
    {
        printf("Error: Monitor is already stopping\n");
        return;
    }

    printf("Stopping monitor... (this will take %d seconds)\n", MONITOR_STOP_DELAY);
    monitor_stopping = 1;
    waiting_for_monitor_end = 1;

    if (kill(monitor_pid, SIGUSR2) == -1)
    {
        perror("Failed to send stop signal to monitor");
        return;
    }

    printf("Stop command sent to monitor. Please wait...\n");
}

void calculate_score()
{
    DIR *dir;
    struct dirent *entry;
    char current_dir_path[MAX_PATH_LENGTH];

    if (getcwd(current_dir_path, sizeof(current_dir_path)) == NULL)
    {
        perror("Failed to get current working directory");
        return;
    }

    dir = opendir(".");
    if (dir == NULL)
    {
        perror("Failed to open current directory to list hunts");
        return;
    }

    printf("\nCalculating scores for all hunts...\n");
    int hunts_found = 0;

    while ((entry = readdir(dir)) != NULL)
    {
        if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
        {
            char treasures_dat_path[MAX_PATH_LENGTH];
            snprintf(treasures_dat_path, sizeof(treasures_dat_path), "%s/%s", entry->d_name, TREASURE_FILE_CHECK);
            struct stat st_check;
            if (stat(treasures_dat_path, &st_check) == 0 && S_ISREG(st_check.st_mode))
            {
                hunts_found++;
                printf("\n--- Scores for Hunt: %s ---\n", entry->d_name);

                int score_pipe[2];
                if (pipe(score_pipe) == -1)
                {
                    perror("Failed to create pipe for score calculator");
                    continue;
                }

                pid_t child_pid = fork();
                if (child_pid == -1)
                {
                    perror("Failed to fork for score calculator");
                    close(score_pipe[0]);
                    close(score_pipe[1]);
                    continue;
                }

                if (child_pid == 0)
                {
                    close(score_pipe[0]);
                    dup2(score_pipe[1], STDOUT_FILENO);
                    close(score_pipe[1]);

                    execl(SCORE_CALCULATOR_EXEC, SCORE_CALCULATOR_EXEC, entry->d_name, (char *)NULL);
                    perror("Failed to execute score_calculator");
                    exit(EXIT_FAILURE);
                }
                else
                {
                    close(score_pipe[1]);

                    char buffer[4096];
                    ssize_t nbytes;
                    while ((nbytes = read(score_pipe[0], buffer, sizeof(buffer) - 1)) > 0)
                    {
                        buffer[nbytes] = '\0';
                        printf("%s", buffer);
                    }
                    if (nbytes < 0 && errno != EINTR)
                    {
                        perror("Error reading from score_calculator pipe");
                    }
                    close(score_pipe[0]);
                    waitpid(child_pid, NULL, 0);
                    printf("--- End of Scores for Hunt: %s ---\n", entry->d_name);
                }
            }
        }
    }
    closedir(dir);
    if (hunts_found == 0)
    {
        printf("No hunts found to calculate scores for.\n");
    }
    printf("\nScore calculation complete.\n");
}

int main()
{
    char command[MAX_CMD_LENGTH];
    setup_signal_handlers();

    printf("Treasure Hub - Interactive Interface\n");
    printf("Available commands: start_monitor, list_hunts, list_treasures, view_treasure, calculate_score, stop_monitor, exit\n");

    while (1)
    {
        printf("> ");
        fflush(stdout);
        errno = 0;
        if (fgets(command, sizeof(command), stdin) == NULL)
        {
            if (feof(stdin))
            {
                if (monitor_running)
                {
                    printf("\nMonitor running. Use 'stop_monitor'.\n> ");
                    clearerr(stdin);
                    continue;
                }
                printf("\nExiting Treasure Hub.\n");
                break;
            }
            else if (errno == EINTR)
            {
                clearerr(stdin);
                continue;
            }
            else
            {
                perror("\nError reading command");
                if (monitor_running)
                    printf("Monitor running.\n");
                clearerr(stdin);
                continue;
            }
        }
        command[strcspn(command, "\n")] = 0;

        if (strcmp(command, "start_monitor") == 0)
        {
            start_monitor();
        }
        else if (strcmp(command, "list_hunts") == 0 ||
                 strcmp(command, "list_treasures") == 0 ||
                 strcmp(command, "view_treasure") == 0)
        {
            if (!monitor_running)
            {
                printf("Error: No monitor is running\n");
                continue;
            }
            if (monitor_stopping)
            {
                printf("Error: Monitor is stopping. Please wait until it terminates.\n");
                continue;
            }

            if (strcmp(command, "list_hunts") == 0)
                list_hunts();
            else if (strcmp(command, "list_treasures") == 0)
                list_treasures();
            else if (strcmp(command, "view_treasure") == 0)
                view_treasure();
        }
        else if (strcmp(command, "calculate_score") == 0)
        {
            calculate_score();
        }
        else if (strcmp(command, "stop_monitor") == 0)
        {
            stop_monitor();
        }
        else if (strcmp(command, "exit") == 0)
        {
            if (monitor_running)
            {
                printf("Error: Cannot exit while monitor is running. Stop monitor first.\n");
            }
            else
            {
                printf("Exiting Treasure Hub.\n");
                break;
            }
        }
        else if (strcmp(command, "") == 0)
        {
        }
        else
        {
            printf("Unknown command: %s\n", command);
        }
    }

    if (monitor_running && monitor_pid != -1)
    {
        kill(monitor_pid, SIGTERM);
        waitpid(monitor_pid, NULL, 0);
    }
    if (monitor_to_hub_pipe[0] != -1)
        close(monitor_to_hub_pipe[0]);
    if (monitor_to_hub_pipe[1] != -1)
        close(monitor_to_hub_pipe[1]);
    unlink(COMMAND_FILE);
    unlink(ARGS_FILE);
    return 0;
}
