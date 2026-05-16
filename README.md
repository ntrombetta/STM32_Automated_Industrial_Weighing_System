This repository showcases the full hardware and bare-metal firmware architectures for a high-reliability industrial monitoring and belt scale integration platform.

🔒 Repository Disclaimer: Due to corporate NDA restrictions, the system architecture images and source files are generalized middleware layouts. They are strictly designed to demonstrate how I handle hardware-software design, memory safety, and timing loops without exposing any confidential company code.

## 🗺️ 1. Architecture Designs

Hardware Top Level Architecture: 
<img width="1436" height="738" alt="image" src="https://github.com/user-attachments/assets/1de3f3eb-b6f5-4567-82d7-efd085488690" />

Motherboard Architecture: 
<img width="1431" height="751" alt="image" src="https://github.com/user-attachments/assets/d2531165-cd99-4f1a-b372-29d235ab30a1" />

Software Top Level Architecture: 
<img width="1447" height="741" alt="image" src="https://github.com/user-attachments/assets/1a6bf5a8-b6d5-4d65-b8af-182c49b40f16" />

Main Program Flowchart: 
<img width="1414" height="796" alt="image" src="https://github.com/user-attachments/assets/4656deb0-2826-4801-b914-9e8e235f092a" />

## ⚙️ Firmware Source Code Examples
### 📌 eeprom.c: Non-Volatile Memory & Wear Leveling
*   **save_totals()** - Ring buffer to sequentially write continuous 1-second data to an external EEPROM to prevent premature wear leveling.
*   **read_totals()** - Called on startup to read an EEPROM address to assist in finding the last known address with valid data.   
*   **find_eeprom_address()** - Executes a linear search combined with a binary search to find the last EEPROM address the device wrote to.
*   Recalls the total saved to this address and increments the address for the next write to the EEPROM chip. 
*   **get_totals()** - Passes both the address and total value(s) the linear/binary search return to the global FW variables. 

### 📌 main.c: Runtime Execution & Complex State Machine 
*   **main()** - Demonstrates a RAM scrub immediately after initializing the system core clock to ensure data integrity before nominal operation.
*   **get_reset_reason()** - Demonstrates querying the STM32 on every power cycle to determine exactly why the MCU was reset/powered off.
*   This is a diagnostic tool for operation in the field and will drive the LCD display to show an error message if a proper power down sequence wasn't executed. 
*   **process_screen_state()** - Example of a multi-layer nested state machine for transitioning between screens on the LCD display.

### 📌 scale_calc.c: Totalizer Output (OPTO22) & Precision Math
*   **add_totals()** Implements a Kahan Summation Algorithm to resolve IEEE floating point issues.
*   **send_totalizer()** Accepts the total increment from add_totals() and accumulates until the "bucket" (i.e. 10 Tons) has been processed on the belt and drives an OPTO22 relay.

### 📌 prox_sensor.c: Input Capture & Speed Tracking
*   **HAL_TIM_IC_CaptureCallback()** - Timer 13 callback to calculate the frequency of the proximity sensor.
*   With a nominal frequency calculated during a calibration period, we can determine the exact belt speed based on the delta between the nominal and calculated frequencies.

### 📌 touch_controller.c: Low-Level I2C Interface & HMI Coordinate Decoding
*   **process_touch_controller()** - Driver to process user touch on an LCD-TFT resitive touch screen and translate the ADC readings to X&Y coordinates that map to pixels on the display. 
*   **get_touch()** - Translate touch controller data to pixels on the display to process which "button" the user has pressed.
*    These flags drive the state machines in the entire project some of which can be seen in main.c.  



