/* ------------------------------------------------------------------------
   phase1.c

   University of Arizona
   Computer Science 452
   Fall 2015

   ------------------------------------------------------------------------ */

#include "phase1.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "kernel.h"
#include "stdbool.h"

/* ------------------------- Prototypes ----------------------------------- */
int sentinel (char *);
void dispatcher(void);
void launch();
static void checkDeadlock();
int zap(int);

/* -------------------------- Globals ------------------------------------- */

// Patrick's debugging global variable...
int debugflag = 1;


// the process table
procStruct ProcTable[MAXPROC];

// Process lists
static procPtr ReadyList;

// current process ID
procPtr Current;

// the next pid to be assigned
unsigned int nextPid = SENTINELPID;

// procTableSize
int pTableSize = 0;


/* -------------------------- Functions ----------------------------------- */
/* ------------------------------------------------------------------------
   Name - startup
   Purpose - Initializes process lists and clock interrupt vector.
             Start up sentinel process and the test process.
   Parameters - argc and argv passed in by USLOSS
   Returns - nothing
   Side Effects - lots, starts the whole thing
   ----------------------------------------------------------------------- */
void startup(int argc, char *argv[])
{
    int result; /* value returned by call to fork1() */

    /* initialize the process table */
    if (DEBUG == 1 && debugflag == 1) {
        USLOSS_Console("startup(): initializing process table, ProcTable[]\n");
    }
        for(int i = 0; i < MAXPROC; i++ ){
            ProcTable[i].nextProcPtr = NULL;
            ProcTable[i].childProcPtr = NULL;
            ProcTable[i].nextSiblingPtr = NULL;
            for(int j = 0; j < MAXNAME; j++){
                ProcTable[i].name[j] = 0;
            }
            for(int j = 0; j < MAXARG; j++){
                ProcTable[i].startArg[j] = 0;
            }

            ProcTable[i].pid = -1;
            ProcTable[i].priority = -1;
            ProcTable[i].startFunc = NULL;
            ProcTable[i].stackSize = -1;
            ProcTable[i].status = -1;

        }


    // Initialize the Ready list, etc.
    if (DEBUG == 1 && debugflag == 1) {
        USLOSS_Console("startup(): initializing the Ready list\n");
       ReadyList = NULL;
    }
    // Initialize the clock interrupt handler

    // startup a sentinel process
    if (DEBUG == 1 && debugflag == 1) {
        USLOSS_Console("startup(): calling fork1() for sentinel\n");
    }
    result = fork1("sentinel", sentinel, NULL, USLOSS_MIN_STACK,
                    SENTINELPRIORITY);
    if (result < 0) {
        if (DEBUG == 1 && debugflag == 1) {
            USLOSS_Console("startup(): fork1 of sentinel returned error, ");
            USLOSS_Console("halting...\n");
        }
        USLOSS_Halt(1);
    }

    // start the test process
    if (DEBUG == 1 && debugflag == 1) {
        USLOSS_Console("startup(): calling fork1() for start1\n");
    }
    result = fork1("start1", start1, NULL, 2 * USLOSS_MIN_STACK, 1);
    if (result < 0) {
        USLOSS_Console("startup(): fork1 for start1 returned an error, ");
        USLOSS_Console("halting...\n");
        USLOSS_Halt(1);
    }

    USLOSS_Console("startup(): Should not see this message! ");
    USLOSS_Console("Returned from fork1 call that created start1\n");

    return;
} /* startup */

/* ------------------------------------------------------------------------
   Name - finish
   Purpose - Required by USLOSS
   Parameters - none
   Returns - nothing
   Side Effects - none
   ----------------------------------------------------------------------- */
void finish(int argc, char *argv[])
{
    if (DEBUG && debugflag)
        USLOSS_Console("in finish...\n");
} /* finish */

/* ------------------------------------------------------------------------
   Name - fork1
   Purpose - Gets a new process from the process table and initializes
             information of the process.  Updates information in the
             parent process to reflect this child process creation.
   Parameters - the process procedure address, the size of the stack and
                the priority to be assigned to the child process.
   Returns - the process id of the created child or -1 if no child could
             be created or if priority is not between max and min priority.
   Side Effects - ReadyList is changed, ProcTable is changed, Current
                  process information changed
   ------------------------------------------------------------------------ */
int fork1(char *name, int (*startFunc)(char *), char *arg,
          int stacksize, int priority)
{
    int procSlot = -1;

    if (DEBUG == 1 && debugflag == 1)
        USLOSS_Console("fork1(): creating process %s\n", name);

    // test if in kernel mode; halt if in user mode
    if(USLOSS_PSR_CURRENT_MODE == 0){
        USLOSS_Console("Requested fork is user mode");
        USLOSS_Halt(1);
    }

    // Return if stack size is too small
    if(stacksize < USLOSS_MIN_STACK){
        return -2;
    }

    // Is there room in the process table? What is the next PID?
    if(pTableSize > MAXPROC) {
        return -1;
    }

    // fill-in entry in process table */
    if ( strlen(name) >= (MAXNAME - 1) ) {
        USLOSS_Console("fork1(): Process name is too long.  Halting...\n");
        USLOSS_Halt(1);
    }


    for(int i = 0, found = 0; i < MAXPROC && found == 0; i++) {
        if (ProcTable[i].pid == -1) {
            procSlot = i;
            found = 1;
        }
        if(i == MAXPROC-1 && found == 0){
            return -1;
        }
    }

    strcpy(ProcTable[procSlot].name, name);
    ProcTable[procSlot].startFunc = startFunc;
    if ( arg == NULL )
        ProcTable[procSlot].startArg[0] = '\0';
    else if ( strlen(arg) >= (MAXARG - 1) ) {
        USLOSS_Console("fork1(): argument too long.  Halting...\n");
        USLOSS_Halt(1);
    }
    else
        strcpy(ProcTable[procSlot].startArg, arg);

    // Initialize context for this process, but use launch function pointer for
    // the initial value of the process's program counter (PC)


    //ProcTable[procSlot].state = USLOSSContent();
    ProcTable[procSlot].stackSize = stacksize;
    ProcTable[procSlot].stack = malloc(sizeof(char)*stacksize);
    ProcTable[procSlot].pid = nextPid;
    nextPid++;
    ProcTable[procSlot].priority = priority;
    ProcTable[procSlot].status = "READY";
    USLOSS_ContextInit(&(ProcTable[procSlot].state),
                       ProcTable[procSlot].stack,
                       ProcTable[procSlot].stackSize,
                       NULL,
                       launch);

    // for future phase(s)
    p1_fork(ProcTable[procSlot].pid);

    // More stuff to do here...

    return ProcTable[procSlot].pid;  // -1 is not correct! Here to prevent warning.
} /* fork1 */

