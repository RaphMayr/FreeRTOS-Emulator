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

static TaskHandle_t DemoTask = NULL;

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

void vDemoTask(void *pvParameters)
{
    signed short mouse_X;
    signed short mouse_Y;

    static char x_position_str[100];
    static int x_position_strwidth = 0;

    static char y_position_str[100];
    static int y_position_strwidth = 0;

    static char quit_str[100];
    static int quit_strwidth = 0;

    // Needed such that Gfx library knows which thread controlls drawing
    // Only one thread can call tumDrawUpdateScreen while and thread can call
    // the drawing functions to draw objects. This is a limitation of the SDL
    // backend.
    tumDrawBindThread();

    while (1) {
        tumEventFetchEvents(); // Query events backend for new events, ie. button presses
        xGetButtonInput(); // Update global input

        // `buttons` is a global shared variable and as such needs to be
        // guarded with a mutex, mutex must be obtained before accessing the
        // resource and given back when you're finished. If the mutex is not
        // given back then no other task can access the reseource.
        if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
            if (buttons.buttons[KEYCODE(
                                    Q)]) { // Equiv to SDL_SCANCODE_Q
                exit(EXIT_SUCCESS);
            }
        }
        xSemaphoreGive(buttons.lock);

        tumDrawClear(White); // Clear screen

        mouse_X = tumEventGetMouseX();
        mouse_Y = tumEventGetMouseY();

        sprintf(x_position_str, "X Position of Mouse: %i", mouse_X);
        sprintf(y_position_str, "Y Position of Mouse: %i", mouse_Y);
        sprintf(quit_str, "press (q) to quit.");

        if (!tumGetTextSize((char *)x_position_str,
                            &x_position_strwidth, NULL))
            tumDrawText(x_position_str,
                        SCREEN_WIDTH / 2 -
                        x_position_strwidth / 2,
                        SCREEN_HEIGHT / 2 - DEFAULT_FONT_SIZE / 2 - 50,
                        TUMBlue);
        
        if (!tumGetTextSize((char *)y_position_str,
                            &y_position_strwidth, NULL))
            tumDrawText(y_position_str,
                        SCREEN_WIDTH / 2 -
                        y_position_strwidth / 2,
                        SCREEN_HEIGHT / 2 - DEFAULT_FONT_SIZE / 2 + 50,
                        TUMBlue);

        if (!tumGetTextSize((char *)quit_str,
                            &quit_strwidth, NULL))
            tumDrawText(quit_str,
                        SCREEN_WIDTH / 2 -
                        quit_strwidth / 2,
                        SCREEN_HEIGHT / 2 - DEFAULT_FONT_SIZE / 2 + 100,
                        TUMBlue);

        
        tumDrawUpdateScreen(); // Refresh the screen to draw string

        // Basic sleep of 20 milliseconds
        vTaskDelay((TickType_t)20);
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

    if (xTaskCreate(vDemoTask, "DemoTask", mainGENERIC_STACK_SIZE * 2, NULL,
                    mainGENERIC_PRIORITY, &DemoTask) != pdPASS) {
        goto err_demotask;
    }

    vTaskStartScheduler();

    return EXIT_SUCCESS;

err_demotask:
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
