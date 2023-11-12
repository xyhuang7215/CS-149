#include "bfs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cstddef>
#include <omp.h>

#include "../common/CycleTimer.h"
#include "../common/graph.h"

#define ROOT_NODE_ID 0
#define NOT_VISITED_MARKER -1
// #define VERBOSE

void vertex_set_clear(vertex_set* list) {
    list->count = 0;
}

void vertex_set_init(vertex_set* list, int count) {
    list->max_vertices = count;
    list->vertices = (int*)malloc(sizeof(int) * list->max_vertices);
    vertex_set_clear(list);
}

void vertex_set_free(vertex_set* list) {
    free(list->vertices);
}


void update_visited(bool* visited, vertex_set* list) {
    for (int i = 0; i < list->count; i++) {
        visited[list->vertices[i]] = true;
    }
}

// Take one step of "top-down" BFS.  For each vertex on the frontier,
// follow all outgoing edges, and add all neighboring vertices to the
// new_frontier.
void top_down_step_parallel(
    Graph g,
    vertex_set* frontier,
    vertex_set* new_frontier,
    int* distances)
{
    int prev_dis = distances[frontier->vertices[0]];
    int curr_dis = prev_dis + 1;

    #pragma omp parallel for schedule(dynamic, 64)
    for (int i=0; i<frontier->count; i++) {
        int node = frontier->vertices[i];

        int start_edge = g->outgoing_starts[node];
        int end_edge = (node == g->num_nodes - 1) ?
                            g->num_edges :
                            g->outgoing_starts[node + 1];

        int num_edge = end_edge - start_edge;
        int local_frontier_vertices[num_edge];
        int local_count = 0;

        // attempt to add all neighbors to the new frontier
        for (int neighbor = start_edge; neighbor < end_edge; neighbor++) {
            int outgoing = g->outgoing_edges[neighbor];
            if (distances[outgoing] != NOT_VISITED_MARKER) continue;

            /* Un-visited node -> add to frontier */
            if (__sync_bool_compare_and_swap(&distances[outgoing], NOT_VISITED_MARKER, curr_dis)) {
                local_frontier_vertices[local_count++] = outgoing;
            }
        }

        if (local_count == 0) continue;

        // Reserve space so that we will not be worry about cirtical section.
        int start_index = __sync_fetch_and_add(&new_frontier->count, local_count);
        for (int n = 0; n < local_count; n++) {
            new_frontier->vertices[start_index + n] = local_frontier_vertices[n];
        }
    }
}

void top_down_step_serial(
    Graph g,
    vertex_set* frontier,
    vertex_set* new_frontier,
    int* distances)
{
    int prev_dis = distances[frontier->vertices[0]];
    int curr_dis = prev_dis + 1;

    for (int i=0; i<frontier->count; i++) {
        int node = frontier->vertices[i];

        int start_edge = g->outgoing_starts[node];
        int end_edge = (node == g->num_nodes - 1) ?
                            g->num_edges :
                            g->outgoing_starts[node + 1];

        // Add all un-visited neighbors to the new frontier
        for (int neighbor=start_edge; neighbor<end_edge; neighbor++) {
            int outgoing = g->outgoing_edges[neighbor];

            if (distances[outgoing] == NOT_VISITED_MARKER) {
                distances[outgoing] = curr_dis;
                new_frontier->vertices[new_frontier->count++] = outgoing;
            }
        }
    }
}

// Implements top-down BFS.
//
// Result of execution is that, for each node in the graph, the
// distance to the root is stored in sol.distances.
void bfs_top_down(Graph graph, solution* sol) {
    vertex_set list1;
    vertex_set list2;
    vertex_set_init(&list1, graph->num_nodes);
    vertex_set_init(&list2, graph->num_nodes);

    vertex_set* frontier = &list1;
    vertex_set* new_frontier = &list2;

    // Initialize all nodes to NOT_VISITED
    for (int i=0; i<graph->num_nodes; i++)
        sol->distances[i] = NOT_VISITED_MARKER;

    // Setup frontier with the root node
    frontier->vertices[frontier->count++] = ROOT_NODE_ID;
    sol->distances[ROOT_NODE_ID] = 0;

    while (frontier->count != 0) {

#ifdef VERBOSE
        double start_time = CycleTimer::currentSeconds();
#endif

        vertex_set_clear(new_frontier);

        if (frontier->count <= 1024)
            top_down_step_serial(graph, frontier, new_frontier, sol->distances);
        else
            top_down_step_parallel(graph, frontier, new_frontier, sol->distances);


#ifdef VERBOSE
    double end_time = CycleTimer::currentSeconds();
    printf("frontier=%-10d %.4f sec\n", frontier->count, end_time - start_time);
#endif

        // swap pointers
        vertex_set* tmp = frontier;
        frontier = new_frontier;
        new_frontier = tmp;
    }
}

/* 
    Since we will traverse every node at each iteration, serial version of bottom_up is slow 
    than parallel version even when frontier_size is small.
*/

// void bottom_up_step_serial(
//     Graph g,
//     vertex_set* frontier,
//     vertex_set* new_frontier,
//     int* distances)
// {
//     int prev_dis = distances[frontier->vertices[0]];
//     int curr_dis = prev_dis + 1;

//     for (int node = 0; node < g->num_nodes; node++) {
//         if (distances[node] != NOT_VISITED_MARKER) continue;

//         int start_edge = g->incoming_starts[node];
//         int end_edge = (node == g->num_nodes - 1)
//                            ? g->num_edges
//                            : g->incoming_starts[node + 1];

