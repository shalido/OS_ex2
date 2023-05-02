/*
 * User-Level Threads Library (uthreads)
 * Hebrew University OS course.
 * Author: OS, os@cs.huji.ac.il
 */

#ifndef _UTHREADS_H
#define _UTHREADS_H

#include "uthreads.h"
#include <iostream>
#include <algorithm>
#include <ctime>
#include <cstdio>
#include <csignal>
#include <sys/time.h>
#include <iostream>
#include <vector>
#include <csetjmp>
#include <unistd.h>
#include <cstring>

#define MAX_THREAD_NUM 100 /* maximal number of threads */
#define STACK_SIZE 4096 /* stack size per thread (in bytes) */
#define READY 0
#define BLOCKED 1
#define RUNNING 2

typedef void (*thread_entry_point) (void);
typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7

address_t translate_address (address_t addr)
{
  address_t ret;
  asm volatile("xor    %%fs:0x30,%0\n"
               "rol    $0x11,%0\n"
  : "=g" (ret)
  : "0" (addr));
  return ret;
}

typedef struct {
    int tid;
    int state;
    int times_of_run;
    thread_entry_point entry_point;
    char *stack;
    int sleeping_ticks;

} uthread;

static int num_of_threads = 0;
static std::vector<uthread> thread_arr;
static std::vector<int> available_index;
static std::vector<uthread> sleeping_threads;

static sigjmp_buf env[MAX_THREAD_NUM];

static int quantum_length;

static int running_tid;
static int times_of_quantum;
struct itimerval timer;

/* External interface */
//TODO fix errors. it shuld print in specific format, and even use exit(1)
//TODO when a thread goes to sleep, is it blocked?
//TODO when they ask for number of quantums of a thread, is it number of actually quantoms or number of timer cycle starts?
//TODO thread cannot free itself. so if it terminates itself then the next thread should delete it.
/**
 * @brief this function is called every timer tick. does the switch, goes over all sleeping threads and decrement its
 * sleeping time.
 *
 * @param STATE the state which the running thread is going to be set to.
 */
void context_switch (int STATE)
{
  int ret_val = sigsetjmp(env[running_tid], 1);
  bool did_just_save_bookmark = ret_val == 0;
  if (did_just_save_bookmark)
    {
      if (STATE == -1)
        {
          //TODO terminate
          delete[] thread_arr.begin ()->stack;
          thread_arr.erase (thread_arr.begin ());
        }
      else
        {
          thread_arr[0].state = STATE;
        }
      std::rotate (thread_arr.begin (), thread_arr.begin () + 1, thread_arr.end ());
      while (thread_arr[0].state != READY)
        {
          std::rotate (thread_arr.begin (), thread_arr.begin () + 1, thread_arr.end ());
        }
      running_tid = thread_arr[0].tid;
      timer.it_value.tv_usec = quantum_length;
      timer.it_interval.tv_usec = quantum_length;
      thread_arr[0].times_of_run++;

      if (setitimer (ITIMER_VIRTUAL, &timer, NULL))
        {
          printf ("setitimer error.");
        }
      thread_arr[0].state = RUNNING;
      int i = 0;
      for (auto &elem: sleeping_threads)
        {
          elem.sleeping_ticks -= 1;
          if (elem.sleeping_ticks == 0)
            {
              sleeping_threads.erase (sleeping_threads.begin () + i);
              elem.state = READY;
              thread_arr.push_back (elem);
            }
          i++;
        }
      siglongjmp (env[running_tid], 1);
    }
}

