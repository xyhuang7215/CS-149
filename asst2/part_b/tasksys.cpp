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
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }

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
    // NOTE: CS149 students are not expected to implement TaskSystemParallelSpawn in Part B.
}

TaskSystemParallelSpawn::~TaskSystemParallelSpawn() {}

void TaskSystemParallelSpawn::run(IRunnable* runnable, int num_total_tasks) {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelSpawn in Part B.
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }
}

TaskID TaskSystemParallelSpawn::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                 const std::vector<TaskID>& deps) {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelSpawn in Part B.
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }

    return 0;
}

void TaskSystemParallelSpawn::sync() {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelSpawn in Part B.
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
    // NOTE: CS149 students are not expected to implement TaskSystemParallelSpawn in Part B.
}

TaskSystemParallelThreadPoolSpinning::~TaskSystemParallelThreadPoolSpinning() {}

void TaskSystemParallelThreadPoolSpinning::run(IRunnable* runnable, int num_total_tasks) {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelSpawn in Part B.
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }
}

TaskID TaskSystemParallelThreadPoolSpinning::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                              const std::vector<TaskID>& deps) {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelSpawn in Part B.
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }

    return 0;
}

void TaskSystemParallelThreadPoolSpinning::sync() {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelSpawn in Part B.
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
    auto worker = [&] () {
        while (true) {
            std::unique_lock<std::mutex> lock(meta_mutex);
            ready_condition.wait(lock, [&] {
                return !ready_queue.empty() || should_terminate;
            });

            if (should_terminate) return;

            // 1. Get dispatched job
            Launch &cur_launch = ready_launchs[ready_queue.front()];
            int id = cur_launch.next_task_id++;
            if (cur_launch.next_task_id == cur_launch.total_task_num) {
                ready_queue.pop();
            }
            lock.unlock();


            // 2. Run the job
            cur_launch.runnable->runTask(id, cur_launch.total_task_num);


            // 3. Update metadata
            lock.lock();
            cur_launch.finish_task_num++;

            // If current task is finished, check dependencies of launchs in waiting line
            if (cur_launch.finish_task_num == cur_launch.total_task_num) {
                LaunchID cur_launch_id = cur_launch.launch_id;
                finish_launchs.insert(cur_launch_id);
                ready_launchs.erase(cur_launch_id);

                std::vector<LaunchID> new_ready_launchs;

                for (auto &launch_pair : wait_launchs) {
                    auto &wait_launch = launch_pair.second;

                    if (wait_launch.deps.find(cur_launch_id) != wait_launch.deps.end()) {
                        if (--wait_launch.wait_dep_num == 0) {
                            new_ready_launchs.push_back(launch_pair.first);
                        }
                    }
                }

                for (TaskID new_task_id : new_ready_launchs) {
                    ready_launchs[new_task_id] = std::move(wait_launchs[new_task_id]);
                    ready_queue.push(new_task_id);
                    wait_launchs.erase(new_task_id);
                    ready_condition.notify_all();
                }

                if (ready_launchs.empty() && wait_launchs.empty()) {
                    finish_condition.notify_one();
                }
            }
        }
    };
    
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back(worker);
    }
}

TaskSystemParallelThreadPoolSleeping::~TaskSystemParallelThreadPoolSleeping() {
    //
    // TODO: CS149 student implementations may decide to perform cleanup
    // operations (such as thread pool shutdown construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
    std::unique_lock<std::mutex> lock(meta_mutex);
    should_terminate = true;
    lock.unlock();
    ready_condition.notify_all();

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

    // Sync to make sure works are done
    runAsyncWithDeps(runnable, num_total_tasks, std::vector<TaskID>{});
    sync();
}

LaunchID TaskSystemParallelThreadPoolSleeping::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                    const std::vector<LaunchID>& deps) {
    //
    // TODO: CS149 students will implement this method in Part B.
    //
    std::unique_lock<std::mutex> meta_lock(meta_mutex);
    LaunchID retID = next_launch_id++;

    // Check if the deps are finished, and transform remaining deps into unordered_map
    std::unordered_set<LaunchID> new_deps;
    for (LaunchID dep_id : deps) {
        if (!finish_launchs.count(dep_id))
            new_deps.insert(dep_id);
    }

    if (new_deps.empty()) {
        ready_launchs[retID] = Launch(retID, num_total_tasks, new_deps, runnable);
        ready_queue.push(retID);
        ready_condition.notify_all();
    }
    else {
        wait_launchs[retID] = Launch(retID, num_total_tasks, new_deps, runnable);
    }

    meta_lock.unlock();
    return retID;
}

void TaskSystemParallelThreadPoolSleeping::sync() {
    //
    // TODO: CS149 students will modify the implementation of this method in Part B.
    //
    std::unique_lock<std::mutex> lock(meta_mutex);
    finish_condition.wait(lock, [&] {
        return ready_launchs.empty() && wait_launchs.empty();
    });
}
