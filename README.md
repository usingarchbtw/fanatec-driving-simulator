# fanatec wizard



[Demonstrations](www.github.com) • [Documentation](docs.md) • [Techstack/Timeline](www.github.com)



## What is fanatec wizard?

---

Fanatec wizard is a Win32 application developed as a solution to manually changing variables in a simulated (most likely but not limited to) vehicle test loop. 



*Data Map*

`Fanatec Pedals -> [Binary Data] -> Computer -> (Windows Sends Binary HID Data To Application) -> Application -> [CAN Data] -> CAN Bus `



Each pedal has functionaility similar to what you would see in a traditional car, but there are differences:

1) Use of Static & Dynamic modes.

2) Clutch Pedal to switch modes.



## Install

fanatec wizard supports windows only (sorry linux and apple)

> For Win32 application use

- clone the repository, run the .exe



> For MatLab S Function use

- clone the repository

- run MatLab, open the command line
  
  `mex FanatecWizard.cpp`

- open simulink, create an S function model with *five* scope outputs, open each scope to see live changes to attributes upon changes to the pedal









##### Static Versus Dynamic

*To switch between modes press all the way down on the clutch pedal.*



In Static mode, pressing all the way down on acceleration will increment the speed of the vehicle by 20 units. Vice versa with the brake pedal. 



In Dynamic mode, the pedal functionality is nearly identical to real vehicle's pedal systems; pressing down on acceleration pedal will increase speed based on the level of pressure on the pedal, Vice versa with the brake pedal.







## FAQ

    Is this application limited to Fanatec hardware?

> no, the software is designed to interface with any HID with hex code 0x04 -> in the category of "joystick." It just so happens that Fanatec falls under this category. But due to the way in which the binary code was reverse engineered, I cannot guarantee the same functionality if using other pieces of hardware. 