/**
 * @brief initializes the thread library.
 *
 * Once this function returns, the main thread (tid == 0) will be set as RUNNING. There is no need to
 * provide an entry_point or to create a stack for the main thread - it will be using the "regular" stack and PC.
 * You may assume that this function is called before any other thread library function, and that it is called
 * exactly once.
 * The input to the function is the length of a quantum in micro-seconds.
 * It is an error to call this function with non-positive quantum_usecs.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_init (int quantum_usecs)
{
  for (int i = 1; i < 100; ++i)
    {
      available_index.push_back (i);
    }

  uthread main_thread = {0, RUNNING};
  thread_arr.push_back (main_thread);
  if (quantum_usecs <= 0)
    {
      std::cout << ::POLL_ERR;
      return -1;
    }
  struct sigaction sa = {0};
  sa.sa_handler = &context_switch;
  if (sigaction (SIGVTALRM, &sa, NULL) < 0)
    {
      printf ("sigaction error.");
    }
  quantum_length = quantum_usecs;
  times_of_quantum += 1;
  thread_arr[0].times_of_run = 1;

  timer.it_value.tv_sec = 0;        // first time interval, seconds part
  timer.it_value.tv_usec = quantum_usecs;        // first time interval, microseconds part

  // configure the timer to expire every 3 sec after that.
  timer.it_interval.tv_sec = 0;    // following time intervals, seconds part
  timer.it_interval.tv_usec = quantum_usecs;    // following time intervals, microseconds part

  int res = setitimer (ITIMER_VIRTUAL, &timer, nullptr);
  if (res)
    {
      printf ("setitimer error.");
    }
  int i = 0;
  for (auto &elem: sleeping_threads)
    {
      elem.sleeping_ticks -= 1;
      if (elem.sleeping_ticks == 0)
        {
          sleeping_threads.erase (sleeping_threads.begin () + i);
          elem.state = READY;
          thread_arr.push_back (elem);
        }
      i++;
    }
  return 0;
}

/**
 * @brief Creates a new thread, whose entry point is the function entry_point with the signature
 * void entry_point(void).
 *
 * The thread is added to the end of the READY threads list.
 * The uthread_spawn function should fail if it would cause the number of concurrent threads to exceed the
 * limit (MAX_THREAD_NUM).
 * Each thread should be allocated with a stack of size STACK_SIZE bytes.
 * It is an error to call this function with a null entry_point.
 *
 * @return On success, return the ID of the created thread. On failure, return -1.
*/
int uthread_spawn (thread_entry_point entry_point)
{
  if (entry_point == nullptr)
    {
      std::cerr << "system error: Parameter is Null." << std::endl;
      return -1;
    }
  if (num_of_threads + 1 > MAX_THREAD_NUM)
    {
      return -1;
    }

  int tid = available_index.front ();
  available_index.erase (available_index.begin ());
  std::cout << "hello" << std::endl;
  char *stack = new char[STACK_SIZE];
  address_t sp = (address_t) stack + STACK_SIZE - sizeof (address_t);
  address_t pc = (address_t) entry_point;
  sigsetjmp(env[tid], 1);
  (env[tid]->__jmpbuf)[JB_SP] = translate_address (sp);
  (env[tid]->__jmpbuf)[JB_PC] = translate_address (pc);
  sigemptyset (&env[tid]->__saved_mask);
  num_of_threads++;
  thread_arr.push_back (uthread{tid, READY, 0, entry_point, stack});
  return tid;
}

/**
 * @brief Terminates the thread with ID tid and deletes it from all relevant control structures.
 *
 * All the resources allocated by the library for this thread should be released. If no thread with ID tid exists it
 * is considered an error. Terminating the main thread (tid == 0) will result in the termination of the entire
 * process using exit(0) (after releasing the assigned library memory).
 *
 * @return The function returns 0 if the thread was successfully terminated and -1 otherwise. If a thread terminates
 * itself or the main thread is terminated, the function does not return.
*/
int uthread_terminate (int tid)
{
  if (tid == 0)
    {
      //TODO make sure if something else needs to be deleted
      for (auto it = thread_arr.begin (); it != thread_arr.end (); it++)
        {
          delete[] it->stack;
          thread_arr.erase (it);
        }
      exit (0);

    }
  else
    {
      for (auto it = thread_arr.begin (); it != thread_arr.end (); it++)
        {
          if ((*it).tid == tid)
            {
              if (running_tid == tid)
                {
                  // TODO implement content switch here. therefore this function will never
                  context_switch (-1);
                }
              else
                {
                  delete[] it->stack;
                  thread_arr.erase (it);
                }
              available_index.push_back (tid);
              std::sort (available_index.begin (), available_index.end ());
            }
        }
    }
  return 0;
}

