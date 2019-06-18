/*---------------------------------------------------------------------------------------------------------*/
/*                                                                                                         */
/* Copyright(c) 2009 Nuvoton Technology Corp. All rights reserved.                                         */
/*                                                                                                         */
/*---------------------------------------------------------------------------------------------------------*/
#ifndef Audio_H
#define Audio_H

#include "NUC1xx.h"

/*---------------------------------------------------------------------------------------------------------*/
/*  Audio Format						                                                       */
/*---------------------------------------------------------------------------------------------------------*/
typedef enum 
{
	E_AudioFormat_PCM 	= 1,	
}E_AudioFormat;

/*---------------------------------------------------------------------------------------------------------*/
/*  Number of Channel						                                                       */
/*---------------------------------------------------------------------------------------------------------*/
typedef enum 
{
	E_NumChannels_Mono 		= 1,
	E_NumChannels_Stereo 	= 2,	
}E_NumChannels;

/*---------------------------------------------------------------------------------------------------------*/
/*  Audio file Header						                                                       */
/*---------------------------------------------------------------------------------------------------------*/

typedef struct
{
  uint32_t      u32ChunkSize;
	uint32_t			u32Subchunk1Size;
	uint16_t			u16AudioFormat;
	uint16_t			u16NumChannels;
	uint32_t			u32SampleRate;
	uint32_t			u32ByteRate;
	uint16_t			u16BlockAlign;
	uint16_t			u16BitsPerSample;
	uint32_t			u32Subchunk2Size;
	uint32_t			u32SampleNumber;
	uint8_t				u8HeaderStatus;
}AudioHeader;
#endif
