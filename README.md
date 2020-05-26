# FreeRTOS Emulator Exercises 1, 2 and 3

### exercise 1
https://github.com/RaphMayr/git-tutorial
tags: CodingChallenge2, CodingChallenge3

### three different screens

screen 0 holds exercise sheet 2
screen 1 holds part 1 of exercise sheet 3
screen 2 holds part 2 of exercise sheet 3

### keyboard buttons to control program

`e` to change between screens
`q` to quit
`a` `b` `c` `d` to count up button presses on screen 0
`c` to stop incrementing counter on screen 1 
`t` to trigger task3 on screen 1 (to count up button press T)
`f` to trigger task4 on screen 1 (to count up button press F)

### theory questions

2.3 Using the mouse
2.3.1 How does the mouse gurantee thread-safe functionality?
The mouse as a shared resource is guarded by a mutex.

3.1 General Questions
What is a kernel tick ?
It is an interrupt generated at a specific frequency as reference for timing issues.
What is a tickless kernel ?
A Kernel operating without a regular tick, so when idle it can potentially save power.

3.2 Basic Tasks
3.2.5 What happens if the Stack size is too low ?
The task can't fully execute, it just gets cut off;
in my case, the circle just displays very shortly

3.3 Scheduling and Priorities
3.3.3 What can you observe?
The tasks with higher priorities get done first.
