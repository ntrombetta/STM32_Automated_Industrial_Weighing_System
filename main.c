#includes .... 

/*******************************************************************************
 *  Main State Machine States (normally inside main.h) 
 ******************************************************************************/
typedef enum
{
	RUN_SCREEN 		= 0,
	RUN_DIALOGUE 	= 1,
	MAIN_SCREEN 	= 2,
	CALIBRATION 	= 3,
	INIT_SETUP 		= 4,
	SYS_DATA 		= 5,
	IO_DEFINE 		= 6,
	COMMUNICATION 	= 7,
	SETTINGS 		= 8,
	PASSWORD 		= 9,
	USB 			= 10
}screen_handle;
extern screen_handle screens;

/*******************************************************************************
 *  Process Screen State Struct Handler (normally inside main.h)
 ******************************************************************************/
typedef struct state_handle
{
	uint8_t last_screen;														// State 1
	uint8_t sub_screen;															// State 2
	uint8_t code_screen;														// State 3
	uint8_t duration_screen;													// State 4
	uint8_t cal_screen;															// State 5
	uint8_t sub_cal_screen;														// State 6
	uint8_t sys_data_screen;													// State 7
	uint8_t io_screen;															// State 8
	uint8_t password_screen;													// State 9
	uint8_t last_state;															// Last state assigned
	bool home_screen;															// Main Menu screen flag
}state_handle;
extern state_handle state;

/*******************************************************************************
 *  State machine for LCD screens
 ******************************************************************************/
static uint8_t states_1;														// Main menu items 1-6
static uint8_t states_2;													    // Initial Setup screens 1-6
static uint8_t states_3;														// Scale code data screens for Initial Setup 2
static uint8_t states_4;														// Test duration screens for Initial Setup 6
static uint8_t states_5;														// Calibration screens
static uint8_t states_6;														// Calibration mode screens
static uint8_t states_7;														// System Data 1-6 screens
static uint8_t states_8;														// Input & Output Setup screens
static uint8_t states_9;														// Password Setup screens

/*******************************************************************************
 *  Main Loop
 *    - Initialize all necessary drivers, peripherals, variables and GPIO pins
 *    - Loop runs every 1mS (driven from SysTick_Handler)
 *    - Process user touch for LCD display
 *    - Drive display screens based on user's touch
 *    - Start ADC conversions for load cell input
 *    - Run belt scale calculations 
 *    - Update LCD display RTC clock/calendar
 *    - Check input interrupt flags and service as needed 
 *    - Calculate on board thermistor temp if samples are > 10
 *    - Poll USB detect to enable power switch to supply VBUS to user flash drive 
 *    - Check if 10 minute timer has expired for system key; If so lock system
 *    - Refresh watchdog
 *    - Service MODBUS TCP/IP comms if enabled 
 ******************************************************************************/
int main(void)
{
 	SystemClock_Config();														// Initialize the system core clock

	RAM_scrub();																// Check RAM on boot-up

	reset = get_reset_reason();													// Get MCU reset reason

	HAL_Init();																	// Initialize HAL drivers
	system_init();																// Software system initialization

	if (reset != RESET_CAUSE_POWER_ON_POWER_DOWN_RESET)							// Normal power down sequence did not occur
		display_reset_reason();													// Display the reset reason on the LCD display 

	run_menu();																	// Draw run screen
	update_run_screen();														// Update run variables
	sys.run = true;
	state.last_screen = RUN_SCREEN;												// Set main state machine to RUN_SCREEN
	state.home_screen = false;													// Clear home screen flag

	draw_logo();																// Draw customer logo
	draw_clock_calendar();														// Draw clock and calendar

	while (1)
	{
 		if (!sys.system_tick)
		{
			sys.system_tick = SYSTEM_TICK;										// Reset 1 mS system tick

			process_touch();													// Get user touch from LCD
			process_loadcell();													// Calculate load cell input
			process_belt();                                                     // Run belt scale calculations 
			update_clock(); 													// Update real time clock
			modbus_status_bits = (uint16_t *)&mb_io;							// Pass MODBUS status message bit field

			if ( *modbus_status_bits & 0xFF00 )									// We received an external interrupt on one of the input pins; Process inputx
				check_inputs();

			if (adc.dma_conversion == true)										// We received 10 readings of the ambient thermistor
			{
				calc_thermistor();												// Calculate ambient temp in degrees C
				adc.dma_conversion = false;										// Clear DMA conversion flag
			}

			if (settings.system_key == false && settings.password_on == true)	// Check if system is unlocked and password is enabled
			{
				if (password.key_tick + 10 == sys_data.display_minute)			// 10 minutes has elapsed; Lock system
				{
					settings.system_key = true; 								// Set system key flag
					password.correct = false;									// Clear password correct flag
				}
			}

			MX_USB_HOST_Process(); 												// Poll USB detect
			HAL_IWDG_Refresh(&hiwdg);											// Refresh watchdog timer
		}

		if (!strncmp(settings.enabled, "Yes", 3))								// User has MODBUS TCP/IP comms enabled
			serviceNetwork();													// Service MODBUS TCP/IP comms
	}
}

