#include <linux/kernel.h>  
#include <linux/module.h>  
#include <linux/fs.h>  
#include <linux/slab.h>  
#include <linux/init.h>  
#include <linux/list.h>  
#include <linux/i2c.h>  
#include <linux/i2c-dev.h>  
#include <linux/delay.h>  
#include <linux/miscdevice.h> 
#include <linux/cdev.h>
#include <linux/gpio.h>
#include <mach/common.h>
#include <mach/hardware.h>
#include <mach/mxc_dvfs.h>
#include <mach/memory.h>

#include "radio-tef6606.h" 
#define VERSION "v1.0.3"
#define I2C_DEV_NAME "tef6606"  
#define DEV_NAME "radio" 
#define TEF6606_ALERT(f, s...) \
    do { \
        printk(KERN_ALERT "TEF6606 " f, ## s); \
    } while(0)

#define AM_SET_STERO_LEVEL	10
#define AM_SET_LEVEL		9
#define TEF6606_GET_WAM		8
#define TEF6606_GET_USN		7
#define TEF6606_GET_LEVLE	6
#define FM_SET_LEVEL		5
#define FM_SET_WAM		4
#define FM_SET_USN		3
#define SET_FREQUENCY		1
#define SET_AM_FM		0

int FMlevel;
int FMWAM;
int FMUSN;
int AM_SEEK_LEVEL=100;
int AM_STERO_LEVEL=110;
uint8_t Tuner_Read[ATomIC_I2C_LENGTH];    //Tuner reading 5 registers   

// ===============================================================================
uint32_t seek_count;   //Timer for waiting action while seeking stations   
uint8_t status;        //Bit4 ~ bit7 indicates Source selected like "Radio","CD","USB","AUX",    
                            //while bit0 ~ bit3 indicates status of related source like "Seek","AMS".   
   
// =================================================================================================   
// Public data   
// =================================================================================================   
uint32_t CurrentFrequency = Frequency_Default;    //current tuning frequency   
enum BAND CurrentMode = Band_Default;           //current tuning band   
uint8_t CurrentForceMono = Forcemono_Default;     //FM band stereo disabled   
   
//Save frequency of different band, restore when switching band   
static uint32_t LastBandFreq[5] =    
{LW_Threshold_Freq, MW_Threshold_Freq, FM_Threshold_Freq, SW_Threshold_Freq, OIRT_Threshold_Freq};   
   
static uint32_t Preset_Freq[5][PresetNum] =    
{   
    {LW_Threshold_Freq,LW_Threshold_Freq,LW_Threshold_Freq,LW_Threshold_Freq,LW_Threshold_Freq,LW_Threshold_Freq},   
    {MW_Threshold_Freq,MW_Threshold_Freq,MW_Threshold_Freq,MW_Threshold_Freq,MW_Threshold_Freq,MW_Threshold_Freq},   
    {FM_Threshold_Freq,FM_Threshold_Freq,FM_Threshold_Freq,FM_Threshold_Freq,FM_Threshold_Freq,FM_Threshold_Freq},   
    {SW_Threshold_Freq,SW_Threshold_Freq,SW_Threshold_Freq,SW_Threshold_Freq,SW_Threshold_Freq,SW_Threshold_Freq},   
    {OIRT_Threshold_Freq,OIRT_Threshold_Freq,OIRT_Threshold_Freq,OIRT_Threshold_Freq,OIRT_Threshold_Freq,OIRT_Threshold_Freq}   
 };                                             //Default Setting of preset stations   
   
uint8_t PresetFlag = 0;                         //Preset station Flag   
enum SEEKSTATE SeekState = Seek_Configure;      //Seek Status while doing seek action   
// ================================================================================== 
static struct i2c_device_id tef6606_id[] = {  
    {I2C_DEV_NAME,0}, 
    {}  
};  
  
MODULE_DEVICE_TABLE(i2c, tef6606_id);  
   
static struct i2c_client *tef6606_client;  

static struct file_operations tef6606_fops = { 
	.owner = THIS_MODULE,  
	.open		= tef6606_dev_open,
	.release	= tef6606_dev_release,
	.unlocked_ioctl = tef6606_dev_ioctl,
}; 

static struct miscdevice tef6606_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = DEV_NAME,
	.fops = &tef6606_fops,
};

