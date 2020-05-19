#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <inttypes.h>

#include <SDL2/SDL_scancode.h>
#include <SDL2/SDL.h>

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

TaskHandle_t task_frequ1 = NULL;
TaskHandle_t task_frequ2 = NULL;

StaticTask_t xTaskBuffer;
StackType_t xStack[mainGENERIC_STACK_SIZE];

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

#define KEYCODE(CHAR) SDL_SCANCODE_##CHAR

void vTask_frequ1(void *pvParameters)   //frequency at 1 Hz
{   

    signed short center_x = SCREEN_WIDTH/2;
    signed short center_y = SCREEN_HEIGHT/2;

    signed short circle_x = center_x;
    signed short circle_y = center_y;
   	signed short circle_radius = 50;

    tumDrawBindThread();

    while (1) {
        tumEventFetchEvents(); // Query events backend for new events, ie. button presses

        tumDrawClear(White); // Clear screen
        tumDrawUpdateScreen();
        vTaskDelay((TickType_t)500);


        tumDrawCircle(circle_x,circle_y,circle_radius,TUMBlue); // Draw Circle
        tumDrawUpdateScreen();
        vTaskDelay((TickType_t)500);
    }
}

void vTask_frequ2(void *pvParameters)   //frequency at 2 Hz
{   

    signed short center_x = SCREEN_WIDTH/2;
    signed short center_y = SCREEN_HEIGHT/2;

    signed short circle_x = center_x;
    signed short circle_y = center_y;
   	signed short circle_radius = 50;
    
    tumDrawBindThread();

    while (1) {
        tumEventFetchEvents(); // Query events backend for new events, ie. button presses

        tumDrawClear(White); // Clear screen
        tumDrawUpdateScreen();
        vTaskDelay((TickType_t)250);


        tumDrawCircle(circle_x,circle_y,circle_radius,TUMBlue); // Draw Circle
        tumDrawUpdateScreen();
        vTaskDelay((TickType_t)250);
    }
}


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

    if (xTaskCreate(vTask_frequ1, "Task with 1Hz", mainGENERIC_STACK_SIZE * 2, NULL,
                    mainGENERIC_PRIORITY, &task_frequ1) != pdPASS) {
        goto err_task_frequ1;
    }
    if (xTaskCreateStatic(vTask_frequ2, "Task with 2Hz", 20 * 2, NULL,
                    mainGENERIC_PRIORITY, xStack, &xTaskBuffer) != pdPASS) {
        goto err_task_frequ2;
    }
    
    vTaskStartScheduler();

    return EXIT_SUCCESS;

err_task_frequ1: 
    vSemaphoreDelete(buttons.lock);
err_task_frequ2:
    vTaskDelete(vTask_frequ2);
err_buttons_lock:
    tumSoundExit();
err_init_audio:
    tumEventExit();
err_init_events:
    tumDrawExit();
err_init_drawing:
    return EXIT_FAILURE;
}

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
