/*******************************************************************************
 *  Process Touch Controller 
		- Active touch screen controller X & Y drivers 
		- Measure X & Y positions respectively
		- Power down TSC
        - Poll TSC IRQ pin to determine if a valid touch has been detected 
        - Convert ADC readings to X&Y coordinates to determine which pixels
          were touched on the LCD display 
 ******************************************************************************/
void process_touch_controller(void)
{
	static uint8_t touch_detected;
	static uint8_t touch_counter;

	static uint16_t x, y, xr, yr;
	static uint8_t receive_x[2], receive_y[2];

	static uint8_t power_down = 0x00;

	static uint8_t activate_x = 0x84;
	static uint8_t measure_x = 0xC4;

	static uint8_t activate_y = 0x94;
	static uint8_t measure_y = 0xD4;

	if ( HAL_I2C_Master_Transmit(&hi2c1, TC_WRITE << 1, &activate_x, 1, 10) == HAL_OK)   // Activate X&Y touch drivers
		HAL_Delay(1);

	if ( HAL_I2C_Master_Transmit(&hi2c1, TC_WRITE << 1, &measure_x, 1, 10) == HAL_OK)   // Measure X touch drivers
	{
		if ( HAL_I2C_Master_Receive(&hi2c1, TC_READ << 1, (uint8_t *)receive_x, 2, 10) == HAL_OK)
			HAL_Delay(1);
	}

	if ( HAL_I2C_Master_Transmit(&hi2c1, TC_WRITE << 1, &activate_y, 1, 10) == HAL_OK)   // Activate Y touch drivers
		HAL_Delay(1);

	if ( HAL_I2C_Master_Transmit(&hi2c1, TC_WRITE << 1, &measure_y, 1, 10) == HAL_OK)   // Measure Y touch drivers
	{
		if ( HAL_I2C_Master_Receive(&hi2c1, TC_READ << 1, (uint8_t *)receive_y, 2, 10) == HAL_OK)
			HAL_Delay(1);
	}

	if ( HAL_I2C_Master_Transmit(&hi2c1, TC_WRITE << 1, &power_down, 1, 10) == HAL_OK)   // Power down TSC 
		HAL_Delay(1);

	touch_detected = HAL_GPIO_ReadPin(TOUCH_DETECT_GPIO_Port, TOUCH_DETECT);	// Poll TC IRQ pin

	if (!touch_detected)
	{
		touch_counter++;

		x = (receive_x[0] << 4) | receive_x[1];									// Convert 8-bit values to 16-bit for X reading
		y = (receive_y[0] << 4) | receive_y[1]; 								// Convert 8-bit values to 16-bit for Y reading

		y = 4095 - y;															// Invert Y-readings
		yr = (y / 8);															// Convert Y ADC reading to Y-pixel


		if (yr < 0)																// Upper and lower pixel guards
			yr = 0;
		else if (yr > MAX_Y_RES)
			yr = MAX_Y_RES - 1;

		xr = x / 5;																// Convert X ADC reading to X-pixel

		if (xr < 0)																// Upper and lower pixel guards
			xr = 0;
		else if (xr > MAX_X_RES)
			xr = MAX_X_RES - 1;

		sys.x_pixel = xr;														// Pass data to system struct to process touch coordinates
		sys.y_pixel = yr;

		if (touch_counter > 3)													// Filter out "light" touches or taps; We want 3+ readings to determine an actual touch 
		{
			touch.flag = true;
			touch_counter = 0;
		}
	}
}

/*******************************************************************************
 *  Processes the touch for all screens
 *  	- Each button on the keypad is defined by the box it is enclosed in
 *  	- Once a button is flag the corresponding flag is set
 *  	- Button flags are cleared in the appropriate screen routine
 ******************************************************************************/