static void tef6606_pwron(void)
{
	Ftun_Init(Band_FM,FMmin);
	/*FMlevel = 120;
	FMWAM = 5;
	FMUSN = 5;*/
	FMlevel = 92;
	FMWAM = 3;
	FMUSN = 3;
}
uint8_t mkd_set_freq(uint32_t freq)
{
	int level = 0;
	Ftun_TunetoFrequency(CurrentMode,freq,ATomIC_I2C_WriteMode_None);
	msleep(100);
	level = getLevel();
	if ( CurrentMode==Band_FM ) 
	{
		int wam = getWAM();
		int usn = getUSN();
		TEF6606_ALERT("Freq(%d) level:%d\twam:%d\tusn:%d\n",freq,level,wam,usn);
		if ( level >= FMlevel && wam <= FMWAM && usn <= FMUSN )
		{
			TEF6606_ALERT("Success!!\n");
			return 1;
		}
		else
		{
			TEF6606_ALERT("Fail!!\n");
			return 0;
		}
	}
	if ( CurrentMode==Band_MW )
	{
		level = getLevel();
		TEF6606_ALERT("Freq(%d) level:%d\n",freq,level);
		if ( level >= AM_STERO_LEVEL )
		{
			TEF6606_ALERT("AM Stero Success!!\n");
			return 2;
		}
		else if ( level >= AM_SEEK_LEVEL )
		{
			TEF6606_ALERT("AM Success!!\n");
			return 1;
		}
		else
		{
			TEF6606_ALERT("Fail!!\n");
			return 0;
		} 
	} 
}

static int tef6606_dev_ioctl(struct file *file,  unsigned int cmd, uint32_t arg) 
{  
/*	
	switch (cmd)
	{
		case Cmd_Seek_1:
			Ftun_Seek(arg,0,0);
		break;
		case Cmd_Seek_2:
			Ftun_Seek(UP,arg,0);
		break;
		case Cmd_Seek_3:
			Ftun_Seek(UP,0,arg);
		break;
		case Cmd_TunetoPreset:
			Ftun_TunetoPreset(arg);	
		break;
		case Cmd_IsPreset:
			Ftun_IsPreset(arg);
		break;
		case Cmd_PresetSort:
		//	Ftun_PresetSort();
		break;
		case Cmd_TunetoFreq:
			Ftun_TunetoFrequency(CurrentMode,arg,ATomIC_I2C_WriteMode_None);	
		break;
	}
	return 0;

	
	int level = 0;
	uint32_t freq = arg;
	Ftun_TunetoFrequency(CurrentMode,freq,ATomIC_I2C_WriteMode_None);
	msleep(100);
	level = Ftun_CheckLevel(Band_FM);
	TEF6606_ALERT("Over level:%d\n",level);
*/
	int temp = 0;
	switch (cmd)
	{
		case SET_AM_FM:
			if ( arg == Band_MW )
			{	
				Ftun_Init(Band_MW,MWmin);
				CurrentMode=Band_MW;
			}
			if ( arg == Band_FM )
			{	
				Ftun_Init(Band_FM,FMmin);
				CurrentMode=Band_FM;
			}
			TEF6606_ALERT("Set Mode : %d\n",arg);
		break;
		case SET_FREQUENCY:
			temp = mkd_set_freq(arg);
		break;
		case FM_SET_LEVEL:
			FMlevel = arg;
			TEF6606_ALERT("Set level : %d\n",arg);
		break;
		case FM_SET_WAM:
			FMWAM = arg;
			TEF6606_ALERT("Set WAM : %d\n",arg);
		break;
		case FM_SET_USN:
			FMUSN = arg;
			TEF6606_ALERT("Set USN : %d\n",arg);
		break;
		case TEF6606_GET_WAM:
			if ( arg == Band_FM )
				temp = getWAM();
			TEF6606_ALERT("Get WAM : %d\n",temp);
		break;
		case TEF6606_GET_USN:
			if ( arg == Band_FM )
				temp = getUSN();
			TEF6606_ALERT("Get USN : %d\n",temp);
		break;
		case TEF6606_GET_LEVLE:
			temp = getLevel();
			TEF6606_ALERT("Get LEVEL : %d\n",temp);
		break;
		case AM_SET_STERO_LEVEL:
			AM_STERO_LEVEL = arg;
			TEF6606_ALERT("Set AM STERO Level : %d\n",arg);		
		break;
		case AM_SET_LEVEL:
			AM_SEEK_LEVEL = arg;
			TEF6606_ALERT("Set AM Seek Level : %d\n",arg);
		break;
	}
	
//	readTunerRegister(ATomIC_I2C_Addr, Tuner_Read, ATomIC_I2C_LENGTH);
//	for( i = 0 ; i < ATomIC_I2C_LENGTH ; i++ )
//		TEF6606_ALERT("Reg[%d]:%x\n",i,Tuner_Read[i]); 
	return temp;
} 


