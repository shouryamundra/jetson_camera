
#ifndef _MV_CAMERA_PARAMS_H_
#define _MV_CAMERA_PARAMS_H_

#include "PixelType.h"

#ifndef __cplusplus
typedef char bool;
#define true 1
#define false 0
#endif

/// The Method of Sorting
typedef enum _MV_SORT_METHOD_
{
  SortMethod_SerialNumber =
    0,  ///< Sorting by SerialNumber
  SortMethod_UserID =
    1,  ///< Sorting by UserID
  SortMethod_CurrentIP_ASC =
    2,  ///< Sorting by current IP(Ascending)
  SortMethod_CurrentIP_DESC =
    3,  ///< Sorting by current IP(Descending)

} MV_SORT_METHOD;

/// GigE device info
typedef struct _MV_GIGE_DEVICE_INFO_
{
  unsigned int
    nIpCfgOption;  ///< [OUT] IP Configuration Options
  unsigned int
    nIpCfgCurrent;          ///< [OUT] IP Configuration
  unsigned int nCurrentIp;  ///< [OUT] Current Ip
  unsigned int
    nCurrentSubNetMask;  ///< [OUT] Curtent Subnet Mask
  unsigned int
    nDefultGateWay;  ///< [OUT] Current Gateway
  unsigned char
    chManufacturerName[32];  ///< [OUT] Manufacturer Name
  unsigned char chModelName[32];  ///< [OUT] Model Name
  unsigned char
    chDeviceVersion[32];  ///< [OUT] Device Version
  unsigned char chManufacturerSpecificInfo
    [48];  ///< [OUT] Manufacturer Specific Information
  unsigned char
    chSerialNumber[16];  ///< [OUT] Serial Number
  unsigned char
    chUserDefinedName[16];  ///< [OUT] User Defined Name
  unsigned int nNetExport;  ///< [OUT] NetWork IP Address

  unsigned int nReserved[4];  ///<       Reserved

} MV_GIGE_DEVICE_INFO;

///< Maximum data information size
#define INFO_MAX_BUFFER_SIZE 64

/// USB device info
typedef struct _MV_USB3_DEVICE_INFO_
{
  unsigned char
    CrtlInEndPoint;  ///< [OUT] Control input endpoint
  unsigned char
    CrtlOutEndPoint;  ///< [OUT] Control output endpoint
  unsigned char StreamEndPoint;  ///< [OUT] Flow endpoint
  unsigned char EventEndPoint;  ///< [OUT] Event endpoint
  unsigned short idVendor;   ///< [OUT] Vendor ID Number
  unsigned short idProduct;  ///< [OUT] Device ID Number
  unsigned int nDeviceNumber;  ///< [OUT] Device Number
  unsigned char chDeviceGUID
    [INFO_MAX_BUFFER_SIZE];  ///< [OUT] Device GUID Number
  unsigned char chVendorName
    [INFO_MAX_BUFFER_SIZE];  ///< [OUT] Vendor Name
  unsigned char chModelName
    [INFO_MAX_BUFFER_SIZE];  ///< [OUT] Model Name
  unsigned char chFamilyName
    [INFO_MAX_BUFFER_SIZE];  ///< [OUT] Family Name
  unsigned char chDeviceVersion
    [INFO_MAX_BUFFER_SIZE];  ///< [OUT] Device Version
  unsigned char chManufacturerName
    [INFO_MAX_BUFFER_SIZE];  ///< [OUT] Manufacturer Name
  unsigned char chSerialNumber
    [INFO_MAX_BUFFER_SIZE];  ///< [OUT] Serial Number
  unsigned char chUserDefinedName
    [INFO_MAX_BUFFER_SIZE];  ///< [OUT] User Defined Name
  unsigned int nbcdUSB;  ///< [OUT] Support USB Protocol
  unsigned int nDeviceAddress;  ///< [OUT] Device Address

  unsigned int nReserved[2];  ///<       Reserved

} MV_USB3_DEVICE_INFO;

/// CameraLink device info
typedef struct _MV_CamL_DEV_INFO_
{
  unsigned char
    chPortID[INFO_MAX_BUFFER_SIZE];  ///< [OUT] Port ID
  unsigned char chModelName
    [INFO_MAX_BUFFER_SIZE];  ///< [OUT] Model Name
  unsigned char chFamilyName
    [INFO_MAX_BUFFER_SIZE];  ///< [OUT] Family Name
  unsigned char chDeviceVersion
    [INFO_MAX_BUFFER_SIZE];  ///< [OUT] Device Version
  unsigned char chManufacturerName
    [INFO_MAX_BUFFER_SIZE];  ///< [OUT] Manufacturer Name
  unsigned char chSerialNumber
    [INFO_MAX_BUFFER_SIZE];  ///< [OUT] Serial Number

  unsigned int nReserved[38];  ///<       Reserved

} MV_CamL_DEV_INFO;

///< Device Transport Layer Protocol Type
#define MV_UNKNOW_DEVICE \
  0x00000000  ///< Unknown Device Type, Reserved
#define MV_GIGE_DEVICE 0x00000001  ///< GigE Device
#define MV_1394_DEVICE \
  0x00000002  ///< 1394-a/b Device
#define MV_USB_DEVICE 0x00000004  ///< USB Device
#define MV_CAMERALINK_DEVICE \
  0x00000008  ///< CameraLink Device
#define MV_VIR_GIGE_DEVICE \
  0x00000010  ///< Virtual GigE Device
#define MV_VIR_USB_DEVICE \
  0x00000020  ///< Virtual USB Device
#define MV_GENTL_GIGE_DEVICE \
  0x00000040  ///< GenTL GigE Device

/// Device info
typedef struct _MV_CC_DEVICE_INFO_
{
  unsigned short nMajorVer;   ///< [OUT] Major Version
  unsigned short nMinorVer;   ///< [OUT] Minor Version
  unsigned int nMacAddrHigh;  ///< [OUT] High MAC Address
  unsigned int nMacAddrLow;   ///< [OUT] Low MAC Address
  unsigned int
    nTLayerType;  ///< [OUT] Device Transport Layer Protocol Type

  unsigned int nReserved[4];  ///<       Reserved

  union {
    MV_GIGE_DEVICE_INFO
      stGigEInfo;  ///< [OUT] GigE Device Info
    MV_USB3_DEVICE_INFO
      stUsb3VInfo;  ///< [OUT] USB Device Info
    MV_CamL_DEV_INFO
      stCamLInfo;  ///< [OUT] CameraLink Device Info
                   // more ...
  } SpecialInfo;

} MV_CC_DEVICE_INFO;

