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

void vdrawFigures(void *pvParameters)
{
    // Needed such that Gfx library knows which thread controlls drawing
    // Only one thread can call tumDrawUpdateScreen while and thread can call
    // the drawing functions to draw objects. This is a limitation of the SDL
    // backend.
    tumDrawBindThread();

    //define center of screen
    signed short center_x = SCREEN_WIDTH/2;
    signed short center_y = SCREEN_HEIGHT/2;

    //initial coordinate definition for circle
    signed short circle_x = center_x - 100;
    signed short circle_y = center_y;
   	signed short circle_radius = 20;
    //angle for circle rotation
    unsigned short circle_angle = 180;

    //initial coordinate definition for triangle 
    coord_t upper_t;
    coord_t lower_l_t;
    coord_t lower_r_t;

    upper_t.x = center_x;
    upper_t.y = center_y - 20;

    lower_l_t.x = center_x - 20;
    lower_l_t.y = center_y + 20;

    lower_r_t.x = center_x + 20;
    lower_r_t.y = center_y + 20;

    coord_t coord_triangle[2];
    coord_triangle[0] = upper_t;
    coord_triangle[1] = lower_l_t;
    coord_triangle[2] = lower_r_t;

    //initial coordinate definition for Square
    signed short box_x = center_x + 100 - 20;
    signed short box_y = center_y - 20;
    signed short box_width = 40;
    signed short box_height = 40;

    //Angle for square rotation
    unsigned short square_angle = 0;

    while (1) {
        tumEventFetchEvents(); // Query events backend for new events, ie. button presses
        xGetButtonInput(); // Update global input

        // `buttons` is a global shared variable and as such needs to be
        // guarded with a mutex, mutex must be obtained before accessing the
        // resource and given back when you're finished. If the mutex is not
        // given back then no other task can access the reseource.
        if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
            if (buttons.buttons[KEYCODE(Q
                            )]) { 
                // Equiv to SDL_SCANCODE_Q
                exit(EXIT_SUCCESS);
            }
            xSemaphoreGive(buttons.lock);
        }
        

        tumDrawClear(White); // Clear screen

        //draw Triangle in Center
        tumDrawTriangle(coord_triangle, Red);

        //update coordinates of circle
        circle_angle--;
        circle_x = center_x + 100*cos(circle_angle);
        circle_y = center_y + 100*sin(circle_angle);

        tumDrawCircle(circle_x,circle_y,circle_radius,TUMBlue); // Draw Circle
        
        //update coordiantes of Square
        square_angle++;
        box_x = center_x - 20 + 100*cos(square_angle);
        box_y = center_y + 100*sin(square_angle);

        tumDrawFilledBox(box_x, box_y, box_width, box_height, Green);
        
        tumDrawUpdateScreen(); // Refresh the screen
        
        // Basic sleep of 1000 milliseconds
        vTaskDelay(
            (TickType_t)100);
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

    if (xTaskCreate(vdrawFigures, "drawFigures", mainGENERIC_STACK_SIZE * 2, NULL,
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
