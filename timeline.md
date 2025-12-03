# Timeline
*complete start to finish roadmap of my process & learning*

given fanatec pedals, tasked with creating a way to read the positions off the pedal for use with simulation software. 

1) tried python + HIDAPI to read inputs from pedals
    - library issues
2) switched to C++ for more flexibility with manipulation of hardware from the codebase

familiarized myself with Win32 syntax to build an application. Why?
    - Windows as an operating systems manages all HID (hardware interface devices)
    - Creating an application allows me to register for real time HID data from windows -> my application can receive messages from windows on USB devices
    - Win32 uses <windows.h>, in which I was already comfortable with C++

1) created simple GUI
2) registered to receive RAW INPUT from windows
3) printed binary data from pedals to output
4) analyzed patterns with each pedal press, what bytes change and which bytes don't

researched bit behaviour and CPU architectures -> endianess (changes how bytes are ordered), byte arrays, bitwise operations, combining bytes into integers

1) converted bytes into integer values
2) one variable -> speed is modified from simple pedal inputs
3) GUI effect to show which pedal is being pressed
4) implemented modes, Static and Dynamic

logic revision to implement edge detection, debouncing pedals, rising edge, falling edge, boolean flags, 

1) pressure detection for dynamic mode, 0-min, 255-max
2) edge detection: increment on rising edge only, vice versa
3) double buffering to avoid flicker when expanding the UI

threading will need to be implemented to simulate a realistic driving simuation, speed is constantly going down unless acceleration is added. chronos will need to be implemented for debouncing pedals. graphs implementation for visualizign speed over time


1) threading implemented
2) chonos implemented in `Debounce` class for creating debounce objects that manage pedals
3) GUI overhaul for slightly cleaner look

now we must connect the Win32 application to the testing software. 
XCP over Ethernet -> liscensing issue
XCP over CAN -> harder implementation, decided not to go with this option (foreshadowing)
creating a MatLab Simulink S-Function would theoretically fix the liscensing issue (foreshadowing)

1) adapted original Win32 application for use in MatlLab
2) creation of new simulink model -> s function
3) linked codebase to the s function

connecting the s function to testign software, we ran into another liscensing issue, my next option is communication over CAN Bus.

1) created loopback between two CAN tools
2) examined CAN messages
3) downloaded PCANBasic toolkit
4) adapted PCANBasic toolkit for use with my Win32 application

 
 
