#include <stdlib.h>    // malloc, realloc, free
#include <stdio.h>     // fopen, fclose, printf, fprintf, getline
#include <string.h>    // strcpy, strcat, strerror, sscanf
#include <sys/types.h> // Types for system functions
#include <sys/stat.h>  // stat
#include <dirent.h>    // opendir, readdir
#include <errno.h>     // errno
#include <pthread.h>   // pthread_t, pthread_create, mutex stuff
#include <time.h>      // time, localtime, strftime


// `typedef`s
typedef enum {false, true} bool; // tfw no C99

typedef enum {START_ROOM, MID_ROOM, END_ROOM} room_type;

typedef struct Room
{
    int id;
    char* name;
    char* connections[6];
    int connection_count;
    room_type type;
} Room;


// Forward declarations
void _Room(Room* room);

bool spawn_child(pthread_t* thread, pthread_mutex_t* mutex);

void try_to_write_date(void* mutex_ptr);

bool get_fresh_dir_path(char* path_buffer);

int parse_room_dir(const char* dir_path, Room** room_buffer, int max_rooms);

Room* parse_file(FILE* file_handle, int room_id, bool* parse_succeeded);

const char* room_type_to_str(room_type rt);

int game_loop(
    Room**           room_buffer,
    int              room_count,
    Room*            current_room,
    pthread_mutex_t* mutex,
    pthread_t*       child
);


// `free`s memory owned by a `Room`, basically a glorified destructor
void _Room(Room* room)
{
    free(room->name);

    int i;
    for (i = 0; i < room->connection_count; ++i)
    {
        free(room->connections[i]);
    }
}

// Spawns the child thread, giving it the mutex and returning `false` in case
// of failure.
bool spawn_child(pthread_t* thread, pthread_mutex_t* mutex)
{
    // No options, just pass the mutex so we don't have to keep it as a global
    // variable
    int thread_spawn_result = pthread_create(
        thread,
        NULL,
        try_to_write_date,
        (void*) mutex
    );
    if (thread_spawn_result != 0)
    {
        fprintf(
            stderr,
            "Failed to spawn child thread, result code: %d\n",
            thread_spawn_result
        );
        return false;
    }

    return true;
}

// This is the entry point/environment for the single child thread of this
// program. It waits for the main thread to unlock the given mutex and then
// greedily writes the current time to a file that the main thread can then
// read.
void try_to_write_date(void* mutex_ptr)
{
    pthread_mutex_t* mutex = (pthread_mutex_t*) mutex_ptr;

    // Waiting for the parent thread to let us do the thing
    pthread_mutex_lock(mutex);

    // We got the lock, so it's time to write the time to a file.
    // First, let's get the date as a string
    time_t current_timestamp = time(NULL);
    struct tm* time_struct = localtime(&current_timestamp);

    char time_str[64];
    strftime(time_str, 64, "%I:%M%p, %A, %B %e, %Y", time_struct);
    // The above ALMOST works, except that %p prints in uppercase and
    // %I is zero-padded (not space-padded). So, in lieu of fixing that
    time_str[0] = ' ';
    if (time_str[5] == 'A') // AM
    {
        time_str[5] = 'a';
    }
    else                    // PM
    {
        time_str[5] = 'p';
    }
    time_str[6] = 'm';

    // Alright, now we write
    FILE* time_file = fopen("currentTime.txt", "w");
    if (time_file == NULL)
    {
        fprintf(
            stderr,
            "Failed to open currentTime.txt for writing. errno: %s\n",
            strerror(errno)
        );

        exit(2); // Exit the program with extreme violence, no mercy, and
                 // no cleanup
    }

    fprintf(time_file, "%s", time_str);

    fclose(time_file);

    // We're done, so we can cede control back to the parent thread by
    // releasing the mutex
    pthread_mutex_unlock(mutex);
}

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

    // Loop over the entities in the current dir
    struct dirent* entity = readdir(cwd);
    while (entity != NULL)
    {
        // Check to see if the entity's name starts with "comitoz.rooms."
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

                closedir(cwd);

                return false;
            }

            // Accumulate on the largest time (i.e. most recent)
            time_t last_time = sb.st_mtime;
            if (last_time > freshest_path_time)
            {
                freshest_path_time = last_time;
                strcpy(path_buffer, entity_name);
            }
        }

        // Get the next entity
        entity = readdir(cwd);
    }

    // Free directory resource
    closedir(cwd);

    if (freshest_path_time == 0)
    {
        fprintf(
            stderr,
            "Did not find any directories of the form `comitoz.rooms.*` in the current dir\n"
        );

        return false;
    }

    return true;
}