static int tef6606_dev_open(struct inode *inode, struct file *file)
{
	TEF6606_ALERT("tef6606_open\n");
	return nonseekable_open(inode, file);
}

static int tef6606_dev_release(struct inode *inode, struct file *file)
{
	TEF6606_ALERT("tef6606_release\n");
	return 0;
}

static int tef6606_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)  
{   
    tef6606_client = client;  
    TEF6606_ALERT("tef6606_i2c_probe\n");
    misc_register(&tef6606_device);
    tef6606_pwron();

    return 0;  
}  
  
static int tef6606_i2c_remove(struct i2c_client *client)  
{
    if( tef6606_client != NULL )
      i2c_unregister_device(tef6606_client);
    
    misc_deregister(&tef6606_device);
    return 0;  
}
  
static struct i2c_driver tef6606_driver = {  
    .driver   = {  
        .name  = "tef6606 i2c driver",  
        .owner = THIS_MODULE,  
    },  
    .probe    = tef6606_i2c_probe,  
    .remove   = __devexit_p(tef6606_i2c_remove),  
    .id_table = tef6606_id,  
};  
  
static int __init tef6606_i2c_init(void)  
{  
    TEF6606_ALERT("tef6606_i2c_init (%s) \n",VERSION);
    return i2c_add_driver(&tef6606_driver);  
}  
  
static void __exit tef6606_i2c_exit(void)  
{  
    i2c_del_driver(&tef6606_driver);  
}  
  
/*-----------------------------------------------------------------------  
Function name:  writeTunerRegister  
Input:          i2c_adr,i2c_subtype,sub_adr,*write_buffer,num  
Output:         N/A  
Description:    Write tuner registers  
------------------------------------------------------------------------*/   
uint8_t writeTunerRegister(uint8_t i2c_adr, uint8_t i2c_subtype, uint32_t sub_adr, uint8_t *write_buffer, uint32_t num)   
{
	uint8_t cmd = 0x00;
//	cmd |= ((i2c_subtype&0x7) << 3) + ( sub_adr & 0x1F);	   
	int i;	
	uint8_t temp_buf[20] = {0};
	temp_buf[0] = sub_adr;
	for(i= 0; i < num ;i++ )
		temp_buf[i+1] = write_buffer[i];
	return i2c_master_send(tef6606_client, temp_buf , num+1);
}

/*-----------------------------------------------------------------------  
Function name:  readTunerRegister  
Input:          i2c_adr,*read_buffer,num  
Output:         N/A  
Description:    Read tuner registers  
------------------------------------------------------------------------*/   
uint8_t readTunerRegister(uint8_t i2c_adr, uint8_t *read_buffer, uint32_t num)   
{      
	return i2c_master_recv(tef6606_client, read_buffer ,num);
}   
/*-----------------------------------------------------------------------  
Function name:  getLevel  
Input:          N/A  
Output:         Level  
Description:    Get Level  
------------------------------------------------------------------------*/   
uint8_t getLevel(void)   
{   
    if(readTunerRegister(ATomIC_I2C_Addr, Tuner_Read, ATomIC_I2C_LENGTH))   
    return(Tuner_Read[Read_LEVEL]);   
       
}   
   
