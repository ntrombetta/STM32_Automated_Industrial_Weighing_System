

/*******************************************************************************
 *  Main Loop
 *    - Initialize all necessary drivers, peripherals, variables and GPIO pins
 *    - Process user touch for LCD display
 *    - Run main loop every 1mS (driven from SysTick_Handler)
 *    - Drive display screens based on user's touch
 *    - Update LCD display clock/calendar
 *    - Check if 10 minute timer has expired for system key; If so lock system
 *    - Refresh watchdog
 *    - Drive run LED
 *    - Start ADC conversions for load cell input
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
			process_belt();
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
