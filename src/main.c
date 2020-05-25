#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <inttypes.h>
#include <stdbool.h>

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

    //define center of screen
    signed short center_x = SCREEN_WIDTH/2;
    signed short center_y = SCREEN_HEIGHT/2;

    // definitions for drawing figures

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

    coord_t coord_triangle[2];

    //initial coordinate definition for Square
    signed short box_x = center_x + 100 - 20;
    signed short box_y = center_y - 60;
    signed short box_width = 40;
    signed short box_height = 40;

    //Angle for square rotation
    unsigned short square_angle = 0;

    //LOWER TEXT
    //structure to store lower text
    static char lower_string[100];
    static int lower_strings_width = 0;

    //UPPER TEXT
    //structure to store upper moving text
    static char upper_string[100];
    static int upper_strings_width = 0;
    //init x-coordinate
    //format string into Char Array
    sprintf(upper_string, "This here needs to move");
    tumGetTextSize((char *)upper_string,
                    &upper_strings_width, NULL);
    signed short upper_x = 640 / 2 - upper_strings_width / 2;
    signed short upper_str_right;
    signed short upper_str_left;
    //initial moving to right side
    bool change_mov_dir = false;
    

    // definitions for drawing button texts

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

    //debouncing declarations
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

    int buttonState_Mouse = 0;
    int lastButtonState_Mouse = 0;
    clock_t lastDebounceTime_Mouse;

    clock_t timestamp;
    double debounce_delay = 0.025;


    // vars for Mouse coordinates
    signed short mouse_X;
    signed short mouse_Y;

    signed short prev_mouse_X = 0;
    signed short prev_mouse_Y = 0;

    int ticks = 0;

    tumDrawBindThread();

    while (1) {
        tumEventFetchEvents(); // Query events backend for new events, ie. button presses
        xGetButtonInput(); // Update global input

        if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
            if (buttons.buttons[KEYCODE(Q
                            )]) { 
                // Equiv to SDL_SCANCODE_Q
                exit(EXIT_SUCCESS);
            }
            xSemaphoreGive(buttons.lock);
        }

        // debounce structure for buttons A - D

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

        //format strings
        sprintf(A_string, "A was pressed %i times.", A_count);
        sprintf(B_string, "B was pressed %i times.", B_count);
        sprintf(C_string, "C was pressed %i times.", C_count);
        sprintf(D_string, "D was pressed %i times.", D_count);
        

        tumDrawClear(White); // Clear screen

        // draw strings of button presses

        if (!tumGetTextSize((char *)A_string,
                            &A_string_width, NULL))
            tumDrawText(A_string,
                        center_x -
                        A_string_width / 2,
                        center_y - DEFAULT_FONT_SIZE / 2 + 110,
                        TUMBlue);
        
        if (!tumGetTextSize((char *)B_string,
                            &B_string_width, NULL))
            tumDrawText(B_string,
                        center_x -
                        B_string_width / 2,
                        center_y - DEFAULT_FONT_SIZE / 2 + 130,
                        TUMBlue);
        
        if (!tumGetTextSize((char *)C_string,
                            &C_string_width, NULL))
            tumDrawText(C_string,
                        center_x -
                        C_string_width / 2,
                        center_y - DEFAULT_FONT_SIZE / 2 + 150,
                        TUMBlue);
        
        if (!tumGetTextSize((char *)D_string,
                            &D_string_width, NULL))
            tumDrawText(D_string,
                        center_x -
                        D_string_width / 2,
                        center_y - DEFAULT_FONT_SIZE / 2 + 170,
                        TUMBlue);


        // debounce structure for mouse button for resetting counter values

        int reading_Mouse = tumEventGetMouseLeft();
        if(reading_Mouse != lastButtonState_Mouse){
            lastDebounceTime_Mouse = clock();
        }
        timestamp = clock();
        if((((double) (timestamp - lastDebounceTime_Mouse)
                ) / CLOCKS_PER_SEC) > debounce_delay){
            if (reading_Mouse != buttonState_Mouse){
                    buttonState_Mouse = reading_Mouse;
                if(buttonState_Mouse == 1){
                    A_count = 0;
                    B_count = 0;
                    C_count = 0;
                    D_count = 0; 
                }
            }
        }
        lastButtonState_Mouse = reading_Mouse; 

        upper_t.x = center_x;
        upper_t.y = center_y - 80;

        lower_l_t.x = center_x - 20;
        lower_l_t.y = center_y - 40;

        lower_r_t.x = center_x + 20;
        lower_r_t.y = center_y - 40;

        coord_triangle[0] = upper_t;
        coord_triangle[1] = lower_l_t;
        coord_triangle[2] = lower_r_t;

        //draw Triangle in Center
        tumDrawTriangle(coord_triangle, Red);

        //update coordinates of circle
        circle_angle--;
        circle_x = center_x + 100*cos(circle_angle);
        circle_y = center_y - 60 + 100*sin(circle_angle);

        //draw Circle
        tumDrawCircle(circle_x,circle_y,circle_radius,TUMBlue); // Draw Circle
        
        //update coordiantes of Square
        square_angle++;
        box_x = center_x - 20 + 100*cos(square_angle);
        box_y = center_y - 60 + 100*sin(square_angle);

        //draw Square
        tumDrawFilledBox(box_x, box_y, box_width, box_height, Green);

        //draw lower TEXT
        //format string into char array
        sprintf(lower_string, "Random; q to exit.");
        tumGetTextSize((char *)lower_string,
                    &lower_strings_width, NULL);
        tumDrawText(lower_string, 
                    center_x -
                    lower_strings_width / 2, 
                    center_y - DEFAULT_FONT_SIZE / 2 + 80,
                    Black);

        //draw upper moving TEXT
        tumDrawText(upper_string, 
                    upper_x, 
                    center_y - DEFAULT_FONT_SIZE / 2 - 200,
                    Black);

        //calculate left and right constraint of text box
        upper_str_right = upper_x + upper_strings_width;
        upper_str_left = upper_x;
        //conditional change in direction
        if(change_mov_dir){
            upper_x = upper_x - 5;
        }
        else{
            upper_x = upper_x + 5;
        }
        if(upper_str_right >= 450){     //collision on right side
            change_mov_dir = true;
        }
        if(upper_str_left <= 20){   //collision on left side
            change_mov_dir = false;
        }

        mouse_X = tumEventGetMouseX();
        mouse_Y = tumEventGetMouseY();

        // convert mouse coordinate origin to center of display
        mouse_X = mouse_X - SCREEN_WIDTH / 2;
        mouse_Y = mouse_Y - SCREEN_HEIGHT / 2;

        center_x = center_x + (mouse_X - prev_mouse_X);
        center_y = center_y + (mouse_Y - prev_mouse_Y);

        prev_mouse_X = mouse_X;
        prev_mouse_Y = mouse_Y;

        if (ticks % 15 == 0) {
            printf("mouse_X: %i\n", mouse_X);
            printf("mouse_Y: %i\n", mouse_Y);
        }

        ticks++;

        tumDrawUpdateScreen(); // Refresh the screen
        
        // Basic sleep of 20 milliseconds
        vTaskDelay(
            (TickType_t)20);
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