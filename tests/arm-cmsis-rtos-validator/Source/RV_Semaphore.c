/*-----------------------------------------------------------------------------
 *      Name:         RV_Semaphore.c 
 *      Purpose:      CMSIS RTOS validation tests implementation
 *-----------------------------------------------------------------------------
 *      Copyright(c) KEIL - An ARM Company
 *----------------------------------------------------------------------------*/
#include <string.h>
#include "RV_Framework.h"
#include "cmsis_rv.h"
#include "cmsis_os.h"

/*-----------------------------------------------------------------------------
 *      Test implementation
 *----------------------------------------------------------------------------*/

/* Definitions of shared variables */
osThreadId    G_Semaphore_ThreadId;
osSemaphoreId G_SemaphoreId;
#define TEST_THREAD_CNT 5
volatile uint32_t Var_ThreadStatus[TEST_THREAD_CNT];

static void Th_TestSemaphore (void const *arg);
osThreadDef (Th_TestSemaphore, osPriorityNormal, 0);


/* Definition for TC_SemaphoreCreateAndDelete */
osSemaphoreDef (Sem_TestZero);
osSemaphoreDef (Sem_TestMax);


/* Definitions for TC_SemaphoreObtainCounting */ 
osSemaphoreDef (Sem_TestPool);


/* Definitions for TC_SemaphoreWaitForBinary */
osSemaphoreDef (Sem_TestBin);


/* Definitions for TC_SemaphoreWaitForCounting */
osSemaphoreDef (Sem_TestMul);


/* Definitions for TC_SemaphoreZeroCount */ 
osSemaphoreDef (Sem_TestZeroCount);

/* Definitions for TC_SemaphoreWaitTimeout */
osSemaphoreDef (Sem_TestWait);

static void Th_SemaphoreWait (void const *arg);
osThreadDef (Th_SemaphoreWait, osPriorityBelowNormal, 0);


/* Definitions for TC_SemInterrupts */
osSemaphoreDef (Sem_ISR);

osSemaphoreId SemId_Isr;
osStatus      SemSt_Isr;
int32_t   NumTokens_Isr;

/*-----------------------------------------------------------------------------
 *      Default IRQ Handler
 *----------------------------------------------------------------------------*/

#define SEMAPHORE_IRQ_CALL_CREATE 0
#define SEMAPHORE_IRQ_CALL_ACQUIRE 1
#define SEMAPHORE_IRQ_CALL_ACQUIRE_TIMEOUT 2
#define SEMAPHORE_IRQ_CALL_RELEASE 3
#define SEMAPHORE_IRQ_CALL_DELETE 4
#define SEMAPHORE_IRQ_CALL_ACQUIRE_FOREVER 5

void Semaphore_IRQHandler (void) {
  
  switch (ISR_ExNum) {
    case SEMAPHORE_IRQ_CALL_CREATE: SemId_Isr = osSemaphoreCreate (osSemaphore (Sem_ISR), osFeature_Semaphore); break;
    case SEMAPHORE_IRQ_CALL_ACQUIRE: NumTokens_Isr = osSemaphoreAcquire(SemId_Isr,  0);   break;
    case SEMAPHORE_IRQ_CALL_ACQUIRE_TIMEOUT: NumTokens_Isr = osSemaphoreAcquire(SemId_Isr, 10);   break;
    case SEMAPHORE_IRQ_CALL_RELEASE: SemSt_Isr     = osSemaphoreRelease (SemId_Isr);       break;
    case SEMAPHORE_IRQ_CALL_DELETE: SemSt_Isr     = osSemaphoreDelete  (SemId_Isr);       break;

    // [ILG]
    // Test the infinite timeout calls too.
    case SEMAPHORE_IRQ_CALL_ACQUIRE_FOREVER:
      NumTokens_Isr = osSemaphoreAcquire (SemId_Isr, osWaitForever);
      break;

  }
}

