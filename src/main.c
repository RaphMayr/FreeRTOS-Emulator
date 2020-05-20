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
static TaskHandle_t task3 = NULL;
static TaskHandle_t task4 = NULL;

static SemaphoreHandle_t ScreenLock = NULL;
static SemaphoreHandle_t task3Lock = NULL;

static void vTask4 (void *pvParameters);

static StaticTask_t xTaskBuffer;
static StackType_t xStack[mainGENERIC_STACK_SIZE * 2];

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

// Tasks #######################################

void vTask_frequ1(void *pvParameters)
{   
    TickType_t xLastWakeTime;
    const TickType_t frameratePeriod = 20;
    
    xLastWakeTime = xTaskGetTickCount();

    tumDrawBindThread();

    signed short center_x = SCREEN_WIDTH/2;
    signed short center_y = SCREEN_HEIGHT/2;

    signed short circle_x = center_x;
    signed short circle_y = center_y;
   	signed short circle_radius = 50;

    unsigned short ticks = 0;
    short change = 0;

    while (1) {
        tumEventFetchEvents();
        xGetButtonInput();

        if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
            if (buttons.buttons[KEYCODE(Q)]) {
                exit(EXIT_SUCCESS);
        xSemaphoreGive(buttons.lock);
        }
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

        // locking screen to draw
        if (xSemaphoreTake(ScreenLock, portMAX_DELAY) == pdTRUE) {

            if(change){
                tumDrawClear(White);
            }
            else{
                tumDrawClear(White);
                tumDrawCircle(circle_x, circle_y, circle_radius,
                                TUMBlue);
            }
            vDrawFPS();

            xSemaphoreGive(ScreenLock);
        }

        tumDrawUpdateScreen(); // Refresh screen 

        ticks++;
        vTaskDelayUntil(&xLastWakeTime, 
                        pdMS_TO_TICKS(frameratePeriod));
    }
}

void vTask_frequ2(void *pvParameters)
{   
    TickType_t xLastWakeTime;
    const TickType_t frameratePeriod = 10;
    
    xLastWakeTime = xTaskGetTickCount();

    tumDrawBindThread();

    signed short center_x = SCREEN_WIDTH/2;
    signed short center_y = SCREEN_HEIGHT/2;

    signed short circle_x = center_x;
    signed short circle_y = center_y;
   	signed short circle_radius = 50;

    unsigned int ticks = 0;
    short change = 0;

    //static BaseType_t xHigherPriorityTaskWoken;
    //xHigherPriorityTaskWoken = pdFALSE; 

    while (1) {
        tumEventFetchEvents();
        xGetButtonInput();
        
        if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
            if (buttons.buttons[KEYCODE(Q)]) {
                exit(EXIT_SUCCESS);
            }
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

        // locking screen to draw
        if (xSemaphoreTake(ScreenLock, portMAX_DELAY) == pdTRUE) {

            if(change){
                tumDrawClear(White);
            }
            else{
                tumDrawClear(White);
                tumDrawCircle(circle_x, circle_y, circle_radius,
                                TUMBlue);
            }
            vDrawFPS();

            xSemaphoreGive(ScreenLock);
        }

        tumDrawUpdateScreen(); // Refresh screen 

        ticks++;
        vTaskDelayUntil(&xLastWakeTime, 
                        pdMS_TO_TICKS(frameratePeriod));
    }
}
/*
void vTask3(void *pvParameters)
{
    TickType_t xLastWakeTime;
    const TickType_t frameratePeriod = 10;
    xLastWakeTime = xTaskGetTickCount();

    static char t_count_str[100];
    static int t_count_str_width = 0;
    
    tumDrawBindThread();

    task3Lock = xSemaphoreCreateBinary();

    while(1) {
        if (xSemaphoreTake (task3Lock, portMAX_DELAY) == pdTRUE) {
            tumEventFetchEvents();
            xGetButtonInput();

            sprintf(t_count_str, "Pressed T (i) times.");
            if (!tumGetTextSize((char *)t_count_str,
                            &t_count_str_width, NULL))
            
        
            // locking screen to draw
            if (xSemaphoreTake(ScreenLock, portMAX_DELAY) == pdTRUE) {
                
                //display how many times letter T was pressed
                tumDrawText(t_count_str, 
                        SCREEN_WIDTH / 2 -
                        t_count_str_width / 2, 
                        SCREEN_HEIGHT / 2 - DEFAULT_FONT_SIZE / 2 + 150,
                        Black);
                tumDrawClear(White);

                xSemaphoreGive(ScreenLock);
            }


            tumDrawUpdateScreen(); // Refresh screen 

            vTaskDelayUntil(&xLastWakeTime, 
                            pdMS_TO_TICKS(frameratePeriod));
        }
    }

}

void vTask4(void *pvParameters)
{
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
 
}
*/
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

    if (xTaskCreate(vTask_frequ1, "Draw Circle with 1Hz", mainGENERIC_STACK_SIZE * 2, 
                    NULL, (configMAX_PRIORITIES - 2), &task_frequ1) != pdPASS) {
        goto err_task_frequ1;
    }
    xTaskCreateStatic(vTask_frequ2, "Draw Circle with 2Hz", mainGENERIC_STACK_SIZE * 2,
                        NULL, (configMAX_PRIORITIES - 1), xStack, &xTaskBuffer);

    

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
