#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <inttypes.h>

#include <SDL2/SDL_scancode.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"
#include "timers.h"

#include "TUM_Ball.h"
#include "TUM_Draw.h"
#include "TUM_Event.h"
#include "TUM_Sound.h"
#include "TUM_Utils.h"
#include "TUM_Font.h"

#include "AsyncIO.h"

#define mainGENERIC_PRIORITY (tskIDLE_PRIORITY)
#define mainGENERIC_STACK_SIZE ((unsigned short)2560)

#define KEYCODE(CHAR) SDL_SCANCODE_##CHAR

#define STATE_QUEUE_LENGTH 1

#define STATE_COUNT 2

#define STATE_ONE 0
#define STATE_TWO 1

#define NEXT_TASK 0
#define PREV_TASK 1

#define STARTING_STATE STATE_ONE

#define STATE_DEBOUNCE_DELAY 300


const unsigned char next_state_signal = NEXT_TASK;
const unsigned char prev_state_signal = PREV_TASK;

static TaskHandle_t state_machine = NULL;
static TaskHandle_t task_frequ1 = NULL;
static TaskHandle_t task_frequ2 = NULL;
static TaskHandle_t task3 = NULL;
static TaskHandle_t task4 = NULL;
static TaskHandle_t timer_task = NULL;
static TaskHandle_t control_task = NULL;
static TaskHandle_t task1_screen2 = NULL;
static TaskHandle_t terminate_process = NULL;

static SemaphoreHandle_t task3_signal = NULL;
static SemaphoreHandle_t counter_T_lock = NULL;
static SemaphoreHandle_t counter_F_lock = NULL;
static SemaphoreHandle_t ScreenLock1 = NULL;
static SemaphoreHandle_t ScreenLock2 = NULL;
static SemaphoreHandle_t state_machine_signal = NULL;

static StaticTask_t xTaskBuffer;
static StackType_t xStack[mainGENERIC_STACK_SIZE * 2];

static int counter_T = 0;
static int counter_F = 0;

TimerHandle_t xResetTimer = NULL;

typedef struct buttons_buffer {
    unsigned char buttons[SDL_NUM_SCANCODES];
    SemaphoreHandle_t lock;
} buttons_buffer_t;

static buttons_buffer_t buttons = { 0 };


void xGetButtonInput(void)
{
    if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
        xQueueReceive(buttonInputQueue, &buttons.buttons, 0);
        xSemaphoreGive(buttons.lock);
    }
}