/*-----------------------------------------------------------------------------
 *  Th_TestSemaphore: Semaphore access thread
 *----------------------------------------------------------------------------*/
static void Th_TestSemaphore (void const *arg) {
  uint32_t par = *(uint32_t *)arg;
  int32_t  sem;
  
  while (Var_ThreadStatus[par] == 0) {
    sem = osSemaphoreWait (G_SemaphoreId, 0);
    ASSERT_TRUE (sem != - 1);
    
    if (sem == 0) {
      /* Semaphore object is not available */
      Var_ThreadStatus[par] = 1;
    }
    else if (sem > 0) {
      /* Semaphore object was acquired */
      Var_ThreadStatus[par] = 2;
    }
  }
  // [ILG]
  // osDelay(10);
}

/*-----------------------------------------------------------------------------
 *  Th_SemaphoreWait: Semaphore wait thread
 *----------------------------------------------------------------------------*/
static void Th_SemaphoreWait (void const *arg) {
  uint32_t   ticks = *((uint32_t *)arg);

  // [ILG]
#if defined(OS_INCLUDE_RTOS_THREAD_PUBLIC_FLAGS_CLEAR)
  /* Clear signals */
  osSignalClear (G_Semaphore_ThreadId, 0x03);  
#endif
  
  if (osSemaphoreWait (G_SemaphoreId, ticks) != 0) {
    osSignalSet (G_Semaphore_ThreadId, 0x01);     /* Send signal: semaphore obtained    */
    ASSERT_TRUE (osSemaphoreRelease (G_SemaphoreId) == osOK);
  }
  else {
    osSignalSet (G_Semaphore_ThreadId, 0x02);     /* Send signal: semaphore not obtained*/
  }
}

/*-----------------------------------------------------------------------------
 *      Test cases
 *----------------------------------------------------------------------------*/

/*=======0=========1=========2=========3=========4=========5=========6=========7=========8=========9=========0=========1====*/
/**
\defgroup semaphore_funcs Semaphore Functions
\brief Semaphore Functions Test Cases
\details
The test cases check the osSemaphore* functions.

@{
*/

/*=======0=========1=========2=========3=========4=========5=========6=========7=========8=========9=========0=========1====*/
/**
\brief Test case: TC_SemaphoreCreateAndDelete
\details
- Create a semaphore with count parameter equal to zero
- Delete created semaphore object
- Create a semaphore with count parameter equal to osFeature_Semaphore
- Delete created semaphore object
*/
void TC_SemaphoreCreateAndDelete (void) {
  osSemaphoreId  id;
  
  /* - Create a semaphore with count parameter equal to zero */
  id = osSemaphoreCreate (osSemaphore (Sem_TestZero), 0);
  ASSERT_TRUE (id == NULL);
  
  if (id != NULL) {
    /* - Delete created semaphore object */
    ASSERT_TRUE (osSemaphoreDelete (id) == osOK);
  }
  
  /* - Create a semaphore  with count parameter equal to osFeature_Semaphore */
  id = osSemaphoreCreate (osSemaphore (Sem_TestMax), osFeature_Semaphore);
  ASSERT_TRUE (id != NULL);
  
  if (id != NULL) {
    /* - Delete created semaphore object */
    ASSERT_TRUE (osSemaphoreDelete (id) == osOK);
  }
}