void get_touch(void)
{
	// Keypad buttons
	if ( (sys.x_pixel >= X_MIN_C1) & (sys.x_pixel <= X_MAX_C1) & (sys.y_pixel >= Y_MIN_R1) & (sys.y_pixel <= Y_MAX_R1) )
		touch.one = true;														// Set touch one key flag
	else if ( (sys.x_pixel >= X_MIN_C2) & (sys.x_pixel <= X_MAX_C2) & (sys.y_pixel >= Y_MIN_R1) & (sys.y_pixel <= Y_MAX_R1) )
		touch.two = true;														// Set touch two key flag
	else if ( (sys.x_pixel >= X_MIN_C3) & (sys.x_pixel <= X_MAX_C3) & (sys.y_pixel >= Y_MIN_R1) & (sys.y_pixel <= Y_MAX_R1) )
		touch.three = true;														// Set touch three key flag
	else if ( (sys.x_pixel >= X_MIN_C1) & (sys.x_pixel <= X_MAX_C1) & (sys.y_pixel >= Y_MIN_R2) & (sys.y_pixel <= Y_MAX_R2) )
		touch.four = true;														// Set touch four key flag
	else if ( (sys.x_pixel >= X_MIN_C2) & (sys.x_pixel <= X_MAX_C2) & (sys.y_pixel >= Y_MIN_R2) & (sys.y_pixel <= Y_MAX_R2) )
		touch.five = true;														// Set touch five key flag
	else if ( (sys.x_pixel >= X_MIN_C3) & (sys.x_pixel <= X_MAX_C3) & (sys.y_pixel >= Y_MIN_R2) & (sys.y_pixel <= Y_MAX_R2) )
		touch.six = true;														// Set touch six key flag
	else if ( (sys.x_pixel >= X_MIN_C1) & (sys.x_pixel <= X_MAX_C1) & (sys.y_pixel >= Y_MIN_R3) & (sys.y_pixel <= Y_MAX_R3) )
		touch.seven = true;														// Set touch seven key flag
	else if ( (sys.x_pixel >= X_MIN_C2) & (sys.x_pixel <= X_MAX_C2) & (sys.y_pixel >= Y_MIN_R3) & (sys.y_pixel <= Y_MAX_R3)  )
		touch.eight = true;														// Set touch eight key flag
	else if ( (sys.x_pixel >= X_MIN_C3) & (sys.x_pixel <= X_MAX_C3) & (sys.y_pixel >= Y_MIN_R3) & (sys.y_pixel <= Y_MAX_R3)  )
		touch.nine = true;														// Set touch nine key flag
	else if ( (sys.x_pixel >= X_MIN_C1) & (sys.x_pixel <= X_MAX_C1) & (sys.y_pixel >= Y_MIN_R4) & (sys.y_pixel <= Y_MAX_R4) )
		touch.decimal = true;													// Set touch decimal key flag
	else if ( (sys.x_pixel >= X_MIN_C2) & (sys.x_pixel <= X_MAX_C2) & (sys.y_pixel >= Y_MIN_R4) & (sys.y_pixel <= Y_MAX_R4) )
		touch.up = true;														// Set touch up key flag
	else if ( (sys.x_pixel >= X_MIN_C3) & (sys.x_pixel <= X_MAX_C3) & (sys.y_pixel >= Y_MIN_R4) & (sys.y_pixel <= Y_MAX_R4) )
		touch.zero = true;														// Set touch zero key flag
	else if ( (sys.x_pixel >= X_MIN_C1) & (sys.x_pixel <= X_MAX_C1) & (sys.y_pixel >= Y_MIN_R5) & (sys.y_pixel <= Y_MAX_R5) )
		touch.left = true;														// Set touch left key flag
	else if ( (sys.x_pixel >= X_MIN_C2) & (sys.x_pixel <= X_MAX_C2) & (sys.y_pixel >= Y_MIN_R5) & (sys.y_pixel <= Y_MAX_R5) )
		touch.enter = true;														// Set touch enter key flag
	else if ( (sys.x_pixel >= X_MIN_C3) & (sys.x_pixel <= X_MAX_C3) & (sys.y_pixel >= Y_MIN_R5) & (sys.y_pixel <= Y_MAX_R5) )
		touch.right = true;														// Set touch right key flag
	else if ( (sys.x_pixel >= X_MIN_C1) & (sys.x_pixel <= X_MAX_C1) & (sys.y_pixel >= Y_MIN_R6) & (sys.y_pixel <= Y_MAX_R6) )
		touch.minus = true;														// Set touch minus key flag
	else if ( (sys.x_pixel >= X_MIN_C2) & (sys.x_pixel <= X_MAX_C2) & (sys.y_pixel >= Y_MIN_R6) & (sys.y_pixel <= Y_MAX_R6) )
		touch.down = true;														// Set touch down key flag
	else if ( (sys.x_pixel >= X_MIN_C3) & (sys.x_pixel <= X_MAX_C3) & (sys.y_pixel >= Y_MIN_R6) & (sys.y_pixel <= Y_MAX_R6) )
		touch.delete = true;													// Set touch delete key flag

	// Run and Menu buttons
	else if ( (sys.x_pixel >= X_MIN_RUN) & (sys.x_pixel <= X_MAX_RUN) & (sys.y_pixel >= Y_MIN_RUN) & (sys.y_pixel <= Y_MAX_RUN) )
		touch.run = true;														// Set touch run flag
	else if ( (sys.x_pixel >= X_MIN_MENU) & (sys.x_pixel <= X_MAX_MENU) & (sys.y_pixel >= Y_MIN_RUN) & (sys.y_pixel <= Y_MAX_RUN) )
		touch.menu = true;														// Set touch menu flag

	// Buttons for bottom boxes
	else if ( (sys.x_pixel >= 0) & (sys.x_pixel <= 102) & (sys.y_pixel >= 390) & (sys.y_pixel <= 480) )
		touch.page_up = true;													// Set touch page up flag
	else if ( (sys.x_pixel >= 102) & (sys.x_pixel <= 204) & (sys.y_pixel >= 390) & (sys.y_pixel <= 480) )
		touch.page_down = true;													// Set touch page down flag
	else if ( (sys.x_pixel >= 204) & (sys.x_pixel <= 306) & (sys.y_pixel >= 390) & (sys.y_pixel <= 480) )
		touch.middle_box = true;												// Set touch middle box flag
	else if ( (sys.x_pixel >= 306) & (sys.x_pixel <= 408) & (sys.y_pixel >= 390) & (sys.y_pixel <= 480) )
		touch.fourth_box = true;												// Set touch fourth box flag
	else if ( (sys.x_pixel >= 408) & (sys.x_pixel <= 510) & (sys.y_pixel >= 390) & (sys.y_pixel <= 480) )
		touch.back_page = true;													// Set touch back page flag

	// Dialogue box buttons
	else if ( (sys.x_pixel >= 68) & (sys.x_pixel <= 160) & (sys.y_pixel >= 280) & (sys.y_pixel <= 330) & (state.last_screen != RUN_SCREEN) )
		touch.yes = true;														// Set touch yes flag (run system dialogue box)
	else if ( (sys.x_pixel >= 360) & (sys.x_pixel <= 440) & (sys.y_pixel >= 280) & (sys.y_pixel <= 330) )
		touch.no = true;														// Set touch no flag (run system dialogue box)
	else if ( (sys.x_pixel >= 200) & (sys.x_pixel <= 280) & (sys.y_pixel >= 280) & (sys.y_pixel <= 330) )
		touch.yes_quit = true;													// Set touch yes quit flag (system running dialogue box)
	else if ( (sys.x_pixel >= 522) & (sys.x_pixel <= X_MAX_C1) & (sys.y_pixel >= 280) & (sys.y_pixel <= 330) )
		touch.no_quit = true;

	// Run screen side bar buttons
	else if ( (sys.x_pixel >= X_MIN_SIDEBAR) & (sys.x_pixel <= X_MAX_SIDEBAR) & (sys.y_pixel >= Y_MIN_HOME) & (sys.y_pixel <= Y_MAX_HOME) )
		touch.home = true;														// Set touch yes flag (run system dialogue box)
	else if ( (sys.x_pixel >= X_MIN_SIDEBAR) & (sys.x_pixel <= X_MAX_SIDEBAR) & (sys.y_pixel >= Y_MIN_PLAY) & (sys.y_pixel <= Y_MAX_PLAY) )
		touch.play = true;														// Set touch yes flag (run system dialogue box)
	else if ( (sys.x_pixel >= X_MIN_SIDEBAR) & (sys.x_pixel <= X_MAX_SIDEBAR) & (sys.y_pixel >= Y_MIN_DIAGNOSTICS) & (sys.y_pixel <= Y_MAX_DIAGNOSTICS) )
		touch.diagnostics = true;												// Set touch no flag (run system dialogue box)
	else if ( (sys.x_pixel >= X_MIN_SIDEBAR) & (sys.x_pixel <= X_MAX_SIDEBAR) & (sys.y_pixel >= Y_MIN_ALARMS) & (sys.y_pixel <= Y_MAX_ALARMS) )
		touch.alarms = true;													// Set touch yes quit flag (system running dialogue box)

	// System status indicator button
	else if ( (sys.x_pixel >= 700) & (sys.x_pixel <= X_MAX_C3) & (sys.y_pixel >= 90) & (sys.y_pixel <= 180) )
		touch.status = true;													// Set touch status flag (run system dialogue box)

	else if ( (sys.x_pixel >= 220) & (sys.x_pixel <= 320) & (sys.y_pixel >= 270) & (sys.y_pixel <= 330) )
		touch.exit = true;                                                      // Set exit dialogue box flag 
}
