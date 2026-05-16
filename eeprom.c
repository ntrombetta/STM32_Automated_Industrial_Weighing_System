/*******************************************************************************
 *  Save Totals
 *  	- Called every one second to continuously save the local and master
 *  	  totals to the external EEPROM IC
 *  	- Once we reach the end of a 4KB sector we erase the next one to write
 *  	  new data to. This ensures we always have backup data as a fail safe
 *  	  	- Not called during simulated belt operations, calibrations, etc.
 *  	- Local total is saved to addresses 0x0 - 0x1FFFFF (blocks 0-31)
 *  	- Master total is saved to addresses 0x200000 - 0x3FFFFF (blocks 32-63)
 ******************************************************************************/
void save_totals(void)
{
	static uint8_t eeprom_write = EEPROM_PAGE_WRITE;                            // Enable EEPROM page write command 
	static uint8_t sector_erase = EEPROM_SECTOR_ERASE;                          // Enable EEPROM sector erase command 
	static uint8_t eeprom_status;                                               // EEPROM status bit during operation 
	static uint32_t start_timer;                                                // Timeouts 
	static float last_local_total;                                              // Last saved local total 
	static bool local_updated;                                                  // Local total updated flag 
	static bool local_zero_saved;                                               // First local total 0 saved flag 

	uint8_t local_address[3];                                                   // Address buffer for local total 
	uint8_t master_address[3];                                                  // Address buffer for master total 

	local_address[0] = (local_index >> 16) & 0xFF;                              // Pass local and master indexes to respective 3 byte buffers so addresses are aligned for EEPROM commands 
	local_address[1] = (local_index >> 8) & 0xFF;
	local_address[2] = local_index & 0xFF;

	master_address[0] = (master_index >> 16) & 0xFF;
	master_address[1] = (master_index >> 8) & 0xFF;
	master_address[2] = master_index & 0xFF;

	if (local_updated == false)													// Check if we need to update local total
	{
		last_local_total = scale_calc.total;									// Update for next loop
		local_updated = true;													// Set local total updated flag
	}
	else if (scale_calc.total <= last_local_total)								// Total has not changed since last loop; exit
	{
		if (scale_calc.total == 0 && local_zero_saved == false)                 // Save very first 0 incase user resets total 
		{
			local_updated = false;                                              // Clear local updated flag
			local_zero_saved = true;                                            // Set zero saved flag 
		}
		else                                                                    
			return;
	}
	else if (scale_calc.total < 0)												// Exit if local total hasn't been calculated yet
		return;
	else																		// Local total has incremented since last loop; save to EEPROM
		local_updated = false;													// Clear local updated flag

	enable_eeprom_write();														// Enable writes to EEPROM

	EEPROM_CS_LOW();                                                            // Send page write command for local total
	HAL_SPI_Transmit(&hspi3, &eeprom_write, 1, 10);
	HAL_SPI_Transmit(&hspi3, local_address, 3, 10);			 					// Send 24-bit local total address
	HAL_SPI_Transmit(&hspi3, (uint8_t *)&scale_calc.total, sizeof(scale_calc.total), 10); // Write local total value
	EEPROM_CS_HIGH();

	start_timer = HAL_GetTick();												// Timer for write timeouts

	eeprom_status = read_eeprom_status();										// Read EEPROM status bit

	while (eeprom_status)														// Loop until write in progress bit is cleared
	{
		eeprom_status = read_eeprom_status();									// Read EEPROM status bit

		if (HAL_GetTick() - 1000 > start_timer)									// Timeout after 10ms
			return;
	}

	local_index += 4; 															// Increment local total address index for next write

	if (local_index >= LOCAL_ADDRESS_END)                                       // We hit the end of the local total block; wrap to start of chip 
		local_index = EEPROM_START_ADDRESS;

	if ( (local_index % SECTOR_SIZE) == 0 )                                     // We filled an entire sector; Implement rolling sector algo and erase next sector 
	{
		local_address[0] = (local_index >> 16) & 0xFF;
		local_address[1] = (local_index >> 8) & 0xFF;
		local_address[2] = local_index & 0xFF;

		enable_eeprom_write();													// Enable writes to EEPROM
		EEPROM_CS_LOW();                                                        // Start EEPROM write sequence
		HAL_SPI_Transmit(&hspi3, &sector_erase, 1, 10); 						// Send sector erase command
		HAL_SPI_Transmit(&hspi3, local_address, 3, 10);							// Erase next sector
		EEPROM_CS_HIGH();                                                       // End EEPROM write sequence

		start_timer = HAL_GetTick();											// Timer for write timeouts

		eeprom_status = read_eeprom_status();									// Read EEPROM status bit

		while (eeprom_status)													// Loop until write in progress bit is cleared
		{
			eeprom_status = read_eeprom_status();								// Read EEPROM status bit

			if (HAL_GetTick() - 1000 > start_timer)								// Timeout after 10ms
				return;
		}
	}

	if (scale_calc.master_total < 0)											// Exit if master total hasn't been calculated yet
		return;

	enable_eeprom_write();                                                      // Enable writes to EEPROM

	EEPROM_CS_LOW();                                                            // Start EEPROM write sequence
	HAL_SPI_Transmit(&hspi3, &eeprom_write, 1, 10); 							// Send page write command
	HAL_SPI_Transmit(&hspi3, master_address, 3, 10);
	HAL_SPI_Transmit(&hspi3, (uint8_t *)&scale_calc.master_total, sizeof(scale_calc.master_total), 10); 	// Write most recent master total
	EEPROM_CS_HIGH();                                                           // End EEPROM write sequence

	start_timer = HAL_GetTick();												// Timer for write timeouts

	eeprom_status = read_eeprom_status();										// Read EEPROM status bit

	while (eeprom_status)														// Loop until write in progress bit is cleared
	{
		eeprom_status = read_eeprom_status();									// Read EEPROM status bit

		if (HAL_GetTick() - 10 > start_timer)									// Timeout after 100ms
			return;
	}

	master_index += 4; 															// Increment master total address index for next write

	if (master_index >= MASTER_ADDRESS_END)                                     // We hit the end of the master total block; wrap back to the start of the second half of the chip 
		master_index = MASTER_ADDRESS_START;

	if ( (master_index % SECTOR_SIZE) == 0 )                                    // We filled an entire sector implement rolling sector algo and erase the next one 
	{
		master_address[0] = (master_index >> 16) & 0xFF;
		master_address[1] = (master_index >> 8) & 0xFF;
		master_address[2] = master_index & 0xFF;

		enable_eeprom_write();													// Enable writes to EEPROM
		EEPROM_CS_LOW();                                                        // Start EEPROM write sequence
		HAL_SPI_Transmit(&hspi3, &sector_erase, 1, 10); 						// Send sector erase command
		HAL_SPI_Transmit(&hspi3, master_address, 3, 10);						// Erase next sector
		EEPROM_CS_HIGH();                                                       // End EEPROM write sequence

		start_timer = HAL_GetTick();											// Timer for write timeouts

		eeprom_status = read_eeprom_status();									// Read EEPROM status bit

		while (eeprom_status)													// Loop until write in progress bit is cleared
		{
			eeprom_status = read_eeprom_status();								// Read EEPROM status bit

			if (HAL_GetTick() - 1000 > start_timer)								// Timeout after 10ms
				return;
		}
	}
}

