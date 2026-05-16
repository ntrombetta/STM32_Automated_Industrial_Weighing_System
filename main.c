#includes .... 

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

	check_crc();																// 

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