// Read directory for room files and parse each one into a `Room`.
// This returns the numebr of `Room`s that were parsed, or -1 on failure.
int parse_room_dir(const char* dir_path, Room** room_buffer, int max_rooms)
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

    int parsed_room_count = 0;
    char rel_path_buffer[256];

    // Snoop this directory looking for any files in it
    struct dirent* entity = readdir(dir);
    while (entity != NULL && parsed_room_count < max_rooms)
    {
        char* entity_name = entity->d_name;
        // We assume all files in the folder are map files, but we still have
        // to filter out the . and .. directories
        if (entity_name[0] == '.')
        {
            entity = readdir(dir);
            continue;
        }

        // Get the relative path, not just the filename
        strcpy(rel_path_buffer, dir_path);
        strcat(rel_path_buffer, "/");
        strcat(rel_path_buffer, entity_name);

        FILE* file_handle = fopen(rel_path_buffer, "r");
        if (file_handle == NULL)
        {
            fprintf(
                stderr,
                "fopen(\"%s\", \"r\") failed with: \"%s\"\n",
                rel_path_buffer,
                strerror(errno)
            );

            closedir(dir); // Always clean up after yourself,
                           // and eat your vegetables

            return -1;
        }

        bool parse_succeeded = true;
        Room* parsed_room = parse_file( // Pass the actual parsing off to
            file_handle,                // another function
            parsed_room_count,
            &parse_succeeded
        );
        if (!parse_succeeded) // D'oh
        {
            fprintf(stderr, "Parse failure was in %s\n", rel_path_buffer);

            closedir(dir); // Stay clean, even in emergencies

            return -1;
        }
        room_buffer[parsed_room_count] = parsed_room; // Now the caller's
        parsed_room_count++;                          // neat little array
                                                      // to all the `Room`s
                                                      // we're parsing
        fclose(file_handle);

        entity = readdir(dir);
    }

    closedir(dir); // If you love it, set it free

    return parsed_room_count;
}

