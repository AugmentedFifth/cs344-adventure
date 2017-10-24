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
bool get_fresh_dir_path(char* path_buffer);

int parse_room_dir(const char* dir_path, Room* room_buffer, int max_rooms);


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

        return false;
    }

    struct stat sb;
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
                freshest_path_time = last_time;
                strcpy(path_buffer, entity_name);
            }
        }

        entity = readdir(cwd);
    }

    if (freshest_path_time == 0)
    {
        fprintf(
            stderr,
            "Did not find any directories of the form `comitoz.rooms.*` in the current dir."
        );

        return false;
    }

    return true;
}

// Read directory for room files and parse each one into a `Room`.
// This returns the numebr of `Room`s that were parsed, or -1 on failure.
int parse_room_dir(const char* dir_path, Room* room_buffer, int max_rooms)
{
    DIR* dir = opendir(dir_path);
    if (dir == NULL)
    {
        fprintf(
            stderr,
            "opendir(\"%s\") failed with: \"%s\"\n",
            dir_path,
            strerror(errno)
        );

        return -1;
    }

    char rel_path_buffer[256];

    struct dirent* entity = readdir(cwd);
    while (entity != NULL)
    {
        char* entity_name = entity->d_name;
        strcpy(rel_path_buffer, dir_path);
        strcat(rel_path_buffer, "/");
        strcat(rel_path_buffer, entity_name);

        FILE* file_handle = fopen(rel_path_buffer, "w");
        if (file_handle == NULL)
        {
            fprintf(
                stderr,
                "fopen(\"%s\", \"w\") failed with: \"%s\"\n",
                rel_path_buffer,
                strerror(errno)
            );

            return -1;
        }

        entity = readdir(cwd);
    }

   
}


// Use file handle to parse file contents into a `Room`


// (main) Get room array, enter game loop
int main(void)
{
    char path_buffer[256];
    if (!get_fresh_dir_path(path_buffer))
    {
        return 1;
    }

    return 0;
}