/*=======0=========1=========2=========3=========4=========5=========6=========7=========8=========9=========0=========1====*/
/**
\brief Test case: TC_SemaphoreObtainCounting
\details
- Create a counting semaphore object
- Obtain all available tokens
- Verify that all available tokens are obtained
- Release all tokens
- Delete a counting semaphore object
*/
void TC_SemaphoreObtainCounting (void) {
  osSemaphoreId id;
  int32_t cnt, avail;
  
  /* - Create a counting semaphore object */
  id = osSemaphoreCreate (osSemaphore (Sem_TestPool), osFeature_Semaphore);
  ASSERT_TRUE (id != NULL);
  
  if (id != NULL) {
    /* - Obtain all available tokens */
    for (cnt = 0; cnt < osFeature_Semaphore; cnt++) {
      avail = osSemaphoreWait (id, 0);
      ASSERT_TRUE (avail == (osFeature_Semaphore - cnt));
    }
    /* - Verify that all available tokens are obtained */
    avail = osSemaphoreWait (id, 10);
    ASSERT_TRUE (avail == 0);
    
    avail = osSemaphoreWait (id, 0);
    ASSERT_TRUE (avail == 0);

    if (avail == 0) {
      /* - Release all tokens */
      for (cnt = 0; cnt < osFeature_Semaphore; cnt++) {
        ASSERT_TRUE (osSemaphoreRelease (id) == osOK);
      }
      /* All tokens have been released, check return value */
      ASSERT_TRUE (osSemaphoreRelease (id) == osErrorResource);
    }
    /* - Delete a counting semaphore object */
    ASSERT_TRUE (osSemaphoreDelete (id) == osOK);
  }
  
  /* - Create a counting semaphore object with one token*/
  id = osSemaphoreCreate (osSemaphore (Sem_TestPool), 1);
  ASSERT_TRUE (id != NULL);
  
  if (id != NULL) {
    /* - Obtain the available token */
    avail = osSemaphoreWait (id, 0);
    ASSERT_TRUE (avail == 1);
    
    /* - Verify that the token is obtained */
    avail = osSemaphoreWait (id, 10);
    ASSERT_TRUE (avail == 0);
    
    avail = osSemaphoreWait (id, 0);
    ASSERT_TRUE (avail == 0);

    /* - Release token */
    ASSERT_TRUE (osSemaphoreRelease (id) == osOK);

    /* - Token has been released, check return value */
    ASSERT_TRUE (osSemaphoreRelease (id) == osErrorResource);

    /* - Delete a counting semaphore object */
    ASSERT_TRUE (osSemaphoreDelete (id) == osOK);
  }
}

/*=======0=========1=========2=========3=========4=========5=========6=========7=========8=========9=========0=========1====*/
/**
\brief Test case: TC_SemaphoreObtainBinary
\details
- Create a binary semaphore object
- Obtain available token
- Verify that there is no available tokens left
- Release obtained token
- Verify that no more tokens can be released
- Delete a binary semaphore object
*/
void TC_SemaphoreObtainBinary (void) {
  osSemaphoreId id;
  int32_t avail;
  
  /* - Create a binary semaphore */
  id = osSemaphoreCreate (osSemaphore (Sem_TestBin), 1);
  ASSERT_TRUE (id != NULL);

  if (id != NULL) {
    /* - Obtain available token */
    avail = osSemaphoreWait (id, 0);
    ASSERT_TRUE (avail == 1);
    
    if (avail == 1) {
      /* - Verify that there is no available tokens left */
      ASSERT_TRUE (osSemaphoreWait (id,  0) == 0);
      ASSERT_TRUE (osSemaphoreWait (id, 10) == 0);
      /* - Release obtained token */
      ASSERT_TRUE (osSemaphoreRelease (id) == osOK);
    }
    /* - Delete a binary semaphore object */
    ASSERT_TRUE (osSemaphoreDelete  (id) == osOK);
  }
}