///< The maximum number of supported transport layer instances
#define MV_MAX_TLS_NUM 8
///< The maximum number of supported devices
#define MV_MAX_DEVICE_NUM 256

/// Device Information List
typedef struct _MV_CC_DEVICE_INFO_LIST_
{
  unsigned int
    nDeviceNum;  ///< [OUT] Online Device Number
  MV_CC_DEVICE_INFO * pDeviceInfo
    [MV_MAX_DEVICE_NUM];  ///< [OUT] Support up to 256 devices

} MV_CC_DEVICE_INFO_LIST;

/// Interface Information with GenTL
typedef struct _MV_GENTL_IF_INFO_
{
  unsigned char chInterfaceID
    [INFO_MAX_BUFFER_SIZE];  ///< [OUT] Interface ID
  unsigned char chTLType
    [INFO_MAX_BUFFER_SIZE];  ///< [OUT] GenTL Type
  unsigned char chDisplayName
    [INFO_MAX_BUFFER_SIZE];  ///< [OUT] Display Name
  unsigned int
    nCtiIndex;  ///< [OUT] The Index of Cti Files

  unsigned int nReserved[8];  ///<       Reserved

} MV_GENTL_IF_INFO;

///< The maximum number of GenTL interface supported
#define MV_MAX_GENTL_IF_NUM 256

/// Inferface Information List with GenTL
typedef struct _MV_GENTL_IF_INFO_LIST_
{
  unsigned int
    nInterfaceNum;  ///< [OUT] Online Inferface Number
  MV_GENTL_IF_INFO * pIFInfo
    [MV_MAX_GENTL_IF_NUM];  ///< [OUT] Support up to 256 inferfaces

} MV_GENTL_IF_INFO_LIST;

/// Device Information with GenTL
typedef struct _MV_GENTL_DEV_INFO_
{
  unsigned char chInterfaceID
    [INFO_MAX_BUFFER_SIZE];  ///< [OUT] Interface ID
  unsigned char chDeviceID
    [INFO_MAX_BUFFER_SIZE];  ///< [OUT] Device ID
  unsigned char chVendorName
    [INFO_MAX_BUFFER_SIZE];  ///< [OUT] Vendor Name
  unsigned char chModelName
    [INFO_MAX_BUFFER_SIZE];  ///< [OUT] Model Name
  unsigned char chTLType
    [INFO_MAX_BUFFER_SIZE];  ///< [OUT] GenTL Type
  unsigned char chDisplayName
    [INFO_MAX_BUFFER_SIZE];  ///< [OUT] Display Name
  unsigned char chUserDefinedName
    [INFO_MAX_BUFFER_SIZE];  ///< [OUT] User Defined Name
  unsigned char chSerialNumber
    [INFO_MAX_BUFFER_SIZE];  ///< [OUT] Serial Number
  unsigned char chDeviceVersion
    [INFO_MAX_BUFFER_SIZE];  ///< [OUT] Device Version
  unsigned int
    nCtiIndex;  ///< [OUT] The Index of Cti Files

  unsigned int nReserved[8];  ///<       Reserved

} MV_GENTL_DEV_INFO;

///< The maximum number of GenTL devices supported
#define MV_MAX_GENTL_DEV_NUM 256

/// Device Information List with GenTL
typedef struct _MV_GENTL_DEV_INFO_LIST_
{
  unsigned int
    nDeviceNum;  ///< [OUT] Online Device Number
  MV_GENTL_DEV_INFO * pDeviceInfo
    [MV_MAX_GENTL_DEV_NUM];  ///< [OUT] Support up to 256 devices

} MV_GENTL_DEV_INFO_LIST;

/// Device Access Mode
#define MV_ACCESS_Exclusive \
  1  /// Exclusive authority, other APP is only allowed to read the CCP register
#define MV_ACCESS_ExclusiveWithSwitch \
  2  /// You can seize the authority from the 5 mode, and then open with exclusive authority
#define MV_ACCESS_Control \
  3  /// Control authority, allows other APP reading all registers
#define MV_ACCESS_ControlWithSwitch \
  4  /// You can seize the authority from the 5 mode, and then open with control authority
#define MV_ACCESS_ControlSwitchEnable \
  5  /// Open with seized control authority
#define MV_ACCESS_ControlSwitchEnableWithKey \
  6  /// You can seize the authority from the 5 mode, and then open with seized control authority
#define MV_ACCESS_Monitor \
  7  /// Open with read mode and is available under control authority

/// The content of ChunkData
typedef struct _MV_CHUNK_DATA_CONTENT_
{
  unsigned char * pChunkData;  ///< [OUT] Chunk Data
  unsigned int nChunkID;       ///< [OUT] Chunk ID
  unsigned int nChunkLen;      ///< [OUT] Chunk Length

  unsigned int nReserved[8];  ///<       Reserved

} MV_CHUNK_DATA_CONTENT;

/// Output Frame Information
typedef struct _MV_FRAME_OUT_INFO_EX_
{
  unsigned short
    nWidth;  ///< [OUT] Image Width (over 65535, use nExtendWidth)
  unsigned short
    nHeight;  ///< [OUT] Image Height(over 65535, use nExtendHeight)
  enum MvGvspPixelType
    enPixelType;  ///< [OUT] Pixel Type

  unsigned int nFrameNum;  ///< [OUT] Frame Number
  unsigned int
    nDevTimeStampHigh;  ///< [OUT] Timestamp high 32 bits
  unsigned int
    nDevTimeStampLow;  ///< [OUT] Timestamp low 32 bits
  unsigned int
    nReserved0;  ///< [OUT] Reserved, 8-byte aligned
  int64_t
    nHostTimeStamp;  ///< [OUT] Host-generated timestamp

  unsigned int nFrameLen;  ///< [OUT] The Length of Frame

  /// Device frame-specific time scale
  unsigned int nSecondCount;  ///< [OUT] The Seconds
  unsigned int
    nCycleCount;  ///< [OUT] The Count of Cycle
  unsigned int
    nCycleOffset;  ///< [OUT] The Offset of Cycle

  float fGain;          ///< [OUT] Gain
  float fExposureTime;  ///< [OUT] Exposure Time
  unsigned int
    nAverageBrightness;  ///< [OUT] Average brightness

  /// White balance
  unsigned int nRed;    ///< [OUT] Red
  unsigned int nGreen;  ///< [OUT] Green
  unsigned int nBlue;   ///< [OUT] Blue

  unsigned int nFrameCounter;  ///< [OUT] Frame Counter
  unsigned int
    nTriggerIndex;  ///< [OUT] Trigger Counting

  unsigned int nInput;   ///< [OUT] Input
  unsigned int nOutput;  ///< [OUT] Output

  /// ROI Region
  unsigned short nOffsetX;  ///< [OUT] OffsetX
  unsigned short nOffsetY;  ///< [OUT] OffsetY
  unsigned short
    nChunkWidth;  ///< [OUT] The Width of Chunk
  unsigned short
    nChunkHeight;  ///< [OUT] The Height of Chunk

  unsigned int
    nLostPacket;  ///< [OUT] Lost Packet Number In This Frame

  unsigned int
    nUnparsedChunkNum;  ///< [OUT] Unparsed Chunk Number
  union {
    MV_CHUNK_DATA_CONTENT *
      pUnparsedChunkContent;  ///< [OUT] Unparsed Chunk Content
    int64_t nAligning;        ///< [OUT] Aligning
  } UnparsedChunkList;

  unsigned int nExtendWidth;   ///< [OUT] Image Width
  unsigned int nExtendHeight;  ///< [OUT] Image Height

  unsigned int nReserved[34];  ///<       Reserved

} MV_FRAME_OUT_INFO_EX;