void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer,
                                    StackType_t **ppxIdleTaskStackBuffer,
                                    uint32_t *pulIdleTaskStackSize )
{
    /* If the buffers to be provided to the Idle task are declared inside this
    function then they must be declared static – otherwise they will be allocated on
    the stack and so not exists after this function exits. */
    static StaticTask_t xIdleTaskTCB;
    static StackType_t uxIdleTaskStack[ configMINIMAL_STACK_SIZE ];

    /* Pass out a pointer to the StaticTask_t structure in which the Idle task’s
    state will be stored. */
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;

    /* Pass out the array that will be used as the Idle task’s stack. */
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;

    /* Pass out the size of the array pointed to by *ppxIdleTaskStackBuffer.
    Note that, as the array is necessarily of type StackType_t,
    configMINIMAL_STACK_SIZE is specified in words, not bytes. */
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

void vApplicationGetTimerTaskMemory( StaticTask_t **ppxTimerTaskTCBBuffer,
                                    StackType_t **ppxTimerTaskStackBuffer,
                                    uint32_t *pulTimerTaskStackSize)
{
    /* If the buffers to be provided to the Timer task are declared inside this
    function then they must be declared static – otherwise they will be allocated on
    the stack and so not exists after this function exits. */
    static StaticTask_t xTimerTaskTCB;
    static StackType_t uxTimerTaskStack[ configTIMER_TASK_STACK_DEPTH ];

    /* Pass out a pointer to the StaticTask_t structure in which the Timer
    task’s state will be stored. */
    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;

    /* Pass out the array that will be used as the Timer task’s stack. */
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;

    /* Pass out the size of the array pointed to by *ppxTimerTaskStackBuffer.
    Note that, as the array is necessarily of type StackType_t,
    configTIMER_TASK_STACK_DEPTH is specified in words, not bytes. */
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}                     

void checkDraw(unsigned char status, const char *msg)
{
    if (status) {
        if (msg)
            fprintf(stderr, "[ERROR] %s, %s\n", msg,
                    tumGetErrorMessage());
        else {
            fprintf(stderr, "[ERROR] %s\n", tumGetErrorMessage());
        }
    }
}

#define FPS_AVERAGE_COUNT 50
#define FPS_FONT "IBMPlexSans-Bold.ttf"

void vDrawFPS(void)
{
    static unsigned int periods[FPS_AVERAGE_COUNT] = { 0 };
    static unsigned int periods_total = 0;
    static unsigned int index = 0;
    static unsigned int average_count = 0;
    static TickType_t xLastWakeTime = 0, prevWakeTime = 0;
    static char str[10] = { 0 };
    static int text_width;
    int fps = 0;
    font_handle_t cur_font = tumFontGetCurFontHandle();

    xLastWakeTime = xTaskGetTickCount();

    if (prevWakeTime != xLastWakeTime) {
        periods[index] =
            configTICK_RATE_HZ / (xLastWakeTime - prevWakeTime);
        prevWakeTime = xLastWakeTime;
    }
    else {
        periods[index] = 0;
    }

    periods_total += periods[index];

    if (index == (FPS_AVERAGE_COUNT - 1)) {
        index = 0;
    }
    else {
        index++;
    }

    if (average_count < FPS_AVERAGE_COUNT) {
        average_count++;
    }
    else {
        periods_total -= periods[index];
    }

    fps = periods_total / average_count;

    tumFontSelectFontFromName(FPS_FONT);

    sprintf(str, "FPS: %2d", fps);

    if (!tumGetTextSize((char *)str, &text_width, NULL))
        checkDraw(tumDrawText(str, SCREEN_WIDTH - text_width - 10,
                              SCREEN_HEIGHT - DEFAULT_FONT_SIZE * 1.5,
                              Skyblue),
                  __FUNCTION__);

    tumFontSelectFontFromHandle(cur_font);
    tumFontPutFontHandle(cur_font);
}

void vSequential_StateMachine(void *pvParameters)
{
    // initial suspend of screen 2 and resume of screen 1
    vTaskSuspend(task1_screen2);
    vTaskResume(task_frequ1);
    vTaskResume(task_frequ2);
    vTaskResume(timer_task);
    vTaskResume(control_task);

    int state = 0;

    const int state_change_period = 1000;
    TickType_t last_change = xTaskGetTickCount();

    while(1){
        if (xSemaphoreTake(state_machine_signal, 
                            portMAX_DELAY)) {
            if ((xTaskGetTickCount() - last_change) >
                    state_change_period) {
                
                if (state == 0) {
                    state = 1;
                }
                else if (state == 1) {
                    state = 0;
                }
                if (state == 0) {
                    printf("displaying solution for 3.2\n");
                        // resuming tasks of screen 1
                    vTaskSuspend(task1_screen2);
                    vTaskResume(task_frequ2);
                    vTaskResume(task_frequ1);
                    vTaskResume(timer_task);
                    vTaskResume(control_task);
                    
                }
                if (state == 1) {
                    printf("displaying solution for 3.3\n");
                        // resuming tasks of screen 2
                    vTaskSuspend(task_frequ1);
                    vTaskSuspend(task_frequ2);
                    vTaskSuspend(timer_task);
                    vTaskSuspend(control_task);
                    vTaskResume(task1_screen2);
                }
                last_change = xTaskGetTickCount();
            }
        }
    }
}

void vTerminateProcess(void *pvParameters) {
    while(1) {
        if(ulTaskNotifyTake(pdTRUE, portMAX_DELAY)) {
            vTaskDelete(control_task);
            vTaskDelete(task_frequ1);
            vTaskDelete(task_frequ2);
            vTaskDelete(task3);
            vTaskDelete(task4);
            vTaskDelete(timer_task);
            vTaskDelete(task1_screen2);
            vTaskDelete(state_machine);
            vTaskDelete(NULL);
        }
    }
}

void vCheckStateInput()
{   
    if (buttons.buttons[KEYCODE(E)]) {
        xSemaphoreGive(state_machine_signal); 
          
    }
    if (buttons.buttons[KEYCODE(Q)]) {
        xTaskNotifyGive(terminate_process);
    }
}

// Tasks Screen 2 #######################################

void vTask1_screen2(void *pvParameters) 
{
    tumDrawBindThread();

    int ticks = 0;
    int change = 0;

    while (1) {
        if(xSemaphoreTake(ScreenLock2, 1000) == pdTRUE){
            for (int counter=0; counter<100; counter++) {
                tumEventFetchEvents();
                xGetButtonInput();
                vCheckStateInput();

                if (ticks >= 50){
                    change = 1;
                }
                else{
                    change = 0;
                }
                if (ticks == 100){
                    ticks = 0;
                }
                if(change){
                    tumDrawClear(White);
                }
                else{
                    tumDrawClear(Black);
                }
                vDrawFPS();

                tumDrawUpdateScreen(); // Refresh screen 

                ticks++;

                vTaskDelay(10);
            }
            xSemaphoreGive(ScreenLock2);
            
        }
        vTaskDelay(100);
    }
}


// Tasks Screen 2 end ##################################

void vCheckInput() {
    if (buttons.buttons[KEYCODE(T)]) {
        xSemaphoreGive(task3_signal);
        vTaskDelay(750);
    }
    if (buttons.buttons[KEYCODE(F)]) {
        xTaskNotifyGive(task4);
        vTaskDelay(750);
    }
    if (buttons.buttons[KEYCODE(C)]) {
        if(eTaskGetState(control_task) == eReady || 
            eTaskGetState(control_task) == eRunning ||
            eTaskGetState(control_task) == eBlocked) {
            vTaskSuspend(control_task);
        }
        else
        {
            vTaskResume(control_task);
        }
        vTaskDelay(750);
        
    }
}
// Tasks Screen 1 #######################################

void vTask_frequ1(void *pvParameters)
{   
    tumDrawBindThread();

    signed short center_x = SCREEN_WIDTH/2;
    signed short center_y = SCREEN_HEIGHT/2;

    signed short circle_x = center_x;
    signed short circle_y = center_y;
   	signed short circle_radius = 50;

    unsigned int ticks = 0;
    short change = 0;

    while (1) {
        if(xSemaphoreTake(ScreenLock1, 1000) == pdTRUE) {
            for(int counter=0; counter < 100; counter++){
                tumEventFetchEvents();
                xGetButtonInput();
                vCheckStateInput();
                vCheckInput();
                
                if (ticks >= 25){
                    change = 1;
                }
                else{
                    change = 0;
                }
                if (ticks == 50){
                    ticks = 0;
                }

                if(change){
                    tumDrawClear(White);
                }
                else{
                    tumDrawClear(White);
                    tumDrawCircle(circle_x, circle_y, circle_radius,
                                    TUMBlue);
                }
                vDrawFPS();

                tumDrawUpdateScreen(); // Refresh screen 
                ticks++;

                vTaskDelay(10); // sleep of 10ms
            }
            xSemaphoreGive(ScreenLock1);
        }
        vTaskDelay(100);
    }
}

void vTask_frequ2(void *pvParameters)
{   
    tumDrawBindThread();

    signed short center_x = SCREEN_WIDTH/2;
    signed short center_y = SCREEN_HEIGHT/2;

    signed short circle_x = center_x;
    signed short circle_y = center_y;
   	signed short circle_radius = 50;

    unsigned int ticks = 0;
    int counter = 0;
    short change = 0;

    while (1) {
        if(xSemaphoreTake(ScreenLock1, 1000) == pdTRUE) {
            for(counter=0; counter < 100; counter++){
                tumEventFetchEvents();
                xGetButtonInput();
                vCheckStateInput();
                vCheckInput();

                if (ticks >= 50){
                    change = 1;
                }
                else{
                    change = 0;
                }
                if (ticks == 100){
                    ticks = 0;
                }

                if(change){
                    tumDrawClear(White);
                }
                else{
                    tumDrawClear(White);
                    tumDrawCircle(circle_x, circle_y, circle_radius,
                                    TUMBlue);
                }
                vDrawFPS();

                tumDrawUpdateScreen(); // Refresh screen 
                ticks++;

                vTaskDelay(10); // sleep of 10ms
            }
            xSemaphoreGive(ScreenLock1);
        }
        vTaskDelay(100);
    }
}

void vTask3 (void *pvParameters) {

    static char counter_t_str[100];
    static int counter_t_str_width = 0;

    tumDrawBindThread();

    while(1) {
        if(xSemaphoreTake(task3_signal, portMAX_DELAY)){
            if(xSemaphoreTake(counter_T_lock, 0)) {
                counter_T++;
                xSemaphoreGive(counter_T_lock);
            }
            for (int i=0; i < 10; i++) {
                tumDrawClear(White);
                sprintf(counter_t_str, 
                        "(T) was pressed %i times.", counter_T);
                
                if (!tumGetTextSize((char *)counter_t_str,
                                    &counter_t_str_width, NULL)) {
                    tumDrawText(counter_t_str,
                                SCREEN_WIDTH / 2 -
                                counter_t_str_width / 2,
                                SCREEN_HEIGHT / 2 - DEFAULT_FONT_SIZE / 2 + 150,
                                TUMBlue);
                }
                vDrawFPS();

                tumDrawUpdateScreen();
                vTaskDelay(10);
            }
        }
    }
}

void vTask4 (void *pvParameters) {

    static char counter_f_str[100];
    static int counter_f_str_width = 0;

    tumDrawBindThread();

    while(1) {
        if(ulTaskNotifyTake(pdTRUE, portMAX_DELAY)) {
            if(xSemaphoreTake(counter_F_lock, 0)) {
                counter_F++;
                xSemaphoreGive(counter_F_lock);
            }
            for (int i=0; i < 10; i++) {
                tumDrawClear(White);
                sprintf(counter_f_str, 
                        "(F) was pressed %i times.", counter_F);
                
                if (!tumGetTextSize((char *)counter_f_str,
                                    &counter_f_str_width, NULL)) {
                    tumDrawText(counter_f_str,
                                SCREEN_WIDTH / 2 -
                                counter_f_str_width / 2,
                                SCREEN_HEIGHT / 2 - DEFAULT_FONT_SIZE / 2 + 175,
                                TUMBlue);
                }
                vDrawFPS();

                tumDrawUpdateScreen();
                vTaskDelay(10);
            }
        }
    }
}

void vTimerCallbackReset( TimerHandle_t xTimer )
{
    if (xTimerReset(xTimer, 0) == pdPASS){
        xTaskNotifyGive(timer_task);
    }
    else{
        printf("failed to reset timer.");
    }
}

void vResetCounterTask (void *pvParameter) {
    
    while(1) {
        if (ulTaskNotifyTake(pdTRUE, portMAX_DELAY)){
            // reset variables
            if(xSemaphoreTake(counter_T_lock, 0)){
                counter_T = 0;
                printf("T reset done.\n");
                xSemaphoreGive(counter_T_lock);
            }
            if(xSemaphoreTake(counter_F_lock, 0)){
                counter_F = 0;
                printf("F reset done.\n");
                xSemaphoreGive(counter_F_lock);
            }
        }

    }
}

void vTaskControlMechanisms (void *pvParameter) {
    
    int counter = 0;
    int ticks = 0;

    while(1) {
        ticks++;
        if(ticks == 100) {
            counter++;
            ticks = 0;
            printf("counter:%i\n", counter);
        }
        vTaskDelay(10);
    }
}


// main function ################################

int main(int argc, char *argv[])
{
    // initialization 
    char *bin_folder_path = tumUtilGetBinFolderPath(argv[0]);

    printf("Initializing: ");

    if (tumDrawInit(bin_folder_path)) {
        PRINT_ERROR("Failed to initialize drawing");
        goto err_init_drawing;
    }

    if (tumEventInit()) {
        PRINT_ERROR("Failed to initialize events");
        goto err_init_events;
    }

    if (tumSoundInit(bin_folder_path)) {
        PRINT_ERROR("Failed to initialize audio");
        goto err_init_audio;
    }

    // creation of semaphores 
    // locking mechanism for buttons
    buttons.lock = xSemaphoreCreateMutex(); 
    if (!buttons.lock) {
        PRINT_ERROR("Failed to create buttons lock");
        goto err_buttons_lock;
    }
    
    ScreenLock1 = xSemaphoreCreateMutex(); // lock for screen 1
    ScreenLock2 = xSemaphoreCreateMutex(); // lock for screen 2

    // signal for triggering state machine.
    state_machine_signal = xSemaphoreCreateBinary();

    task3_signal = xSemaphoreCreateBinary();

    counter_T_lock = xSemaphoreCreateMutex();
    counter_F_lock = xSemaphoreCreateMutex();

    xResetTimer = xTimerCreate("timer", pdMS_TO_TICKS(15000), pdTRUE, 
                                (void*) 0, vTimerCallbackReset);
    xTimerStart(xResetTimer, 0);

    // Task Creation ##############################

    xTaskCreate(vTerminateProcess, "", 
                mainGENERIC_STACK_SIZE * 2,
                NULL, (configMAX_PRIORITIES - 1), &terminate_process);

    xTaskCreate(vSequential_StateMachine, "state machine", 
                mainGENERIC_STACK_SIZE * 2,
                NULL, (configMAX_PRIORITIES - 1), &state_machine);

    xTaskCreate(vTask_frequ1, "", 
                mainGENERIC_STACK_SIZE * 2, 
                NULL, mainGENERIC_PRIORITY + 2, &task_frequ1);

    task_frequ2 = xTaskCreateStatic(vTask_frequ2, "",
                                    mainGENERIC_STACK_SIZE * 2,
                                    NULL, (mainGENERIC_PRIORITY + 3),
                                    xStack, &xTaskBuffer);

    xTaskCreate(vTask3, "", mainGENERIC_STACK_SIZE * 2,
                NULL, (mainGENERIC_PRIORITY + 1), &task3);

    xTaskCreate(vTask4, "", mainGENERIC_STACK_SIZE * 2,
                NULL, (mainGENERIC_PRIORITY + 1), &task4);

    xTaskCreate(vResetCounterTask, "Reset Counters", 
                mainGENERIC_STACK_SIZE * 2,
                NULL, (mainGENERIC_PRIORITY + 1), &timer_task);

    xTaskCreate(vTaskControlMechanisms, "",
                mainGENERIC_STACK_SIZE * 2,
                NULL, (mainGENERIC_PRIORITY + 1), &control_task);

    xTaskCreate(vTask1_screen2, "", 
                mainGENERIC_STACK_SIZE * 2,
                NULL, mainGENERIC_PRIORITY, &task1_screen2);

    // End of Task Creation #########################

    vTaskStartScheduler();

    return EXIT_SUCCESS;

err_buttons_lock:
    tumSoundExit();
err_init_audio:
    tumEventExit();
err_init_events:
    tumDrawExit();
err_init_drawing:
    return EXIT_FAILURE;
}

// ###########################################


// cppcheck-suppress unusedFunction
__attribute__((unused)) void vMainQueueSendPassed(void)
{
    /* This is just an example implementation of the "queue send" trace hook. */
}

// cppcheck-suppress unusedFunction
__attribute__((unused)) void vApplicationIdleHook(void)
{
#ifdef __GCC_POSIX__
    struct timespec xTimeToSleep, xTimeSlept;
    /* Makes the process more agreeable when using the Posix simulator. */
    xTimeToSleep.tv_sec = 1;
    xTimeToSleep.tv_nsec = 0;
    nanosleep(&xTimeToSleep, &xTimeSlept);
#endif
}