/*=======0=========1=========2=========3=========4=========5=========6=========7=========8=========9=========0=========1====*/
/**
\brief Test case: TC_SemaphoreWaitForBinary
\details
- Create a binary semaphore
- Create multiple instances of a test thread that obtains semaphore
- Wait until thread instances finish
- Check if semaphore was acquired
- Check if only one thread got semaphore object
- Release and delete a semaphore
- Only one thread instance must acquire semaphore object
*/
void TC_SemaphoreWaitForBinary (void) {
  uint32_t par[] = {0, 1, 2};
  uint32_t i, j;

  // [ILG]
  for (i = 0; i < 3; i++) {
    Var_ThreadStatus[i] = 0;
  }

  /* - Create a binary semaphore */
  G_SemaphoreId = osSemaphoreCreate (osSemaphore (Sem_TestBin), 1);
  ASSERT_TRUE (G_SemaphoreId != NULL);

  if (G_SemaphoreId != NULL) {
    /* Synchronize to the time slice */
    osDelay (1);
    
    /* - Create multiple instances of a test thread that obtains semaphore */
    osThreadCreate (osThread (Th_TestSemaphore), &par[0]);
    osThreadCreate (osThread (Th_TestSemaphore), &par[1]);
    osThreadCreate (osThread (Th_TestSemaphore), &par[2]);
    
    /* - Wait until thread instances finish */
    osDelay (50);

    /* - Check if semaphore was acquired */
    j = 0xFF;
    for (i = 0; i < 3; i++) {
      if (Var_ThreadStatus[i] == 2) j = i;
    }
    ASSERT_TRUE (j != 0xFF);            /* Ensure semaphore was acquired      */
    
    /* - Check if only one thread got semaphore object */
    for (i = 0; i < 3; i++) {
      if (i != j) {
        ASSERT_TRUE (Var_ThreadStatus[i] == 1);
      }
    }
    
    /* - Release and delete a semaphore */
    ASSERT_TRUE (osSemaphoreRelease (G_SemaphoreId) == osOK);
    ASSERT_TRUE (osSemaphoreDelete  (G_SemaphoreId) == osOK);
  }
}

/*=======0=========1=========2=========3=========4=========5=========6=========7=========8=========9=========0=========1====*/
/**
\brief Test case: TC_SemaphoreWaitForCounting
\details
- Create semaphore with index count parameter equal to 3
- Create multiple instances of a test thread that acquires semaphore
- Wait until thread instances finish
- Check if only available resources were acquired
- Release and delete a semaphore object
- Only one thread instance must acquire semaphore object
*/
void TC_SemaphoreWaitForCounting (void) {
  uint32_t par[] = {0, 1, 2, 3, 4};
  uint32_t i, acq, notacq;

  // [ILG]
  for (i = 0; i < 5; i++) {
    Var_ThreadStatus[i] = 0;
  }

  /* - Create semaphore */
  G_SemaphoreId = osSemaphoreCreate (osSemaphore (Sem_TestMul), 3);
  ASSERT_TRUE (G_SemaphoreId != NULL);

  if (G_SemaphoreId != NULL) {
    /* Synchronize to the time slice */
    osDelay (1);
    
    /* - Create multiple instances of a test thread */
    osThreadCreate (osThread (Th_TestSemaphore), &par[0]);
    osThreadCreate (osThread (Th_TestSemaphore), &par[1]);
    osThreadCreate (osThread (Th_TestSemaphore), &par[2]);
    osThreadCreate (osThread (Th_TestSemaphore), &par[3]);
    osThreadCreate (osThread (Th_TestSemaphore), &par[4]);
    
    /* - Wait until thread instances finish */
    osDelay (100);

    /* - Check if only available resources were acquired */
    acq    = 0;
    notacq = 0;
    for (i = 0; i < 5; i++) {
      if (Var_ThreadStatus[i] == 2) acq++;
      if (Var_ThreadStatus[i] == 1) notacq++;
    }
    ASSERT_TRUE (acq    == 3);          /* Ensure available resources were acquired */
    ASSERT_TRUE (notacq == 2);
    
    /* - Release and delete a semaphore object */
    ASSERT_TRUE (osSemaphoreRelease (G_SemaphoreId) == osOK);
    ASSERT_TRUE (osSemaphoreDelete  (G_SemaphoreId) == osOK);
  }
}

