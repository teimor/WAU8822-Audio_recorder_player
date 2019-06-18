#include <stdio.h>
#include <string.h>
#include "UART.h"
#include "GPIO.h"
#include "I2C.h"
#include "I2S.h"
#include "SYS.h"
#include "Scankey.h"
#include "diskio.h"
#include "ff.h"
#include <Audio.h>
#include "adpcm4bit.h"
#include "LCD.h"
#include "WAU8822.h"
#include "Utilities.h"

// Set Buffer define 
#define i16PCMBuffSize 504
#define	u8FileBuffSize 512
#define BUFF_LEN    32*12
#define REC_LEN     REC_RATE / 1000

// Set Wav File define
#define WAV_FILE_HEADER_SIZE	52
#define WAV_DATA_SIZE_FOR_ONE	4122
#define AVG_SAMPLES_PER_ONE		51
#define SAMPLE_SIZE 161
#define MIN_RECORD_SAMPLE_SIZE 41055  // 5s
#define MAX_RECORD_SAMPLE_SIZE 492660	// 60s

// Clean Template for Wav Header
const int8_t i8WavHeader[]=
{0x52,0x49,0x46,0x46
,0x00,0x00,0x00,0x00 // File Size  (4-7)
,0x57,0x41,0x56,0x45,0x66,0x6D,0x74,0x20,0x14,0x00,0x00,0x00,0x11,0x00,0x01,0x00,0x40,0x1F,0x00,0x00
,0xDF,0x0F,0x00,0x00,0x00,0x01,0x04,0x00,0x02,0x00,0xF9,0x01,0x66,0x61,0x63,0x74,0x04,0x00,0x00,0x00
,0x00,0x00,0x00,0x00 // Number of samples (48-51)
,0x64,0x61,0x74,0x61 
,0x00,0x00,0x00,0x00 // Data Size (56-59)
};

// New Header after recording
int8_t i8WavHeaderNew[]=
{0x52,0x49,0x46,0x46
,0x00,0x00,0x00,0x00 // File Size  (4-7)
,0x57,0x41,0x56,0x45,0x66,0x6D,0x74,0x20,0x14,0x00,0x00,0x00,0x11,0x00,0x01,0x00,0x40,0x1F,0x00,0x00
,0xDF,0x0F,0x00,0x00,0x00,0x01,0x04,0x00,0x02,0x00,0xF9,0x01,0x66,0x61,0x63,0x74,0x04,0x00,0x00,0x00
,0x00,0x00,0x00,0x00 // Number of samples (48-51)
,0x64,0x61,0x74,0x61 
,0x00,0x00,0x00,0x00 // Data Size (56-59)
};

char FirstRun = '0';
int8_t i8FileName[13]="tmp.wav";

// All buffer, data & pointers as global variables
uint32_t u32DataSize;
int16_t i16PCMBuff[i16PCMBuffSize];
int16_t i16PCMBuff1[i16PCMBuffSize];
uint32_t u32PCMBuffPointer;
uint32_t u32PCMBuffPointer1;

/* Recoder Buffer and its pointer */
uint16_t PcmRecBuff[BUFF_LEN] = {0};
uint32_t u32RecPos_Out = 0;
uint32_t u32RecPos_In = 0;

/* Player Buffer and its pointer */
uint32_t PcmPlayBuff[BUFF_LEN] = {0};
uint32_t u32PlayPos_Out = 0;
uint32_t u32PlayPos_In = 0;

// File system object for logical drive
FATFS FatFs[_DRIVES];

// File buffer
uint8_t	u8FileBuff[u8FileBuffSize];
uint32_t u32FileBuffPointer;

// Audio decoder & encoder
int32_t i32AdpcmStatus=0;
AudioHeader WavFile;

// I2S TX & RX function declaration
void Tx_thresholdCallbackfn1(uint32_t status);
void Tx_thresholdCallbackfn0(uint32_t status);
void Rx_thresholdCallbackfn1(uint32_t status);
void Rx_thresholdCallbackfn0(uint32_t status);