/*******************************************************************************
 *  LCD State Machine
 *   	- Processes the user's touch to navigate through each screen
 *  	  on the LCD display
 *  	- 1st layer controls the flow and display of main menu items 1-6
 *  	- 2nd layer controls the flow of each main menu item's screens
 *  	- If necessary there is a 3rd layer for main menu item screens that
 *  	  have additional sub screens such as Initial Setup 2 scale code screens
 *  	  and Initial Setup 6 test duration screens, etc.
 ******************************************************************************/
void process_screen_state(void)
{
	states_1 = state.last_screen;												// Screens for main menu items 1-6
	states_2 = state.sub_screen;												// Sub screens for each main menu item
	states_3 = state.code_screen;												// Scale code screens for Initial Setup 2
	states_4 = state.duration_screen;											// Screens for auto and manual test duration for calibration
	states_5 = state.cal_screen;												// Calibration screens
	states_6 = state.sub_cal_screen;											// Calibration mode screens
	states_7 = state.sys_data_screen;											// System Data screens
	states_8 = state.io_screen;													// Input and output definition screens
	states_9 = state.password_screen;											// System password screens

	switch(states_1)															// Main menu items 1-6 and System Run screen
	{
		default:
			break;
		case RUN_SCREEN:                                                        // Main screen with total, flow rate, belt speed and sidebar for diagnostics, alarms, totals, and main menu 
			process_run_screen(); 												// Process run screen touch
		break;
		case INIT_SETUP:														// Main menu item #1 has been selected
			switch(states_2)													// Initial Setup 1-6 screens
			{
				default:
					break;
				case SETUP_1:
					process_init_setup_1();										// Process user input for initial setup 1 screen
				break;
				case SETUP_2:
					process_init_setup_2();										// User has navigated to initial setup 2 screen
					switch(states_3)											// If scale code data button is pressed
					{
						default:
							break;
						case CODE_DATA_1:
							process_code_data_1();								// User has navigated to scale code data 1 screen
						break;
						case CODE_DATA_2:
							process_code_data_2();								// User has navigated to scale code data 2 screen
						break;
						case CODE_DATA_3:
							process_code_data_3();								// User has navigated to scale code data 3 screen
						break;
					}
				break;
				case SETUP_3:
					process_init_setup_3();										// User has navigated to initial setup 3 screen
				break;
				case SETUP_4:
					process_init_setup_4();										// User has navigated to initial setup 4 screen
				break;
				case SETUP_5:
					process_init_setup_5();										// User has navigated to initial setup 5 screen
				break;
				case SETUP_6:
					process_init_setup_6();										// User has navigated to initial setup 6 screen
					switch(states_4)											// Manual or Auto duration buttons have been pressed
					{
						default:
							break;
						case MANUAL:
							process_manual_duration();							// Manual duration test setup has been selected
						break;
						case AUTO:
							process_auto_duration();							// Auto duration test setup has been selected
						break;
						case BEGIN_AUTO:
							process_begin_duration();							// User has begun the duration testing
						break;
						case BEGIN_MANUAL:
							begin_manual_duration();							// Process the duration testing results
						break;
					}
				break;
			}
		break;
		case CALIBRATION: 														// Main menu item #2 has been selected
			switch(states_5)                                                    // Determine cal state 
			{
				default:
					break;
				case NO_CAL:
					process_calibration();										// Process calibration main screen touch
				break;
				case ZERO:
					process_zero_cal();											// User has selected zero calibration item
					switch(states_6)											// Begin auto or manual zero buttons have been pressed
					{
						default:
							break;
						case BEGIN_AUTO_ZERO:
							process_begin_auto_zero();							// Start auto zero calibration
						break;
						case BEGIN_MAN_ZERO:
							process_begin_man_zero();							// Start manual zero calibration
						break;
					}
				break;
				case SPAN:
					process_span_cal();											// User has selected span calibration item
					switch(states_6)											// Auto or Manual span buttons have been pressed
					{
						default:
							break;
						case BEGIN_AUTO_SPAN:
							run_auto_span();									// Begin auto span calibration
						break;
					}
				break;
				case MATERIAL:
					process_material_cal();										// User has selected material calibration item
				break;
				case MATERIAL_COMPLETE:
					material_cal_complete();                                    // Material cal complete; process user touch 
				break;
			}
		break;
		case SYS_DATA:															// Main menu item #3 has been selected
			switch(states_7)
			{
				default:
					break;
				case SYSDATA_1:
					process_sys_data_1();										// Process user input for System Data 1 screen
				break;
				case SYSDATA_2:
					process_sys_data_2();										// Process user input for System Data 2 screen
				break;
				case SYSDATA_3:
					process_sys_data_3();										// Process user input for System Data 3 screen
				break;
				case SYSDATA_4:
					process_sys_data_4();										// Process user input for System Data 4 screen
				break;
				case SYSDATA_5:
					process_sys_data_5();										// Process user input for System Data 5 screen
				break;
			}
		break;
		case IO_DEFINE:															// Main menu item #4 has been selected
			switch(states_8)
			{
				default:
					break;
				case INPUT1:
					process_io_define();										// Input setup 1 screen
				break;
				case INPUT2:
					process_input_2();											// Input setup 2 screen
				break;
				case OUTPUT1:
					process_output_1();											// Output setup 1 screen
				break;
				case OUTPUT2:
					process_output_2();											// Output setup 2 screen
				break;
				case ANALOG:
					process_analog();											// Analog output screen
				break;
				case SIMULATED:
					process_simulated();										// Simulated output setup screen
				break;
				case TOTALIZER:
					process_totalizer();                                        // Totalizer output screen 
				break;
				case ALARM1:
					process_alarm_1();											// Rate alarm setup screen
				break;
				case ALARM2:
					process_alarm_2();											// Load alarm setup screen
				break;
				case ALARM3:
					process_alarm_3();											// Speed alarm setup screen
				break;
				case RESET_ALARM:
					process_reset_alarm();										// Alarm reset setup screen
				break;
			}
		break;
		case COMMUNICATION:
			process_comms();													// Main menu item #5 has been selected
		break;
		case SETTINGS:
			process_settings();                                                // Main menu item #6 has been selected 
		break;
		case PASSWORD:
			switch(states_9)
			{
				default:
					break;
				case NO_PASSWORD:
					process_password();											// Process password main screen touch
				break;
				case CURRENT:
					process_current_password();									// Ask user to enter the current system password
				break;
				case NEW:
					process_new_password();										// Ask user to enter new password
				break;
				case RE_ENTER:
					process_re_entry();											// Ask user to re-enter new password
				break;
				case ENTER:
					process_enter_password();									// Process the password user entered to unlock the system
				break;
			}
		break;
		case USB:
			process_usb();                                                      // User is trying to extract or load parameter files 
		break;
	}
}

