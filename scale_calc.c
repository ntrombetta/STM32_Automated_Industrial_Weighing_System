/*******************************************************************************
 *  Add Totals
 *  	- Kahan Summation Algorithm
 *  	   Accurately add small increments to a large float
 *  	- Once a float is above 32768 this algorithm is required to add any
 *  	  increment below 0.0019 to the total
 *  	- The minimum increment requirement increases proportionally with the
 *  	  total
 ******************************************************************************/
void add_totals(void)
{
	static float local_offset;
	static float local_realized;
	static float local_error;

	static float master_offset;
	static float master_realized;
	static float master_error;

	local_offset = total_increment - local_error;
	local_realized = scale_calc.total + local_offset;

	local_error = (local_realized - scale_calc.total) - local_offset;

	scale_calc.total = local_realized;

	master_offset = master_total_increment - master_error;
	master_realized = scale_calc.master_total + master_offset;

	master_error = (master_realized - scale_calc.master_total) - master_offset;

	scale_calc.master_total = master_realized;

	if (sys_data.display_mat_factor > 0)
	{
		scale_calc.total = (scale_calc.total * (sys_data.display_mat_factor / 100) ) + scale_calc.total;
		scale_calc.master_total = (scale_calc.master_total * (sys_data.display_mat_factor / 100) ) + scale_calc.master_total;
	}

	send_totalizer(total_increment);

	total_increment = 0;
	master_total_increment = 0;
}

/*******************************************************************************
 *  Send Totalizer
 *  	- Uses the local total current increment from the Kahan algorithm to
 *  	  accumulate a totalizer "bucket" to send the totalizer and/or OPTO22
 *  	  output(s)
 *  	- Once the bucket reaches the value specified by the user (io.totalizer)
 *  	  we fire the pulse(s)
 ******************************************************************************/
void send_totalizer(float current_increment)
{
	static float epsilon = 0.00001f;
	static float totalizer_bucket;
	static uint32_t totalizer_tick;												// Tick value for generating the totalizer output pulse width
	static bool totalizer_generated;											// Flag for generating the falling edge of the totalizer output
	static uint32_t totalizer_counter;

	totalizer_bucket += current_increment;

	if (sys.total_reset == true)												// User has reset the local total
	{
		totalizer_bucket = 0;
		sys.total_reset = false;
	}

	if (totalizer_bucket >= (io.totalizer - epsilon) )						// Check if total has exceeded totalizer output setting
	{
		process_universal_outputs(totalizer);									// Generate rising edge for totalizer output
		RELAY_HIGH(); 															// Generate rising edge for OPTO22 output
		mb_io.output8 = true;													// Set modbus output 8 when driving OPTO22 totalizer output
		totalizer_tick = HAL_GetTick();											// Get time stamp to generate falling edge

		totalizer_bucket -= io.totalizer;
		totalizer_counter++;
		totalizer_generated = true;
	}

	if ( (totalizer_generated == true) && (HAL_GetTick() - io.totalizer_pw >= totalizer_tick) )		// Elapsed time = totalizer pulse width set by user
	{
		toggle_universal_outputs(totalizer);									// Generate falling edge for totalizer output
		RELAY_LOW(); 															// Generate falling edge for OPTO22 output
		mb_io.output8 = false;													// Clear modbus output 8 when driving OPTO22 totalizer output low
		totalizer_generated = false;
	}
}
