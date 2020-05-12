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
    unsigned short A_count = 0;
    unsigned short B_count = 0;
    unsigned short C_count = 0;
    unsigned short D_count = 0;

    static char A_string[50];
    static int A_string_width = 0;

    static char B_string[50];
    static int B_string_width = 0;

    static char C_string[50];
    static int C_string_width = 0;

    static char D_string[50];
    static int D_string_width = 0;

    static char End_string[100];
    static int End_string_width = 0;

    //debouncing declarations for A
    int buttonState_A = 0;
    int lastButtonState_A = 0;
    clock_t lastDebounceTime_A;
    
    int buttonState_B = 0;
    int lastButtonState_B = 0;
    clock_t lastDebounceTime_B;

    int buttonState_C = 0;
    int lastButtonState_C = 0;
    clock_t lastDebounceTime_C;

    int buttonState_D = 0;
    int lastButtonState_D = 0;
    clock_t lastDebounceTime_D;

    clock_t timestamp;
    double debounce_delay = 0.025;

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
            if (buttons.buttons[KEYCODE(Q)]) { // Equiv to SDL_SCANCODE_Q
                exit(EXIT_SUCCESS);
            }
            //Get input values for A, B, C, D
            //can be either 0 or 1
            //1 signals pressed
            int reading_A = buttons.buttons[KEYCODE(A)];
            int reading_B = buttons.buttons[KEYCODE(B)];
            int reading_C = buttons.buttons[KEYCODE(C)];
            int reading_D = buttons.buttons[KEYCODE(D)];

            //Debouncing structure for A
            if(reading_A != lastButtonState_A){
                lastDebounceTime_A = clock();
            }
            timestamp = clock();
            if((((double) (timestamp - lastDebounceTime_A)
                    )/ CLOCKS_PER_SEC) > debounce_delay){
                if (reading_A != buttonState_A){
                    buttonState_A = reading_A;
                    if (buttonState_A == 1){
                        A_count++;
                    }
                }
            }
            lastButtonState_A = reading_A;

            //Debouncing structure for B 
            if(reading_B != lastButtonState_B){
                lastDebounceTime_B = clock();
            }
            timestamp = clock();
            if((((double) (timestamp - lastDebounceTime_B)
                    )/ CLOCKS_PER_SEC) > debounce_delay){
                if (reading_B != buttonState_B){
                    buttonState_B = reading_B;
                    if (buttonState_B == 1){
                        B_count++;
                    }
                }
            }
            lastButtonState_B = reading_B;

            //Debouncing structure for C
            if(reading_C != lastButtonState_C){
                lastDebounceTime_C = clock();
            }
            timestamp = clock();
            if((((double) (timestamp - lastDebounceTime_C)
                    )/ CLOCKS_PER_SEC) > debounce_delay){
                if (reading_C != buttonState_C){
                    buttonState_C = reading_C;
                    if (buttonState_C == 1){
                        C_count++;
                    }
                }
            }
            lastButtonState_C = reading_C;

            //Debouncing structure for D
            if(reading_D != lastButtonState_D){
                lastDebounceTime_D = clock();
            }
            timestamp = clock();
            if((((double) (timestamp - lastDebounceTime_D)
                    )/ CLOCKS_PER_SEC) > debounce_delay){
                if (reading_D != buttonState_D){
                    buttonState_D = reading_D;
                    if (buttonState_D == 1){
                        D_count++;
                    }
                }
            }
            lastButtonState_D = reading_D;
        }
        xSemaphoreGive(buttons.lock);

        tumDrawClear(White); // Clear screen

        //format strings
        sprintf(A_string, "A was pressed %i times.", A_count);
        sprintf(B_string, "B was pressed %i times.", B_count);
        sprintf(C_string, "C was pressed %i times.", C_count);
        sprintf(D_string, "D was pressed %i times.", D_count);
        sprintf(End_string, "press (q) to end program.");

        if (!tumGetTextSize((char *)A_string,
                            &A_string_width, NULL))
            tumDrawText(A_string,
                        SCREEN_WIDTH / 2 -
                        A_string_width / 2,
                        SCREEN_HEIGHT / 2 - DEFAULT_FONT_SIZE / 2 - 150,
                        TUMBlue);
        
        if (!tumGetTextSize((char *)B_string,
                            &B_string_width, NULL))
            tumDrawText(B_string,
                        SCREEN_WIDTH / 2 -
                        B_string_width / 2,
                        SCREEN_HEIGHT / 2 - DEFAULT_FONT_SIZE / 2 - 50,
                        TUMBlue);
        
        if (!tumGetTextSize((char *)C_string,
                            &C_string_width, NULL))
            tumDrawText(C_string,
                        SCREEN_WIDTH / 2 -
                        C_string_width / 2,
                        SCREEN_HEIGHT / 2 - DEFAULT_FONT_SIZE / 2 + 50,
                        TUMBlue);
        
        if (!tumGetTextSize((char *)D_string,
                            &D_string_width, NULL))
            tumDrawText(D_string,
                        SCREEN_WIDTH / 2 -
                        D_string_width / 2,
                        SCREEN_HEIGHT / 2 - DEFAULT_FONT_SIZE / 2 + 150,
                        TUMBlue);

        if (!tumGetTextSize((char *)A_string,
                            &End_string_width, NULL))
            tumDrawText(End_string,
                        SCREEN_WIDTH / 2 -
                        End_string_width / 2,
                        SCREEN_HEIGHT / 2 - DEFAULT_FONT_SIZE / 2 + 200,
                        TUMBlue);

        if (tumEventGetMouseLeft()){
            A_count = 0;
            B_count = 0;
            C_count = 0;
            D_count = 0;    
        }

        tumDrawUpdateScreen(); // Refresh the screen 
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