/*******************************************************************************
 *  RAM Scrub
 *  	- Copies 0x77777777 to entire RAM location and reads it back
 *  	- If the data does not match we assume the RAM is corrupted and call
 *  	  Error_Handler()
 ******************************************************************************/
void RAM_scrub(void)
{
	uint32_t *data_ptr;															// Pointer to RAM location
	uint32_t data = 0x77777777;													// Data to fill RAM

	for (data_ptr = (uint32_t *)RAM_START; data_ptr < (uint32_t *)RAM_END - 0x0000FFFF; data_ptr++)
		*data_ptr = data; 														// Pass data to each RAM location

	for (data_ptr = (uint32_t *)RAM_START; data_ptr < (uint32_t *)RAM_END - 0x0000FFFF; data_ptr++)
	{
		if (*data_ptr != data)													// Check each location has 0x77777777 written to it
			Error_Handler();													// Lock up if failed
	}
}

/*******************************************************************************
 *  Get Reset Reason
 *  	- Diagnoses the reason the MCU reset most recently
 *  	- List of relevant reset reasons
 *  	 	- Low power reset
 *  	 	- WWDG reset
 *  	 	- IWDG reset
 *  	 	- NVIC_SystemReset()
 *  	 	- Power on reset
 *  	 	- BOR reset
 *  	 	- EXTI pin reset
 ******************************************************************************/
