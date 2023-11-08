#include "tasksys.h"


IRunnable::~IRunnable() {}

ITaskSystem::ITaskSystem(int num_threads) {}
ITaskSystem::~ITaskSystem() {}

/*
 * ================================================================
 * Serial task system implementation
 * ================================================================
 */

const char* TaskSystemSerial::name() {
    return "Serial";
}

TaskSystemSerial::TaskSystemSerial(int num_threads): ITaskSystem(num_threads) {
}

TaskSystemSerial::~TaskSystemSerial() {}

void TaskSystemSerial::run(IRunnable* runnable, int num_total_tasks) {
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }
}

TaskID TaskSystemSerial::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                          const std::vector<TaskID>& deps) {
    return 0;
}

void TaskSystemSerial::sync() {
    return;
}

/*
 * ================================================================
 * Parallel Task System Implementation
 * ================================================================
 */

const char* TaskSystemParallelSpawn::name() {
    return "Parallel + Always Spawn";
}

TaskSystemParallelSpawn::TaskSystemParallelSpawn(int num_threads): ITaskSystem(num_threads) {
    //
    // TODO: CS149 student implementations may decide to perform setup
    // operations (such as thread pool construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
    // mutex = std::unique_ptr<std::mutex>(new std::mutex());
    threads = std::vector<std::thread>(num_threads);
    mutex = new std::mutex();
    this->num_threads = num_threads;
}

TaskSystemParallelSpawn::~TaskSystemParallelSpawn() {
    delete mutex;
}

void TaskSystemParallelSpawn::run(IRunnable* runnable, int num_total_tasks) {
    //
    // TODO: CS149 students will modify the implementation of this
    // method in Part A.  The implementation provided below runs all
    // tasks sequentially on the calling thread.
    //

    int next_work = 0;

    auto runTask = [&] () {
        while (true) {
            mutex->lock();
            int cur_work = next_work++;
            mutex->unlock();

            if (cur_work >= num_total_tasks) {
                return;
            }
            
            runnable->runTask(cur_work, num_total_tasks);
        }
    };

    for (int i = 0; i < num_threads; i++) {
        threads[i] = std::thread(runTask);
    }

    for (int i = 0; i < num_threads; i++) {
        threads[i].join();
    }
}


TaskID TaskSystemParallelSpawn::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                 const std::vector<TaskID>& deps) {
    return 0;
}

void TaskSystemParallelSpawn::sync() {
    return;
}

/*
 * ================================================================
 * Parallel Thread Pool Spinning Task System Implementation
 * ================================================================
 */

const char* TaskSystemParallelThreadPoolSpinning::name() {
    return "Parallel + Thread Pool + Spin";
}

TaskSystemParallelThreadPoolSpinning::TaskSystemParallelThreadPoolSpinning(int num_threads): ITaskSystem(num_threads) {
    //
    // TODO: CS149 student implementations may decide to perform setup
    // operations (such as thread pool construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //

    auto func = [&] () {
        std::function<void()> job;
        while (true) {
            std::unique_lock<std::mutex> lock(status_mutex);
            mutex_condition.wait(lock, [&] {
                return next_task_id < cur_num_total_tasks || 
                       should_terminate;
            });

            if (should_terminate) return;

            int id = next_task_id++;
            lock.unlock();

            // do job and update # of finished tasks
            if (id < cur_num_total_tasks) {
                cur_runnable->runTask(id, cur_num_total_tasks);
                std::unique_lock<std::mutex> fin_lock(finish_mutex);
                finish_task_num++;
            }
        }
    };
    
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back(std::thread(func));
    }
}

TaskSystemParallelThreadPoolSpinning::~TaskSystemParallelThreadPoolSpinning() {
    std::unique_lock<std::mutex> lock(status_mutex);
    should_terminate = true;
    lock.unlock();

    mutex_condition.notify_all();
    for (std::thread& active_thread : threads) {
        active_thread.join();
    }
    threads.clear();
}

void TaskSystemParallelThreadPoolSpinning::run(IRunnable* runnable, int num_total_tasks) {


    //
    // TODO: CS149 students will modify the implementation of this
    // method in Part A.  The implementation provided below runs all
    // tasks sequentially on the calling thread.
    //

    std::unique_lock<std::mutex> status_lock(status_mutex);
    cur_runnable = runnable;
    cur_num_total_tasks = num_total_tasks;
    finish_task_num = 0;
    next_task_id = 0;
    status_lock.unlock();

    mutex_condition.notify_all();

    while (true) {
        std::unique_lock<std::mutex> lock(finish_mutex);
        if (finish_task_num == num_total_tasks) {
            break;
        }
    }    
}

