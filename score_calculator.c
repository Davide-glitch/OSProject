#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#define MAX_PATH_LENGTH 512
#define TREASURE_FILE "treasures.dat"

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

typedef struct
{
    char username[MAX_USERNAME_LENGTH];
    int score;
} UserScore;

#define MAX_UNIQUE_USERS 100

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("Error: Usage: %s <hunt_id>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *hunt_id = argv[1];
    char treasure_path[MAX_PATH_LENGTH];
    snprintf(treasure_path, MAX_PATH_LENGTH, "%s/%s", hunt_id, TREASURE_FILE);

    int fd = open(treasure_path, O_RDONLY);
    if (fd == -1)
    {
        printf("Error: Could not open treasure file '%s' for hunt '%s'. (%s)\n", treasure_path, hunt_id, strerror(errno));
        return EXIT_FAILURE;
    }

    Treasure treasure;
    UserScore scores[MAX_UNIQUE_USERS];
    int num_users = 0;

    for (int i = 0; i < MAX_UNIQUE_USERS; i++)
    {
        scores[i].username[0] = '\0';
        scores[i].score = 0;
    }

    while (read(fd, &treasure, sizeof(Treasure)) == sizeof(Treasure))
    {
        int user_idx = -1;
        for (int i = 0; i < num_users; i++)
        {
            if (strcmp(scores[i].username, treasure.username) == 0)
            {
                user_idx = i;
                break;
            }
        }

        if (user_idx != -1)
        {
            scores[user_idx].score += treasure.value;
        }
        else
        {
            if (num_users < MAX_UNIQUE_USERS)
            {
                strncpy(scores[num_users].username, treasure.username, MAX_USERNAME_LENGTH - 1);
                scores[num_users].username[MAX_USERNAME_LENGTH - 1] = '\0';
                scores[num_users].score = treasure.value;
                num_users++;
            }
            else
            {
                printf("Warning: Maximum unique users (%d) reached for scoring in hunt '%s'. Some user scores might be omitted.\n", MAX_UNIQUE_USERS, hunt_id);
            }
        }
    }
    close(fd);

    if (num_users == 0)
    {
        printf("No treasures found or no users with treasures in hunt '%s'.\n", hunt_id);
    }
    else
    {
        for (int i = 0; i < num_users; i++)
        {
            printf("User: %s, Score: %d\n", scores[i].username, scores[i].score);
        }
    }

    return EXIT_SUCCESS;
}
