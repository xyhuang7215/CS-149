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
// #include <unistd.h>

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
    private:
        std::vector<std::thread> threads;
        std::mutex* mutex;
        int num_threads;
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

    private:
        std::vector<std::thread> threads;
        bool should_terminate = false;           // Tells threads to stop looking for jobs
        std::mutex status_mutex;                  // Prevents data races to the job queue
        std::condition_variable mutex_condition; // Allows threads to wait on new jobs or termination 
        std::mutex finish_mutex;

        // task status -> need init for each run
        int finish_task_num = 0;
        int next_task_id = 0;
        int cur_num_total_tasks = 0;
        IRunnable* cur_runnable;
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
        bool should_terminate = false;           // Tells threads to stop looking for jobs
        std::mutex status_mutex;                  // Prevents data races to the job queue
        std::condition_variable mutex_condition; // Allows threads to wait on new jobs or termination 
        std::mutex finish_mutex;
        std::condition_variable finish_condition;

        // task status -> need init for each run
        int finish_task_num = 0;
        int next_task_id = 0;
        int cur_num_total_tasks = 0;
        IRunnable* cur_runnable;
        bool isFinish = false;
};



/*------------------------------------------------------------------------------------------------------------------------------*/
/*------------------------------------------------------------------------------------------------------------------------------*/


// /*
//  * TaskSystemParallelThreadPoolSpinning: This class is the student's
//  * implementation of a parallel task execution engine that uses a
//  * thread pool. See definition of ITaskSystem in itasksys.h for
//  * documentation of the ITaskSystem interface.
//  */
// class TaskSystemParallelThreadPoolSpinning: public ITaskSystem {
//     public:
//         TaskSystemParallelThreadPoolSpinning(int num_threads);
//         ~TaskSystemParallelThreadPoolSpinning();
//         const char* name();
//         void run(IRunnable* runnable, int num_total_tasks);
//         TaskID runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
//                                 const std::vector<TaskID>& deps);
//         void sync();

//     private:
//         std::vector<std::thread> threads;
//         std::queue<std::function<void()>> jobs;
//         bool should_terminate = false;           // Tells threads to stop looking for jobs
//         std::mutex queue_mutex;                  // Prevents data races to the job queue
//         std::condition_variable mutex_condition; // Allows threads to wait on new jobs or termination 
//         std::mutex finish_mutex;
//         int finish_task_num = 0;
// };


// /*
//  * TaskSystemParallelThreadPoolSleeping: This class is the student's
//  * optimized implementation of a parallel task execution engine that uses
//  * a thread pool. See definition of ITaskSystem in
//  * itasksys.h for documentation of the ITaskSystem interface.
//  */
// class TaskSystemParallelThreadPoolSleeping: public ITaskSystem {
//     public:
//         TaskSystemParallelThreadPoolSleeping(int num_threads);
//         ~TaskSystemParallelThreadPoolSleeping();
//         const char* name();
//         void run(IRunnable* runnable, int num_total_tasks);
//         TaskID runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
//                                 const std::vector<TaskID>& deps);
//         void sync();
//     private:
//         std::vector<std::thread> threads;
//         std::queue<std::function<void()>> jobs;
//         bool should_terminate = false;           // Tells threads to stop looking for jobs
//         std::mutex queue_mutex;                  // Prevents data races to the job queue
//         std::condition_variable mutex_condition; // Allows threads to wait on new jobs or termination 
//         std::mutex finish_mutex;
//         std::condition_variable finish_condition;
//         int finish_task_num = 0;
//         int cur_num_total_tasks = 0;
//         bool isFinish = false;                   // additional variable to handle the case that notify before wait
// };

#endif
