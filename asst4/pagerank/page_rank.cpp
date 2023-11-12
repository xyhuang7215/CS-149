#include "page_rank.h"

#include <stdlib.h>
#include <cmath>
#include <omp.h>
#include <utility>

#include "../common/CycleTimer.h"
#include "../common/graph.h"


// pageRank --
//
// g:           graph to process (see common/graph.h)
// solution:    array of per-vertex vertex scores (length of array is num_nodes(g))
// damping:     page-rank algorithm's damping parameter
// convergence: page-rank algorithm's convergence threshold
//
void pageRank(Graph g, double* solution, double damping, double convergence)
{
  /*
    CS149 students: Implement the page rank algorithm here.  You
    are expected to parallelize the algorithm using openMP.  Your
    solution may need to allocate (and free) temporary arrays.

    Basic page rank pseudocode is provided below to get you started:

    // initialization: see example code above
    score_old[vi] = 1/numNodes;

    while (!isConverged) {

      // compute new_score[vi] for all nodes vi:
      new_score[vi] = sum over all nodes vj reachable from incoming edges
                        { score_old[vj] / number of edges leaving vj  }
      new_score[vi] = (damping * new_score[vi]) + (1.0-damping) / numNodes;

      new_score[vi] += sum over all nodes v in graph with no outgoing edges
                        { damping * score_old[v] / numNodes }

      // compute how much per-node scores have changed
      // quit once algorithm has isConverged

      global_diff = sum over all nodes vi { abs(new_score[vi] - score_old[vi]) };
      isConverged = (global_diff < convergence)
    }
  */

  // Initialize vertex weights to uniform probability. 
  int numNodes = num_nodes(g);
  double equal_prob = 1.0 / numNodes;
  #pragma omp parallel for
  for (int i = 0; i < numNodes; ++i) {
    solution[i] = equal_prob;
  }

  // Record indexes of nodes without outgoing edges
  int sink_nodes_num = 0;
  Vertex* sink_nodes = new int[numNodes];
  #pragma omp parallel for
  for (Vertex i = 0; i < numNodes; ++i) {
    if (outgoing_size(g, i) == 0) {
        #pragma omp critical
        sink_nodes[sink_nodes_num++] = i;
    }
  }    

  bool isConverged = false;
  double rest = (1.0 - damping) / numNodes;
  double *new_score = new double[numNodes];
  
  while (!isConverged) {
    // Sum over all nodes vj reachable from incoming edges
    #pragma omp parallel for schedule(dynamic, 128)
    for (int i = 0; i < numNodes; i++) {
      double score = 0;
      const Vertex* start = incoming_begin(g, i);
      const Vertex* end = incoming_end(g, i);
      for (const Vertex* vec_ptr = start; vec_ptr != end; ++vec_ptr) {
        int vj = *vec_ptr;
        score += solution[vj] / outgoing_size(g, vj);
      }
      new_score[i] = (damping * score) + rest;
    }

    // For each node, add the sum over all nodes v with no outgoing edges
    double randomJumpPr = 0.0;
    #pragma omp parallel for reduction (+:randomJumpPr) schedule(static, 256)
    for (int i = 0; i < sink_nodes_num; i++) {
      randomJumpPr += solution[sink_nodes[i]];
    }
    randomJumpPr = damping * randomJumpPr / numNodes;
    #pragma omp parallel for schedule(static, 256)
    for (int i = 0; i < numNodes; i++) {
      new_score[i] += randomJumpPr;
    }

    // Check the convergence criterion
    double global_diff = 0;
    #pragma omp parallel for reduction (+:global_diff) schedule(static, 256)
    for (int i = 0; i < numNodes; i++) {
      global_diff += abs(new_score[i] - solution[i]);
      solution[i] = new_score[i];
    }
    isConverged = (global_diff < convergence);
  }

  free(new_score);
}
