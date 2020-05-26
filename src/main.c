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

static TaskHandle_t state_machine = NULL;
static TaskHandle_t terminate_process = NULL;

static TaskHandle_t task0 = NULL;
static TaskHandle_t task_frequ1 = NULL;
static TaskHandle_t task_frequ2 = NULL;
static TaskHandle_t task3 = NULL;
static TaskHandle_t task4 = NULL;
static TaskHandle_t timer_task = NULL;
static TaskHandle_t control_task = NULL;

static TaskHandle_t task1_screen2 = NULL;
static TaskHandle_t task2_screen2 = NULL;
static TaskHandle_t task3_screen2 = NULL;
static TaskHandle_t task4_screen2 = NULL;
static TaskHandle_t output_task_screen2 = NULL;

static SemaphoreHandle_t task3_signal = NULL;
static SemaphoreHandle_t task3_screen2_signal = NULL;
static SemaphoreHandle_t counter_T_lock = NULL;
static SemaphoreHandle_t counter_F_lock = NULL;
static SemaphoreHandle_t ScreenLock0 = NULL;
static SemaphoreHandle_t ScreenLock1 = NULL;
static SemaphoreHandle_t ScreenLock2 = NULL;
static SemaphoreHandle_t state_machine_signal = NULL;

static QueueHandle_t task1_screen2_qu = NULL;
static QueueHandle_t task2_screen2_qu = NULL;
static QueueHandle_t task3_screen2_qu = NULL;
static QueueHandle_t task4_screen2_qu = NULL;

static StaticTask_t xTaskBuffer;
static StackType_t xStack[mainGENERIC_STACK_SIZE * 2];

static int counter_T = 0;
static int counter_F = 0;

// priorities of task 1 to 4 (SCREEN 2)
static int priority_1 = 1;
static int priority_2 = 2;
static int priority_3 = 3;
static int priority_4 = 4;

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
    // initial suspend of screen 1 and 2
    vTaskResume(task0);
    vTaskSuspend(timer_task);
    vTaskSuspend(control_task);
    vTaskSuspend(task_frequ1);
    vTaskSuspend(task_frequ2);
    vTaskSuspend(task1_screen2);
    vTaskSuspend(task2_screen2);
    vTaskSuspend(task3_screen2);
    vTaskSuspend(task4_screen2);
    vTaskSuspend(output_task_screen2);
    

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
                    state = 2;
                }
                else if (state == 2) {
                    state = 0;
                }
                if (state == 0) {
                    printf("displaying solution for 2\n");
                    vTaskResume(task0);
                    vTaskSuspend(timer_task);
                    vTaskSuspend(control_task);
                    vTaskSuspend(task_frequ1);
                    vTaskSuspend(task_frequ2);
                    vTaskSuspend(task1_screen2);
                    vTaskSuspend(task2_screen2);
                    vTaskSuspend(task3_screen2);
                    vTaskSuspend(task4_screen2);
                    vTaskSuspend(output_task_screen2);
                }
                if (state == 1) {
                    printf("displaying solution for 3.2\n");
                        // resuming tasks of screen 1
                    vTaskSuspend(task0);
                    vTaskSuspend(task1_screen2);
                    vTaskSuspend(task2_screen2);
                    vTaskSuspend(task3_screen2);
                    vTaskSuspend(task4_screen2);
                    vTaskSuspend(output_task_screen2);
                    vTaskResume(task_frequ2);
                    vTaskResume(task_frequ1);
                    vTaskResume(timer_task);
                    vTaskResume(control_task);

                    xTimerStart(xResetTimer, 0);
                    
                }
                if (state == 2) {
                    printf("displaying solution for 3.3\n");
                        // resuming tasks of screen 2
                    vTaskSuspend(task0);
                    vTaskSuspend(timer_task);
                    vTaskSuspend(control_task);
                    vTaskSuspend(task_frequ1);
                    vTaskSuspend(task_frequ2);
                    vTaskResume(task1_screen2);
                    vTaskResume(task2_screen2);
                    vTaskResume(task3_screen2);
                    vTaskResume(task4_screen2);
                    vTaskResume(output_task_screen2);

                }
                last_change = xTaskGetTickCount();
            }
        }
    }
}