/*******************************************************************************
 *  Read Scale Totals Inside External EEPROM
 *  	- Called inside the binary search algorithm below on start up to find
 *  	  the next memory address for the master and local totals
 ******************************************************************************/
void read_totals(uint32_t address, uint8_t *dest)
{
	static uint8_t eeprom_read = EEPROM_READ;                                   // EEPROM read command  
	static uint8_t eeprom_address[3];                                           // 3 byte address buffer 

	eeprom_address[0] = (address >> 16) & 0xFF;                                 // Pass index to buffer so we are aligned to 3-bytes 
	eeprom_address[1] = (address >> 8) & 0xFF;
	eeprom_address[2] = address & 0xFF;

	enable_eeprom_write();

	EEPROM_CS_LOW();															// Start SPI transfer sequence
	HAL_SPI_Transmit(&hspi3, (uint8_t *)&eeprom_read, 1, 10);					// Send read command
	HAL_SPI_Transmit(&hspi3, eeprom_address, 3, 10);							// Start reading first page of local total
	HAL_SPI_Receive(&hspi3, dest, 4, 10);										// Pass local total from EEPROM
	EEPROM_CS_HIGH();
}

/*******************************************************************************
 *  Find External EEPROM Address
 *  	- Performs linear search for each 4KB sector in the 4MB EEPROM chip
 *  	- If the last 4 bytes in a sector are empty we know our address is here
 *  	- We then perform a binary search inside this sector to find our last
 *  	  address
 ******************************************************************************/