// Swap Buffer functions
uint32_t u32SwapBuffer(void* address)
{
	uint8_t* u8Char;
	u8Char = (uint8_t*) address;
	
	return (*u8Char|*(u8Char+1)<<8|*(u8Char+2)<<16|*(u8Char+3)<<24);
}

uint16_t u16SwapBuffer(void* address)
{
	uint8_t* u8Char;
	u8Char = (uint8_t*) address;
	
	return (*u8Char|*(u8Char+1)<<8);
}


/*
 * Function:  Tx_thresholdCallbackfn 
 * --------------------
 * The callback function when Data in Tx FIFO is less than Tx FIFO Threshold Level.
 * It is used to transfer data in Play Buffer to Tx FIFO.
 *
 *  inputs : status - I2S Status register value.
 * 
 *	outputs: None
 * 
 *  returns: None
 */
void Tx_thresholdCallbackfn0(uint32_t status)
{
	uint32_t i;
	int32_t i32Data;
	
	for	( i = 0; i < 4; i++)
	{
		i32Data=(i16PCMBuff[u32PCMBuffPointer++])<<16;
		_DRVI2S_WRITE_TX_FIFO(i32Data);
		if(--u32DataSize==0)
		{
			DrvI2S_DisableInt(I2S_TX_FIFO_THRESHOLD);
			DrvI2S_DisableTx();
			return;
		}
	}
	if(u32PCMBuffPointer==i16PCMBuffSize)
	{
		DrvI2S_EnableInt(I2S_TX_FIFO_THRESHOLD, Tx_thresholdCallbackfn1);
		if(u32PCMBuffPointer1!=0)
			while(1);
	} 
}
void Tx_thresholdCallbackfn1(uint32_t status)
{
	uint32_t i;
	int32_t i32Data;
	
	for	( i = 0; i < 4; i++)
	{
		i32Data=(i16PCMBuff1[u32PCMBuffPointer1++])<<16;
		_DRVI2S_WRITE_TX_FIFO(i32Data);
		if(--u32DataSize==0)
		{
			DrvI2S_DisableInt(I2S_TX_FIFO_THRESHOLD);
			DrvI2S_DisableTx();
			return;
		}
	}
	if(u32PCMBuffPointer1==i16PCMBuffSize)
	{
		DrvI2S_EnableInt(I2S_TX_FIFO_THRESHOLD, Tx_thresholdCallbackfn0);
		if(u32PCMBuffPointer!=0)
			while(1);		
	}	
}

/*
 * Function:  Rx_thresholdCallbackfn 
 * --------------------
 * The callback function when Data in Rx FIFO is more than Rx FIFO Threshold Level.
 * It is used to transfer data in Rx FIFO to Recode Buffer
 *
 *  inputs : status - I2S Status register value.
 * 
 *	outputs: None
 * 
 *  returns: None
 */
void Rx_thresholdCallbackfn0(uint32_t status)
{
	uint32_t  i;
	int32_t i32Data;
	
	for	( i = 0; i < 4; i++)
	{
		i32Data=_DRVI2S_READ_RX_FIFO()>>16;
		i16PCMBuff[u32PCMBuffPointer++]=i32Data;
	}
	if(u32PCMBuffPointer==i16PCMBuffSize)
	{
		DrvI2S_EnableInt(I2S_RX_FIFO_THRESHOLD, Rx_thresholdCallbackfn1);
		if(u32PCMBuffPointer1!=0)
			while(1);
	} 
}
void Rx_thresholdCallbackfn1(uint32_t status)
{
	uint32_t  i;
	int32_t i32Data;
	
	for	( i = 0; i < 4; i++)
	{
		i32Data=_DRVI2S_READ_RX_FIFO()>>16;
		i16PCMBuff1[u32PCMBuffPointer1++]=i32Data;
	}
	if(u32PCMBuffPointer1==i16PCMBuffSize)
	{
		DrvI2S_EnableInt(I2S_RX_FIFO_THRESHOLD, Rx_thresholdCallbackfn0);
		if(u32PCMBuffPointer!=0)
			while(1);
	}
}

