#include <stdio.h>     // fopen, fclose, printf, scanf, sprintf
#include <stdlib.h>    // malloc, free, rand, bsearch
#include <string.h>    // strcpy, strcat
#include <assert.h>    // Good ol' assertions
#include <sys/types.h> // Types for system functions
#include <sys/stat.h>  // stat
#include <unistd.h>    // getpid


// `typedef`s
typedef enum {false, true} bool;

// Define `struct` for a "room"
struct Room
{
    int id;
    char* name;
    int* connections;
    int connection_count;
};


// Room names
const char* ROOM_NAMES[] =
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
void make_connections(Room* rooms, int room_count);

bool is_graph_full(Room* rooms, int room_count);

void add_random_connection(Room* rooms, int room_count);

bool can_add_connection_from(Room room);

bool connection_already_exists(Room room1, Room room2);

void connect_rooms(Room* room1, Room* room2);

bool is_same_room(Room room1, Room room2);

char* get_dir_name();


// `malloc` an array of `Room`s


// Create all connections of the graph
void make_connections(Room* rooms, int room_count)
{
    while (!is_graph_full(rooms, room_count))
    {
        add_connection(rooms, room_count);
    }
}

// Checking to see if all rooms have 3 to 6 outbound connections
bool is_graph_full(Room* rooms, int room_count)
{
    for (int i = 0; i < room_count; ++i)
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

    for (int i = 0; i < room_count; ++i) 
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

    for (int i = 0; i < room_count; ++i)
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
    for (int i = 0; i < room1.connection_count; ++i)
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

// Generate dir name to put files in
char* get_dir_name()
{
    char pid_str_buffer[24];
    int format_success = sprintf(pid_str_buffer, "%d", getpid());
    assert(format_success > 0);

    char* full_buffer = malloc(40 * sizeof(char));
    strcpy(full_buffer, "comitoz.rooms.");
    strcat(full_buffer, pid_str_buffer);

    return full_buffer;
}

// Write the room files


// (main) 