/// Image Struct, output the pointer of Image and the information of the specific image
typedef struct _MV_FRAME_OUT_
{
  unsigned char * pBufAddr;  ///< [OUT] pointer of image
  MV_FRAME_OUT_INFO_EX
    stFrameInfo;  ///< [OUT] information of the specific image

  unsigned int nRes[16];  ///<       Reserved

} MV_FRAME_OUT;

/// The strategy of Grabbing
typedef enum _MV_GRAB_STRATEGY_
{
  MV_GrabStrategy_OneByOne =
    0,  ///< Grab One By One
  MV_GrabStrategy_LatestImagesOnly =
    1,  ///< Grab The Latest Image
  MV_GrabStrategy_LatestImages =
    2,  ///< Grab The Latest Images
  MV_GrabStrategy_UpcomingImage =
    3,  ///< Grab The Upcoming Image

} MV_GRAB_STRATEGY;

/// Network transmission information
typedef struct _MV_NETTRANS_INFO_
{
  int64_t
    nReceiveDataSize;  ///< [OUT] Received Data Size
  int
    nThrowFrameCount;  ///< [OUT] Throw frame number
  unsigned int
    nNetRecvFrameCount;  ///< [OUT] Received Frame Count
  int64_t
    nRequestResendPacketCount;  ///< [OUT] Request Resend Packet Count
  int64_t
    nResendPacketCount;  ///< [OUT] Resend Packet Count

} MV_NETTRANS_INFO;

/// Information Type
#define MV_MATCH_TYPE_NET_DETECT \
  0x00000001  ///< Network traffic and packet loss information
#define MV_MATCH_TYPE_USB_DETECT \
  0x00000002  ///< The total number of bytes host received from U3V device

/// A fully matched information structure
typedef struct _MV_ALL_MATCH_INFO_
{
  unsigned int
    nType;  ///< [IN]  Information type need to output ,e.g. MV_MATCH_TYPE_NET_DETECT
  void *
    pInfo;  ///< [OUT] Output information cache, which is allocated by the caller
  unsigned int
    nInfoSize;  ///< [IN]  Information cache size

} MV_ALL_MATCH_INFO;

/// Network traffic and packet loss feedback structure, the corresponding type is MV_MATCH_TYPE_NET_DETECT
typedef struct _MV_MATCH_INFO_NET_DETECT_
{
  int64_t
    nReceiveDataSize;  ///< [OUT] Received data size
  int64_t
    nLostPacketCount;  ///< [OUT] Number of packets lost
  unsigned int
    nLostFrameCount;  ///< [OUT] Number of frames lost
  unsigned int
    nNetRecvFrameCount;  ///< [OUT] Received Frame Count
  int64_t
    nRequestResendPacketCount;  ///< [OUT] Request Resend Packet Count
  int64_t
    nResendPacketCount;  ///< [OUT] Resend Packet Count

} MV_MATCH_INFO_NET_DETECT;

/// The total number of bytes host received from the u3v device side, the corresponding type is MV_MATCH_TYPE_USB_DETECT
typedef struct _MV_MATCH_INFO_USB_DETECT_
{
  int64_t
    nReceiveDataSize;  ///< [OUT] Received data size
  unsigned int
    nReceivedFrameCount;  ///< [OUT] Number of frames received
  unsigned int
    nErrorFrameCount;  ///< [OUT] Number of error frames

  unsigned int
    nReserved[2];  ///<       Reserved

} MV_MATCH_INFO_USB_DETECT;

/// Display frame information
typedef struct _MV_DISPLAY_FRAME_INFO_
{
  void * hWnd;             ///< [IN] HWND
  unsigned char * pData;   ///< [IN] Data Buffer
  unsigned int nDataLen;   ///< [IN] Data Size
  unsigned short nWidth;   ///< [IN] Width
  unsigned short nHeight;  ///< [IN] Height
  enum MvGvspPixelType
    enPixelType;  ///< [IN] Pixel format

  unsigned int nRes[4];  ///<      Reserved

} MV_DISPLAY_FRAME_INFO;

typedef struct _MV_DISPLAY_FRAME_INFO_EX_
{
  unsigned int nWidth;   ///< [IN]   Width
  unsigned int nHeight;  ///< [IN]   Height
  enum MvGvspPixelType
    enPixelType;  ///< [IN]   Pixel type

  unsigned char *
    pImageBuf;  ///< [IN]   Input image buffer
  unsigned int
    nImageBufLen;  ///< [IN]   Input image length

  unsigned int nRes[4];  ///<      Reserved

} MV_DISPLAY_FRAME_INFO_EX;

/// The saved format for 3D data
enum MV_SAVE_POINT_CLOUD_FILE_TYPE
{
  MV_PointCloudFile_Undefined =
    0,  ///< Undefined point cloud format
  MV_PointCloudFile_PLY =
    1,  ///< The point cloud format named PLY
  MV_PointCloudFile_CSV =
    2,  ///< The point cloud format named CSV
  MV_PointCloudFile_OBJ =
    3,  ///< The point cloud format named OBJ

};

/// Save 3D data to buffer
typedef struct _MV_SAVE_POINT_CLOUD_PARAM_
{
  unsigned int
    nLinePntNum;  ///< [IN]  The number of points in each row,which is the width of the image
  unsigned int
    nLineNum;  ///< [IN]  The number of rows,which is the height of the image

  enum MvGvspPixelType
    enSrcPixelType;  ///< [IN]  The pixel format of the input data
  unsigned char * pSrcData;  ///< [IN]  Input data buffer
  unsigned int nSrcDataLen;  ///< [IN]  Input data length

  unsigned char *
    pDstBuf;  ///< [OUT] Output pixel data buffer
  unsigned int
    nDstBufSize;  ///< [IN]  Output buffer size provided(nLinePntNum * nLineNum * (16*3 + 4) + 2048)
  unsigned int
    nDstBufLen;  ///< [OUT] Output pixel data buffer size
  MV_SAVE_POINT_CLOUD_FILE_TYPE
    enPointCloudFileType;  ///< [IN]  Output point data file type provided

  unsigned int nReserved[8];  ///<       Reserved

} MV_SAVE_POINT_CLOUD_PARAM;