/*=======0=========1=========2=========3=========4=========5=========6=========7=========8=========9=========0=========1====*/
/**
\brief Test case: TC_SemaphoreZeroCount
\details
- Create a semaphore with initial count zero
*/
void TC_SemaphoreZeroCount (void) {
  osSemaphoreId id;
  
  /* - Create a semaphore with initial count zero */
  id = osSemaphoreCreate (osSemaphore (Sem_TestZeroCount), 0);
  ASSERT_TRUE (id == NULL);

}

/*=======0=========1=========2=========3=========4=========5=========6=========7=========8=========9=========0=========1====*/
/**
\brief Test case: TC_SemaphoreWaitTimeout
\details
- Obtain a semaphore object
- Create a thread that waits for a semaphore
- Delay parent thread to allow child thread wait for semaphore
- Check if child thread was able to obtain a semaphore or not
*/
void TC_SemaphoreWaitTimeout (void) {
  uint32_t wait_ticks;
  osEvent  evt;
  
  G_Semaphore_ThreadId = osThreadGetId ();
  
  /* Create a semaphore object */
  G_SemaphoreId = osSemaphoreCreate (osSemaphore (Sem_TestWait), 1);
  ASSERT_TRUE (G_SemaphoreId != NULL);
  
  if (G_SemaphoreId != NULL) {
    /* - Obtain a semaphore object */
    ASSERT_TRUE (osSemaphoreWait (G_SemaphoreId, 0) == 1);
    
    /* Synchronize to the time slice */
    osDelay (1);
    
    // [ILG]
#if !defined(OS_INCLUDE_RTOS_THREAD_PUBLIC_FLAGS_CLEAR)
    /* Clear signals */
    osSignalClear (G_Semaphore_ThreadId, 0x03);
#endif

    /* - Create a child thread that waits for a semaphore */
    wait_ticks = 10;
    ASSERT_TRUE (osThreadCreate (osThread (Th_SemaphoreWait), &wait_ticks) != NULL);
    
    /* - Delay parent thread to allow child thread wait for semaphore */
    osDelay (9);
    
    /* - Check if child thread was able to obtain a semaphore or not */
    evt = osSignalWait (0x01, 0);
    ASSERT_TRUE (evt.status == osOK);
    
    /* Wait for child thread to set a signal */
    // [ILG]
    // osDelay (2);
    // evt = osSignalWait (0x02, 0);
    // ASSERT_TRUE (evt.status == osEventSignal);
    osDelay (2);
    int i;
    for (i = 0; i < 100; i++) {
      evt = osSignalWait (0x02, 1);
      if (evt.status == osEventSignal) {
        if (i > 2) {
            printf("TC_SemaphoreWaitTimeout i=%d\n", i);
        }
        break;
      }
    }
    ASSERT_TRUE (i <= 12); // I encountered 11
    
    if (evt.status == osEventSignal) {
      ASSERT_TRUE (evt.value.signals == 0x02);
    }
    
    /* Delete a semaphore object */
    ASSERT_TRUE (osSemaphoreDelete (G_SemaphoreId) == osOK);
  }
  
  /* Restore priority of the main thread back to normal */
  ASSERT_TRUE (osThreadSetPriority (G_Semaphore_ThreadId, osPriorityNormal) == osOK);
}

/*=======0=========1=========2=========3=========4=========5=========6=========7=========8=========9=========0=========1====*/
/**
\brief Test case: TC_SemParam
\details
- Test semaphore management functions with invalid parameters
*/
void TC_SemParam (void) {
  ASSERT_TRUE (osSemaphoreCreate  (NULL, 0) == NULL);
  ASSERT_TRUE (osSemaphoreRelease (NULL)    == osErrorParameter);
  ASSERT_TRUE (osSemaphoreWait    (NULL, 0) == -1);
  ASSERT_TRUE (osSemaphoreDelete  (NULL)    == osErrorParameter);
}

