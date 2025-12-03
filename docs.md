# Documentation
---
*simple document describing functionality of each part*

### Left Pedal
otherwise known as the clitch pedal, this pedal changes the mode of the codebase.
Upon startup, the application will always start in Static mode. Pressign this pedal will change the mode from eithe r Static -> Dynamic *or* Dynamic -> Static

The pressure threshold for this pedal is set to **Maximum** by default. This means that in order to change the mode you must press all the way down on the pedal. 

### Middle Pedal
otherwise known as the brake pedal, this pedal decrements the speed based on the mode the software is in. 
In Static Mode, pressing down on the middle pedal will decrement spede by 20, using this in conjuction with the Right Pedal (acceleration), allows for precise variable access when using pedals for testing. 
In Dynamic Mode, pressing down on the middle pedal will decrement speed based on the pressure placed on the pedal, along side the thread that is decrementing speed.


### Right Pedal
otherwise known as the acceleration pedal, this pedal increases the speed based on the mode the software is in. 
In Static Mode, pressing down on the middle pedal will increment speed by 20
In Dynamic Mode, pressing down on the right pedal will increase speed based on the ammount of pressure placed on the pedal. 


### Threading 
This application uses threading to simulate a dynamic driving environment. 

