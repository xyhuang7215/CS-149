#ifndef _TASKSYS_H
#define _TASKSYS_H

#include "itasksys.h"
#include <queue>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <memory>
#include <atomic>
#include <iostream>
#include <map>
#include <unordered_set>

/*
 * TaskSystemSerial: This class is the student's implementation of a
 * serial task execution engine.  See definition of ITaskSystem in
 * itasksys.h for documentation of the ITaskSystem interface.
 */
class TaskSystemSerial: public ITaskSystem {
    public:
        TaskSystemSerial(int num_threads);
        ~TaskSystemSerial();
        const char* name();
        void run(IRunnable* runnable, int num_total_tasks);
        TaskID runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                const std::vector<TaskID>& deps);
        void sync();
};

/*
 * TaskSystemParallelSpawn: This class is the student's implementation of a
 * parallel task execution engine that spawns threads in every run()
 * call.  See definition of ITaskSystem in itasksys.h for documentation
 * of the ITaskSystem interface.
 */
class TaskSystemParallelSpawn: public ITaskSystem {
    public:
        TaskSystemParallelSpawn(int num_threads);
        ~TaskSystemParallelSpawn();
        const char* name();
        void run(IRunnable* runnable, int num_total_tasks);
        TaskID runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                const std::vector<TaskID>& deps);
        void sync();
};

/*
 * TaskSystemParallelThreadPoolSpinning: This class is the student's
 * implementation of a parallel task execution engine that uses a
 * thread pool. See definition of ITaskSystem in itasksys.h for
 * documentation of the ITaskSystem interface.
 */
class TaskSystemParallelThreadPoolSpinning: public ITaskSystem {
    public:
        TaskSystemParallelThreadPoolSpinning(int num_threads);
        ~TaskSystemParallelThreadPoolSpinning();
        const char* name();
        void run(IRunnable* runnable, int num_total_tasks);
        TaskID runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                const std::vector<TaskID>& deps);
        void sync();
};


/*
 * Helper struct for manage Launch list
 */

struct TASK {
    TASK() = default;
    
    TASK (int LaunchID_, int total_task_num_, std::unordered_set<TaskID> deps_, IRunnable* runnable_) 
    : LaunchID(LaunchID_), total_task_num(total_task_num_), deps(deps_), runnable(runnable_) {
        next_task_id = 0;
        finish_task_num = 0;
        wait_dep_num = deps.size();
    }

    ~TASK () = default;

    int LaunchID;
    int total_task_num;
    std::unordered_set<TaskID> deps;
    IRunnable* runnable;
    int next_task_id;
    int finish_task_num;
    int wait_dep_num;
};


/*
 * TaskSystemParallelThreadPoolSleeping: This class is the student's
 * optimized implementation of a parallel task execution engine that uses
 * a thread pool. See definition of ITaskSystem in
 * itasksys.h for documentation of the ITaskSystem interface.
 */
class TaskSystemParallelThreadPoolSleeping: public ITaskSystem {
    public:
        TaskSystemParallelThreadPoolSleeping(int num_threads);
        ~TaskSystemParallelThreadPoolSleeping();
        const char* name();
        void run(IRunnable* runnable, int num_total_tasks);
        TaskID runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                const std::vector<TaskID>& deps);
        void sync();
    
    private:
        std::vector<std::thread> threads;
        
        std::map<TaskID, TASK> wait_launchs;
        std::queue<TASK> ready_launchs;
        std::unordered_set<TaskID> fin_taskIDs;
        int nextTaskID = 0;

        std::mutex meta_mutex;
        std::condition_variable ready_condition; // Allows threads to wait on new jobs or termination
        std::condition_variable finish_condition;
        bool should_terminate = false;
};


#endif
