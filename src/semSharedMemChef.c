/**
 *  \file semSharedMemChef.c (implementation file)
 *
 *  \brief Problem name: Restaurant
 *
 *  Synchronization based on semaphores and shared memory.
 *  Implementation with SVIPC.
 *
 *  Definition of the operations carried out by the chef:
 *     \li waitOrder
 *     \li processOrder
 *
 *  \author Nuno Lau - December 2023
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <math.h>
#include <signal.h>
#include <sys/time.h>
#include <errno.h>

#include "probConst.h"
#include "probDataStruct.h"
#include "logging.h"
#include "sharedDataSync.h"
#include "semaphore.h"
#include "sharedMemory.h"


/** \brief logging file name */
static char nFic[51];

/** \brief shared memory block access identifier */
static int shmid;

/** \brief semaphore set access identifier */
static int semgid;

/** \brief group that requested cooking food */
static int lastGroup;

/** \brief pointer to shared memory region */
static SHARED_DATA *sh;

static void waitForOrder ();
static void processOrder ();

/**
 *  \brief Main program.
 *
 *  Its role is to generate the life cycle of one of intervening entities in the problem: the chef.
 */
int main (int argc, char *argv[])
{
    int key;                                          /*access key to shared memory and semaphore set */
    char *tinp;                                                     /* numerical parameters test flag */

    /* validation of command line parameters */

    if (argc != 4) { 
        freopen ("error_CH", "a", stderr);
        fprintf (stderr, "Number of parameters is incorrect!\n");
        return EXIT_FAILURE;
    }
    else {
       freopen (argv[3], "w", stderr);
       setbuf(stderr,NULL);
    }
    strcpy (nFic, argv[1]);
    key = (unsigned int) strtol (argv[2], &tinp, 0);
    if (*tinp != '\0') {
        fprintf (stderr, "Error on the access key communication!\n");
        return EXIT_FAILURE;
    }

    /* connection to the semaphore set and the shared memory region and mapping the shared region onto the
       process address space */
    if ((semgid = semConnect (key)) == -1) { 
        perror ("error on connecting to the semaphore set");
        return EXIT_FAILURE;
    }
    if ((shmid = shmemConnect (key)) == -1) { 
        perror ("error on connecting to the shared memory region");
        return EXIT_FAILURE;
    }
    if (shmemAttach (shmid, (void **) &sh) == -1) { 
        perror ("error on mapping the shared region on the process address space");
        return EXIT_FAILURE;
    }

    /* initialize random generator */
    srandom ((unsigned int) getpid ());                                      

    /* simulation of the life cycle of the chef */

    int nOrders=0;
    while(nOrders < sh->fSt.nGroups) {
       waitForOrder();
       processOrder();

       nOrders++;
    }

    /* unmapping the shared region off the process address space */

    if (shmemDettach (sh) == -1) { 
        perror ("error on unmapping the shared region off the process address space");
        return EXIT_FAILURE;;
    }

    return EXIT_SUCCESS;
}

/**
 *  \brief chefs wait for a food order.
 *
 *  The chef waits for the food request that will be provided by the waiter.
 *  Updates its state and saves internal state.
 *  Received order should be acknowledged.
 */
static void waitForOrder()
{
    if (semDown(semgid, sh->waitOrder) == -1)
    {
        perror("error on the down operation for semaphore access");
        exit(EXIT_FAILURE);
    }

    if (semDown(semgid, sh->mutex) == -1)
    {
        perror("error on the down operation for semaphore access (Chef)");
        exit(EXIT_FAILURE);
    }

    lastGroup = sh->fSt.foodGroup;
    
    sh->fSt.st.chefStat = COOK;
    saveState(nFic, &sh->fSt);

    if (semUp(semgid, sh->orderReceived) == -1)
    {
        perror("error on the down operation for semaphore access");
        exit(EXIT_FAILURE);
    }

    if (semUp(semgid, sh->mutex) == -1)
    {
        perror("error on the up operation for semaphore access (Chef)");
        exit(EXIT_FAILURE);
    }
}

/**
 *  \brief chef cooks, then delivers the food to the waiter 
 *
 *  The chef takes some time to cook and signals the waiter that food is 
 *  ready (this may only happen when waiter is available)
 *  then updates its state.
 *  The internal state should be saved.
 */
static void processOrder()
{
    usleep((unsigned int)floor((MAXCOOK * random()) / RAND_MAX + 100.0));

    if (semDown(semgid, sh->waiterRequestPossible) == -1)
    {
        perror("error on the down operation for semaphore access");
        exit(EXIT_FAILURE);
    }

    //entrada na zona critica
    if (semDown(semgid, sh->mutex) == -1)       // down no mutex (identificador para o semaforo associado ao acesso à memoria partilhada)
    {
        perror("error on the down operation for semaphore access (Chef)");
        exit(EXIT_FAILURE);
    }


    sh->fSt.waiterRequest.reqType = FOODREADY;
    sh->fSt.waiterRequest.reqGroup = lastGroup;


    sh->fSt.st.chefStat = WAIT_FOR_FOOD;
    saveState(nFic, &sh->fSt);


    if (semUp(semgid, sh->mutex) == -1)
    {
        perror("error on the up operation for semaphore access (Chef)");
        exit(EXIT_FAILURE);
    }
    
    if (semUp(semgid, sh->waiterRequest) == -1)
    {
        perror("error on the down operation for semaphore access");
        exit(EXIT_FAILURE);
    }
}