/*
 * Function:  RecordAudioToWav 
 * --------------------
 * Recording the audio from I2S RX(J1 port), decoding it and saving as WAV file
 *
 *  inputs: fileName - file name to save
 * 
 *	outputs: Saves a WAV file in the sdcard with the name from input
 * 
 *  returns: None
 */
void RecordAudioToWav(int8_t fileName[13])
{
	FIL file1;				/* File objects */
	FRESULT res;

	int8_t bytes[4];
	
	unsigned long NumberOfSamples = 0;
	unsigned long DataSize = 0;
	unsigned long FileSize = 0;
	unsigned long SecCalc = 0;
	
	uint32_t SamplesSize = 0;
	
	char RecordFlag = '1';
	
	res	= (FRESULT)disk_initialize(0);	
	if(res)
	{
		print_Line(3,"SD card Error   ");
	}

	res	= f_mount(0, &FatFs[0]);	
	if(res)
	{
		print_Line(3,"File sys Error");
		return;
	}
	// Set WAV file Header
	u32FileBuffPointer=sizeof(i8WavHeader);
	memcpy(u8FileBuff,i8WavHeader,u32FileBuffPointer);
	f_open(&file1, (TCHAR*)fileName, FA_CREATE_ALWAYS | FA_WRITE);
	f_write(&file1, u8FileBuff, u32FileBuffPointer, &u32FileBuffPointer);
	
	u32PCMBuffPointer=0;
	u32PCMBuffPointer1=0;

	InitWAU8822();
	
	if(FirstRun == '0')
	{
		FirstRun = '1';
		return;
	}
	
	/* Eanble play hardware */
	DrvI2S_EnableInt(I2S_RX_FIFO_THRESHOLD, Rx_thresholdCallbackfn0);
	DrvI2S_EnableRx();
	print_Line(2, "Recording      ");
	print_Line(3, "               ");
	
	u32DataSize=0;
	while(RecordFlag == '1')
	{
		if(u32PCMBuffPointer>=i16PCMBuffSize)
		{
			AdpcmEnc4((short*)i16PCMBuff,u32PCMBuffPointer,&i32AdpcmStatus,u8FileBuff);
			u32DataSize+=u32PCMBuffPointer;
			u32PCMBuffPointer=0;
			u32FileBuffPointer=256;
			f_write(&file1, u8FileBuff, u32FileBuffPointer, &u32FileBuffPointer);
		}
		if(u32PCMBuffPointer1>=i16PCMBuffSize)
		{
			AdpcmEnc4((short*)i16PCMBuff1,u32PCMBuffPointer1,&i32AdpcmStatus,u8FileBuff);
			u32DataSize+=u32PCMBuffPointer1;
			u32PCMBuffPointer1=0;

			u32FileBuffPointer=256;
			f_write(&file1, u8FileBuff, u32FileBuffPointer, &u32FileBuffPointer);
		}
		
		// Min record time check
		if(u32DataSize > MIN_RECORD_SAMPLE_SIZE)
		{
			if((ScanKey() != 2) || (u32DataSize >= MAX_RECORD_SAMPLE_SIZE))
			{
				SamplesSize = u32DataSize;
				RecordFlag = '0';
				break;	
			}
		}
	}
	
	// Disable I2S Rx & Close File
	DrvI2S_DisableInt(I2S_RX_FIFO_THRESHOLD);
	DrvI2S_DisableRx();
	f_close(&file1);
	
	while(ScanKey() == 2)
	{
		print_Line(2, "Recording ended");
		print_Line(3, "Please realse  ");
	}
	print_Line(3, "               ");
	
	// Calculate the time recorded
	SecCalc = SamplesSize/(AVG_SAMPLES_PER_ONE*SAMPLE_SIZE);
	
	// Set number of samples
	NumberOfSamples = SamplesSize;
	HexConv(NumberOfSamples, bytes);
	
	i8WavHeaderNew[48] = bytes[3];
	i8WavHeaderNew[49] = bytes[2];
	i8WavHeaderNew[50] = bytes[1];
	i8WavHeaderNew[51] = bytes[0];
	
	// Set Data size
	DataSize = SecCalc * WAV_DATA_SIZE_FOR_ONE;
	HexConv(DataSize, bytes);
	
	i8WavHeaderNew[56] = bytes[3];
	i8WavHeaderNew[57] = bytes[2];
	i8WavHeaderNew[58] = bytes[1];
	i8WavHeaderNew[59] = bytes[0];
	
	// Set File Size
	FileSize = DataSize + WAV_FILE_HEADER_SIZE;
	HexConv(FileSize, bytes);
	
	i8WavHeaderNew[4] = bytes[3];
	i8WavHeaderNew[5] = bytes[2];
	i8WavHeaderNew[6] = bytes[1];
	i8WavHeaderNew[7] = bytes[0];
	
	// Set WAV file Header with correct values
	u32FileBuffPointer=sizeof(i8WavHeaderNew);
	memcpy(u8FileBuff,i8WavHeaderNew,u32FileBuffPointer);
	f_open(&file1, (TCHAR*)fileName, FA_OPEN_EXISTING | FA_WRITE);
	f_write(&file1, u8FileBuff, u32FileBuffPointer, &u32FileBuffPointer);
	f_close(&file1);
}

