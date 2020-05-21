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

static TaskHandle_t task_frequ1 = NULL;
static TaskHandle_t task4 = NULL;

static SemaphoreHandle_t ScreenLock = NULL;
static SemaphoreHandle_t DrawSignal = NULL;

static SemaphoreHandle_t task3_signal = NULL;

static StaticTask_t xTaskBuffer;
static StackType_t xStack[mainGENERIC_STACK_SIZE];

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

void vCheckInput(void) {

    if (buttons.buttons[KEYCODE(Q)]) {
         exit(EXIT_SUCCESS);
    }

    // debounce structure for T and F
    // (T) for triggering Task 3
    int reading_T = buttons.buttons[KEYCODE(T)];
    // (F) for triggering Task 4
    int reading_F = buttons.buttons[KEYCODE(F)];
    
    if (reading_T) {
        xSemaphoreGive(task3_signal);
        vTaskDelay(750);
    }

    if(reading_F){
        xTaskNotifyGive(task4);
        vTaskDelay(750);
    }
}

// Tasks #######################################

void vTask_frequ1(void *pvParameters)
{   
    tumDrawBindThread();

    signed short center_x = SCREEN_WIDTH/2;
    signed short center_y = SCREEN_HEIGHT/2;

    signed short circle_x = center_x;
    signed short circle_y = center_y;
   	signed short circle_radius = 50;

    unsigned short ticks = 0;
    int counter = 0;
    short change = 0;

    while (1) {
        if (xSemaphoreTake(DrawSignal, 1000) == pdTRUE){
            for (counter=0; counter<1000; counter++) {
                tumEventFetchEvents();
                xGetButtonInput();

                if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
                    vCheckInput();
                    xSemaphoreGive(buttons.lock);
                }
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

                vTaskDelay(10);
            }
            xSemaphoreGive(DrawSignal);
        }
        vTaskDelay(100);
        printf("task 1 waiting...\n");
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
        if(xSemaphoreTake(DrawSignal, 1000) == pdTRUE) {
            for(counter=0; counter < 1000; counter++){
                tumEventFetchEvents();
                xGetButtonInput();
                
                if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
                    vCheckInput();
                    xSemaphoreGive(buttons.lock);
                }

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
            xSemaphoreGive(DrawSignal);
        }
        vTaskDelay(100);
        printf("task 2 waiting...\n");
    }
}

void vTask3 (void *pvParameters) {
    // counter needs to be global
    int counter_t = 0;

    static char counter_t_str[100];
    static int counter_t_str_width = 0;

    tumDrawBindThread();

    while(1) {
        if(xSemaphoreTake(task3_signal, portMAX_DELAY)){
            
            counter_t++;
            for (int i=0; i < 10; i++) {
                tumDrawClear(White);
                sprintf(counter_t_str, 
                        "(T) was pressed %i times.", counter_t);
                
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

    int counter_f = 0;

    static char counter_f_str[100];
    static int counter_f_str_width = 0;

    tumDrawBindThread();

    while(1) {
        if(ulTaskNotifyTake(pdTRUE, portMAX_DELAY)) {

            counter_f++;
            for (int i=0; i < 10; i++) {
                tumDrawClear(White);
                sprintf(counter_f_str, 
                        "(F) was pressed %i times.", counter_f);
                
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


// main function ################################

int main(int argc, char *argv[])
{
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

    buttons.lock = xSemaphoreCreateMutex(); // Locking mechanism
    if (!buttons.lock) {
        PRINT_ERROR("Failed to create buttons lock");
        goto err_buttons_lock;
    }

    ScreenLock = xSemaphoreCreateMutex();
    if (!ScreenLock){
        PRINT_ERROR("Failed to create Screen lock");
    }

    DrawSignal = xSemaphoreCreateMutex();
    if (!DrawSignal){
        PRINT_ERROR("Failed to create Draw Signal");
    }

    // Binary Semaphore for triggering task 3
    vSemaphoreCreateBinary(task3_signal);
    xSemaphoreTake(task3_signal, 0);

    if (xTaskCreate(vTask_frequ1, "Draw Circle with 1Hz", mainGENERIC_STACK_SIZE * 2, 
                    NULL, (configMAX_PRIORITIES - 2), &task_frequ1) != pdPASS) {
        goto err_task_frequ1;
    }
    xTaskCreateStatic(vTask_frequ2, "Draw Circle with 2Hz", mainGENERIC_STACK_SIZE,
                        NULL, (configMAX_PRIORITIES - 3), xStack, &xTaskBuffer);

    xTaskCreate(vTask3, "Task3", mainGENERIC_STACK_SIZE,
                NULL, (configMAX_PRIORITIES - 1), NULL);

    xTaskCreate(vTask4, "Task4", mainGENERIC_STACK_SIZE,
                NULL, (configMAX_PRIORITIES - 1), &task4);

    vTaskStartScheduler();

    return EXIT_SUCCESS;

err_task_frequ1:
    vSemaphoreDelete(buttons.lock);
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