/// Save image type
enum MV_SAVE_IAMGE_TYPE
{
  MV_Image_Undefined =
    0,                ///< Undefined Image Type
  MV_Image_Bmp = 1,   ///< BMP Image Type
  MV_Image_Jpeg = 2,  ///< Jpeg Image Type
  MV_Image_Png = 3,   ///< Png  Image Type
  MV_Image_Tif = 4,   ///< TIFF Image Type

};

/// Save Image Parameters
typedef struct _MV_SAVE_IMAGE_PARAM_EX3_
{
  unsigned char * pData;  ///< [IN]  Input Data Buffer
  unsigned int nDataLen;  ///< [IN]  Input Data length
  enum MvGvspPixelType
    enPixelType;  ///< [IN]  Input Data Pixel Format
  unsigned int nWidth;   ///< [IN]  Image Width
  unsigned int nHeight;  ///< [IN]  Image Height

  unsigned char *
    pImageBuffer;          ///< [OUT] Output Image Buffer
  unsigned int nImageLen;  ///< [OUT] Output Image length
  unsigned int
    nBufferSize;  ///< [IN]  Output buffer size provided
  enum MV_SAVE_IAMGE_TYPE
    enImageType;  ///< [IN]  Output Image Format
  unsigned int
    nJpgQuality;  ///< [IN]  Encoding quality(50-99], Other formats are invalid

  unsigned int
    iMethodValue;  ///< [IN]  Bayer interpolation method  0-Fast 1-Equilibrium 2-Optimal

  unsigned int nReserved[3];  ///<       Reserved

} MV_SAVE_IMAGE_PARAM_EX3;

// Parameters for saving an image to a file
typedef struct _MV_SAVE_IMAGE_TO_FILE_PARAM_EX_
{
  unsigned int nWidth;          // [IN]     Image width
  unsigned int nHeight;         // [IN]     Image height
  MvGvspPixelType enPixelType;  // [IN]     Pixel format of the input data
  unsigned char * pData;        // [IN]     Input data buffer
  unsigned int nDataLen;        // [IN]     Input data size

  MV_SAVE_IAMGE_TYPE enImageType;  // [IN]     Input image format
  char * pcImagePath;              // [IN]     Input file path

  unsigned int nQuality;  // [IN]     JPG encoding quality (50-99], PNG encoding quality [0-9]
  int iMethodValue;       // [IN]     Interpolation: 0-fast, 1-balanced, 2-optimal (other values default to optimal)
  unsigned int nReserved[8];

} MV_SAVE_IMAGE_TO_FILE_PARAM_EX;

/// Rotation angle
typedef enum _MV_IMG_ROTATION_ANGLE_
{
  MV_IMAGE_ROTATE_90 = 1,
  MV_IMAGE_ROTATE_180 = 2,
  MV_IMAGE_ROTATE_270 = 3,

} MV_IMG_ROTATION_ANGLE;

/// Rotate image structure
typedef struct _MV_CC_ROTATE_IMAGE_PARAM_T_
{
  enum MvGvspPixelType
    enPixelType;         ///< [IN]  Pixel format
  unsigned int nWidth;   ///< [IN][OUT] Width
  unsigned int nHeight;  ///< [IN][OUT] Height

  unsigned char * pSrcData;  ///< [IN]  Input data buffer
  unsigned int nSrcDataLen;  ///< [IN]  Input data length

  unsigned char * pDstBuf;  ///< [OUT] Output data buffer
  unsigned int nDstBufLen;  ///< [OUT] Output data length
  unsigned int
    nDstBufSize;  ///< [IN]  Provided output buffer size

  MV_IMG_ROTATION_ANGLE
    enRotationAngle;  ///< [IN]  Rotation angle

  unsigned int nRes[8];  ///<       Reserved

} MV_CC_ROTATE_IMAGE_PARAM;

/// Flip type
typedef enum _MV_IMG_FLIP_TYPE_
{
  MV_FLIP_VERTICAL = 1,
  MV_FLIP_HORIZONTAL = 2,

} MV_IMG_FLIP_TYPE;

/// Flip image structure
typedef struct _MV_CC_FLIP_IMAGE_PARAM_T_
{
  enum MvGvspPixelType
    enPixelType;         ///< [IN]  Pixel format
  unsigned int nWidth;   ///< [IN]  Width
  unsigned int nHeight;  ///< [IN]  Height

  unsigned char * pSrcData;  ///< [IN]  Input data buffer
  unsigned int nSrcDataLen;  ///< [IN]  Input data length

  unsigned char * pDstBuf;  ///< [OUT] Output data buffer
  unsigned int nDstBufLen;  ///< [OUT] Output data length
  unsigned int
    nDstBufSize;  ///< [IN]  Provided output buffer size

  MV_IMG_FLIP_TYPE enFlipType;  ///< [IN]  Flip type

  unsigned int nRes[8];  ///<       Reserved

} MV_CC_FLIP_IMAGE_PARAM;

/// Pixel convert structure
typedef struct _MV_CC_PIXEL_CONVERT_PARAM_EX_
{
  unsigned int nWidth;   ///< [IN]  Width
  unsigned int nHeight;  ///< [IN]  Height

  enum MvGvspPixelType
    enSrcPixelType;  ///< [IN]  Source pixel format
  unsigned char * pSrcData;  ///< [IN]  Input data buffer
  unsigned int nSrcDataLen;  ///< [IN]  Input data length

  enum MvGvspPixelType
    enDstPixelType;  ///< [IN]  Destination pixel format
  unsigned char *
    pDstBuffer;          ///< [OUT] Output data buffer
  unsigned int nDstLen;  ///< [OUT] Output data length
  unsigned int
    nDstBufferSize;  ///< [IN]  Provided output buffer size

  unsigned int nRes[4];  ///<       Reserved

} MV_CC_PIXEL_CONVERT_PARAM_EX;

