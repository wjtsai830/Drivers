
#ifndef _TEF6606_H_
#define _TEF6606_H_
/*------------------------------------------------- 
define for I2C info 
--------------------------------------------------*/ 
#define ATomIC_I2C_LENGTH 5 
#define ATomIC_I2C_Addr 0xC0 
#define ATomIC_I2C_SubType 1 
#define ATomIC_I2C_FreqAdr 0x00 
#define ATomIC_I2C_MonoAdr 0x03 
#define ATomIC_I2C_SetupAdr 0x02 
#define ATomIC_I2C_WriteMode_None 0x00 
#define ATomIC_I2C_WriteMode_Preset 0x20 
#define ATomIC_I2C_WriteMode_Search 0x40 
#define ATomIC_I2C_WriteMode_End 0xe0 
 
/*------------------------------------------------- 
define for I2C Read subaddress and mask 
--------------------------------------------------*/ 
#define Read_STATUS 0 
#define Read_LEVEL 1 
#define Read_USN_WAM 2 
#define Read_IFCOUNT 3 
#define Read_ID 4 
#define Read_Bit_IFCOUNT 0x1F 
#define Read_Bit_IFCREADY 0xc0 
#define Read_Bit_USN 0xF0 
#define Read_Bit_WAM 0x0F 
#define Read_Bit_FORCEMONO 0x04 
 
 
#define TEF6606_BYTE3H_DEFAULT 80 


// ==============================================
/*---------------------------------------- 
define for tuning step,frequency range, 
Seek level,ifcounter for all bands. 
-----------------------------------------*/ 
#define FMTuneStep 100  	// unit: KHz 
#define FMSeekStep 100 		// unit: KHz 
#define FMmin 87500 
#define FMmax 108000 
//#define FMlevel 125
#define FMifcount 15		//unit: KHz	 
 
#define LWmax 288 
#define LWmin 144 
#define LWTuneStep 9 
#define LWSeekStep 9 
#define LWlevel 0xB4 
#define LWifcount 15		//uint: 0.1KHz	 
 
#define MWmax 1620
#define MWmin 522 
#define MWTuneStep 9 
#define MWSeekStep 9 
#define MWlevel 0xB4 
#define MWifcount 15		//uint: 0.1KHz 
 
#define SWmax 18135 
#define SWmin 2940 
#define SWTuneStep 5 
#define SWSeekStep 5 
#define SWlevel 0xB4 
#define SWifcount 15		//uint: 0.1KHz 
 
#define OIRTmax 74000 
#define OIRTmin 65000 
#define OIRTTuneStep 10 
#define OIRTSeekStep 10 
#define OIRTlevel 0xB4 
#define OIRTifcount 15 		//uint: KHz	 
 
/*-------------------------------------------------- 
define for initialization of frequency, band, mono 
---------------------------------------------------*/ 
#define Band_Default Band_FM 
#define Frequency_Default FM_Threshold_Freq 
#define Forcemono_Default N 
#define LW_Threshold_Freq LWmin 
#define MW_Threshold_Freq MWmin 
#define FM_Threshold_Freq FMmin 
#define SW_Threshold_Freq SWmin 
#define OIRT_Threshold_Freq OIRTmin 
 
/*------------------------------------------------- 
define for Seek Condition 
--------------------------------------------------*/ 
#define Check_USN_WAM 1				//Check USN and WAM while doing seek action  
#define SeekCondition_FM_Level 0x80		//FM station stop LEVEL condition 
#define SeekCondition_Ifcount 15		//AM,FM station stop IFCounter condition 
#define	SeekCondition_FM_USN 5			//FM station stop USN condition 
#define SeekCondition_FM_WAM 5			//FM station stop WAM condition 
#define StereoSeparation_Level 200		//Stereo indication by LEVEL 
#define SeekCondition_AM_Level 100		//AM station stop LEVEL condition 
#define SeekDelay_FM 50					//FM delay 5ms in seek action 
#define SeekDelay_AM 200				//AM delay 20ms in seek action 
#define PresetNum 6						//Preset stations number 

enum Command
{
	Cmd_Seek_1,
	Cmd_Seek_2,
	Cmd_Seek_3=3,
	Cmd_TunetoPreset,
	Cmd_IsPreset,
	Cmd_PresetSort,
	Cmd_TunetoFreq,
};

 
/*--------------------------------------------------- 
Public enum type definition 
----------------------------------------------------*/ 
enum BAND {Band_MW=0,Band_FM,Band_LW,Band_SW,Band_OIRT};		//Bands supported in TEF6606 
enum DIRECTION {DOWN,UP};									//Seek action direction 
enum FORCEMONO {N,Y};										//Force mono output 
enum STIN {STIN_MONO,STIN_STEREO,FORCE_MONO};				//Stereo indicator 
enum PRESET {PRESET1,PRESET2,PRESET3,PRESET4,PRESET5,PRESET6,NOPRESET}; 
 
/*--------------------------------------------------------------------------------------------------- 
SEEKSTATE is the status while controlling the whole process during the seek action, the detailed 
description is given in TEF6606_SW_GUIDELINE_V1.pdf. 
---------------------------------------------------------------------------------------------------*/ 
enum SEEKSTATE {Seek_Configure,Seek_Idle,Seek_Request,Seek_Change_Freqency,Seek_Wait_Level, 
				Seek_Check_Level,Seek_Check_USN,Seek_Check_WAM,Seek_Wait_IFC,Seek_Check_IFC,Seek_AMS}; 
 
/*------------------------------------------------------------------ 
STATUS_RADIO implements the radio actions and status in the whole 
system which is refered to in resource.c. In version 1.0, the status 
except Status_RDS are involved in AtomIC reference design v1.0. 
-------------------------------------------------------------------*/ 
enum STATUS_RADIO{Status_Idle, Status_Seek, Status_AMS, Status_RDS}; 
 

// ==================================================
 static int tef6606_dev_open(struct inode *inode, struct file *file);
static int tef6606_dev_release(struct inode *inode, struct file *file);
static int tef6606_dev_ioctl(struct file *file,  unsigned int cmd, uint32_t arg);


uint8_t Ftun_CheckLevel(enum BAND band);
void PresetSort(uint8_t count, uint32_t *preset_level, uint32_t *preset_sta) ;

uint8_t writeTunerRegister(uint8_t i2c_adr, uint8_t i2c_subtype, uint32_t sub_adr, uint8_t *write_buffer, uint32_t num);

uint8_t readTunerRegister(uint8_t i2c_adr, uint8_t *read_buffer, uint32_t num);
uint8_t getLevel(void);
uint8_t getIFCounter(uint8_t IFCReady);
uint8_t getUSN(void);  
uint8_t getWAM(void);
enum STIN getStereoIndicator(void);
void setFrequency (enum BAND band, uint32_t freq, uint8_t write_mode);
void setMono(uint8_t ForceMono);
void Ftun_TunetoFrequency(enum BAND band, uint32_t freq, uint8_t write_mode);
void Ftun_Init(enum BAND band, uint32_t freq);   
enum PRESET Ftun_IsPreset (uint32_t freq);
void Ftun_Seek(enum DIRECTION direction, uint8_t ams, uint8_t singlestep);
uint8_t Ftun_CheckLevel(enum BAND band);
void PresetSort(uint8_t count, uint32_t *preset_level, uint32_t *preset_sta);
void Ftun_BandSwitch(void);
void Ftun_StereoMonoSwitch(void);
void Ftun_SaveCurrentStation(uint32_t freq, enum PRESET preset);
void Ftun_TunetoPreset(enum PRESET preset);
#endif