/*-----------------------------------------------------------------------  
Function name:  getIFCounter  
Input:          IFCReady  
Output:         IFcount_result  
Description:    Get IFCOUNT result while IFCReady is true (32ms IFCounter  
                result available), or return IFCounter result status.  
------------------------------------------------------------------------*/   
uint8_t getIFCounter(uint8_t IFCReady)    
{      
    readTunerRegister(ATomIC_I2C_Addr, Tuner_Read, ATomIC_I2C_LENGTH);   
    if(IFCReady)   
        return((Tuner_Read[Read_IFCOUNT] & Read_Bit_IFCOUNT)*5+5);   
    else   
        return((Tuner_Read[Read_IFCOUNT] & Read_Bit_IFCREADY));   
   
}   
   
/*-----------------------------------------------------------------------  
Function name:  getUSN  
Input:          N/A  
Output:         USN  
Description:    Get USN indication  
------------------------------------------------------------------------*/   
uint8_t getUSN(void)   
{   
    readTunerRegister(ATomIC_I2C_Addr, Tuner_Read, ATomIC_I2C_LENGTH);   
    return((Tuner_Read[Read_USN_WAM] & Read_Bit_USN) >> 4);   
}   
   
/*-----------------------------------------------------------------------  
Function name:  getWAM  
Input:          N/A  
Output:         WAM  
Description:    Get WAM indication  
------------------------------------------------------------------------*/   
uint8_t getWAM(void)   
{   
    readTunerRegister(ATomIC_I2C_Addr, Tuner_Read, ATomIC_I2C_LENGTH);   
    return(Tuner_Read[Read_USN_WAM] & Read_Bit_WAM);   
}   
   
/*-----------------------------------------------------------------------  
Function name:  getStereoIndicator  
Input:          N/A  
Output:         STIN  
Description:    Get Stereo Pilot and judge stereo output according to  
                signal level configured in stereo blend control of weak  
                signal processing function.  
------------------------------------------------------------------------*/   
enum STIN getStereoIndicator(void)   
{   
    readTunerRegister(ATomIC_I2C_Addr, Tuner_Read, ATomIC_I2C_LENGTH);   
    if ((Tuner_Read[Read_STATUS] >> Read_Bit_FORCEMONO) & 0x1) //Read STIN indicator   
        if(getLevel() > StereoSeparation_Level)                  //Judge Stereo by LEVEL   
                                                                //according to SNC setting   
            return(STIN_STEREO);   
    return(STIN_MONO);   
}
/*-----------------------------------------------------------------------  
Function name:  setFrequency  
Input:          band,freqency,write_mode  
Output:         N/A  
Description:    Set tuning band and frequency  
------------------------------------------------------------------------*/   
void setFrequency (enum BAND band, uint32_t freq, uint8_t write_mode)    
{   uint8_t freq_array[2];   
    uint8_t factor,setband;   
    uint16_t unit;   
    switch (band)   
    {   
    case Band_LW:      
    case Band_MW:       factor = 1;   
                        unit = 1;   
                        setband = 0;   
                        break;   
    case Band_FM:       factor = 20;   
                        unit = 1000;   
                        setband = 1;   
                        break;   
    case Band_SW:       factor = 1;   
                        unit = 5;   
                        setband = 2;   
                        break;   
    case Band_OIRT:     factor = 100;   
                        unit = 1000;   
                        setband = 3;   
                        break;   
    default: break;   
    }   
       
    if (write_mode == ATomIC_I2C_WriteMode_End)   
        {   freq_array[0] = 0x00;   
            writeTunerRegister (ATomIC_I2C_Addr, ATomIC_I2C_SubType, write_mode, freq_array, 1);   
        }   
    else   
    {   
        freq_array[1] = (freq*factor/unit) & 0xff;    
        freq_array[0] = ((freq*factor/unit)>>8) | (setband << 5);    
        writeTunerRegister (ATomIC_I2C_Addr, ATomIC_I2C_SubType, write_mode|ATomIC_I2C_FreqAdr, freq_array, 2);   
    }   
}   
   
