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
#define SCREEN_WIDTH 640 
#define SCREEN_HEIGTH 480

static TaskHandle_t DemoTask = NULL;

extern SDL_Window *window;

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

void vMoveWindowTask(void *pvParameters)
{   
    // vars for Mouse coordinates
    signed short mouse_X;
    signed short mouse_Y;
    
    // vars for displaying text on window
    static char x_position_str[100];
    static int x_position_strwidth = 0;

    static char y_position_str[100];
    static int y_position_strwidth = 0;

    static char quit_str[100];
    static int quit_strwidth = 0;

    // vars for retrieving window coordinates
    int *x = NULL;
    int *y = NULL;
    int x_coord, y_coord;
    x = &x_coord;
    y = &y_coord;

    // vars for new coordinates
    int x_new, y_new = 0;

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

        // gets window position and prints it to console
        SDL_GetWindowPosition(window, x, y);
        printf("x: %i y: %i \n", x_coord, y_coord);

        tumDrawClear(White); // Clear screen

        // gets current mouse coordinates relative 
        // to upper left corner of window
        mouse_X = tumEventGetMouseX();
        mouse_Y = tumEventGetMouseY();

        // moving coordinate origin of mouse coord. to center of window
        mouse_X = mouse_X - SCREEN_WIDTH / 2;
        mouse_Y = mouse_Y - SCREEN_HEIGTH / 2;

        // moving coordinate origin of window coord. to center of window
        x_coord = x_coord - SCREEN_WIDTH / 2;
        y_coord = y_coord - SCREEN_HEIGTH / 2;

        // formatting and setting text for X and Y position of Mouse
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

        // calculating new coordinates relative to old ones
        x_new = x_coord + mouse_X;
        y_new = y_coord + mouse_Y;

        // moving coordinate origin back to upper left corner of window
        x_new = x_new + SCREEN_WIDTH / 2;
        y_new = y_new + SCREEN_HEIGTH / 2;

        // constraining moving space of window (Res. 1920x1080 px.)
        if(x_new >= 1060){
            x_new = 1060;
        }
        if(x_new <= 220){
            x_new = 220;
        }
        if(y_new >= 420){
            y_new = 420;
        }
        if(y_new <= 180){
            y_new = 180;
        }
        
        SDL_SetWindowPosition(window, x_new, y_new); // update window position
        tumDrawUpdateScreen(); // Refresh window 

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

    if (xTaskCreate(vMoveWindowTask, "MoveWindowTask", mainGENERIC_STACK_SIZE * 2, NULL,
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