/// Gamma type
typedef enum _MV_CC_GAMMA_TYPE_
{
  MV_CC_GAMMA_TYPE_NONE = 0,   ///< Disable
  MV_CC_GAMMA_TYPE_VALUE = 1,  ///< Gamma value
  MV_CC_GAMMA_TYPE_USER_CURVE =
    2,  ///< Gamma curve
  ///< 8bit,length:256*sizeof(unsigned char)
  ///< 10bit,length:1024*sizeof(unsigned short)
  ///< 12bit,length:4096*sizeof(unsigned short)
  ///< 16bit,length:65536*sizeof(unsigned short)
  MV_CC_GAMMA_TYPE_LRGB2SRGB =
    3,  ///< linear RGB to sRGB
  MV_CC_GAMMA_TYPE_SRGB2LRGB =
    4,  ///< sRGB to linear RGB

} MV_CC_GAMMA_TYPE;

// Gamma information
/// Gamma info structure
typedef struct _MV_CC_GAMMA_PARAM_T_
{
  MV_CC_GAMMA_TYPE enGammaType;  ///< [IN]  Gamma type
  float fGammaValue;  ///< [IN]  Gamma value:0.1 ~ 4.0
  unsigned char *
    pGammaCurveBuf;  ///< [IN]  Gamma curve buffer
  unsigned int
    nGammaCurveBufLen;  ///< [IN]  Gamma curve buffer size

  unsigned int nRes[8];  ///<       Reserved

} MV_CC_GAMMA_PARAM;

/// CCM param
typedef struct _MV_CC_CCM_PARAM_T_
{
  bool bCCMEnable;  ///< [IN]  CCM enable
  int nCCMat
    [9];  ///< [IN]  Color correction matrix(-8192~8192)

  unsigned int nRes[8];  ///<       Reserved

} MV_CC_CCM_PARAM;

/// CCM param
typedef struct _MV_CC_CCM_PARAM_EX_T_
{
  bool bCCMEnable;  ///< [IN]  CCM enable
  int nCCMat
    [9];  ///< [IN]  Color correction matrix(-65536~65536)
  unsigned int
    nCCMScale;  ///< [IN]  Quantitative scale(Integer power of 2, <= 65536)

  unsigned int nRes[8];  ///<       Reserved

} MV_CC_CCM_PARAM_EX;

/// Contrast structure
typedef struct _MV_CC_CONTRAST_PARAM_T_
{
  unsigned int nWidth;      ///< [IN]  Image Width
  unsigned int nHeight;     ///< [IN]  Image Height
  unsigned char * pSrcBuf;  ///< [IN]  Input data buffer
  unsigned int nSrcBufLen;  ///< [IN]  Input data length
  enum MvGvspPixelType
    enPixelType;  ///< [IN]  Pixel format

  unsigned char * pDstBuf;  ///< [OUT] Output data buffer
  unsigned int
    nDstBufSize;  ///< [IN]  Provided output buffer size
  unsigned int nDstBufLen;  ///< [OUT] Output data length

  unsigned int
    nContrastFactor;  ///< [IN]  Contrast factor,[1,10000]

  unsigned int nRes[8];  ///<       Reserved

} MV_CC_CONTRAST_PARAM;

/// Frame-specific information
typedef struct _MV_CC_FRAME_SPEC_INFO_
{
  /// Device frame-specific time scale
  unsigned int nSecondCount;  ///< [OUT] The Seconds
  unsigned int
    nCycleCount;  ///< [OUT] The Count of Cycle
  unsigned int
    nCycleOffset;  ///< [OUT] The Offset of Cycle

  float fGain;          ///< [OUT] Gain
  float fExposureTime;  ///< [OUT] Exposure Time
  unsigned int
    nAverageBrightness;  ///< [OUT] Average brightness

  /// White balance
  unsigned int nRed;    ///< [OUT] Red
  unsigned int nGreen;  ///< [OUT] Green
  unsigned int nBlue;   ///< [OUT] Blue

  unsigned int nFrameCounter;  ///< [OUT] Frame Counter
  unsigned int
    nTriggerIndex;  ///< [OUT] Trigger Counting

  unsigned int nInput;   ///< [OUT] Input
  unsigned int nOutput;  ///< [OUT] Output

  /// ROI Region
  unsigned short nOffsetX;  ///< [OUT] OffsetX
  unsigned short nOffsetY;  ///< [OUT] OffsetY
  unsigned short
    nFrameWidth;  ///< [OUT] The Width of Chunk
  unsigned short
    nFrameHeight;  ///< [OUT] The Height of Chunk

  unsigned int nReserved[16];  ///<       Reserved

} MV_CC_FRAME_SPEC_INFO;

/// High Bandwidth decode structure
typedef struct _MV_CC_HB_DECODE_PARAM_T_
{
  unsigned char * pSrcBuf;  ///< [IN]  Input data buffer
  unsigned int nSrcLen;     ///< [IN]  Input data size

  unsigned int nWidth;      ///< [OUT] Width
  unsigned int nHeight;     ///< [OUT] Height
  unsigned char * pDstBuf;  ///< [OUT] Output data buffer
  unsigned int
    nDstBufSize;  ///< [IN]  Provided output buffer size
  unsigned int nDstBufLen;  ///< [OUT] Output data size
  enum MvGvspPixelType
    enDstPixelType;  ///< [OUT] Output pixel format

  MV_CC_FRAME_SPEC_INFO
    stFrameSpecInfo;  ///< [OUT] Frame Spec Info

  unsigned int nRes[8];  ///<       Reserved

} MV_CC_HB_DECODE_PARAM;

/// Record Format Type
typedef enum _MV_RECORD_FORMAT_TYPE_
{
  MV_FormatType_Undefined =
    0,  ///< Undefined Recode Format Type
  MV_FormatType_AVI =
    1,  ///< AVI Recode Format Type

} MV_RECORD_FORMAT_TYPE;

/// Record Parameters
typedef struct _MV_CC_RECORD_PARAM_T_
{
  enum MvGvspPixelType
    enPixelType;  ///< [IN]  Pixel Type

  unsigned short nWidth;   ///< [IN]  Width
  unsigned short nHeight;  ///< [IN]  Height

  float fFrameRate;       ///< [IN]  The Rate of Frame
  unsigned int nBitRate;  ///< [IN]  The Rate of Bitrate

  MV_RECORD_FORMAT_TYPE
    enRecordFmtType;  ///< [IN]  Recode Format Type

  char *
    strFilePath;  ///< [IN]  File Path

  unsigned int nRes[8];  ///<       Reserved

} MV_CC_RECORD_PARAM;

/// Input Data
typedef struct _MV_CC_INPUT_FRAME_INFO_T_
{
  unsigned char * pData;  ///< [IN]  Record Data
  unsigned int
    nDataLen;  ///< [IN]  The Length of Record Data

  unsigned int nRes[8];  ///<       Reserved

} MV_CC_INPUT_FRAME_INFO;