//         for (int neighbor = start_edge; neighbor < end_edge; neighbor++) {
//             int incoming = g->incoming_edges[neighbor];
//             if (distances[incoming] == prev_dis) {
//                 distances[node] = curr_dis;
//                 new_frontier->vertices[new_frontier->count++] = node;
//                 break;
//             }
//         }
//     }
// }

void bottom_up_step_parallel(
    Graph g,
    vertex_set* frontier,
    vertex_set* new_frontier,
    int* distances,
    bool* visited)
{
    int prev_dis = distances[frontier->vertices[0]];
    int curr_dis = prev_dis + 1;

    //  Spilt all nodes into groups in advanced, so that we can update local frontiers
    int nodes_per_thread = 256;
    
    #pragma omp parallel for schedule(dynamic, 32)
    for (int start_idx = 0; start_idx < g->num_nodes; start_idx += nodes_per_thread) {

        int local_frontier_vertices[nodes_per_thread];
        int local_count = 0;
        int end_idx = (start_idx + nodes_per_thread > g->num_nodes) ?
                        g->num_nodes :
                        start_idx + nodes_per_thread;

        for (int node = start_idx; node < end_idx; node++) {
            if (distances[node] != NOT_VISITED_MARKER) continue;

            int start_edge = g->incoming_starts[node];
            int end_edge = (node == g->num_nodes - 1) ?
                            g->num_edges :
                            g->incoming_starts[node + 1];

            for (int neighbor = start_edge; neighbor < end_edge; neighbor++) {
                int incoming = g->incoming_edges[neighbor];
                
                // if (distances[incoming] != prev_dis) continue;
                if (!visited[incoming]) continue;
                distances[node] = curr_dis;
                local_frontier_vertices[local_count++] = node;
                break;
            }
        }

        if (local_count == 0) continue;
        int start_index = __sync_fetch_and_add(&new_frontier->count, local_count);
        for (int n = 0; n < local_count; n++) {
            new_frontier->vertices[start_index + n] = local_frontier_vertices[n];
        }        
    }
}


void bfs_bottom_up(Graph graph, solution* sol)
{
    // CS149 students:
    //
    // You will need to implement the "bottom up" BFS here as
    // described in the handout.
    //
    // As a result of your code's execution, sol.distances should be
    // correctly populated for all nodes in the graph.
    //
    // As was done in the top-down case, you may wish to organize your
    // code by creating subroutine bottom_up_step() that is called in
    // each step of the BFS process.

    vertex_set list1;
    vertex_set list2;
    vertex_set_init(&list1, graph->num_nodes);
    vertex_set_init(&list2, graph->num_nodes);

    vertex_set* frontier = &list1;
    vertex_set* new_frontier = &list2;
    bool* visited = (bool*) malloc(graph->num_nodes * sizeof(bool));

    // Initialize all nodes to NOT_VISITED
    for (int i=0; i<graph->num_nodes; i++) {
        sol->distances[i] = NOT_VISITED_MARKER;
        visited[i] = false;
    }

    // Setup frontier with the root node
    frontier->vertices[frontier->count++] = ROOT_NODE_ID;
    sol->distances[ROOT_NODE_ID] = 0;
    visited[ROOT_NODE_ID] = true;


    while (frontier->count != 0) {

#ifdef VERBOSE
        double start_time = CycleTimer::currentSeconds();
#endif

        vertex_set_clear(new_frontier);

        bottom_up_step_parallel(graph, frontier, new_frontier, sol->distances, visited);

        update_visited(visited, new_frontier);

#ifdef VERBOSE
    double end_time = CycleTimer::currentSeconds();
    printf("frontier=%-10d %.4f sec\n", frontier->count, end_time - start_time);
#endif

        // Swap pointers
        vertex_set* tmp = frontier;
        frontier = new_frontier;
        new_frontier = tmp;
    }
}

void bfs_hybrid(Graph graph, solution* sol)
{
    // CS149 students:
    //
    // You will need to implement the "hybrid" BFS here as
    // described in the handout.
    vertex_set list1;
    vertex_set list2;
    vertex_set_init(&list1, graph->num_nodes);
    vertex_set_init(&list2, graph->num_nodes);

    vertex_set* frontier = &list1;
    vertex_set* new_frontier = &list2;
    bool* visited = (bool*) malloc(graph->num_nodes * sizeof(bool));

    // Initialize all nodes to NOT_VISITED
    for (int i=0; i<graph->num_nodes; i++) {
        sol->distances[i] = NOT_VISITED_MARKER;
        visited[i] = false;
    }

    // Setup frontier with the root node
    frontier->vertices[frontier->count++] = ROOT_NODE_ID;
    sol->distances[ROOT_NODE_ID] = 0;
    visited[ROOT_NODE_ID] = true;

    // Hardcoded threshold for deciding to use bottom_up or top_down
    const int THRESHOLD = 2000000;

    while (frontier->count != 0) {

#ifdef VERBOSE
        double start_time = CycleTimer::currentSeconds();
#endif

        vertex_set_clear(new_frontier);

        if (frontier->count >= THRESHOLD) {
            bottom_up_step_parallel(graph, frontier, new_frontier, sol->distances, visited);
        } else {
            top_down_step_parallel(graph, frontier, new_frontier, sol->distances);
        }

        update_visited(visited, new_frontier);


#ifdef VERBOSE
    double end_time = CycleTimer::currentSeconds();
    printf("frontier=%-10d %.4f sec\n", frontier->count, end_time - start_time);
#endif

        // Swap pointers
        vertex_set* tmp = frontier;
        frontier = new_frontier;
        new_frontier = tmp;
    }
}
