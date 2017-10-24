#include <stdlib.h>    // malloc, free, rand
#include <assert.h>    // Good ol' assertions
#include <unistd.h>    // getpid
#include <time.h>      // Seed for rand


#include <stdio.h>     // fopen, fclose, printf, getline
#include <string.h>    // memcpy, strcpy, strcat, strerror, sscanf
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
    char* connections[6];
    int connection_count;
    room_type type;
} Room;


// Forward declarations
void _Room(Room* room);

bool get_fresh_dir_path(char* path_buffer);

int parse_room_dir(const char* dir_path, Room** room_buffer, int max_rooms);

Room* parse_file(FILE* file_handle, int room_id, bool* parse_succeeded);

void room_print_debug(Room* room, char* str_buffer);

const char* room_type_to_str(room_type rt);
    

// `free`s memory owned by a `Room`
void _Room(Room* room)
{
    free(room->name);

    int i;
    for (i = 0; i < room->connection_count; ++i)
    {
        free(room->connections[i]);
    }
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

            return -1;
        }
        
        bool parse_succeeded = true;
        Room* parsed_room = parse_file(
            file_handle,
            parsed_room_count,
            &parse_succeeded
        );
        if (!parse_succeeded)
        {
            fprintf(stderr, "Parse failure was in %s\n", rel_path_buffer);

            return -1;
        }
        room_buffer[parsed_room_count] = parsed_room;
        parsed_room_count++;

        fclose(file_handle);

        entity = readdir(dir);
    }

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

                    char* connection_name = malloc(20 * sizeof(char));
                    strcpy(connection_name, property_value);

                    room->connections[room->connection_count] =
                        connection_name;
                    room->connection_count++;
                    break;
                }
            }
        }

        chars_read = getline(&line, &getline_buffer_len, file_handle);
    }

    free(line);
    *parse_succeeded = true;

    return room;
}

// Print a `Room` struct, for debugging purposes only
void room_print_debug(Room* room, char* str_buffer)
{
    strcpy(str_buffer, "{id: ");
    char id_buffer[3];
    sprintf(id_buffer, "%d", room->id);
    strcat(str_buffer, id_buffer);
    strcat(str_buffer, ", name: ");
    strcat(str_buffer, room->name);
    strcat(str_buffer, ", connections: [");
    int i;
    for (i = 0; i < room->connection_count; ++i)
    {
        strcat(str_buffer, room->connections[i]);
        strcat(str_buffer, ", ");
    }
    strcat(str_buffer, "], room_type: ");
    strcat(str_buffer, room_type_to_str(room->type));
    strcat(str_buffer, "}");
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
            return "wat";
    }
}

// (main) Get room array, enter game loop
int main(void)
{
    char path_buffer[256];
    if (!get_fresh_dir_path(path_buffer))
    {
        return 1;
    }

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

        int i;
        for (i = 0; i < parse_result; ++i)
        {
            char room_debug_str_buffer[256];
            room_print_debug(room_buffer[i], room_debug_str_buffer);
            printf("%s\n", room_debug_str_buffer);
        }

        return parse_result == 0 ? 100 : parse_result;
    }

    int i;
    for (i = 0; i < 7; ++i)
    {
        char room_debug_str_buffer[256];
        room_print_debug(room_buffer[i], room_debug_str_buffer);
        printf("%s\n", room_debug_str_buffer);
    }

    return 0;
}