// Use file handle to parse file contents into a `Room*`. Caller must `free`
// the returned pointer
Room* parse_file(FILE* file_handle, int room_id, bool* parse_succeeded)
{
    char* line = NULL;
    size_t getline_buffer_len = 0;

    // Initialize `Room` to be returned
    Room* room = malloc(sizeof(Room));
    room->id = room_id;
    room->name = malloc(20 * sizeof(char));
    room->connection_count = 0;

    // Parsing state
    bool got_name = false;

    ssize_t chars_read = getline(&line, &getline_buffer_len, file_handle);
    while (chars_read != -1)
    {
        if (chars_read == 0)
        {
            // Ignore empty lines
            chars_read = getline(&line, &getline_buffer_len, file_handle);

            continue;
        }

        // Classic `sscanf`. This relies on the format to not incur buffer
        // overflow, so uh, make sure no one tampered with your room
        // files since you last checked.
        char second_token[6];
        char property_value[20];
        int sscanf_result = sscanf(
            line,
            "%*s %s %s\n",
            second_token, property_value
        );
        if (sscanf_result == 2)
        {
            // We only need the first character of the second token of the
            // property name to distinguish them due to the format
            switch (second_token[0])
            {
                case 'N': // ROOM NAME: ...
                {
                    if (got_name)
                    {
                        _Room(room);
                        fprintf(
                            stderr,
                            "Parsing failed: saw 'N' but already parsed ROOM NAME\n"
                        );
                        *parse_succeeded = false;

                        return room;
                    }

                    strcpy(room->name, property_value);
                    got_name = true;
                    break;
                }
                case 'T': // ROOM TYPE: ...
                {
                    // There is too much error handling here but I suppose
                    // there's no reason to remove it and make it harder to
                    // fix anyways
                    if (!got_name || room->connection_count < 3)
                    {
                        _Room(room);
                        if (!got_name)
                        {
                            fprintf(
                                stderr,
                                "Parsing failed: saw 'T' but no ROOM NAME yet\n"
                            );
                        }
                        else
                        {
                            fprintf(
                                stderr,
                                "Parsing failed: saw 'T' but only %d CONNECTIONs so far\n",
                                room->connection_count
                            );
                        }
                        *parse_succeeded = false;

                        return room;
                    }

                    // Due to the format, again, we only need the first
                    // character of the room type token
                    switch (property_value[0])
                    {
                        case 'S': // START_ROOM
                            room->type = START_ROOM;
                            break;
                        case 'M': // MID_ROOM
                            room->type = MID_ROOM;
                            break;
                        default:  // END_ROOM
                            room->type = END_ROOM;
                            break;
                    }
                    break;
                }
                default:  // CONNECTION #: ...
                {
                    if (!got_name || room->connection_count >= 6)
                    {
                        _Room(room);
                        if (!got_name)
                        {
                            fprintf(
                                stderr,
                                "Parsing failed: saw %c, but no ROOM NAME yet\n",
                                second_token[0]
                            );
                        }
                        else
                        {
                            fprintf(
                                stderr,
                                "Parsing failed: saw %c, but there's already %d connections\n",
                                second_token[0],
                                room->connection_count
                            );
                        }
                        *parse_succeeded = false;

                        return room;
                    }

                    // This looks sketchy but don't worry because we have the
                    // `_Room` function which is basically a destructor that
                    // takes care of this balogna
                    char* connection_name = malloc(20 * sizeof(char));
                    strcpy(connection_name, property_value);

                    room->connections[room->connection_count] =
                        connection_name;
                    room->connection_count++;
                    break;
                }
            }
        }

        // Consume the next line 'fore we go 'round again
        chars_read = getline(&line, &getline_buffer_len, file_handle);
    }

    free(line); // I have ethical objections to nonfree lines
    *parse_succeeded = true;

    return room;
}

// Convert `room_type` enum into its string representation
const char* room_type_to_str(room_type rt)
{
    switch (rt)
    {
        case START_ROOM:
            return "START_ROOM";
        case MID_ROOM:
            return "MID_ROOM";
        case END_ROOM:
            return "END_ROOM";
        default:
            return "wat"; // If you get this you almost certainly have
                          // uninitialized memory or some other sort of nasal
                          // demon
    }
}