uint8_t get_reset_reason(void)
{
    uint8_t reset_cause;

    if (__HAL_RCC_GET_FLAG(RCC_FLAG_LPWRRST))									// Low power reset
        reset_cause = RESET_CAUSE_LOW_POWER_RESET;
    else if (__HAL_RCC_GET_FLAG(RCC_FLAG_WWDGRST))								// Window watchdog reset
        reset_cause = RESET_CAUSE_WINDOW_WATCHDOG_RESET;
    else if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST))								// Independent watchdog reset
        reset_cause = RESET_CAUSE_INDEPENDENT_WATCHDOG_RESET;
    else if (__HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST))								// NVIC_SystemReset() called
        reset_cause = RESET_CAUSE_SOFTWARE_RESET;
    else if (__HAL_RCC_GET_FLAG(RCC_FLAG_PORRST))								// Power on reset
        reset_cause = RESET_CAUSE_POWER_ON_POWER_DOWN_RESET;
    else if (__HAL_RCC_GET_FLAG(RCC_FLAG_PINRST))								// External reset pin toggled
        reset_cause = RESET_CAUSE_EXTERNAL_RESET_PIN_RESET;
    else if (__HAL_RCC_GET_FLAG(RCC_FLAG_BORRST))								// Brown-out reset; must be checked after PORRST
        reset_cause = RESET_CAUSE_BROWNOUT_RESET;
    else
        reset_cause = RESET_CAUSE_UNKNOWN;

    __HAL_RCC_CLEAR_RESET_FLAGS();     											// Clear all the reset flags or else they will remain set during future

    return reset_cause;
}

/*******************************************************************************
 *  Display Reset Reason
 *  	- Draws a dialogue box to display the exact reason the MCU reset if
 *  	  the reset was not a normal POR
 ******************************************************************************/
void display_reset_reason(void)
{
	DMA2D_FillRect(LCD_COLOR_DARKGRAY, DIALOGUE_BOX_START_X, DIALOGUE_BOX_START_Y, 470, 336);	// Draw dialogue box

	BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
	BSP_LCD_SetFont(&Font24);

	if (reset == RESET_CAUSE_LOW_POWER_RESET)
	{
		BSP_LCD_DisplayStringAt(80, DRAW_STRING_Y2 + 25, (uint8_t *)"Low Power Reset!", LEFT_MODE);	// Error dialogue
		BSP_LCD_DisplayStringAt(80, DRAW_STRING_Y3 + 40, (uint8_t *)"Power cycle required!", LEFT_MODE);
	}
	else if (reset == RESET_CAUSE_WINDOW_WATCHDOG_RESET)
	{
		BSP_LCD_DisplayStringAt(80, DRAW_STRING_Y2 + 25, (uint8_t *)"Watchdog Reset!", LEFT_MODE);	// Error dialogue
		BSP_LCD_DisplayStringAt(80, DRAW_STRING_Y3 + 40, (uint8_t *)"Power cycle required!", LEFT_MODE);
		while(1);
	}
	else if (reset == RESET_CAUSE_SOFTWARE_RESET)
	{
		BSP_LCD_DisplayStringAt(80, DRAW_STRING_Y2 + 25, (uint8_t *)"NVIC System Reset!", LEFT_MODE);	// Error dialogue
		BSP_LCD_DisplayStringAt(80, DRAW_STRING_Y3 + 40, (uint8_t *)"Power cycle required!", LEFT_MODE);
	}
	else if (reset == RESET_CAUSE_BROWNOUT_RESET)
	{
		BSP_LCD_DisplayStringAt(80, DRAW_STRING_Y2 + 25, (uint8_t *)"Brown Out Reset!", LEFT_MODE);	// Error dialogue
		BSP_LCD_DisplayStringAt(80, DRAW_STRING_Y3 + 40, (uint8_t *)"Check power connections!", LEFT_MODE);
	}
	else
	{
		BSP_LCD_DisplayStringAt(80, DRAW_STRING_Y2 + 25, (uint8_t *)"Reset Cause Unknown!", LEFT_MODE);	// Error dialogue
		BSP_LCD_DisplayStringAt(80, DRAW_STRING_Y3 + 40, (uint8_t *)"Power cycle required!", LEFT_MODE);
	}
}
