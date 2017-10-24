#include <stdio.h>     // fopen, fclose, printf, scanf, sprintf
#include <stdlib.h>    // malloc, free, rand
#include <string.h>    // memcpy, strcpy, strcat
#include <assert.h>    // Good ol' assertions
#include <sys/types.h> // Types for system functions
#include <sys/stat.h>  // stat
#include <unistd.h>    // getpid


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


// Room names
#define ROOM_NAME_COUNT 10
const char* ROOM_NAMES[ROOM_NAME_COUNT] =
{
    "Semigroupoid",
    "Category",
    "Groupoid",
    "Magma",
    "Quasigroup",
    "Loop",
    "Semigroup",
    "Monoid",
    "Group",
    "Abelian"
};


// Forward declarations
void _Room(Room* r);

void initialize_rooms(Room* room_buffer, int room_count);

void get_random_room_names(char* room_name_buffer[]);

void make_connections(Room* rooms, int room_count);

bool is_graph_full(const Room* rooms, int room_count);

void add_random_connection(Room* rooms, int room_count);

bool can_add_connection_from(Room room);

bool connection_already_exists(Room room1, Room room2);

void connect_rooms(Room* room1, Room* room2);

bool is_same_room(Room room1, Room room2);

void get_dir_name(char* buffer);

bool write_room_files(const Room* rooms, int room_count);

const char* room_type_to_str(room_type rt);


// Use this to free `Room`s
void _Room(Room* r)
{
    free(r->connections);
}

// Initialize an array of `Room`s
void initialize_rooms(Room* room_buffer, int room_count)
{
    assert(room_count <= ROOM_NAME_COUNT);
    
    char* shuffled_room_names[ROOM_NAME_COUNT];
    get_random_room_names(shuffled_room_names);

    int i;
    for (i = 0; i < room_count; ++i)
    {
        Room r;
        r.id = i;
        r.name = shuffled_room_names[i];
        r.connections = malloc(6 * sizeof(int));
        r.connection_count = 0;
        if (i == 0)
        {
            r.type = START_ROOM;
        }
        else if (i == 1)
        {
            r.type = END_ROOM;
        }
        else
        {
            r.type = MID_ROOM;
        }

        room_buffer[i] = r;
    }
}

// Gets a shuffled copy of `ROOM_NAMES` and puts it into `room_name_buffer`.
// So `room_name_buffer` should be at least large enough to hold
// `ROOM_NAME_COUNT` `char*`s
void get_random_room_names(char* room_name_buffer[])
{
    memcpy(room_name_buffer, ROOM_NAMES, ROOM_NAME_COUNT * sizeof(char*));

    // Fisher-Yates
    int i;
    for (i = ROOM_NAME_COUNT - 1; i >= 1; --i)
    {
        int j = rand() % (i + 1);
        char* temp = room_name_buffer[j];
        room_name_buffer[j] = room_name_buffer[i];
        room_name_buffer[i] = temp;
    }
}

// Create all connections of the graph
void make_connections(Room* rooms, int room_count)
{
    while (!is_graph_full(rooms, room_count))
    {
        add_random_connection(rooms, room_count);
    }
}

// Checking to see if all rooms have 3 to 6 outbound connections
bool is_graph_full(const Room* rooms, int room_count)
{
    int i;
    for (i = 0; i < room_count; ++i)
    {
        int connection_count = rooms[i].connection_count;
        assert(connection_count <= 6);

        if (connection_count < 3)
        {
            return false;
        }
    }

    return true;
}

// Add a random (valid) connection to the graph
void add_random_connection(Room* rooms, int room_count)
{
    /* I'd prefer to do this in a deterministic linear time,
     * not an symptotically linear but indeterministic (unbounded)
     * time.
     *
     * So, we simply filter the array into a new array and then get
     * from the filtered array using a random index.
     */
    int choices_for_a[room_count];
    int possible_as = 0;

    int i;
    for (i = 0; i < room_count; ++i) 
    {
        if (can_add_connection_from(rooms[i]))
        {
            choices_for_a[possible_as] = i;
            possible_as++;
        }
    }

    assert(possible_as > 0);

    Room* a = &rooms[choices_for_a[rand() % possible_as]];

    int choices_for_b[room_count];
    int possible_bs = 0;

    for (i = 0; i < room_count; ++i)
    {
        if (
            can_add_connection_from(rooms[i]) &&
            !is_same_room(*a, rooms[i])       &&
            !connection_already_exists(*a, rooms[i])
        ) {
            choices_for_b[possible_bs] = i;
            possible_bs++;
        }
    }

    assert(possible_bs > 0);

    Room* b = &rooms[choices_for_b[rand() % possible_bs]];

    connect_rooms(a, b);
}

// Can this room handle another connection?
bool can_add_connection_from(Room room)
{
    return room.connection_count < 6;
}

// Is there already a connection from `room1` to `room2`?
bool connection_already_exists(Room room1, Room room2)
{
    int i;
    for (i = 0; i < room1.connection_count; ++i)
    {
        if (room1.connections[i] == room2.id)
        {
            return true;
        }
    }

    return false;
}

// Connect two rooms together.
void connect_rooms(Room* room1, Room* room2)
{
    room1->connections[room1->connection_count] = room2->id;
    room1->connection_count++;

    room2->connections[room2->connection_count] = room1->id;
    room2->connection_count++;
}

// Are `room1` and `room2` the same room?
bool is_same_room(Room room1, Room room2)
{
    return room1.id == room2.id;
}

// Generate dir name to put files in.
// `buffer` should be large enough to hold 40 `char`s or more.
void get_dir_name(char* buffer)
{
    char pid_str_buffer[24];
    int format_success = sprintf(pid_str_buffer, "%d", getpid());
    assert(format_success > 0);

    strcpy(buffer, "comitoz.rooms.");
    strcat(buffer, pid_str_buffer);
}

// Write the room files, returning `false` on failure
bool write_room_files(const Room* rooms, int room_count)
{
    char* dir_name = malloc(40 * sizeof(char));
    get_dir_name(dir_name);

    mkdir(dir_name, 0777);

    int i;
    for (i = 0; i < room_count; ++i)
    {
        FILE* file_handle = fopen(dir_name, "w");
        assert(file_handle != NULL);

        Room room = rooms[i];
        
        fprintf(file_handle, "ROOM NAME: %s\n", room.name);
        int j;
        for (j = 0; j < room.connection_count; ++j)
        {
            fprintf(
                file_handle,
                "CONNECTION %d: %s\n",
                j + 1,
                rooms[room.connections[j]].name
            );
        }
        fprintf(file_handle, "ROOM_TYPE: %s\n", room_type_to_str(room.type));
        
        fclose(file_handle);
    }    

    free(dir_name);
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
    }

    return "wat";
}


int main(int argc, char* argv[])
{
    printf("=== Entry point ===\n");
    int room_count = 7;
    Room* room_buffer = malloc(room_count * sizeof(Room));

    printf("=== initialize_rooms ===\n");
    initialize_rooms(room_buffer, room_count);
    
    printf("=== make_connections ===\n");
    make_connections(room_buffer, room_count);
    
    printf("=== write_room_files ===\n");
    write_room_files(room_buffer, room_count);

    printf("=== Cleanup ===\n");
    // Cleanup
    int i;
    for (i = 0; i < room_count; ++i)
    {
        _Room(&room_buffer[i]);
    }
    free(room_buffer);

    printf("=== Success? ===\n");
    return 0;
}