/* ------------------------------------------------------------------------
   Name - launch
   Purpose - Dummy function to enable interrupts and launch a given process
             upon startup.
   Parameters - none
   Returns - nothing
   Side Effects - enable interrupts
   ------------------------------------------------------------------------ */
void launch()
{
    int result;

    if (DEBUG && debugflag)
        USLOSS_Console("launch(): started\n");

    // Enable interrupts

    // Call the function passed to fork1, and capture its return value
    result = Current->startFunc(Current->startArg);

    if (DEBUG && debugflag)
        USLOSS_Console("Process %d returned to launch\n", Current->pid);

    quit(result);

} /* launch */


/* ------------------------------------------------------------------------
   Name - join
   Purpose - Wait for a child process (if one has been forked) to quit.  If 
             one has already quit, don't wait.
   Parameters - a pointer to an int where the termination code of the 
                quitting process is to be stored.
   Returns - the process id of the quitting child joined on.
             -1 if the process was zapped in the join
             -2 if the process has no children
   Side Effects - If no child process has quit before join is called, the 
                  parent is removed from the ready list and blocked.
   ------------------------------------------------------------------------ */
int join(int *status)
{
    return -1;  // -1 is not correct! Here to prevent warning.
} /* join */


/* ------------------------------------------------------------------------
   Name - quit
   Purpose - Stops the child process and notifies the parent of the death by
             putting child quit info on the parents child completion code
             list.
   Parameters - the code to return to the grieving parent
   Returns - nothing
   Side Effects - changes the parent of pid child completion status list.
   ------------------------------------------------------------------------ */
void quit(int status)
{
    p1_quit(Current->pid);
} /* quit */


/* ------------------------------------------------------------------------
   Name - dispatcher
   Purpose - dispatches ready processes.  The process with the highest
             priority (the first on the ready list) is scheduled to
             run.  The old process is swapped out and the new process
             swapped in.
   Parameters - none
   Returns - nothing
   Side Effects - the context of the machine is changed
   ----------------------------------------------------------------------- */
void dispatcher(void)
{
    procPtr nextProcess = NULL;

    p1_switch(Current->pid, nextProcess->pid);
} /* dispatcher */


/* ------------------------------------------------------------------------
   Name - sentinel
   Purpose - The purpose of the sentinel routine is two-fold.  One
             responsibility is to keep the system going when all other
             processes are blocked.  The other is to detect and report
             simple deadlock states.
   Parameters - none
   Returns - nothing
   Side Effects -  if system is in deadlock, print appropriate error
                   and halt.
   ----------------------------------------------------------------------- */
int sentinel (char *dummy)
{
    if (DEBUG && debugflag)
        USLOSS_Console("sentinel(): called\n");
    while (1)
    {
        checkDeadlock();
        USLOSS_WaitInt();
    }
} /* sentinel */


/* check to determine if deadlock has occurred... */
static void checkDeadlock()
{
} /* checkDeadlock */


/*
 * Disables the interrupts.
 */
void disableInterrupts()
{
    // turn the interrupts OFF iff we are in kernel mode
    // if not in kernel mode, print an error message and
    // halt USLOSS

} /* disableInterrupts */

/*
* Enables the interrupts
*/
void enableInteruppts()
{

}

/*
int zap(int pid)
{
	if (DEBUG && debugflag)
        	USLOSS_Console("sentinel(): called\n");

	requireKernelMode("zap()");
	// obviously I need to fill in the code for disableInterrupts - I'll look into it later
	disableInterrupts();

	if(Current->pid == pid)
	{
		USLOSS_Console("zap(): %d tried to call itself. Calling USLOSS_Halt(1)\n", pid);
		USLOSS_Halt(1);
	}

	procPtr proc = &ProcTable[pid];
	enableInteruppts();


	process = &ProcTable[pid % MAXPROC];

    	if (process->status == EMPTY || process->pid != pid)
	{
        	USLOSS_Console("zap(): %d does not exist.  Calling USLOSS_HALT(1)\n", pid);
        	USLOSS_Halt(1);
    	}

    	if (process->status != QUIT)
	{
		// TODO: let other process know that it is zapped
    		// need to block still
	}
    	enableInterrupts();
    	if (Current->zapQueue.size > 0)
	{
        	return -1;
    	}
    	return 0;

}
*/