uint32_t find_eeprom_address(uint32_t start, uint32_t end)
{
	uint32_t addr = 0;															// EEPROM address
	uint32_t val = 0;															// Data in EEPROM address
	uint32_t nxt_val = 0;														// Data in the next EEPROM address
	uint32_t low = 0;															// Lower boundary of binary search
	uint32_t high = 0;															// Upper boundary of binary search
	uint32_t mid = 0;															// Middle of binary search
	uint32_t result = start;													// Address found from binary search

	for (addr = start + 4092; addr <= end; addr += 4096)						// Check the last 4 bytes of each 4KB sector; These will be empty in the current sector
	{
		read_totals(addr, (uint8_t*)&val);										// Read data at each address

		if (val == 0xFFFFFFFF)													// Empty data; Now perform the binary search
		{
			low = (addr < start + 4092) ? start : (addr - 4092);				// Check if we are in the first sector; If not move to previous
			high = addr;

			while (low <= high) 												// Perform binary search only in this sector
			{
				mid = (low + (high - low) / 2) & ~0x03;							// Break searches up into halves and align with 4 byte memory addresses
				read_totals(mid, (uint8_t*)&val);								// Read address

				if (val != 0xFFFFFFFF) 											// Valid data is here
				{
					read_totals(mid + 4, (uint8_t*)&nxt_val);					// Check the very next byte to see if it is also valid

					if (nxt_val == 0xFFFFFFFF) 									// Nothing in next byte; This our address we want
						return mid;												// Return correct address

					result = mid;												// Still valid data; Narrow down binary search
					low = mid + 4;												// Narrow search bottom window
				}
				else
					high = (mid >= 4) ? mid - 4 : start;						// Make sure we aren't at the start of the sector and work the search backwards
			}
			return result;														// This is the address we want
		}
	}
	return start;																// Search came back completely empty; Chip is empty; Address = start of chip
}

/*******************************************************************************
 *  Get Totals
 *  	- Calls find_eeprom_address then increments the local and master address
 *  	  indexes for the next writes
 ******************************************************************************/
void get_totals(void)
{
	local_index = find_eeprom_address(0, HALF_EEPROM - 3);                      // Find last saved local total address 
	master_index = find_eeprom_address(HALF_EEPROM, EEPROM_SIZE - 3);           // Find last saved master total address 

	read_totals(local_index, (uint8_t*)&scale_calc.total);                      // Get local total value from EEPROM

	if (isnan(scale_calc.total))                                                // Should never be here unless chip is empty 
	{
		read_totals(local_index - 4, (uint8_t*)&scale_calc.total);              // Read previous address just incase

		if (isnan(scale_calc.total))                                            // Chip is empty 
			scale_calc.total = 0;                                               // Set local total to 0 as this is the first time the unit has been powered up 
	}
	else
		local_index += 4;                                                       // We found a valid address with data; Increment local address for next write 

	read_totals(master_index, (uint8_t*)&scale_calc.master_total);              // Repeat steps for master total 

	if (isnan(scale_calc.master_total))
	{
		read_totals(master_index - 4, (uint8_t*)&scale_calc.master_total);

		if (isnan(scale_calc.master_total))
			scale_calc.master_total = 0;
	}
	else
		master_index += 4;
}
