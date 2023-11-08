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


  // initialize vertex weights to uniform probability. Double
  // precision scores are used to avoid underflow for large graphs

  int numNodes = num_nodes(g);
  double equal_prob = 1.0 / numNodes;
  #pragma omp parallel for
  for (int i = 0; i < numNodes; ++i) {
    solution[i] = equal_prob;
  }

  // record nodes without outgoing edges
  int sink_nodes_num = 0;
  Vertex* sink_nodes = new int[numNodes];
  #pragma omp parallel for
  for (Vertex i = 0; i < numNodes; i++) {
    if (outgoing_size(g, i) == 0) {
        #pragma omp critical
        sink_nodes[sink_nodes_num++] = i;
    }
  }
  
  /*
     CS149 students: Implement the page rank algorithm here.  You
     are expected to parallelize the algorithm using openMP.  Your
     solution may need to allocate (and free) temporary arrays.

     Basic page rank pseudocode is provided below to get you started:

     // initialization: see example code above
     score_old[vi] = 1/numNodes;

     while (!isConverged) {

       // compute score_new[vi] for all nodes vi:
       score_new[vi] = sum over all nodes vj reachable from incoming edges
                          { score_old[vj] / number of edges leaving vj  }
       score_new[vi] = (damping * score_new[vi]) + (1.0-damping) / numNodes;

       score_new[vi] += sum over all nodes v in graph with no outgoing edges
                          { damping * score_old[v] / numNodes }

       // compute how much per-node scores have changed
       // quit once algorithm has isConverged

       global_diff = sum over all nodes vi { abs(score_new[vi] - score_old[vi]) };
       isConverged = (global_diff < convergence)
     }
   */

    

    bool isConverged = false;
    double rest = (1.0 - damping) / numNodes;
    double *score_new = new double[numNodes];
    
    // #pragma omp parallel
    while (!isConverged) {
      // sum over all nodes vj reachable from incoming edges
      #pragma omp parallel for schedule(dynamic, 128)
      for (int i = 0; i < numNodes; i++) {
        double score = 0;
        const Vertex* start = incoming_begin(g, i);
        const Vertex* end = incoming_end(g, i);
        for (const Vertex* pv = start; pv != end; pv++) {
          int vj = *pv;
          score += solution[vj] / outgoing_size(g, vj);
        }
        score_new[i] = (damping * score) + rest;
      }

      // sum over all nodes v in graph with no outgoing edges
      double randomJumpPr = 0.0;
      #pragma omp parallel for reduction (+:randomJumpPr) schedule(dynamic, 128)
      for (int i = 0; i < sink_nodes_num; i++) {
        randomJumpPr += solution[sink_nodes[i]];
      }
      randomJumpPr = damping * randomJumpPr / numNodes;

      // update all nodes vj with randomJumpPr
      #pragma omp parallel for schedule(static, 256)
      for (int i = 0; i < numNodes; i++) {
        score_new[i] += randomJumpPr;
      }

      double global_diff = 0;
      #pragma omp parallel for reduction (+:global_diff) schedule(static, 256)
      for (int i = 0; i < numNodes; i++) {
        global_diff += abs(score_new[i] - solution[i]);
        solution[i] = score_new[i];
      }
      isConverged = (global_diff < convergence);
    }
    free(score_new);
}