TaskID TaskSystemParallelThreadPoolSpinning::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                              const std::vector<TaskID>& deps) {
    return 0;
}

void TaskSystemParallelThreadPoolSpinning::sync() {
    return;
}


/*
 * ================================================================
 * Parallel Thread Pool Sleeping Task System Implementation
 * ================================================================
 */

const char* TaskSystemParallelThreadPoolSleeping::name() {
    return "Parallel + Thread Pool + Sleep";
}

TaskSystemParallelThreadPoolSleeping::TaskSystemParallelThreadPoolSleeping(int num_threads): ITaskSystem(num_threads) {
    //
    // TODO: CS149 student implementations may decide to perform setup
    // operations (such as thread pool construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
    auto func = [&] () {
        std::function<void()> job;
        while (true) {
            std::unique_lock<std::mutex> lock(status_mutex);
            mutex_condition.wait(lock, [&] {
                return next_task_id < cur_num_total_tasks || 
                       should_terminate;
            });

            if (should_terminate) return;

            int id = next_task_id++;
            lock.unlock();

            // do job and update # of finished tasks
            if (id < cur_num_total_tasks) {
                cur_runnable->runTask(id, cur_num_total_tasks);
                std::unique_lock<std::mutex> fin_lock(finish_mutex);
                int fin_num = ++finish_task_num;
                fin_lock.unlock();

                if (fin_num >= cur_num_total_tasks) {
                    isFinish = true;
                    finish_condition.notify_one();
                }
            }
        }
    };
    
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back(std::thread(func));
    }
}

TaskSystemParallelThreadPoolSleeping::~TaskSystemParallelThreadPoolSleeping() {
    //
    // TODO: CS149 student implementations may decide to perform cleanup
    // operations (such as thread pool shutdown construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
    std::unique_lock<std::mutex> lock(status_mutex);
    should_terminate = true;
    lock.unlock();

    mutex_condition.notify_all();
    for (std::thread& active_thread : threads) {
        active_thread.join();
    }
    threads.clear();
}


void TaskSystemParallelThreadPoolSleeping::run(IRunnable* runnable, int num_total_tasks) {


    //
    // TODO: CS149 students will modify the implementation of this
    // method in Parts A and B.  The implementation provided below runs all
    // tasks sequentially on the calling thread.
    //
    std::unique_lock<std::mutex> status_lock(status_mutex);
    cur_runnable = runnable;
    cur_num_total_tasks = num_total_tasks;
    finish_task_num = 0;
    next_task_id = 0;
    isFinish = false;
    status_lock.unlock();

    mutex_condition.notify_all();

    std::unique_lock<std::mutex> fin_lock(finish_mutex);
    finish_condition.wait(fin_lock, [&] {return isFinish;});
}

TaskID TaskSystemParallelThreadPoolSleeping::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                    const std::vector<TaskID>& deps) {


    //
    // TODO: CS149 students will implement this method in Part B.
    //

    return 0;
}

void TaskSystemParallelThreadPoolSleeping::sync() {

    //
    // TODO: CS149 students will modify the implementation of this method in Part B.
    //

    return;
}


/*------------------------------------------------------------------------------------------------------------------------------*/
/*------------------------------------------------------------------------------------------------------------------------------*/


// /*
//  * ================================================================
//  * Parallel Thread Pool Spinning Task System Implementation
//  * ================================================================
//  */

// const char* TaskSystemParallelThreadPoolSpinning::name() {
//     return "Parallel + Thread Pool + Spin";
// }

// TaskSystemParallelThreadPoolSpinning::TaskSystemParallelThreadPoolSpinning(int num_threads): ITaskSystem(num_threads) {
//     //
//     // TODO: CS149 student implementations may decide to perform setup
//     // operations (such as thread pool construction) here.
//     // Implementations are free to add new class member variables
//     // (requiring changes to tasksys.h).
//     //

//     auto func = [&] () {
//         std::function<void()> job;
//         while (true) {
//             std::unique_lock<std::mutex> lock(queue_mutex);
//             mutex_condition.wait(lock, [&] {
//                 return !jobs.empty() || should_terminate;
//             });

//             if (should_terminate) return;

//             job = jobs.front(); jobs.pop();
//             lock.unlock();
//             job(); // do job

//             finish_mutex.lock();
//             finish_task_num++;
//             finish_mutex.unlock();
//         }
//     };
    
//     for (int i = 0; i < num_threads; i++) {
//         threads.emplace_back(std::thread(func));
//     }
// }

// TaskSystemParallelThreadPoolSpinning::~TaskSystemParallelThreadPoolSpinning() {
//     std::unique_lock<std::mutex> lock(queue_mutex);
//     should_terminate = true;
//     lock.unlock();