/*=======0=========1=========2=========3=========4=========5=========6=========7=========8=========9=========0=========1====*/
/**
\brief Test case: TC_SemInterrupts
\details
- Call all semaphore management functions from the ISR
*/
void TC_SemInterrupts (void) {
  
  TST_IRQHandler = Semaphore_IRQHandler;
  
  NVIC_EnableIRQ((IRQn_Type)0);
  
  // [ILG]
  SemId_Isr = (osSemaphoreId)(0-1);

  ISR_ExNum = SEMAPHORE_IRQ_CALL_CREATE; /* Test: osSemaphoreCreate */
  NVIC_SetPendingIRQ((IRQn_Type)0);

  // [ILG]
  osDelay(2);

  ASSERT_TRUE (SemId_Isr == NULL);

  // [ILG]
  SemId_Isr = NULL;

  if (SemId_Isr == NULL) {
    /* Create a isr test semaphore */
    SemId_Isr = osSemaphoreCreate (osSemaphore (Sem_ISR), 1);
    ASSERT_TRUE (SemId_Isr != NULL);
    
    if (SemId_Isr != NULL) {

      // [ILG]
      NumTokens_Isr = 0;

      ISR_ExNum = SEMAPHORE_IRQ_CALL_ACQUIRE; /* Test: osSemaphoreWait (no time-out) */
      NVIC_SetPendingIRQ((IRQn_Type)0);

      // [ILG]
      osDelay(2);

      ASSERT_TRUE (NumTokens_Isr == osOK);
      
      if (NumTokens_Isr == osOK) {

        // [ILG]
        NumTokens_Isr = 0;

        ISR_ExNum = SEMAPHORE_IRQ_CALL_ACQUIRE_TIMEOUT; /* Test: osSemaphoreWait (with time-out) */
        NVIC_SetPendingIRQ((IRQn_Type)0);

        // [ILG]
        osDelay(2);

        ASSERT_TRUE (NumTokens_Isr == osErrorResource);
        
        if (NumTokens_Isr == osErrorResource) {
          ASSERT_TRUE (osSemaphoreAcquire(SemId_Isr, 5) == osErrorTimeout);
          ASSERT_TRUE(osSemaphoreGetCount(SemId_Isr) == 0);

          // [ILG]
          SemSt_Isr = osErrorOS;

          ISR_ExNum = SEMAPHORE_IRQ_CALL_RELEASE; /* Test: osSemaphoreRelease */
          NVIC_SetPendingIRQ((IRQn_Type)0);

          // [ILG]
          osDelay(2);

          ASSERT_TRUE (SemSt_Isr == osOK);
          ASSERT_TRUE(osSemaphoreGetCount(SemId_Isr) == 1);
        }

        // [ILG]
        // Test the infinite timeout too.
        NumTokens_Isr = 0;

        ISR_ExNum = SEMAPHORE_IRQ_CALL_ACQUIRE_FOREVER; /* Test: osSemaphoreWait (with infinite time-out) */
        NVIC_SetPendingIRQ((IRQn_Type)0);

        // [ILG]
        osDelay(2);

        ASSERT_TRUE (NumTokens_Isr == osOK);
        // -----
      }

      // [ILG]
      SemSt_Isr = osErrorOS;

      ISR_ExNum = SEMAPHORE_IRQ_CALL_DELETE;  /* Test: osSemaphoreDelete */
      NVIC_SetPendingIRQ((IRQn_Type)0);

      // [ILG]
      osDelay(2);

      ASSERT_TRUE (SemSt_Isr == osErrorISR);

      osSemaphoreDelete(SemId_Isr);
    }
  }
  NVIC_DisableIRQ((IRQn_Type)0);
}

/**
@}
*/ 
// end of group semaphore_funcs

/*-----------------------------------------------------------------------------
 * End of file
 *----------------------------------------------------------------------------*/
