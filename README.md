This repository showcases the full hardware and bare-metal firmware architectures for a high-reliability industrial monitoring and belt scale integration platform.

🔒 Repository Disclaimer: Due to corporate NDA restrictions, the architectural assets and codebase variations hosted here are generalized, platform-agnostic middleware layouts. They are designed to demonstrate system-level planning, memory management, and deterministic execution without exposing proprietary commercial IP.

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
*   **Silicon Lifespan Preservation:** Employs a ring-buffered tracking pointer to sequentially write continuous 1-second data logs across a 4MB EEPROM chip, protecting the silicon from premature single-address burnouts.
*   **Look-Ahead Block Management:** Intercepts upcoming sector boundaries to trigger sector erases ahead of active write states, completely removing write-latency execution drops.
*   **Anti-Lock Polling Guards:** Enforces strict execution limits on status register polling loops via `HAL_GetTick()` delta timers, eliminating the risk of a firmware lockup if an external memory chip suffers a localized bus fault.
### 📌 Module D: Fast State Recovery Engine (`boot_recovery.c`)
*   **Zero-Metadata Architecture:** Because the physical records' positioning inherently defines the chronological advancement of the logs, the filesystem requires zero master indexing. If an unexpected power outage occurs mid-write, the data structure cannot corrupt.
*   **Hybrid Search Optimization:** On startup, the firmware executes a rapid linear sweep across 4KB sector margins (`addr += 4096`) checking for unwritten blocks (`0xFFFFFFFF`). Once trapped, it switches to a localized **Binary Search** to find the system's last valid log in a fraction of a millisecond.
*   **Word-Aligned Masking:** Utilizes a bitwise alignment mask (`& ~0x03`) during the binary search phase to force all midpoints to land perfectly on 32-bit boundary alignments, neutralizing any architectural hardware unaligned-access faults.
*   
*   
### 📌 main.c: Deterministic Runtime Execution
*   **Pre-Init Memory Validation:** Executes a destructive bare-metal power-on memory matrix check (`RAM_scrub()`) immediately post-clock initialization and prior to `HAL_Init()`. This validates physical address and data bus integrity before the C runtime environment maps or utilizes active stack frames.
*   **Defense-in-Depth Diagnostics:** Evaluates hardware state registers via `get_reset_reason()` on boot. If an abnormal cycle occurs (Brownouts, Independent Watchdogs, Low Power entry flags), it bypasses silent booting to immediately force a localized warning dialogue onto the display.
*   **Low-Overhead Scheduler:** Avoids the context-switching overhead of a heavy RTOS task scheduler; operates as a lean bare-metal loop timed via a hardware-decremented 1ms `sys.system_tick` timer flag.
### 📌 Module G: Multi-Layered UI Routing (`ui_state_machine.c`)
*   **Data Encapsulation Design:** Restricts all screen state mappings and transitions to a private `typedef enum` isolated strictly within the `.c` source module. This ensures outer peripheral modules cannot accidentally modify structural application navigation layers.
*   **Nested Switch-Case Execution:** Processes active screen changes, password verification loops, and calibration state jumps through nested state blocks, routing raw decoded boundary coordinate flags directly into execution subroutines.


### 📌 Module B: Real-Time Signal Processing & Precision Math
*   **IEEE 754 Precision Defense:** Implements a dual-layer **Kahan Summation Algorithm** to resolve tiny fractional increments (`< 0.0019`) into massive running accumulation total structures over continuous 10ms integration loops. This completely eliminates mantissa-shifting truncation errors.
*   **Algebraic Error Tracking:** Maintains running error compensation variables (`local_error`, `master_error`) to trap truncated low-order bits, carrying them forward into subsequent integration steps for permanent numerical tracking stability.
*   **Non-Blocking Pulse Generation:** Leverages non-blocking `HAL_GetTick()` delta matching inside `send_totalizer` to safely toggle physical Opto22 relays and MODBUS status registers without introducing timing jitter or locking the core execution loop thread.


### 📌 prox_sensor.c: Asynchronous Input Capture & Speed Tracking
*   **Hardware Interface:** Driven asynchronously by an STM32 Timer Input Capture hardware interrupt event (`HAL_TIM_IC_CaptureCallback`).
*   **16-Bit Roll-Over Mitigation:** Tracks and recalculates timing metrics across physical timer count wrap-around margins (`0xFFFF`), eliminating catastrophic timing delta drop-offs when a pulse bridges the hardware overflow boundary.
*   **Arithmetic Fault Defense:** Implements localized conditional checks to neutralize division-by-zero errors across dynamic timing variables and baseline sensor denominators.
*   **Dynamic Rolling Filter:** Aggregates raw microsecond-interval pulse captures into a 1000-sample running-average filter array to flatten high-frequency signaling noise before passing data to the master weight calculation loops.

### 📌 touch_controller.c: Low-Level I2C Interface & HMI Coordinate Decoding
*   **Analog Settling Guards:** Implements strict command-to-read delays (`HAL_Delay(1)`) inside initialization and query sequencing to satisfy the physical charge-transfer settling windows mandated by the resistive touch controller hardware.
*   **ADC-to-Pixel Interpolation:** Translates raw 12-bit resistive matrix data vectors directly into linearized screen space coordinates using coordinate-inversion and ratio-scaling mapping matrices.
*   **Non-Branching Bounding Evaluations:** Employs optimized bitwise validation arrays (`&`) over traditional logical gates (`&&`) inside `get_touch()`. This evaluates 2D bounding boxes simultaneously, cutting CPU pipeline branch mispredictions inside high-frequency processing loops.
*   **Context-Aware Spatial Filtering:** Implements conditional menu overlay constraints (`state.last_screen != RUN_SCREEN`) to cleanly isolate active dialogue box inputs from underlying menu button coordinates.