/*-----------------------------------------------------------------------  
Function name:  setMono  
Input:          ForceMono  
Output:         N/A  
Description:    Switch Forcemono on/off  
------------------------------------------------------------------------*/   
void setMono(uint8_t ForceMono)   
{   
	uint8_t monoreg;   
    if (ForceMono == 0)   
        monoreg = TEF6606_BYTE3H_DEFAULT;   
    else if (ForceMono == 1)   
        monoreg = TEF6606_BYTE3H_DEFAULT | 0X10;   
    writeTunerRegister (ATomIC_I2C_Addr, ATomIC_I2C_SubType, ATomIC_I2C_WriteMode_None|ATomIC_I2C_MonoAdr, &monoreg, 1);   
               
}

/*-----------------------------------------------------------------------  
Function name:  Ftun_TunetoFrequency  
Input:          band,frequency,tuner_write_mode  
Output:         N/A  
Description:    Tune to target frequency.  
------------------------------------------------------------------------*/   
void Ftun_TunetoFrequency(enum BAND band, uint32_t freq, uint8_t write_mode)   
{   
    uint32_t fmin,fmax;   
       
    switch (band)   
    {   
        case Band_LW:   fmin = LWmin;   
                        fmax = LWmax;   
                        break;   
           
        case Band_MW:   fmin = MWmin;   
                        fmax = MWmax;   
                        break;   
                                           
        case Band_FM:   fmin = FMmin;   
                        fmax = FMmax;   
                        break;   
           
        case Band_SW:   fmin = SWmin;   
                        fmax = SWmax;   
                        break;   
                           
        case Band_OIRT: fmin = OIRTmin;   
                        fmax = OIRTmax;   
                        break;   
        default:break;   
           
    }   
    if (freq < fmin)   
        {   freq = fmin;   
            CurrentFrequency = fmin;   
        }   
    if (freq > fmax)   
        {   freq = fmax;   
            CurrentFrequency = fmax;   
        }   
    setFrequency(band,freq,write_mode);   
       
}   
   
/*---------------------------------------------------------------------  
Function name:  Ftun_Init  
Input:          band,frequency  
Output:         N/A  
Description:    Tuner initialization, including all the parameters and  
                weak signal processing setting up,user can specify  
                particular configuration for each band according to  
                actual test results sepearately.  
---------------------------------------------------------------------*/   
void Ftun_Init(enum BAND band, uint32_t freq)   
{   /*****14 write mode registers, can be specified according to different band*****/   
    uint8_t SetupBuffer_FM[14] = {0x00,0x00,0x09,0x4D,0x99,0x0E,0xCD,0x66,0x15,0xCD,0xEE,0x14,0x40,0x14};   
//    uint8_t SetupBuffer_FM[14] = {0x00,0x24,0x09,0x4D,0x99,0x0E,0xCD,0xF6,0x15,0xC0,0xFE,0x34,0x40,0x14};   
    uint8_t SetupBuffer_AM[14] = {0x00,0x24,0x0E,0x56,0x08,0x12,0x55,0x00,0x00,0x00,0x00,0x14,0x40,0x14};   
       
    switch(band)   
    {   
    	case Band_FM:   
	case Band_OIRT:   
    		writeTunerRegister(ATomIC_I2C_Addr, ATomIC_I2C_SubType, ATomIC_I2C_WriteMode_None|ATomIC_I2C_SetupAdr, SetupBuffer_FM, 14);   
    	break;   
    	case Band_MW:   
    	case Band_LW:   
    	case Band_SW:   
    		writeTunerRegister(ATomIC_I2C_Addr, ATomIC_I2C_SubType, ATomIC_I2C_WriteMode_None|ATomIC_I2C_SetupAdr, SetupBuffer_AM, 14);   
    	break;   
    }   
    Ftun_TunetoFrequency(band,freq,ATomIC_I2C_WriteMode_Preset);   
//    SeekState = Seek_Idle; 
}   
   