void vTerminateProcess(void *pvParameters) {
    while(1) {
        if(ulTaskNotifyTake(pdTRUE, portMAX_DELAY)) {
            vTaskDelete(task0);

            vTaskDelete(control_task);
            vTaskDelete(task_frequ1);
            vTaskDelete(task_frequ2);
            vTaskDelete(task3);
            vTaskDelete(task4);
            vTaskDelete(timer_task);

            vTaskDelete(task1_screen2);
            vTaskDelete(task2_screen2);
            vTaskDelete(task3_screen2);
            vTaskDelete(task4_screen2);
            vTaskDelete(output_task_screen2);

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

// Task exercise 2 ######################################

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
    int change_mov_dir = 0;
    

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

    while (1) {
        if(xSemaphoreTake(ScreenLock0, portMAX_DELAY) 
            == pdTRUE) { 
            tumDrawBindThread();
            for (int counter=0; counter<100; counter++) {
                tumEventFetchEvents();
                xGetButtonInput(); // Update global input

                vCheckStateInput();

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
                    change_mov_dir = 1;
                }
                if(upper_str_left <= 20){   //collision on left side
                    change_mov_dir = 0;
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
                vDrawFPS();

                tumDrawUpdateScreen();

                vTaskDelay(10);
            }
            xSemaphoreGive(ScreenLock0);
        }
    }
}

// end Task exercise 2 ##################################

// Tasks Screen 2 #######################################

void vTask1_screen2(void *pvParameters) 
{
    TickType_t xLastWakeTime;    
    const TickType_t xFrequency = 1;

    static int task1_output[15];

    int ticks = 0;

    xLastWakeTime = xTaskGetTickCount();

    while (1) {

        if (ticks >= 15) {
            printf("exiting.. task 1\n");
            for(int i=0; i<15; i++) {
                xQueueSend(task1_screen2_qu, &task1_output[i], 0);
            }
            vTaskSuspend(NULL);
        }
        
        task1_output[ticks] = 1;

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        ticks++;
    }
}

QueueHandle_t process3_to2;

void vTask2_screen2 (void *pvParameters) 
{
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = 2;

    static int task2_output[15];
    static int task3_output[15];

    static int task3_buffer;

    process3_to2 = xQueueCreate(1, sizeof(unsigned int));

    int ticks = 0;

    xLastWakeTime = xTaskGetTickCount();

    while (1) {

        if (ticks >= 15) {
            printf("exiting.. task 2 and 3\n");
            for(int i=0; i<15; i++) {
                xQueueSend(task2_screen2_qu, &task2_output[i], 0);
                xQueueSend(task3_screen2_qu, &task3_output[i], 0);
            }
            vTaskSuspend(task3_screen2);
            vTaskSuspend(NULL);
        }
        
        task2_output[ticks] = 2;

        xSemaphoreGive(task3_screen2_signal);
        xQueueReceive(process3_to2, &task3_buffer, 0);

        task3_output[ticks] = task3_buffer;

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        ticks = ticks + 2;
    }

}

void vTask3_screen2 (void *pvParameters)
{   
    static int task3_output = 3;

    while (1) {
        if(xSemaphoreTake(task3_screen2_signal, portMAX_DELAY)){
                // same tick count as task 2
            xQueueSend(process3_to2, &task3_output, 0);
        }
    }
}

void vTask4_screen2 (void *pvParameters)
{
    TickType_t xLastWakeTime;
    xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = 4;

    static int task4_output[15];

    int ticks = 0;

    while (1) {
        
        if (ticks >= 15) {
            printf("exiting.. task 4\n"); 
            for(int i=0; i<15; i++)  {
                xQueueSend(task4_screen2_qu, &task4_output[i], 0);
            }
            vTaskSuspend(NULL);
        }

        task4_output[ticks] = 4;

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        ticks = ticks + 4;
    }
}

void vOutputTask_screen2 (void *pvParameters) 
{
    // terminate other 4 tasks after 15 ticks.
    // output array of all numbers

    static int task_output[15][4];

    static char str_tick[50];
    static char str_column1[50];
    static char str_column2[50];
    static char str_column3[50];
    static char str_column4[50];

    static int task1_buffer;
    static int task2_buffer;
    static int task3_buffer;
    static int task4_buffer;

    int y_coord = 0;

    int checked = 0;

    int priority1_to_array = priority_1 - 1;
    int priority2_to_array = priority_2 - 1;
    int priority3_to_array = priority_3 - 1;
    int priority4_to_array = priority_4 - 1;

    vTaskDelay(100);

    while(1) {
        if(!checked) {
            for(int tick=0; tick<15; tick++) {
                xQueueReceive(task1_screen2_qu, &task1_buffer, 
                                portMAX_DELAY);
                task_output[tick][priority1_to_array] = task1_buffer;
            
                xQueueReceive(task2_screen2_qu, &task2_buffer, 
                                portMAX_DELAY);
                task_output[tick][priority2_to_array] = task2_buffer;

                xQueueReceive(task3_screen2_qu, &task3_buffer, 
                                portMAX_DELAY);
                task_output[tick][priority3_to_array] = task3_buffer;

                xQueueReceive(task4_screen2_qu, &task4_buffer, 
                                portMAX_DELAY);
                task_output[tick][priority4_to_array] = task4_buffer;   
            }
            checked = 1;
        }

        if(xSemaphoreTake(ScreenLock2, 1000) == pdTRUE) {
            tumDrawBindThread();
            
            for(int i = 0; i<1000; i++) {
                tumEventFetchEvents();
                xGetButtonInput();
                vCheckStateInput();

                tumDrawClear(White);

                y_coord = SCREEN_HEIGHT / 2 - DEFAULT_FONT_SIZE / 2 - 100;

                for (int row=0; row<15; row++) {
                    sprintf(str_tick,  "Tick %i:", row);
                    sprintf(str_column1, "%i", task_output[row][3]);
                    sprintf(str_column2, "%i", task_output[row][2]);
                    sprintf(str_column3, "%i", task_output[row][1]);
                    sprintf(str_column4, "%i", task_output[row][0]);
                    
                    tumDrawText(str_tick, 
                                SCREEN_WIDTH / 2 - 75,
                                y_coord, TUMBlue);
                    if(task_output[row][3] != 0) {
                        tumDrawText(str_column1,
                                    SCREEN_WIDTH / 2,
                                    y_coord, Black);
                    }
                    if(task_output[row][2] != 0) {
                        tumDrawText(str_column2,
                                    SCREEN_WIDTH / 2 + 25,
                                    y_coord, Black);
                    }
                    if(task_output[row][1] != 0) {
                        tumDrawText(str_column3,
                                    SCREEN_WIDTH / 2 + 50,
                                    y_coord, Black);
                    }
                    if(task_output[row][0] != 0) {
                        tumDrawText(str_column4,
                                    SCREEN_WIDTH / 2 + 75,
                                    y_coord, Black);
                    }
                    y_coord = y_coord + 20;
                }
            
                vDrawFPS();
                
                tumDrawUpdateScreen(); 
                vTaskDelay(10);
            }
            xSemaphoreGive(ScreenLock2);
            vTaskDelay(100);
        }
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
    signed short center_x = SCREEN_WIDTH/2;
    signed short center_y = SCREEN_HEIGHT/2;

    signed short circle_x = center_x - 60;
    signed short circle_y = center_y;
   	signed short circle_radius = 50;

    unsigned int ticks = 0;
    short change = 0;

    while (1) {
        if(xSemaphoreTake(ScreenLock1, 1000) == pdTRUE) {
            tumDrawBindThread();
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
    signed short center_x = SCREEN_WIDTH/2;
    signed short center_y = SCREEN_HEIGHT/2;

    signed short circle_x = center_x + 60;
    signed short circle_y = center_y;
   	signed short circle_radius = 50;

    unsigned int ticks = 0;
    int counter = 0;
    short change = 0;

    while (1) {
        if(xSemaphoreTake(ScreenLock1, 1000) == pdTRUE) {
            tumDrawBindThread();
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

    while(1) {
        if(xSemaphoreTake(task3_signal, portMAX_DELAY)){
            tumDrawBindThread();
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

    while(1) {
        if(ulTaskNotifyTake(pdTRUE, portMAX_DELAY)) {
            tumDrawBindThread();
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

    // locking mechanism for buttons
    // shared resource -> mutex
    buttons.lock = xSemaphoreCreateMutex(); 
    if (!buttons.lock) {
        PRINT_ERROR("Failed to create buttons lock");
        goto err_buttons_lock;
    }

    ScreenLock0 = xSemaphoreCreateMutex(); // lock for screen 0
    
    ScreenLock1 = xSemaphoreCreateMutex(); // lock for screen 1

    // signal to trigger state machine.
    // trigger with (E) to switch between states
    state_machine_signal = xSemaphoreCreateBinary();

    // signal to trigger task3 
    // trigger with (T)
    task3_signal = xSemaphoreCreateBinary();

    // mutexes to secure incrementing and resetting of counter vars
    counter_T_lock = xSemaphoreCreateMutex();
    counter_F_lock = xSemaphoreCreateMutex();

    // timer to reset counters for (T) and (F)
    xResetTimer = xTimerCreate("timer", pdMS_TO_TICKS(15000), pdTRUE, 
                                (void*) 0, vTimerCallbackReset);

    ScreenLock2 = xSemaphoreCreateMutex(); // lock for screen 2

    // signal to trigger task3 on screen 2 
    task3_screen2_signal = xSemaphoreCreateBinary();

    // queues to transmit char array to output process
    task1_screen2_qu = xQueueCreate(15, sizeof(char));
    task2_screen2_qu = xQueueCreate(15, sizeof(char));
    task3_screen2_qu = xQueueCreate(15, sizeof(char));
    task4_screen2_qu = xQueueCreate(15, sizeof(char));

    // Task Creation ##############################

    xTaskCreate(vTerminateProcess, "", 
                mainGENERIC_STACK_SIZE * 2,
                NULL, (configMAX_PRIORITIES - 1), &terminate_process);

    xTaskCreate(vSequential_StateMachine, "state machine", 
                mainGENERIC_STACK_SIZE * 2,
                NULL, (configMAX_PRIORITIES - 1), &state_machine);

    // Screen 0 Tasks
    xTaskCreate(vdrawFigures, "", mainGENERIC_STACK_SIZE * 2,
                NULL, mainGENERIC_PRIORITY, &task0);

    // Screen 1 Tasks
    xTaskCreate(vTask_frequ1, "", 
                mainGENERIC_STACK_SIZE * 2, 
                NULL, mainGENERIC_PRIORITY , &task_frequ1);

    task_frequ2 = xTaskCreateStatic(vTask_frequ2, "",
                                    mainGENERIC_STACK_SIZE * 2,
                                    NULL, (mainGENERIC_PRIORITY + 1),
                                    xStack, &xTaskBuffer);

    xTaskCreate(vTask3, "", mainGENERIC_STACK_SIZE * 2,
                NULL, (mainGENERIC_PRIORITY + 2), &task3);

    xTaskCreate(vTask4, "", mainGENERIC_STACK_SIZE * 2,
                NULL, (mainGENERIC_PRIORITY + 2), &task4);

    xTaskCreate(vResetCounterTask, "Reset Counters", 
                mainGENERIC_STACK_SIZE * 2,
                NULL, (mainGENERIC_PRIORITY + 2), &timer_task);

    xTaskCreate(vTaskControlMechanisms, "",
                mainGENERIC_STACK_SIZE * 2,
                NULL, (mainGENERIC_PRIORITY + 2), &control_task);

    // Screen 2 Tasks
    xTaskCreate(vTask1_screen2, "", 
                mainGENERIC_STACK_SIZE * 2,
                NULL, priority_1, &task1_screen2);

    xTaskCreate(vTask2_screen2, "", 
                mainGENERIC_STACK_SIZE * 2,
                NULL, priority_2, &task2_screen2);

    xTaskCreate(vTask3_screen2, "", 
                mainGENERIC_STACK_SIZE * 2,
                NULL, priority_3, &task3_screen2);

    xTaskCreate(vTask4_screen2, "", 
                mainGENERIC_STACK_SIZE * 2,
                NULL, priority_4, &task4_screen2);

    xTaskCreate(vOutputTask_screen2, "", 
                mainGENERIC_STACK_SIZE * 2,
                NULL, mainGENERIC_PRIORITY, &output_task_screen2);


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