/// Acquisition mode
typedef enum _MV_CAM_ACQUISITION_MODE_
{
  MV_ACQ_MODE_SINGLE = 0,      ///< Single Mode
  MV_ACQ_MODE_MUTLI = 1,       ///< Multi Mode
  MV_ACQ_MODE_CONTINUOUS = 2,  ///< Continuous Mode

} MV_CAM_ACQUISITION_MODE;

/// Gain Mode
typedef enum _MV_CAM_GAIN_MODE_
{
  MV_GAIN_MODE_OFF = 0,   ///< Single Mode
  MV_GAIN_MODE_ONCE = 1,  ///< Multi Mode
  MV_GAIN_MODE_CONTINUOUS =
    2,  ///< Continuous Mode

} MV_CAM_GAIN_MODE;

/// Exposure Mode
typedef enum _MV_CAM_EXPOSURE_MODE_
{
  MV_EXPOSURE_MODE_TIMED = 0,  ///< Timed
  MV_EXPOSURE_MODE_TRIGGER_WIDTH =
    1,  ///< TriggerWidth
} MV_CAM_EXPOSURE_MODE;

/// Auto Exposure Mode
typedef enum _MV_CAM_EXPOSURE_AUTO_MODE_
{
  MV_EXPOSURE_AUTO_MODE_OFF = 0,   ///< Off
  MV_EXPOSURE_AUTO_MODE_ONCE = 1,  ///< Once
  MV_EXPOSURE_AUTO_MODE_CONTINUOUS =
    2,  ///< Continuous

} MV_CAM_EXPOSURE_AUTO_MODE;

/// Trigger Mode
typedef enum _MV_CAM_TRIGGER_MODE_
{
  MV_TRIGGER_MODE_OFF = 0,  ///< Off
  MV_TRIGGER_MODE_ON = 1,   ///< ON

} MV_CAM_TRIGGER_MODE;

/// Gamma Selector
typedef enum _MV_CAM_GAMMA_SELECTOR_
{
  MV_GAMMA_SELECTOR_USER =
    1,  ///< Gamma Selector User
  MV_GAMMA_SELECTOR_SRGB =
    2,  ///< Gamma Selector sRGB

} MV_CAM_GAMMA_SELECTOR;

/// White Balance
typedef enum _MV_CAM_BALANCEWHITE_AUTO_
{
  MV_BALANCEWHITE_AUTO_OFF = 0,   ///< Off
  MV_BALANCEWHITE_AUTO_ONCE = 2,  ///< Once
  MV_BALANCEWHITE_AUTO_CONTINUOUS =
    1,  ///< Continuous

} MV_CAM_BALANCEWHITE_AUTO;

/// Trigger Source
typedef enum _MV_CAM_TRIGGER_SOURCE_
{
  MV_TRIGGER_SOURCE_LINE0 = 0,     ///< Line0
  MV_TRIGGER_SOURCE_LINE1 = 1,     ///< Line1
  MV_TRIGGER_SOURCE_LINE2 = 2,     ///< Line2
  MV_TRIGGER_SOURCE_LINE3 = 3,     ///< Line3
  MV_TRIGGER_SOURCE_COUNTER0 = 4,  ///< Conuter0

  MV_TRIGGER_SOURCE_SOFTWARE = 7,  ///< Software
  MV_TRIGGER_SOURCE_FrequencyConverter =
    8,  ///< Frequency Converter

} MV_CAM_TRIGGER_SOURCE;

/// GigEVision IP Configuration
#define MV_IP_CFG_STATIC 0x05000000  ///< Static
#define MV_IP_CFG_DHCP 0x06000000    ///< DHCP
#define MV_IP_CFG_LLA 0x04000000     ///< LLA

/// GigEVision Net Transfer Mode
#define MV_NET_TRANS_DRIVER 0x00000001  ///< Driver
#define MV_NET_TRANS_SOCKET 0x00000002  ///< Socket

/// CameraLink Baud Rates (CLUINT32)
#define MV_CAML_BAUDRATE_9600 0x00000001  ///< 9600
#define MV_CAML_BAUDRATE_19200 \
  0x00000002  ///< 19200
#define MV_CAML_BAUDRATE_38400 \
  0x00000004  ///< 38400
#define MV_CAML_BAUDRATE_57600 \
  0x00000008  ///< 57600
#define MV_CAML_BAUDRATE_115200 \
  0x00000010  ///< 115200
#define MV_CAML_BAUDRATE_230400 \
  0x00000020  ///< 230400
#define MV_CAML_BAUDRATE_460800 \
  0x00000040  ///< 460800
#define MV_CAML_BAUDRATE_921600 \
  0x00000080  ///< 921600
#define MV_CAML_BAUDRATE_AUTOMAX \
  0x40000000  ///< Auto Max

/// Exception message type
#define MV_EXCEPTION_DEV_DISCONNECT \
  0x00008001  ///< The device is disconnected
#define MV_EXCEPTION_VERSION_CHECK \
  0x00008002  ///< SDK does not match the driver version

/// \~english U3V stream exception types
typedef enum _MV_CC_STREAM_EXCEPTION_TYPE_
{
  MV_CC_STREAM_EXCEPTION_ABNORMAL_IMAGE = 0x4001,  ///< \~english Abnormal image; frame discarded
  MV_CC_STREAM_EXCEPTION_LIST_OVERFLOW = 0x4002,   ///< \~english Buffer list overflow; discard the oldest frame
  MV_CC_STREAM_EXCEPTION_LIST_EMPTY = 0x4003,      ///< \~english Buffer list empty; frame discarded
  MV_CC_STREAM_EXCEPTION_RECONNECTION = 0x4004,    ///< \~english Recover from stream disconnection
  MV_CC_STREAM_EXCEPTION_DISCONNECTED = 0x4005,    ///< \~english Stream disconnected; recovery failed; streaming stopped
  MV_CC_STREAM_EXCEPTION_DEVICE = 0x4006,          ///< \~english Device error; streaming stopped

} MV_CC_STREAM_EXCEPTION_TYPE;

///< Max length of event name
#define MAX_EVENT_NAME_SIZE 128

/// Event callback infomation
typedef struct _MV_EVENT_OUT_INFO_
{
  char EventName
    [MAX_EVENT_NAME_SIZE];  ///< [OUT] Event name

  unsigned short nEventID;  ///< [OUT] Event ID
  unsigned short
    nStreamChannel;  ///< [OUT] Circulation number

  unsigned int nBlockIdHigh;  ///< [OUT] BlockId high
  unsigned int nBlockIdLow;   ///< [OUT] BlockId low

  unsigned int
    nTimestampHigh;            ///< [OUT] Timestramp high
  unsigned int nTimestampLow;  ///< [OUT] Timestramp low

  void * pEventData;            ///< [OUT] Event data
  unsigned int nEventDataSize;  ///< [OUT] Event data len

  unsigned int nReserved[16];  ///<       Reserved

} MV_EVENT_OUT_INFO;

