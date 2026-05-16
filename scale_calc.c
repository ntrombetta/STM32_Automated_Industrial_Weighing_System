/*******************************************************************************
 *  Add Totals
 *      - Integration period for the belt scale is only 10mS so we are 
          inherently adding very small floats to a potentially large float 
          every single loop. Thus, we run into IEEE 754 floating-point issues  
 *  	- Kahan Summation Algorithm
 *  	   Accurately adds small increments to a large float
 *  	- Once a float is above 32768 this algorithm is required to add any
 *  	  increment below 0.0019 to the total
 *  	- The minimum increment requirement increases proportionally with the
 *  	  total
 *      - NOTE: total_increment and master_total_increment are the calculated
          values from the 10mS integration period. These are calculated in
          a previous (proprietary) function and add_totals() is called 
          immediately after 
 ******************************************************************************/
void add_totals(void)
{
	static float local_offset;                                                  // Local total current increment  
	static float local_realized;                                                // Local total buffer for the actual value we calculate by adding a small float to a large float 
	static float local_error;                                                   // Local total error accumulator  

	static float master_offset;                                                 // Master total current increment                                                 
	static float master_realized;                                               // Master total buffer for the actual value we calculate by adding a small float to a large float
	static float master_error;                                                  // Master total error accumulator

	local_offset = total_increment - local_error;                               // Subtract the running error from our total increment                               
	local_realized = scale_calc.total + local_offset;                           // Add new increment to the local total buffer

	local_error = (local_realized - scale_calc.total) - local_offset;           // Calculate the remainder that would have been truncated for the next loop 

	scale_calc.total = local_realized;                                          // Assign the actual local total the correct value 

	master_offset = master_total_increment - master_error;                      // Repeat for master total 
	master_realized = scale_calc.master_total + master_offset;

	master_error = (master_realized - scale_calc.master_total) - master_offset;

	scale_calc.master_total = master_realized;

	send_totalizer(total_increment);                                            // Pass our local total increment to the totalizer function 

	total_increment = 0;                                                        // Reset local and master total increments 
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
	static float epsilon = 0.00001f;                                            // Floating point comparison constant 
	static float totalizer_bucket;                                              // Total accumulator 
	static uint32_t totalizer_tick;												// Tick value for generating the totalizer output pulse width
	static bool totalizer_generated;											// Flag for generating the falling edge of the totalizer output

	totalizer_bucket += current_increment;

	if (sys.total_reset == true)												// User has reset the local total
	{
		totalizer_bucket = 0;                                                   // Reset bucket 
		sys.total_reset = false;                                                // clear reset flag 
	}

	if (totalizer_bucket >= (io.totalizer - epsilon) )						    // Check if total has exceeded totalizer output setting
	{
		process_universal_outputs(totalizer);									// Generate rising edge for totalizer output
		RELAY_HIGH(); 															// Generate rising edge for OPTO22 output
		mb_io.output8 = true;													// Set modbus output 8 when driving OPTO22 totalizer output
		totalizer_tick = HAL_GetTick();											// Get time stamp to generate falling edge

		totalizer_bucket -= io.totalizer;                                       // Reset bucket for next loop  
		totalizer_generated = true;                                             // Set totalizer generated flag so we know we need to generate a falling edge 
	}

	if ( (totalizer_generated == true) && (HAL_GetTick() - io.totalizer_pw >= totalizer_tick) )		// Elapsed time = totalizer pulse width set by user
	{
		toggle_universal_outputs(totalizer);									// Generate falling edge for totalizer output
		RELAY_LOW(); 															// Generate falling edge for OPTO22 output
		mb_io.output8 = false;													// Clear modbus output 8 when driving OPTO22 totalizer output low
		totalizer_generated = false;
	}
}