/*
 * Function:  PlayAudioFromWav 
 * --------------------
 * Playing audio from SDcard, first opening the WAV file, reading the headers and playing the audio.
 * Audio plays from I2S TX(J2)
 *
 *  inputs: fileName - file name to play
 * 
 *	outputs: Sound from I2S, WAU8822, J2 port
 * 
 *  returns: None
 */
void PlayAudioFromWav(int8_t fileName[13])
{
	DIR dir;				/* Directory object */
	FIL file1;				/* File objects */
	char *ptr="\\";
	FRESULT res;
	
	res	= (FRESULT)disk_initialize(0);	
	if(res)
	{
		print_Line(3,"SD card Error");
	}

	res	= f_mount(0, &FatFs[0]);		
	if(res)
	{
		print_Line(3,"File sys Error");
	} 

 	// List direct information	
	f_opendir(&dir, ptr); 
	res = f_open(&file1, (TCHAR*)fileName, FA_READ);
	if(res != 0)
	{
		print_Line(3, "Please Record");
		return;
	}
	
	u32FileBuffPointer=u8FileBuffSize;
	f_lseek(&file1,0);
	f_read(&file1, u8FileBuff, u32FileBuffPointer, &u32FileBuffPointer);
	while(u32FileBuffPointer!=u8FileBuffSize);
	u32FileBuffPointer=0;
	if(memcmp(u8FileBuff+u32FileBuffPointer, "RIFF", 4)==0)
	{
		u32FileBuffPointer+=4;
		printf("RIFF format\n");
		WavFile.u32ChunkSize=u32SwapBuffer(u8FileBuff+u32FileBuffPointer);
		u32FileBuffPointer+=4;
		if(memcmp(u8FileBuff+u32FileBuffPointer, "WAVEfmt ", 8)==0)
		{
			u32FileBuffPointer+=8;
			WavFile.u32Subchunk1Size=u32SwapBuffer(u8FileBuff+u32FileBuffPointer);u32FileBuffPointer+=4;		
		  WavFile.u16AudioFormat=u16SwapBuffer(u8FileBuff+u32FileBuffPointer);u32FileBuffPointer+=2;				
			WavFile.u16NumChannels=u16SwapBuffer(u8FileBuff+u32FileBuffPointer);u32FileBuffPointer+=2;			
			WavFile.u32SampleRate=u32SwapBuffer(u8FileBuff+u32FileBuffPointer);u32FileBuffPointer+=4;
			WavFile.u32ByteRate=u32SwapBuffer(u8FileBuff+u32FileBuffPointer);u32FileBuffPointer+=4;
			WavFile.u16BlockAlign=u16SwapBuffer(u8FileBuff+u32FileBuffPointer);u32FileBuffPointer+=2;
			WavFile.u16BitsPerSample=u16SwapBuffer(u8FileBuff+u32FileBuffPointer);u32FileBuffPointer+=2;
  			u32FileBuffPointer+=4;
			if(memcmp(u8FileBuff+u32FileBuffPointer, "fact", 4)!=0)
				while(1);
			u32FileBuffPointer+=8;
			WavFile.u32SampleNumber=u32SwapBuffer(u8FileBuff+u32FileBuffPointer);u32FileBuffPointer+=4;

			if(memcmp(u8FileBuff+u32FileBuffPointer, "data", 4)==0)
			{
				u32FileBuffPointer+=4;
				WavFile.u8HeaderStatus=1;
				WavFile.u32Subchunk2Size=u32SwapBuffer(u8FileBuff+u32FileBuffPointer);u32FileBuffPointer+=4;
				u32DataSize=WavFile.u32Subchunk2Size;
			}else
				while(1);
		}			  
	}
	print_Line(2, "Playing   ");
	
		u32DataSize=WavFile.u32SampleNumber;
		u32FileBuffPointer=0x28+WavFile.u32Subchunk1Size;
		f_lseek(&file1,u32FileBuffPointer);
	
		u32PCMBuffPointer=0;
		u32PCMBuffPointer1=0;

 		InitWAU8822();
	
	    /* Eanble play hardware */
		DrvI2S_EnableInt(I2S_TX_FIFO_THRESHOLD, Tx_thresholdCallbackfn0);
		DrvI2S_EnableTx();
		printf("Enable I2S and play first sound\n");
	
		while(1)
		{
			if(u32PCMBuffPointer>=i16PCMBuffSize)
			{
				u32FileBuffPointer=u8FileBuffSize;
				f_read(&file1, u8FileBuff, u32FileBuffPointer, &u32FileBuffPointer);
				AdpcmDec4(u8FileBuff,(short*)i16PCMBuff,i16PCMBuffSize);
				u32PCMBuffPointer=0;
			}
			if(u32PCMBuffPointer1>=i16PCMBuffSize)
			{
				AdpcmDec4(u8FileBuff+(i16PCMBuffSize/2+4),(short*)i16PCMBuff1,i16PCMBuffSize);
				u32PCMBuffPointer1=0;
			}
			if(u32DataSize==0)
				break;
		}		

}

