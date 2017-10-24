#include <stdlib.h>    // malloc, free, rand
#include <assert.h>    // Good ol' assertions
#include <unistd.h>    // getpid
#include <time.h>      // Seed for rand


#include <stdio.h>     // fopen, fclose, printf, scanf, sprintf
#include <string.h>    // memcpy, strcpy, strcat, strerror
#include <sys/types.h> // Types for system functions
#include <sys/stat.h>  // stat
#include <dirent.h>    // opendir, readdir
#include <errno.h>     // errno


// `typedef`s
typedef enum {false, true} bool;

typedef enum {START_ROOM, MID_ROOM, END_ROOM} room_type;

typedef struct Room
{
    int id;
    char* name;
    int* connections;
    int connection_count;
    room_type type;
} Room;


// Forward declarations



// Get path of most recently modified room file directory. `false` signifies
// failure.
bool get_fresh_dir_path(char* path_buffer)
{
    const char* dir_prefix = "comitoz.rooms.";
    const int dir_prefix_len = 14;

    DIR* cwd = opendir(".");
    if (cwd == NULL)
    {
        fprintf(
            stderr,
            "opendir(\".\") failed with: \"%s\"\n",
            strerror(errno)
        );
    }

    struct stat sb;
    char freshest_path[256];
    time_t freshest_path_time = 0;

    struct dirent* entity = readdir(cwd);
    while (entity != NULL)
    {
        char* entity_name = entity->d_name;
        bool matching_prefix = true;
        int i;
        for (i = 0; i < dir_prefix_len; ++i)
        {
            if (entity_name[i] == '\0' || entity_name[i] != dir_prefix[i])
            {
                matching_prefix = false;

                break;
            }
        }

        if (matching_prefix)
        {
            if (stat(entity_name, &sb) == -1)
            {
                fprintf(
                    stderr,
                    "stat(%s, &sb) failed with: \"%s\"\n",
                    entity_name,
                    strerror(errno)
                );

                return false;
            }

            time_t last_time = sb.st_mtime;
            if (last_time > freshest_path_time)
            {
                strcpy
            }
        }

        entity = readdir(cwd);
    }
}

// Read directory for room files and parse each one into a `Room`


// Use file handle to parse file contents into a `Room`


// (main) Get room array, enter game loop