int game_loop(
    Room**           room_buffer,
    int              room_count,
    Room*            current_room,
    pthread_mutex_t* mutex,
    pthread_t*       child
) {
    // `getline` stuff
    char* line = NULL;
    size_t getline_buffer_size = 0;
    ssize_t chars_read;

    // Path history storage
    int path_history_cap = 16;
    char** path_history = malloc(path_history_cap * sizeof(char*));
    int path_history_len = 0;

    bool changed_room = false;

    do
    {
        if (changed_room)
        {
            if (path_history_len >= path_history_cap)
            {
                // Reallocate more storage for the history if necessary
                path_history_cap *= 2;
                path_history = realloc(
                    path_history,
                    path_history_cap * sizeof(char*)
                );
            }
            // Update path history
            char* current_path_point = malloc(20 * sizeof(char));
            strcpy(current_path_point, current_room->name);
            path_history[path_history_len] = current_path_point;
            path_history_len++;

            changed_room = false;
        }

        if (current_room->type == END_ROOM)
        {
            break;
        }

        printf("CURRENT LOCATION: %s\n", current_room->name);
        printf("POSSIBLE CONNECTIONS: %s", current_room->connections[0]);
        int i;
        for (i = 1; i < current_room->connection_count; ++i)
        {
            printf(", %s", current_room->connections[i]);
        }
        printf(".\n");
        printf("WHERE TO? >");

        chars_read = getline(&line, &getline_buffer_size, stdin);

        line[chars_read - 1] = '\0'; // Overwriting captured '\n'
        if (strcmp(line, "time") == 0)
        {
            pthread_mutex_unlock(mutex); // The child now gets the lock and
                                         // uses it to write the date
            void* res; // Dummy variable
            pthread_join(*child, &res); // Wait for the child to finish writing
            // Get the mutex back and bring the child back up
            pthread_mutex_lock(mutex);
            if (!spawn_child(child, mutex))
            {
                return 1;
            }

            // Read what the child just wrote and display it to the user
            FILE* time_file = fopen("currentTime.txt", "r");
            if (time_file == NULL)
            {
                fprintf(
                    stderr,
                    "Failed to open currentTime.txt for reading. errno: %s\n",
                    strerror(errno)
                );
                return 1;
            }

            char* time_str = NULL;
            size_t buf_size = 0;
            if (getline(&time_str, &buf_size, time_file) != -1)
            {
                printf("\n%s\n", time_str); // Print date
            }
            else
            {
                fprintf(stderr, "getline() failed on currentTime.txt\n");
                return 1; // R.I.P.
            }

            // Cleanup
            free(time_str);

            fclose(time_file);
        }
        else
        {
            // Loop through possible connections the player can move to
            for (i = 0; i < current_room->connection_count; ++i)
            {
                // Did they choose this one?
                if (strcmp(line, current_room->connections[i]) == 0)
                {
                    // Ok, they did, so we have to now find the actual
                    // corresponding `Room` struct, so we just use a linear
                    // search again
                    int j;
                    for (j = 0; j < room_count; ++j)
                    {
                        if (strcmp(line, room_buffer[j]->name) == 0)
                        {
                            current_room = room_buffer[j];
                            changed_room = true;

                            break;
                        }
                    }

                    break;
                }
            }

            if (!changed_room) // No dice
            {
                printf("\nHUH? I DON'T UNDERSTAND THAT ROOM. TRY AGAIN.\n");
            }
        }

        printf("\n");
    } while (chars_read != -1);

    // Looks like they won
    printf("YOU HAVE FOUND THE END ROOM. CONGRATULATIONS!\n");
    printf("YOU TOOK %d STEPS. YOUR PATH TO VICTORY WAS:\n", path_history_len);
    int i;
    for (i = 0; i < path_history_len; ++i)
    {
        printf("%s\n", path_history[i]);
    }

    // Cleanup
    for (i = 0; i < path_history_len; ++i)
    {
        free(path_history[i]);
    }
    free(path_history);

    free(line);

    return 0;
}

// Get room array, enter game loop
int main(void)
{
    // Find the newest directory of files to play from
    char path_buffer[256];
    if (!get_fresh_dir_path(path_buffer))
    {
        return 1;
    }

    // Get our `Room`s into memory using the files in that dir
    int room_count = 7;
    Room* room_buffer[room_count];
    int parse_result = parse_room_dir(path_buffer, room_buffer, room_count);
    if (parse_result != 7)
    {
        fprintf(
            stderr,
            "Error: parse_room_dir() returned %d\n",
            parse_result
        );

        return parse_result == 0 ? 100 : parse_result;
    }

    // Create a mutex and start up child process to write date to file as
    // indicated by the mutex.
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock(&mutex);

    pthread_t child;
    if (!spawn_child(&child, &mutex))
    {
        return 1;
    }

    // Prepare game loop by obtaining start room and initializing variables
    // for `getline` to use, as well as making memory for the path history
    // to be stored in
    int i = 0;
    Room* current_room = room_buffer[i];
    for (i = 1; i < room_count && current_room->type != START_ROOM; ++i)
    {
        current_room = room_buffer[i];
    }
    if (current_room->type != START_ROOM)
    {
        fprintf(
            stderr,
            "None of the %d rooms are starting rooms",
            room_count
        );
        return 1;
    }

    // Rev up the game loop
    int game_loop_result = game_loop(
        room_buffer,
        room_count,
        current_room,
        &mutex,
        &child
    );

    // Cleanup
    void* res; // Dummy variable
    pthread_mutex_unlock(&mutex); // This actually is necessary to free the
    pthread_join(child, &res);    // child thread's memory

    for (i = 0; i < room_count; ++i)
    {
        _Room(room_buffer[i]);
        free(room_buffer[i]);
    }

    // With any luck, it's good
    return game_loop_result;
}