/*
 * Function:  InitUART0 
 * --------------------
 * Set UART0 Settings for Peripherals use.
 *
 */
void InitUART0()
{
	STR_UART_T sParam;
	
		/* UART Setting */
    sParam.u32BaudRate 		= 115200;
    sParam.u8cDataBits 		= DRVUART_DATABITS_8;
    sParam.u8cStopBits 		= DRVUART_STOPBITS_1;
    sParam.u8cParity 		= DRVUART_PARITY_NONE;
    sParam.u8cRxTriggerLevel= DRVUART_FIFO_1BYTES;
	/* Select UART Clock Source From 12MHz */
	DrvSYS_SelectIPClockSource(E_SYS_UART_CLKSRC,0); 
	/* Set UART Configuration */
	DrvUART_Open(UART_PORT0,&sParam);	
	/* Set UART Pin */
	DrvGPIO_InitFunction(E_FUNC_UART0);	
}

/*
 * Function:  SetHighSpeedClock 
 * --------------------
 * Unlock the registers, Set High speed clock to 50MHz and lock.
 *
 */
void SetHighSpeedClock()
{
  UNLOCKREG();
	DrvSYS_Open(50000000);
	LOCKREG();	
}

/*
 * --------------------
 * Main Function 
 * --------------------
 */
int32_t main (void)
{
	int8_t keynum;

	SetHighSpeedClock();
	InitUART0();

	OpenKeyPad();
  init_LCD();
	clear_LCD();

	print_Line(0, "Audio Project   ");
	print_Line(1, "8KHz,16bits,Mono");
	print_Line(2, "Idle            ");
	
	while(1)
	{
		keynum = ScanKey(); 	        // scan keypad to input 
		if(keynum == 1)
		{
			PlayAudioFromWav(i8FileName);
		}
		else if(keynum == 2)
		{
			RecordAudioToWav(i8FileName);
		}
		else
		{
			print_Line(2, "Idle            ");
		}
	}
}
