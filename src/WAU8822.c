#include <NUC1xx.h>
#include "I2C.h"
#include "I2S.h"
#include "GPIO.h"
#include "Utilities.h"

__IO uint32_t EndFlag0 = 0;
uint8_t DataCnt0;

// WAU8822 Device ID
uint8_t Device_Addr0 = 0x1A;
uint8_t Tx_Data0[2];


/*
 * Function:  I2C0_Callback_Tx 
 * --------------------
 * The callback function is to send Device ID and data to CODEC with I2C0
 *
 *  inputs : None
 * 
 *	outputs: None
 * 
 *  returns: None
 */
void I2C0_Callback_Tx(uint32_t status)
{
	// START has been transmitted
	if (status == 0x08)
	{
		DrvI2C_WriteData(I2C_PORT0, Device_Addr0<<1);
		DrvI2C_Ctrl(I2C_PORT0, 0, 0, 1, 0);
	}
	// SLA+W has been transmitted and ACK has been received
	else if (status == 0x18)
	{
		DrvI2C_WriteData(I2C_PORT0, Tx_Data0[DataCnt0++]);
		DrvI2C_Ctrl(I2C_PORT0, 0, 0, 1, 0);
	}
	// SLA+W has been transmitted and NACK has been received
	else if (status == 0x20)
	{
		DrvI2C_Ctrl(I2C_PORT0, 1, 1, 1, 0);
	}	
	// DATA has been transmitted and ACK has been received
	else if (status == 0x28)
	{
		if (DataCnt0 != 2)
		{
			DrvI2C_WriteData(I2C_PORT0, Tx_Data0[DataCnt0++]);
			DrvI2C_Ctrl(I2C_PORT0, 0, 0, 1, 0);
		}
		else
		{
			DrvI2C_Ctrl(I2C_PORT0, 0, 1, 1, 0);
			EndFlag0 = 1;
		}		
	}
	else
	{
		while(1);
	}		
}

/*
 * Function:  I2C_WriteWAU8822 
 * --------------------
 * The function is to write 9-bit data to 7-bit address register of WAU8822 with I2C interface.
 *
 *  inputs : None
 * 
 *	outputs: None
 * 
 *  returns: None
 */
static void I2C_WriteWAU8822(uint8_t u8addr, uint16_t u16data)
{		
	DataCnt0 = 0;
	EndFlag0 = 0;
	
	Tx_Data0[0] = (uint8_t)((u8addr << 1) | (u16data >> 8));
	Tx_Data0[1] = (uint8_t)(u16data & 0x00FF);

	// Install I2C0 call back function for write data to slave
	DrvI2C_InstallCallback(I2C_PORT0, I2CFUNC, I2C0_Callback_Tx);
		
	// I2C0 as master sends START signal
	DrvI2C_Ctrl(I2C_PORT0, 1, 0, 0, 0);
		
	// Wait I2C0 Tx Finish
	while (EndFlag0 == 0);
}

/*
 * Function:  WAU8822_Setup 
 * --------------------
 * The function is to configure CODEC WAU8822 with I2C interface.
 *
 *  inputs : None
 * 
 *	outputs: None
 * 
 *  returns: None
 */
static void WAU8822_Setup(void)
{
	I2C_WriteWAU8822(0,  0x000);   // Reset all registers
	RoughDelay(0x200);
		
	I2C_WriteWAU8822(1,  0x02F);        
	I2C_WriteWAU8822(2,  0x1B3);	// Enable L/R Headphone, ADC Mix/Boost, ADC
	I2C_WriteWAU8822(3,  0x07F);	// Enable L/R main mixer, DAC
	I2C_WriteWAU8822(4,  0x010);	// 16-bit word length, I2S format, Stereo
	I2C_WriteWAU8822(5,  0x000);	// Companding control and loop back mode (all disable)
	I2C_WriteWAU8822(6,  0x1ED);	// Divide by 12, 8K
	I2C_WriteWAU8822(7,  0x00A);	// 8K for internal filter cofficients
	I2C_WriteWAU8822(10, 0x008);	// DAC softmute is disabled, DAC oversampling rate is 128x
	I2C_WriteWAU8822(14, 0x108);	// ADC HP filter is disabled, ADC oversampling rate is 128x
	I2C_WriteWAU8822(15, 0x1EF);	// ADC left digital volume control
	I2C_WriteWAU8822(16, 0x1EF);	// ADC right digital volume control
	I2C_WriteWAU8822(43, 0x010);   
	I2C_WriteWAU8822(44, 0x000);	// LLIN/RLIN is not connected to PGA
	I2C_WriteWAU8822(45, 0x150);	// LLIN connected, and its Gain value
	I2C_WriteWAU8822(46, 0x150);	// RLIN connected, and its Gain value
	I2C_WriteWAU8822(47, 0x007);	// LLIN connected, and its Gain value
	I2C_WriteWAU8822(48, 0x007);	// RLIN connected, and its Gain value
	I2C_WriteWAU8822(49, 0x047);
	I2C_WriteWAU8822(50, 0x001);	// Left DAC connected to LMIX
	I2C_WriteWAU8822(51, 0x000);	// Right DAC connected to RMIX
 	I2C_WriteWAU8822(54, 0x139);	// LSPKOUT Volume
	I2C_WriteWAU8822(55, 0x139);	// RSPKOUT Volume

	DrvGPIO_Open(E_GPE,14, E_IO_OUTPUT);	
	DrvGPIO_ClrBit(E_GPE,14);
}

void InitWAU8822(void)
{
	S_DRVI2S_DATA_T st;
	// Tri-state for FS and BCLK of CODEC
	DrvGPIO_Open(E_GPC, 0, E_IO_OPENDRAIN);
	DrvGPIO_Open(E_GPC, 1, E_IO_OPENDRAIN);
	DrvGPIO_SetBit(E_GPC, 0);
	DrvGPIO_SetBit(E_GPC, 1);
	
	// Set I2C0 I/O
  DrvGPIO_InitFunction(E_FUNC_I2C0);    	
	// Open I2C0, and set clock = 100Kbps
	DrvI2C_Open(I2C_PORT0, 100000);		
	// Enable I2C0 interrupt and set corresponding NVIC bit
	DrvI2C_EnableInt(I2C_PORT0);
	// Configure CODEC
	WAU8822_Setup();

	// Configure I2S
	st.u32SampleRate			= 8000;
	st.u8WordWidth				= DRVI2S_DATABIT_16;
	st.u8AudioFormat			= DRVI2S_STEREO;  		
	st.u8DataFormat				= DRVI2S_FORMAT_I2S;   
	st.u8Mode							= DRVI2S_MODE_SLAVE;
	st.u8TxFIFOThreshold	= DRVI2S_FIFO_LEVEL_WORD_4;
	st.u8RxFIFOThreshold	= DRVI2S_FIFO_LEVEL_WORD_4;
	DrvI2S_SelectClockSource(0);
	DrvI2S_Open(&st);
	// Set I2S I/O
  DrvGPIO_InitFunction(E_FUNC_I2S); 
	// Set MCLK and enable MCLK
	DrvI2S_SetMCLKFreq(12000000);	
	DrvI2S_EnableMCLK();
}