/*---------------------------------------------------------------------  
Function name:  Ftun_IsPreset  
Input:          frequency  
Output:         preset station  
Description:    Judge if preset station or not  
---------------------------------------------------------------------*/   
enum PRESET Ftun_IsPreset (uint32_t freq)   
{   uint8_t i;   
       
    if (PresetFlag != 0)      //Tuner already tunes to a preset station    
        {   
        return(PresetFlag-1);}   
       
    for (i=0;i<PresetNum;i++)    //query if it's a preset station   
        if (freq == Preset_Freq[CurrentMode][i])   
            return (i);   
}   
   
/*---------------------------------------------------------------------  
Function name:  Ftun_Seek  
Input:          direction,auto_store,single_step  
Output:         N/A  
Description:    Tuning action including single step tune, seek,   
                auto search and store  
---------------------------------------------------------------------*/   
void Ftun_Seek(enum DIRECTION direction, uint8_t ams, uint8_t singlestep)   
{      
    static uint32_t step,max,min;   
    static uint8_t i=0;                       //Total numbers of preset stations   
    uint8_t j=0;   
    static uint32_t freq;   
    static uint16_t stepnum;                  //Step numbers going on of seeking action   
    static uint32_t temp_preset_level[64];    //Buffer of preset stations,MSB 8 bit stores level,    
                                            //LSB 24 bit stores frequency   
       
    switch (SeekState) {   
       
        case Seek_Idle:   
            Ftun_TunetoFrequency(CurrentMode,CurrentFrequency,ATomIC_I2C_WriteMode_Preset);   
            stepnum = 0;   
            status = (status & 0xf0) | Status_Idle;   
            SeekState = Seek_Configure;   
        break;   
        case Seek_Configure:   
            switch(CurrentMode)             //setting up of tuning step and edge limit subject to different bands   
            {   
            case Band_LW:   step = LWSeekStep;   
                            max = LWmax;   
                            min = LWmin;   
                            break;   
            case Band_MW:   step = MWSeekStep;   
                            max = MWmax;   
                            min = MWmin;   
                            break;   
            case Band_FM:   step = FMSeekStep;   
                            max = FMmax;   
                            min = FMmin;   
                            break;   
            case Band_SW:   step = SWSeekStep;   
                            max = SWmax;   
                            min = SWmin;   
                            break;   
            case Band_OIRT: step = OIRTSeekStep;   
                            max = OIRTmax;   
                            min = OIRTmin;   
                            break;   
            }   
               
            while(temp_preset_level[j])                         //Clear preset stations buffer   
                temp_preset_level[j++] = 0;   
            stepnum = 0;   
            i = 0;   
            j = 0;   
            SeekState = Seek_Request;   
            if (singlestep == 1)                             //Single step tuning up or down   
                status = (status & 0xf0)| ((uint8_t)Status_Idle);   
            else   
                if (ams == 1)                                //Auto search and store   
                    status = status | ((uint8_t)Status_AMS);   
                else                                            //Seek up or down   
                    status = status | ((uint8_t)Status_Seek);   
            break;   
        case Seek_Request:   
            if (direction == UP)   
                freq=CurrentFrequency+step;   
            else   
                freq=CurrentFrequency-step;    
            if (freq > max)   
                freq= min;   
            if (freq < min)   
                freq = max;   
            if((stepnum++) <= (max-min)/step)   
                {SeekState = Seek_Change_Freqency;   
                }   
            else                                                //No available stations within the whole band   
                if(ams == 1)   
                SeekState = Seek_AMS;   
                else   
                SeekState = Seek_Idle;   
            break;   
        case Seek_Change_Freqency:   
            Ftun_TunetoFrequency(CurrentMode,freq,ATomIC_I2C_WriteMode_Search);   
            CurrentFrequency = freq;   
            if (singlestep == 0)                            //Seek action, begins to wait level result   
                SeekState = Seek_Wait_Level;   
            else                                                //Single step tuning action, turn to Idle   
            {      
                SeekState = Seek_Idle;   
            }   
            break;   
        case Seek_Wait_Level:   
            if(CurrentMode == Band_LW || CurrentMode == Band_MW || CurrentMode == Band_SW)   
                seek_count = SeekDelay_AM;   
            else   
                seek_count = SeekDelay_FM;   
            SeekState = Seek_Check_Level;   
            break;   
        case Seek_Check_Level:   
            if(seek_count > 0) break;                            //Waiting until timer runs up   
            if(Ftun_CheckLevel(CurrentMode) == 1)            //Meet level condition   
            {   if(CurrentMode == Band_FM || CurrentMode == Band_OIRT)   
                {   
                    if(Check_USN_WAM == 1)                   //Going on with checking USN and WAM   
                        SeekState = Seek_Check_USN;   
                    else                                        //Check IF counter   
                        SeekState = Seek_Wait_IFC;   
                }   
                else                                            //USN and WAM are involved only in FM   
                    SeekState = Seek_Wait_IFC;   
            }   
            else                                                //Not an available station, going on seeking   
                SeekState = Seek_Request;   
            break;   
        case Seek_Check_USN:   
            if(getUSN() <= SeekCondition_FM_USN)    
                SeekState = Seek_Check_WAM;   
            else   
                SeekState = Seek_Request;   
            break;   
        case Seek_Check_WAM:   
            if(getWAM() <= SeekCondition_FM_WAM)   
                SeekState = Seek_Wait_IFC;   
            else   
                SeekState = Seek_Request;   
            break;   
        case Seek_Wait_IFC:   
            if(getIFCounter(0) == Read_Bit_IFCREADY)        //Waiting until 32ms IF counter result is available   
                SeekState = Seek_Check_IFC;   
            break;   
        case Seek_Check_IFC:   
            if(getIFCounter(1) <= SeekCondition_Ifcount)   
                {   
                    if(ams == 1)   
                        SeekState = Seek_AMS;   
                    else   
                        SeekState = Seek_Idle;   
                }   
            else   
                SeekState = Seek_Request;   
            break;   
        case Seek_AMS:   
            if(stepnum <= (max-min)/step)   
            {   
                                                                                   
                temp_preset_level[i++] = (getLevel() << 24) | CurrentFrequency;   //save each available station to temp buffer   
                SeekState = Seek_Request;   
            }   
            else   
            {                                                      
                PresetSort(i,temp_preset_level,&Preset_Freq[CurrentMode][0]);   //sort total stations according to field    
                                                                                //strength (LEVEL) and save to Preset array   
                SeekState = Seek_Idle;   
            }   
            break;   
   
        default:   
            break;   
    }   
}   
/*---------------------------------------------------------------------  
Function name:  Ftun_CheckLevel  
Input:          band  
Output:         N/A  
Description:    Check level while doing seek action  
---------------------------------------------------------------------*/   
uint8_t Ftun_CheckLevel(enum BAND band)   
{     
    uint8_t level = getLevel(); 
    TEF6606_ALERT("Level:%d\n",level);
    if (band == Band_FM || band == Band_OIRT)   
    { 
	if(level >= SeekCondition_FM_Level)   
        	return(1);   
        else   
            	return(0);   
    }   
    else   
    {   
	if(level >= SeekCondition_AM_Level)   
                return(1);   
        else   
                return(0);   
    }   
}   
/*---------------------------------------------------------------------  
Function name:  PresetSort  
Input:          count,*preset_level,*preset_station  
Output:         *preset_sta  
Description:    Sort preset stations according to signal level, number  
                of presets are defined in PresetNum. The bit6 and bit7  
                of *preset_level indicate level of each frequency while  
                the rest 6 bits carry the frequency.Save the preset  
                stations in sort to *preset_sta.  
---------------------------------------------------------------------*/   
void PresetSort(uint8_t count, uint32_t *preset_level, uint32_t *preset_sta)   
{   
    uint8_t i,num;   
    uint8_t j;   
    uint32_t temp;   
    uint32_t temp_sta;   
    if (count>PresetNum)   
        num = PresetNum;   
    else   
        num = count;   
    for (j=0;j<num;j++){   
        for(i=count-1;i>j;i--)   
        {   
            if (preset_level[i-1] < preset_level[i])   
            {   
                temp = preset_level[i];   
                preset_level[i] = preset_level[i-1];   
                preset_level[i-1] = temp;   
            }   
        }   
    }   
   
    for (j=0;j<num;j++){   
        for(i=num-1;i>j;i--)   
        {   
            if ((preset_level[i-1]&0x00FFFFFF) > (preset_level[i]&0x00FFFFFF))   
            {   
                temp_sta = preset_level[i]&0x00FFFFFF;   
                preset_level[i] = preset_level[i-1]&0x00FFFFFF;   
                preset_level[i-1] = temp_sta;   
            }   
         }   
        preset_sta[j] = preset_level[j]&0x00FFFFFF;   
    }    
}    
/*---------------------------------------------------------------------  
Function name:  Ftun_BandSwitch  
Input:          N/A  
Output:         N/A  
Description:    Switch bands among FM,MW,LW,SW,OIRT by means of rotation.  
---------------------------------------------------------------------*/   
void Ftun_BandSwitch(void)   
{   enum BAND band;   
    if (CurrentMode == Band_OIRT)   
        band = Band_LW;   
    else   
        band = CurrentMode + 1;   
    LastBandFreq[CurrentMode] = CurrentFrequency;   
    Ftun_TunetoFrequency(band, LastBandFreq[band], ATomIC_I2C_WriteMode_Preset);   
    CurrentMode = band;   
    CurrentFrequency = LastBandFreq[band];   
               
}   
/*---------------------------------------------------------------------  
Function name:  Ftun_StereoMonoSwitch  
Input:          N/A  
Output:         N/A  
Description:    Force mono output  
---------------------------------------------------------------------*/   
void Ftun_StereoMonoSwitch(void)   
{      
    setMono (CurrentForceMono);   
    CurrentForceMono = ~CurrentForceMono;   
       
}   
/*---------------------------------------------------------------------  
Function name:  Ftun_SaveCurrentStation  
Input:          frequency,preset  
Output:         N/A  
Description:    Save current frequency to preset station  
---------------------------------------------------------------------*/   
void Ftun_SaveCurrentStation(uint32_t freq, enum PRESET preset)   
{      
        Preset_Freq[CurrentMode][preset] = freq;   
}   
/*---------------------------------------------------------------------  
Function name:  Ftun_TunetoPreset  
Input:          preset  
Output:         N/A  
Description:    Tune to preset station  
---------------------------------------------------------------------*/   
void Ftun_TunetoPreset(enum PRESET preset)   
{   uint32_t max,min,preset_default;                  //preset_default, threshold preset stations   
    switch (CurrentMode)   
                {   
                case Band_LW:   max = LWmax;min = LWmin;   
                                preset_default = LW_Threshold_Freq;    
                                break;   
                case Band_MW:   max = MWmax;min = MWmin;       
                                preset_default = MW_Threshold_Freq;   
                                break;   
                case Band_FM:   max = FMmax;min = FMmin;                   
                                preset_default = FM_Threshold_Freq;   
                                break;   
                case Band_SW:   max = SWmax;min = SWmin;       
                                preset_default = SW_Threshold_Freq;   
                                break;   
                case Band_OIRT: max = OIRTmax;min = OIRTmin;       
                                preset_default = OIRT_Threshold_Freq;   
                                break;   
                default: break;   
                }   
       
    /******** read frequency from Preset array ***********/   
    if (Preset_Freq[CurrentMode][preset] > max || Preset_Freq[CurrentMode][preset] < min)   
        Preset_Freq[CurrentMode][preset] = preset_default;   
       
    CurrentFrequency = Preset_Freq[CurrentMode][preset];   
    Ftun_TunetoFrequency(CurrentMode,CurrentFrequency,ATomIC_I2C_WriteMode_Preset);   
    PresetFlag = (uint8_t)(preset) + 1;   
       
}



MODULE_DESCRIPTION("tef6606 i2c driver");  
MODULE_AUTHOR("Simon Hsueh");  
MODULE_LICENSE("GPL");  
  
module_init(tef6606_i2c_init);  
module_exit(tef6606_i2c_exit);  