/// File Access
typedef struct _MV_CC_FILE_ACCESS_T
{
  const char * pUserFileName;  ///< [IN]  User file name
  const char * pDevFileName;  ///< [IN]  Device file name

  unsigned int nReserved[32];  ///<       Reserved

} MV_CC_FILE_ACCESS;

/// File Access
typedef struct _MV_CC_FILE_ACCESS_E
{
  char *
    pUserFileBuf;  ///< [IN]       User data Buffer
  unsigned int
    pFileBufSize;  ///< [IN]       data buffer size
  unsigned int
    pFileBufLen;  ///< [OUT][IN]  data buffer len
  const char *
    pDevFileName;  ///< [IN]       Device file name

  unsigned int nReserved
    [32];  ///<           Reserved
} MV_CC_FILE_ACCESS_EX;

/// File Access Progress
typedef struct _MV_CC_FILE_ACCESS_PROGRESS_T
{
  int64_t nCompleted;  ///< [OUT] Completed Length
  int64_t nTotal;      ///< [OUT] Total Length

  unsigned int nReserved[8];  ///<       Reserved

} MV_CC_FILE_ACCESS_PROGRESS;

/// The transmission type of Gige
typedef enum _MV_GIGE_TRANSMISSION_TYPE_
{
  MV_GIGE_TRANSTYPE_UNICAST =
    0x0,  ///< Unicast mode
  MV_GIGE_TRANSTYPE_MULTICAST =
    0x1,  ///< Multicast mode
  MV_GIGE_TRANSTYPE_LIMITEDBROADCAST =
    0x2,  ///< Limited broadcast mode,not support
  MV_GIGE_TRANSTYPE_SUBNETBROADCAST =
    0x3,  ///< Subnet broadcast mode,not support
  MV_GIGE_TRANSTYPE_CAMERADEFINED =
    0x4,  ///< Transtype from camera,not support
  MV_GIGE_TRANSTYPE_UNICAST_DEFINED_PORT =
    0x5,  ///< User Defined Receive Data Port
  MV_GIGE_TRANSTYPE_UNICAST_WITHOUT_RECV =
    0x00010000,  ///< Unicast without receive data
  MV_GIGE_TRANSTYPE_MULTICAST_WITHOUT_RECV =
    0x00010001,  ///< Multicast without receive data

} MV_GIGE_TRANSMISSION_TYPE;

/// Transmission type
typedef struct _MV_TRANSMISSION_TYPE_T
{
  MV_GIGE_TRANSMISSION_TYPE
    enTransmissionType;  ///< [IN]  Transmission type
  unsigned int nDestIp;  ///< [IN]  Destination IP
  unsigned short
    nDestPort;  ///< [IN]  Destination port

  unsigned int nReserved[32];  ///<       Reserved

} MV_TRANSMISSION_TYPE;

/// Action Command
typedef struct _MV_ACTION_CMD_INFO_T
{
  unsigned int
    nDeviceKey;  ///< [IN]  Device Key;
  unsigned int
    nGroupKey;  ///< [IN]  Group Key
  unsigned int
    nGroupMask;  ///< [IN]  Group Mask

  unsigned int
    bActionTimeEnable;  ///< [IN]  Action Time Enable
  int64_t
    nActionTime;  ///< [IN]  Action Time

  const char *
    pBroadcastAddress;  ///< [IN]  Broadcast Address
  unsigned int
    nTimeOut;  ///< [IN]  TimeOut

  unsigned int
    bSpecialNetEnable;  ///< [IN]  Special IP Enable
  unsigned int
    nSpecialNetIP;  ///< [IN]  Special Net IP address

  unsigned int nReserved
    [14];  ///<       Reserved

} MV_ACTION_CMD_INFO;

/// Action Command Result
typedef struct _MV_ACTION_CMD_RESULT_T
{
  unsigned char strDeviceAddress
    [12 + 3 + 1];  ///< [OUT] IP address of the device

  int
    nStatus;  ///< [OUT] status code returned by the device
              //1.0x0000:success.
              //2.0x8001:Command is not supported by the device.
  //3.0x8013:The device is not synchronized to a master clock to be used as time reference.
  //4.0x8015:A device queue or packet data has overflowed.
  //5.0x8016:The requested scheduled action command was requested at a time that is already past.

  unsigned int nReserved[4];  ///<      Reserved

} MV_ACTION_CMD_RESULT;

/// Action Command Result List
typedef struct _MV_ACTION_CMD_RESULT_LIST_T
{
  unsigned int
    nNumResults;  ///< [OUT] Number of returned values
  MV_ACTION_CMD_RESULT *
    pResults;  ///< [OUT] Reslut of action command

} MV_ACTION_CMD_RESULT_LIST;

/// Interface type corresponds to each node
enum MV_XML_InterfaceType
{
  IFT_IValue,        ///< IValue interface
  IFT_IBase,         ///< IBase interface
  IFT_IInteger,      ///< IInteger interface
  IFT_IBoolean,      ///< IBoolean interface
  IFT_ICommand,      ///< ICommand interface
  IFT_IFloat,        ///< IFloat interface
  IFT_IString,       ///< IString interface
  IFT_IRegister,     ///< IRegister interface
  IFT_ICategory,     ///< ICategory interface
  IFT_IEnumeration,  ///< IEnumeration interface
  IFT_IEnumEntry,    ///< IEnumEntry interface
  IFT_IPort,         ///< IPort interface
};

/// Node Access Mode
enum MV_XML_AccessMode
{
  AM_NI,         ///< Not implemented
  AM_NA,         ///< Not available
  AM_WO,         ///< Write Only
  AM_RO,         ///< Read Only
  AM_RW,         ///< Read and Write
  AM_Undefined,  ///< Object is not yet initialized
  AM_CycleDetect,  ///< used internally for AccessMode cycle detection
};

/// Max XML Symbolic Number
#define MV_MAX_XML_SYMBOLIC_NUM 64
/// Enumeration Value
typedef struct _MVCC_ENUMVALUE_T
{
  unsigned int nCurValue;  ///< [OUT] Current Value
  unsigned int
    nSupportedNum;  ///< [OUT] Number of valid data
  unsigned int nSupportValue
    [MV_MAX_XML_SYMBOLIC_NUM];  ///< [OUT] Support Value

  unsigned int nReserved[4];  ///<       Reserved

} MVCC_ENUMVALUE;