//     mutex_condition.notify_all();
//     for (std::thread& active_thread : threads) {
//         active_thread.join();
//     }
//     threads.clear();
// }

// void TaskSystemParallelThreadPoolSpinning::run(IRunnable* runnable, int num_total_tasks) {


//     //
//     // TODO: CS149 students will modify the implementation of this
//     // method in Part A.  The implementation provided below runs all
//     // tasks sequentially on the calling thread.
//     //

//     auto runTask = [&] (int i) {
//         runnable->runTask(i, num_total_tasks);
//     };

//     for (int i = 0; i < num_total_tasks; i++) {
//         std::unique_lock<std::mutex> lock(queue_mutex);
//         jobs.push(std::bind(runTask, i));
//         lock.unlock();
//         mutex_condition.notify_one();
//     }

//     while (true) {
//         std::unique_lock<std::mutex> lock(finish_mutex);
//         if (finish_task_num == num_total_tasks) {
//             finish_task_num = 0;
//             break;
//         }
//     }    
// }

// TaskID TaskSystemParallelThreadPoolSpinning::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
//                                                               const std::vector<TaskID>& deps) {
//     return 0;
// }

// void TaskSystemParallelThreadPoolSpinning::sync() {
//     return;
// }

// /*
//  * ================================================================
//  * Parallel Thread Pool Sleeping Task System Implementation
//  * ================================================================
//  */

// const char* TaskSystemParallelThreadPoolSleeping::name() {
//     return "Parallel + Thread Pool + Sleep";
// }

// TaskSystemParallelThreadPoolSleeping::TaskSystemParallelThreadPoolSleeping(int num_threads): ITaskSystem(num_threads) {
//     //
//     // TODO: CS149 student implementations may decide to perform setup
//     // operations (such as thread pool construction) here.
//     // Implementations are free to add new class member variables
//     // (requiring changes to tasksys.h).
//     //
//     auto func = [&] () {
//         std::function<void()> job;
//         while (true) {
//             std::unique_lock<std::mutex> lock(queue_mutex);
//             mutex_condition.wait(lock, [&] {
//                 // if (isFinish) finish_condition.notify_one();
//                 return !jobs.empty() || should_terminate;
//             });

//             if (should_terminate) return;

//             job = jobs.front(); jobs.pop();
//             lock.unlock();
//             job(); // do job

//             finish_mutex.lock();
//             finish_task_num++;
//             if (finish_task_num == cur_num_total_tasks) {
//                 isFinish = true;
//                 finish_condition.notify_one();
//             }
//             finish_mutex.unlock();
//         }
//     };
    
//     for (int i = 0; i < num_threads; i++) {
//         threads.emplace_back(std::thread(func));
//     }
// }

// TaskSystemParallelThreadPoolSleeping::~TaskSystemParallelThreadPoolSleeping() {
//     //
//     // TODO: CS149 student implementations may decide to perform cleanup
//     // operations (such as thread pool shutdown construction) here.
//     // Implementations are free to add new class member variables
//     // (requiring changes to tasksys.h).
//     //
//     std::unique_lock<std::mutex> lock(queue_mutex);
//     should_terminate = true;
//     lock.unlock();

//     mutex_condition.notify_all();
//     for (std::thread& active_thread : threads) {
//         active_thread.join();
//     }
//     threads.clear();
// }


// void TaskSystemParallelThreadPoolSleeping::run(IRunnable* runnable, int num_total_tasks) {


//     //
//     // TODO: CS149 students will modify the implementation of this
//     // method in Parts A and B.  The implementation provided below runs all
//     // tasks sequentially on the calling thread.
//     //
//     auto runTask = [&] (int i) {
//         runnable->runTask(i, num_total_tasks);
//     };

//     finish_task_num = 0;
//     cur_num_total_tasks = num_total_tasks;
//     isFinish = false;

//     for (int i = 0; i < num_total_tasks; i++) {
//         std::unique_lock<std::mutex> lock(queue_mutex);
//         jobs.push(std::bind(runTask, i));
//         lock.unlock();
//         mutex_condition.notify_one();
//     }

//     std::unique_lock<std::mutex> fin_lock(finish_mutex);
//     finish_condition.wait(fin_lock, [&]{return isFinish;});
//     isFinish = false;
// }

// TaskID TaskSystemParallelThreadPoolSleeping::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
//                                                     const std::vector<TaskID>& deps) {


//     //
//     // TODO: CS149 students will implement this method in Part B.
//     //

//     return 0;
// }

// void TaskSystemParallelThreadPoolSleeping::sync() {

//     //
//     // TODO: CS149 students will modify the implementation of this method in Part B.
//     //

//     return;
// }
