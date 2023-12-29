#include "puser-functions.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <ucontext.h>
#include <sys/time.h>
#include "../util/globals.h"
#include "../filesystem/filesystem.h"
#include "../logger/logger.h"
#include <time.h>
#include <valgrind/valgrind.h>

static ucontext_t mainContext;
ucontext_t schedulerContext;
ucontext_t reaperContext;
static ucontext_t *activeContext = NULL;
static const int centisecond = 10000; // 10 milliseconds

/**
 * scheduler function - function to be run at every tick
 * decides which PCB to be run for the remainder of current tick
 * @return none
 */
static void scheduler(void)
{
    if (count_running(pcb_list) == 0)
    {
        // TODO: idle()
        exit(12);
    }

    int priority;
    int roulette = rand() % (9 + 6 + 4);
    if (roulette < 9)
    {
        priority = -1;
    }
    else if (roulette < 9 + 6)
    {
        priority = 0;
    }
    else
    {
        priority = 1;
    }

    while (count_running_priority(pcb_list, priority) == 0)
    {
        priority = ((priority + 2) % 3) - 1;
    }

    int target = rand() % count_running_priority(pcb_list, priority);
    int visited = 0;
    while (true)
    {
        if (pcb_list->status == T_RUNNING && pcb_list->priority == priority)
        {
            visited++;
            if (visited > target)
            {
                break;
            }
        }
        pcb_list = pcb_list->next;
    }

    current_pcb = pcb_list;

    activeContext = current_pcb->context;
    activeContext->uc_link = &schedulerContext;

    log_schedule_event(current_pcb->pid, current_pcb->priority, current_pcb->name);
    setcontext(activeContext);
    exit(EXIT_FAILURE);
}

/**
 * reaper function that runs at termination of PCB
 * increments ticks and sets current context back to scheduler
 * @return none
 */
static void reaper()
{   p_exit();
    ticks++;
    setcontext(&schedulerContext);
}

/**
 * alarm handler that is invoked when alarm is signalled at every quanta
 * increments ticks and sets current context to scheduler, effectively allowing the scheduler to run at every quanta
 * @return none
 */
static void alarmHandler(int signum)
{ // SIGALARM
    ticks++;
    swapcontext(current_pcb->context, &schedulerContext);
}

/**
 * sets \ref alarmHandler to be called when alarm is signalled
 * @return none
 */
static void setAlarmHandler(void)
{
    struct sigaction act;
    act.sa_handler = alarmHandler;
    act.sa_flags = SA_RESTART;
    sigfillset(&act.sa_mask);
    sigaction(SIGALRM, &act, NULL);
}

/**
 * sets timer to invoke alarm every centisecond
 * @return none
 */
static void setTimer(void)
{
    struct itimerval it;
    it.it_interval = (struct timeval){.tv_usec = centisecond};
    it.it_value = it.it_interval;
    setitimer(ITIMER_REAL, &it, NULL);
}

/**
 * frees all contexts prior to exit
 * @return none
 */
static void freeStacks(void)
{
    // free(schedulerContext.uc_stack.ss_sp);
    // for (int i = 0; i < THREAD_COUNT; i++) {
    //     free(threadContexts[i].uc_stack.ss_sp);
    // }
}

/**
 * initializes and start scheduler
 * @return none
 */
void start_scheduler()
{
    srand(time(0));
    signal(SIGINT, SIG_IGN);  // Ctrl-C
    signal(SIGQUIT, SIG_IGN); /* Ctrl-\ */
    signal(SIGTSTP, SIG_IGN); // Ctrl-Z

    getcontext(&schedulerContext);
    char *stack = malloc(STACKSIZE);
    schedulerContext.uc_stack.ss_sp = stack;
    schedulerContext.uc_stack.ss_size = STACKSIZE;
    schedulerContext.uc_stack.ss_flags = 0;
    schedulerContext.uc_link = NULL;
    makecontext(&schedulerContext, scheduler, 0);
    VALGRIND_STACK_REGISTER(stack, stack + STACKSIZE);

    getcontext(&reaperContext);
    stack = malloc(STACKSIZE);
    reaperContext.uc_stack.ss_sp = stack;
    reaperContext.uc_stack.ss_size = STACKSIZE;
    reaperContext.uc_stack.ss_flags = 0;
    reaperContext.uc_link = NULL;
    makecontext(&reaperContext, reaper, 0);
    VALGRIND_STACK_REGISTER(stack, stack + STACKSIZE);

    setAlarmHandler();
    setTimer();

    // printf("swapcontext\n");
    // printf("ss_size: %zu\n", activeContext->uc_stack.ss_size);
    // swapcontext(&mainContext, activeContext);
    getcontext(&mainContext);
    setcontext(&schedulerContext);
    // swapcontext(&mainContext, &schedulerContext);

    // printf("freeStacks\n");
    // freeStacks();
}
