/*******************************************************************************
 *  Process Proximity Sensor #1 
 *    - Capture first time stamp 
 *    - Capture second time stamp 
 *    - Calculate difference between time stamps 
 *    - Calculate speed of proximity sensor 
 *    - Compare to nominal speed in a percentage  
 ******************************************************************************/
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
	prox1.sensor_absent = false;												// Clear sensor flag for detecting absent pulses

	if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1)
	{
		if (prox1.first_time_stamp == 0)                                        // Check for first pulse/sample
			prox1.first_time_stamp = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);	// Get timer count
		else                                                                    // Look for second pulse/sample
		{
			prox1.second_time_stamp = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);	// Get timer count for second pulse

			if(prox1.first_time_stamp > prox1.second_time_stamp)                // Guard for timer roll over
				prox1.time_delta = (0xFFFF - prox1.first_time_stamp) - prox1.second_time_stamp;	// Calculate delta between time stamps if timer rolled over
			else
				prox1.time_delta = prox1.second_time_stamp - prox1.first_time_stamp;  // Calculate delta between time stamps (no roll over)

			if (prox1.time_delta == 0)											// Prevent divide by 0
				prox1.time_delta = 1;

			prox1.speed_samples[prox1.sample_count] = (float)TIMER13_FREQ / (float)prox1.time_delta;	// Calculate proximity sensor frequency
			average_speed += prox1.speed_samples[prox1.sample_count++];			// Add proximity sensor samples
			prox1.calculated_speed = average_speed / prox1.sample_count;		// Calculate average proximity sensor frequency (1000 samples max)

			if ( (prox1.calculated_speed < 0.5) && (cal_modes.auto_zero == true || cal_modes.auto_span == true) )	// Flag belt stops if sensor speed is below 0.5Hz
			{
				cal_modes.display_belt_stops++;									// Increment belt stop counter
				cal_modes.belt_stop_flag = true; 								// Flag belt stop
			}

			if (prox1.nominal_calculated == false && setup.duration_finished == true)	// Once test duration is finished establish baseline frequency for prox sensor
			{
				scale.nominal_speed = prox1.calculated_speed;					// Pass averaged calculated speed to nominal speed
				flash_scale.nominal_speed = scale.nominal_speed;
				prox1.nominal_calculated = true;								// Set nominal calculated speed flag
				write_flash_scale();
			}
			else
			{
				if (scale.nominal_speed == 0)									// Prevent divide by 0
					scale.nominal_speed = 1;

				prox1.speed_delta = ( (scale.nominal_speed - prox1.calculated_speed) / scale.nominal_speed);  // Calculate speed delta from measured to nominal
				scale_calc.belt_speed = scale.theoretical_speed * (1.0 - prox1.speed_delta);	// Calculate actual belt speed
			}

			TIM13->CNT = 0;                                                     // Reload Timer 13
			prox1.first_time_stamp = 0;                                         // Reset first time stamp for next set of samples

			if (prox1.sample_count >= MAX_PROX_SAMPLES)							// Reached 1000 samples
			{
				prox1.sample_count = 0;											// Reset sample count
				average_speed = 0;												// Reset average speed
			}
		}
	}
}