/**
 * @brief Blocks the thread with ID tid. The thread may be resumed later using uthread_resume.
 *
 * If no thread with ID tid exists it is considered as an error. In addition, it is an error to try blocking the
 * main thread (tid == 0). If a thread blocks itself, a scheduling decision should be made. Blocking a thread in
 * BLOCKED state has no effect and is not considered an error.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_block (int tid)
{
  if (tid == 0)
    {
      std::cout << "runtime error: Can't bloack main thread." << std::endl;
      return -1;
    }
  if (running_tid == tid)
    {
      context_switch (BLOCKED);
      return 0;
    }
  else
    {
      auto iter = std::find_if (thread_arr.begin (), thread_arr.end (),
                                [&] (const uthread &thread)
                                {
                                    return thread.tid == tid;
                                });
      if (iter != thread_arr.end ())
        {
          iter->state = BLOCKED;
        }
      else
        {
          std::cout << "runtime error: not a valid tid to block." << std::endl;
          return -1;
        }
    }
  return 0;
}

/**
 * @brief Resumes a blocked thread with ID tid and moves it to the READY state.
 *
 * Resuming a thread in a RUNNING or READY state has no effect and is not considered as an error. If no thread with
 * ID tid exists it is considered an error.
 *
 * @return On success, return 0. On failure, return -1.
*/

//TODO BUG: when
int uthread_resume (int tid)
{
  auto iter = std::find_if (thread_arr.begin (), thread_arr.end (),
                            [&] (const uthread &thread)
                            {
                                return thread.tid == tid;
                            });
  if (iter != thread_arr.end ())
    {
      iter->state = READY;
    }
  else
    {
      std::cout << "runtime error: not a valid tid to block." << std::endl;
      return -1;
    }
  return 0;
}

/**
 * @brief Blocks the RUNNING thread for num_quantums quantums.
 *
 * Immediately after the RUNNING thread transitions to the BLOCKED state a scheduling decision should be made.
 * After the sleeping time is over, the thread should go back to the end of the READY queue.
 * If the thread which was just RUNNING should also be added to the READY queue, or if multiple threads wake up
 * at the same time, the order in which they're added to the end of the READY queue doesn't matter.
 * The number of quantums refers to the number of times a new quantum starts, regardless of the reason. Specifically,
 * the quantum of the thread which has made the call to uthread_sleep isn’t counted.
 * It is considered an error if the main thread (tid == 0) calls this function.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_sleep (int num_quantums)
{
  if (running_tid == 0)
    {
      std::cerr << "system error: Parameter is Null." << std::endl;
    }
  uthread sleeping;
  uthread temp = *thread_arr.begin ();
  std::memcpy (&temp, &sleeping, sizeof (uthread));
  sleeping.sleeping_ticks =
      num_quantums + 1; //+1 because this content switch should not decrease the thread thathust went to sleep
  sleeping.state = BLOCKED;
  sleeping_threads.push_back (sleeping);
  context_switch (-1);
}

/**
 * @brief Returns the thread ID of the calling thread.
 *
 * @return The ID of the calling thread.
*/

//TODO it says ID of calling thread, not running thread. HOW TO DO IT?
int uthread_get_tid ()
{
  return running_tid;
}

/**
 * @brief Returns the total number of quantums since the library was initialized, including the current quantum.
 *
 * Right after the call to uthread_init, the value should be 1.
 * Each time a new quantum starts, regardless of the reason, this number should be increased by 1.
 *
 * @return The total number of quantums.
*/
int uthread_get_total_quantums ()
{
  return times_of_quantum;
}

/**
 * @brief Returns the number of quantums the thread with ID tid was in RUNNING state.
 *
 * On the first time a thread runs, the function should return 1. Every additional quantum that the thread starts should
 * increase this value by 1 (so if the thread with ID tid is in RUNNING state when this function is called, include
 * also the current quantum). If no thread with ID tid exists it is considered an error.
 *
 * @return On success, return the number of quantums of the thread with ID tid. On failure, return -1.
*/
int uthread_get_quantums (int tid)
{
  for (const uthread &thread: thread_arr)
    {
      if (thread.tid == tid)
        {
          return thread.times_of_run;
        }
    }
  return -1;
}

#endif