/// Max Enum Entry Symbolic Number
#define MV_MAX_SYMBOLIC_LEN 64
/// Enumeration Entry
typedef struct _MVCC_ENUMENTRY_T
{
  unsigned int nValue;  ///< [IN]  Value
  char
    chSymbolic[MV_MAX_SYMBOLIC_LEN];  ///< [OUT] Symbolic

  unsigned int nReserved[4];  ///< Reserved

} MVCC_ENUMENTRY;

/// Int Value
typedef struct _MVCC_INTVALUE_T
{
  unsigned int nCurValue;  ///< [OUT] Current Value
  unsigned int nMax;       ///< [OUT] Max
  unsigned int nMin;       ///< [OUT] Min
  unsigned int nInc;       ///< [OUT] Inc

  unsigned int nReserved[4];  ///<       Reserved

} MVCC_INTVALUE;

/// Int Value Ex
typedef struct _MVCC_INTVALUE_EX_T
{
  int64_t nCurValue;  ///< [OUT] Current Value
  int64_t nMax;       ///< [OUT] Max
  int64_t nMin;       ///< [OUT] Min
  int64_t nInc;       ///< [OUT] Inc

  unsigned int nReserved[16];  ///<       Reserved

} MVCC_INTVALUE_EX;

/// Float Value
typedef struct _MVCC_FLOATVALUE_T
{
  float fCurValue;  ///< [OUT] Current Value
  float fMax;       ///< [OUT] Max
  float fMin;       ///< [OUT] Min

  unsigned int nReserved[4];  ///<       Reserved

} MVCC_FLOATVALUE;

/// String Value
typedef struct _MVCC_STRINGVALUE_T
{
  char chCurValue[256];  ///< [OUT] Current Value

  int64_t nMaxLength;         ///< [OUT] MaxLength
  unsigned int nReserved[2];  ///<       Reserved

} MVCC_STRINGVALUE;

/// Color of Auxiliary Line
typedef struct _MVCC_COLORF
{
  float
    fR;  ///< [IN] Red, Range[0.0, 1.0]
  float
    fG;  ///< [IN] Green, Range[0.0, 1.0]
  float
    fB;  ///< [IN] Blue, Range[0.0, 1.0]
  float
    fAlpha;  ///< [IN] Alpha, Range[0.0, 1.0](Not Support)
  unsigned int nReserved[4];  ///<      Reserved

} MVCC_COLORF;

/// Point defined
typedef struct _MVCC_POINTF
{
  float
    fX;  ///< [IN] Distance From Left, Range[0.0, 1.0]
  float
    fY;  ///< [IN] Distance From Top, Range[0.0, 1.0]
  unsigned int nReserved[4];  ///<      Reserved

} MVCC_POINTF;

/// Rect Area Info
typedef struct _MVCC_RECT_INFO
{
  float
    fTop;  ///< [IN] Distance From Top, Range[0, 1.0]
  float
    fBottom;  ///< [IN] Distance From Top, Range[0, 1.0]
  float
    fLeft;  ///< [IN] Distance From Left, Range[0, 1.0]
  float
    fRight;  ///< [IN] Distance From Left, Range[0, 1.0]

  MVCC_COLORF
    stColor;  ///< [IN] Color of Auxiliary Line
  unsigned int
    nLineWidth;  ///< [IN] Width of Auxiliary Line, width is 1 or 2
  unsigned int nReserved[4];  ///<      Reserved

} MVCC_RECT_INFO;

/// Circle Area Info
typedef struct _MVCC_CIRCLE_INFO
{
  MVCC_POINTF
    stCenterPoint;  ///< [IN] Circle Point Info

  float
    fR1;  ///< [IN] Windth Radius, Range[0, 1.0]
  float
    fR2;  ///< [IN] Height Radius, Range[0, 1.0]

  MVCC_COLORF
    stColor;  ///< [IN] Color of Auxiliary Line
  unsigned int
    nLineWidth;  ///< [IN] Width of Auxiliary Line, width is 1 or 2
  unsigned int nReserved[4];  ///<      Reserved

} MVCC_CIRCLE_INFO;

/// Linear Auxiliary Line Info
typedef struct _MVCC_LINES_INFO
{
  MVCC_POINTF
    stStartPoint;  ///< [IN] The Start Point of Auxiliary Line
  MVCC_POINTF
    stEndPoint;  ///< [IN] The End Point of Auxiliary Line
  MVCC_COLORF
    stColor;  ///< [IN] Color of Auxiliary Line
  unsigned int
    nLineWidth;  ///< [IN] Width of Auxiliary Line, width is 1 or 2
  unsigned int nReserved[4];  ///<  Reserved

} MVCC_LINES_INFO;

///< The maximum number of source image to be split in time-division exposure
#define MV_MAX_SPLIT_NUM 8

/// Image reconstruction method
typedef enum _MV_IMAGE_RECONSTRUCTION_METHOD_
{
  MV_SPLIT_BY_LINE =
    1,  ///< Source image split into multiple images by line

} MV_IMAGE_RECONSTRUCTION_METHOD;

/// List of images after image reconstruction
typedef struct _MV_OUTPUT_IMAGE_INFO_
{
  unsigned int
    nWidth;  ///< [OUT]       Source Image Width
  unsigned int
    nHeight;  ///< [OUT]       Source Image Height
  enum MvGvspPixelType
    enPixelType;  ///< [OUT]       Pixel format

  unsigned char *
    pBuf;  ///< [IN][OUT]   Output data buffer
  unsigned int
    nBufLen;  ///< [OUT]       Output data length
  unsigned int
    nBufSize;  ///< [IN]        Provided output buffer size

  unsigned int nRes[8];  ///<             Reserved
} MV_OUTPUT_IMAGE_INFO;

/// Restructure image parameters
typedef struct _MV_RECONSTRUCT_IMAGE_PARAM_
{
  unsigned int nWidth;   ///< [IN]  Source Image Width
  unsigned int nHeight;  ///< [IN]  Source Image Height
  enum MvGvspPixelType enPixelType;  ///< [IN]  Pixel format

  unsigned char * pSrcData;  ///< [IN]  Input data buffer
  unsigned int nSrcDataLen;  ///< [IN]  Input data length

  unsigned int nExposureNum;  ///< [IN]  Exposure number
  MV_IMAGE_RECONSTRUCTION_METHOD
    enReconstructMethod;  ///< [IN]  Image restructuring method

  MV_OUTPUT_IMAGE_INFO stDstBufList
    [MV_MAX_SPLIT_NUM];  ///< [OUT] Output data info

  unsigned int nRes[4];
} MV_RECONSTRUCT_IMAGE_PARAM;

#endif /* _MV_CAMERA_PARAMS_H_ */
