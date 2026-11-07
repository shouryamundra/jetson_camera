
#ifndef _MV_ISP_ERROR_DEFINE_H_
#define _MV_ISP_ERROR_DEFINE_H_

/************************************************************************
* Error codes from the ISP algorithm library
************************************************************************/
// Common types
#define MV_ALG_OK 0x00000000   //Processing succeeded
#define MV_ALG_ERR 0x10000000  //Unspecified error type

// Capability checks
#define MV_ALG_E_ABILITY_ARG 0x10000001  //Invalid parameter in the capability set

// Memory checks
#define MV_ALG_E_MEM_NULL 0x10000002        //Memory address is null
#define MV_ALG_E_MEM_ALIGN 0x10000003       //Memory alignment does not meet requirements
#define MV_ALG_E_MEM_LACK 0x10000004        //Insufficient memory space
#define MV_ALG_E_MEM_SIZE_ALIGN 0x10000005  //Memory size does not meet alignment requirements
#define MV_ALG_E_MEM_ADDR_ALIGN 0x10000006  //Memory address does not meet alignment requirements

// Image checks
#define MV_ALG_E_IMG_FORMAT 0x10000007     //Incorrect or unsupported image format
#define MV_ALG_E_IMG_SIZE 0x10000008       //Image dimensions are incorrect or out of range
#define MV_ALG_E_IMG_STEP 0x10000009       //Image dimensions do not match the step parameter
#define MV_ALG_E_IMG_DATA_NULL 0x1000000A  //Image data address is null

// Input/output parameter validation
#define MV_ALG_E_CFG_TYPE 0x1000000B   //Incorrect parameter type when setting or getting parameters
#define MV_ALG_E_CFG_SIZE 0x1000000C   //Incorrect input/output structure size when setting or getting parameters
#define MV_ALG_E_PRC_TYPE 0x1000000D   //Incorrect processing type
#define MV_ALG_E_PRC_SIZE 0x1000000E   //Incorrect input/output parameter size for processing
#define MV_ALG_E_FUNC_TYPE 0x1000000F  //Incorrect subprocess type
#define MV_ALG_E_FUNC_SIZE 0x10000010  //Incorrect input/output parameter size for subprocess

// Runtime parameter validation
#define MV_ALG_E_PARAM_INDEX 0x10000011  //Invalid index parameter
#define MV_ALG_E_PARAM_VALUE 0x10000012  //value parameter is incorrect or out of range
#define MV_ALG_E_PARAM_NUM 0x10000013    //Invalid param_num parameter

// API call checks
#define MV_ALG_E_NULL_PTR 0x10000014      //Function parameter pointer is null
#define MV_ALG_E_OVER_MAX_MEM 0x10000015  //Maximum memory limit exceeded
#define MV_ALG_E_CALL_BACK 0x10000016     //Callback function error

// Algorithm library encryption checks
#define MV_ALG_E_ENCRYPT 0x10000017  //Encryption error
#define MV_ALG_E_EXPIRE 0x10000018   //Algorithm library validity-period error

// Basic error types returned by internal modules
#define MV_ALG_E_BAD_ARG 0x10000019    //Invalid parameter range
#define MV_ALG_E_DATA_SIZE 0x1000001A  //Incorrect data size
#define MV_ALG_E_STEP 0x1000001B       //Incorrect data step

// CPU instruction-set support error codes
#define MV_ALG_E_CPUID 0x1000001C  //CPU does not support the instruction set used by the optimized code

#define MV_ALG_WARNING 0x1000001D  //Warning

#define MV_ALG_E_TIME_OUT 0x1000001E          //Algorithm library timeout
#define MV_ALG_E_LIB_VERSION 0x1000001F       //Algorithm version error
#define MV_ALG_E_MODEL_VERSION 0x10000020     //Model version error
#define MV_ALG_E_GPU_MEM_ALLOC 0x10000021     //GPU memory allocation error
#define MV_ALG_E_FILE_NON_EXIST 0x10000022    //File does not exist
#define MV_ALG_E_NONE_STRING 0x10000023       //String is empty
#define MV_ALG_E_IMAGE_CODEC 0x10000024       //Image decoder error
#define MV_ALG_E_FILE_OPEN 0x10000025         //Error opening file
#define MV_ALG_E_FILE_READ 0x10000026         //Error reading file
#define MV_ALG_E_FILE_WRITE 0x10000027        //Error writing file
#define MV_ALG_E_FILE_READ_SIZE 0x10000028    //Incorrect file read size
#define MV_ALG_E_FILE_TYPE 0x10000029         //Invalid file type
#define MV_ALG_E_MODEL_TYPE 0x1000002A        //Invalid model type
#define MV_ALG_E_MALLOC_MEM 0x1000002B        //Memory allocation error
#define MV_ALG_E_BIND_CORE_FAILED 0x1000002C  //Failed to bind the thread to a CPU core

// Denoising-specific error codes
#define MV_ALG_E_DENOISE_NE_IMG_FORMAT 0x10402001      //Invalid noise-profile image format
#define MV_ALG_E_DENOISE_NE_FEATURE_TYPE 0x10402002    //Invalid noise-profile type
#define MV_ALG_E_DENOISE_NE_PROFILE_NUM 0x10402003     //Invalid noise-profile count
#define MV_ALG_E_DENOISE_NE_GAIN_NUM 0x10402004        //Invalid noise-profile gain count
#define MV_ALG_E_DENOISE_NE_GAIN_VAL 0x10402005        //Invalid input noise-curve gain
#define MV_ALG_E_DENOISE_NE_BIN_NUM 0x10402006         //Invalid noise-curve bin count
#define MV_ALG_E_DENOISE_NE_INIT_GAIN 0x10402007       //Invalid initial gain for noise estimation
#define MV_ALG_E_DENOISE_NE_NOT_INIT 0x10402008        //Noise estimation is not initialized
#define MV_ALG_E_DENOISE_COLOR_MODE 0x10402009         //Invalid color-space mode
#define MV_ALG_E_DENOISE_ROI_NUM 0x1040200a            //Invalid image ROI count
#define MV_ALG_E_DENOISE_ROI_ORI_PT 0x1040200b         //Invalid image ROI origin
#define MV_ALG_E_DENOISE_ROI_SIZE 0x1040200c           //Invalid image ROI size
#define MV_ALG_E_DENOISE_GAIN_NOT_EXIST 0x1040200d     //Input camera gain does not exist (maximum gain count reached)
#define MV_ALG_E_DENOISE_GAIN_BEYOND_RANGE 0x1040200e  //Input camera gain is out of range
#define MV_ALG_E_DENOISE_NP_BUF_SIZE 0x1040200f        //Invalid input noise-profile memory size

#endif  //_MV_ISP_ERROR_DEFINE_H_
