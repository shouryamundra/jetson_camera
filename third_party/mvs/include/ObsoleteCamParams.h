
#ifndef _MV_OBSOLETE_CAM_PARAMS_H_
#define _MV_OBSOLETE_CAM_PARAMS_H_

#include "PixelType.h"

/// Output Frame Information
typedef struct _MV_FRAME_OUT_INFO_
{
  unsigned short nWidth;             ///< [OUT]  Image Width
  unsigned short nHeight;            ///< [OUT]  Image Height
  enum MvGvspPixelType enPixelType;  ///< [OUT]  Pixel Type

  unsigned int nFrameNum;  ///< [OUT]  Frame Number
  unsigned int
    nDevTimeStampHigh;  ///< [OUT]  Timestamp high 32 bits
  unsigned int
    nDevTimeStampLow;  ///< [OUT]  Timestamp low 32 bits
  unsigned int
    nReserved0;  ///< [OUT]  Reserved, 8-byte aligned
  int64_t
    nHostTimeStamp;  ///< [OUT]  Host-generated timestamp

  unsigned int nFrameLen;

  unsigned int nLostPacket;  // Lost packet count for this frame
  unsigned int nReserved[2];
} MV_FRAME_OUT_INFO;

/// Save image type
typedef struct _MV_SAVE_IMAGE_PARAM_T_
{
  unsigned char * pData;  ///< [IN]   Input Data Buffer
  unsigned int nDataLen;  ///< [IN]   Input Data Size
  enum MvGvspPixelType
    enPixelType;           ///< [IN]   Input Data Pixel Format
  unsigned short nWidth;   ///< [IN]   Image Width
  unsigned short nHeight;  ///< [IN]   Image Height

  unsigned char *
    pImageBuffer;          ///< [OUT]  Output Image Buffer
  unsigned int nImageLen;  ///< [OUT]  Output Image Size
  unsigned int
    nBufferSize;  ///< [IN]   Output buffer size provided
  enum MV_SAVE_IAMGE_TYPE
    enImageType;  ///< [IN]   Output Image Format

} MV_SAVE_IMAGE_PARAM;

typedef struct _MV_IMAGE_BASIC_INFO_
{
  unsigned short nWidthValue;
  unsigned short nWidthMin;
  unsigned int nWidthMax;
  unsigned int nWidthInc;

  unsigned int nHeightValue;
  unsigned int nHeightMin;
  unsigned int nHeightMax;
  unsigned int nHeightInc;

  float fFrameRateValue;
  float fFrameRateMin;
  float fFrameRateMax;

  unsigned int enPixelType;  ///< [OUT]  Current pixel format
  unsigned int
    nSupportedPixelFmtNum;  ///< [OUT]  Support pixel format
  unsigned int enPixelList[MV_MAX_XML_SYMBOLIC_NUM];
  unsigned int nReserved[8];

} MV_IMAGE_BASIC_INFO;

/// Noise feature type
typedef enum _MV_CC_BAYER_NOISE_FEATURE_TYPE
{
  MV_CC_BAYER_NOISE_FEATURE_TYPE_INVALID =
    0,  ///<        Invalid
  MV_CC_BAYER_NOISE_FEATURE_TYPE_PROFILE =
    1,  ///<        Noise curve
  MV_CC_BAYER_NOISE_FEATURE_TYPE_LEVEL =
    2,  ///<        Noise level
  MV_CC_BAYER_NOISE_FEATURE_TYPE_DEFAULT =
    1,  ///<        Default

} MV_CC_BAYER_NOISE_FEATURE_TYPE;

/// Denoise profile info
typedef struct _MV_CC_BAYER_NOISE_PROFILE_INFO_T_
{
  unsigned int nVersion;  ///<  version
  MV_CC_BAYER_NOISE_FEATURE_TYPE
    enNoiseFeatureType;  ///<  noise feature type
  enum MvGvspPixelType
    enPixelType;    ///<  image format
  int nNoiseLevel;  ///<  noise level
  unsigned int
    nCurvePointNum;   ///<  curve point number
  int * nNoiseCurve;  ///<  noise curve
  int * nLumCurve;    ///<  luminance curve

  unsigned int nRes[8];  ///<       Reserved

} MV_CC_BAYER_NOISE_PROFILE_INFO;

/// Bayer noise estimate param
typedef struct _MV_CC_BAYER_NOISE_ESTIMATE_PARAM_T_
{
  unsigned int nWidth;   ///< [IN]  Width
  unsigned int nHeight;  ///< [IN]  Height
  enum MvGvspPixelType
    enPixelType;  ///< [IN]  Pixel format

  unsigned char *
    pSrcData;  ///< [IN]  Input data buffer
  unsigned int
    nSrcDataLen;  ///< [IN]  Input data size

  unsigned int
    nNoiseThreshold;  ///< [IN]  Noise Threshold

  unsigned char *
    pCurveBuf;  ///< [IN]  Buffer used to store noise and brightness curves, size:4096 * sizeof(int) * 2)
  MV_CC_BAYER_NOISE_PROFILE_INFO
    stNoiseProfile;  ///< [OUT]  Denoise profile

  unsigned int
    nThreadNum;  ///< [IN]  Thread number, 0 means that the library is adaptive to the hardware, 1 means single thread(Default value), Greater than 1 indicates the number of threads

  unsigned int nRes[8];  ///<       Reserved

} MV_CC_BAYER_NOISE_ESTIMATE_PARAM;

/// Bayer spatial Denoise param
typedef struct _MV_CC_BAYER_SPATIAL_DENOISE_PARAM_T_
{
  unsigned int nWidth;   ///< [IN]  Width
  unsigned int nHeight;  ///< [IN]  Height
  enum MvGvspPixelType
    enPixelType;  ///< [IN]  Pixel format

  unsigned char *
    pSrcData;  ///< [IN]  Input data buffer
  unsigned int
    nSrcDataLen;  ///< [IN]  Input data size

  unsigned char *
    pDstBuf;  ///< [OUT] Output data buffer
  unsigned int
    nDstBufSize;  ///< [IN]  Provided output buffer size
  unsigned int
    nDstBufLen;  ///< [OUT] Output data length

  MV_CC_BAYER_NOISE_PROFILE_INFO
    stNoiseProfile;  ///< [IN]  Denoise profile
  unsigned int
    nDenoiseStrength;  ///< [IN]  nDenoise Strength
  unsigned int
    nSharpenStrength;  ///< [IN]  Sharpen Strength
  unsigned int
    nNoiseCorrect;  ///< [IN]  Noise Correct

  unsigned int
    nThreadNum;  ///< [IN]  Thread number, 0 means that the library is adaptive to the hardware, 1 means single thread(Default value), Greater than 1 indicates the number of threads

  unsigned int nRes[8];  ///<       Reserved

} MV_CC_BAYER_SPATIAL_DENOISE_PARAM;

/// CLUT param
typedef struct _MV_CC_CLUT_PARAM_T_
{
  bool bCLUTEnable;  ///< [IN]  CLUT enable
  unsigned int
    nCLUTScale;  ///< [IN]  Quantitative scale(Integer power of 2, <= 65536)
  unsigned int
    nCLUTSize;  ///< [IN]  CLUT size[17,33](Recommended values of 17)
  unsigned char * pCLUTBuf;  ///< [IN]  CLUT buffer
  unsigned int
    nCLUTBufLen;  ///< [IN]  CLUT buffer length(nCLUTSize*nCLUTSize*nCLUTSize*sizeof(int)*3)

  unsigned int nRes[8];  ///<       Reserved

} MV_CC_CLUT_PARAM;

/// Sharpen structure
typedef struct _MV_CC_SHARPEN_PARAM_T_
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
    nSharpenAmount;  ///< [IN]  Sharpen amount,[0,500]  //   [nSharpenAmount is deprecated; use nSharpenPosAmount & nSharpenNegAmount instead ]
  unsigned int
    nSharpenRadius;  ///< [IN]  Sharpen radius(The larger the radius, the longer it takes),[1,21]
  unsigned int
    nSharpenThreshold;  ///< [IN]  Sharpen threshold,[0,255]

  unsigned int nSharpenPosAmount;  // [IN]     Positive sharpening strength, range: [0, 500]
  unsigned int nSharpenNegAmount;  // [IN]     Negative sharpening strength, range: [0, 500]

  unsigned int nRes[6];  ///<       Reserved

} MV_CC_SHARPEN_PARAM;

/// Color correct structure
typedef struct _MV_CC_COLOR_CORRECT_PARAM_T_
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
    nImageBit;  ///< [IN]  Image bit(8 or 10 or 12 or 16)
  MV_CC_GAMMA_PARAM stGammaParam;  ///< [IN]  Gamma info
  MV_CC_CCM_PARAM_EX stCCMParam;   ///< [IN]  CCM info
  MV_CC_CLUT_PARAM stCLUTParam;    ///< [IN]  CLUT info

  unsigned int nRes[8];  ///<       Reserved

} MV_CC_COLOR_CORRECT_PARAM;

/// Rect ROI structure
typedef struct _MV_CC_RECT_I_
{
  unsigned int nX;       ///< X Position
  unsigned int nY;       ///< Y Position
  unsigned int nWidth;   ///< Rect Width
  unsigned int nHeight;  ///< Rect Height

} MV_CC_RECT_I;

/// Noise estimate structure
typedef struct _MV_CC_NOISE_ESTIMATE_PARAM_T_
{
  unsigned int nWidth;   ///< [IN]  Image Width
  unsigned int nHeight;  ///< [IN]  Image Height
  enum MvGvspPixelType
    enPixelType;            ///< [IN]  Pixel format
  unsigned char * pSrcBuf;  ///< [IN]  Input data buffer
  unsigned int nSrcBufLen;  ///< [IN]  Input data length

  MV_CC_RECT_I * pstROIRect;  ///< [IN]  Image ROI
  unsigned int nROINum;       ///< [IN]  ROI number

  ///< Bayer Noise estimate param,Mono8/RGB formats are invalid
  unsigned int
    nNoiseThreshold;  ///< [IN]  Noise threshold[0,4095]
  ///< Suggestive value:8bit,0xE0;10bit,0x380;12bit,0xE00

  unsigned char *
    pNoiseProfile;  ///< [OUT] Output Noise Profile
  unsigned int
    nNoiseProfileSize;  ///< [IN]  Provided output buffer size
  unsigned int
    nNoiseProfileLen;  ///< [OUT] Output Noise Profile length

  unsigned int nRes[8];  ///<       Reserved

} MV_CC_NOISE_ESTIMATE_PARAM;

/// Spatial denoise structure
typedef struct _MV_CC_SPATIAL_DENOISE_PARAM_T_
{
  unsigned int nWidth;   ///< [IN]  Image Width
  unsigned int nHeight;  ///< [IN]  Image Height
  enum MvGvspPixelType
    enPixelType;            ///< [IN]  Pixel format
  unsigned char * pSrcBuf;  ///< [IN]  Input data buffer
  unsigned int nSrcBufLen;  ///< [IN]  Input data length

  unsigned char * pDstBuf;  ///< [OUT] Output data buffer
  unsigned int
    nDstBufSize;  ///< [IN]  Provided output buffer size
  unsigned int nDstBufLen;  ///< [OUT] Output data length

  unsigned char *
    pNoiseProfile;  ///< [IN]  Input Noise Profile
  unsigned int
    nNoiseProfileLen;  ///< [IN]  Input Noise Profile length

  ///< Bayer Spatial denoise param,Mono8/RGB formats are invalid
  unsigned int
    nBayerDenoiseStrength;  ///< [IN]  Denoise Strength[0,100]
  unsigned int
    nBayerSharpenStrength;  ///< [IN]  Sharpen Strength[0,32]
  unsigned int
    nBayerNoiseCorrect;  ///< [IN]  Noise Correct[0,1280]

  ///< Mono8/RGB Spatial denoise param,Bayer formats are invalid
  unsigned int
    nNoiseCorrectLum;  ///< [IN]  Noise Correct Lum[1,2000]
  unsigned int
    nNoiseCorrectChrom;  ///< [IN]  Noise Correct Chrom[1,2000]
  unsigned int
    nStrengthLum;  ///< [IN]  Strength Lum[0,100]
  unsigned int
    nStrengthChrom;  ///< [IN]  Strength Chrom[0,100]
  unsigned int
    nStrengthSharpen;  ///< [IN]  Strength Sharpen[1,1000]

  unsigned int nRes[8];  ///<       Reserved

} MV_CC_SPATIAL_DENOISE_PARAM;

/// LSC calib structure
typedef struct _MV_CC_LSC_CALIB_PARAM_T_
{
  unsigned int nWidth;   ///< [IN]  Image Width
  unsigned int nHeight;  ///< [IN]  Image Height
  enum MvGvspPixelType
    enPixelType;            ///< [IN]  Pixel format
  unsigned char * pSrcBuf;  ///< [IN]  Input data buffer
  unsigned int nSrcBufLen;  ///< [IN]  Input data length

  unsigned char *
    pCalibBuf;  ///< [OUT] Output calib buffer
  unsigned int
    nCalibBufSize;  ///< [IN]  Provided output buffer size
  unsigned int
    nCalibBufLen;  ///< [OUT] Output calib buffer length

  unsigned int nSecNumW;  ///< [IN]  Width Sec num
  unsigned int nSecNumH;  ///< [IN]  Height Sec num
  unsigned int nPadCoef;  ///< [IN]  Pad Coef[1,5]
  unsigned int
    nCalibMethod;  ///< [IN]  Calib method
  unsigned int
    nTargetGray;  ///< [IN]  Target Gray
  ///< 8bit,range:[0,255]
  ///< 10bit,range:[0,1023]
  ///< 12bit,range:[0,4095]

  unsigned int nRes[8];  ///<       Reserved

} MV_CC_LSC_CALIB_PARAM;

/// LSC correct structure
typedef struct _MV_CC_LSC_CORRECT_PARAM_T_
{
  unsigned int nWidth;   ///< [IN]  Image Width
  unsigned int nHeight;  ///< [IN]  Image Height
  enum MvGvspPixelType
    enPixelType;            ///< [IN]  Pixel format
  unsigned char * pSrcBuf;  ///< [IN]  Input data buffer
  unsigned int nSrcBufLen;  ///< [IN]  Input data length

  unsigned char * pDstBuf;  ///< [OUT] Output data buffer
  unsigned int
    nDstBufSize;  ///< [IN]  Provided output buffer size
  unsigned int nDstBufLen;  ///< [OUT] Output data length

  unsigned char *
    pCalibBuf;  ///< [IN]  Input calib buffer
  unsigned int
    nCalibBufLen;  ///< [IN]  Input calib buffer length

  unsigned int nRes[8];  ///<       Reserved

} MV_CC_LSC_CORRECT_PARAM;

/// The maximum number of child nodes corresponding to a node
#define MV_MAX_XML_NODE_NUM_C 128

/// The maximum length of node name string
#define MV_MAX_XML_NODE_STRLEN_C 64

/// The maximum length of Node String
#define MV_MAX_XML_STRVALUE_STRLEN_C 64

/// The maximum length of the node description field
#define MV_MAX_XML_DISC_STRLEN_C 512

/// The maximum number of units
#define MV_MAX_XML_ENTRY_NUM 10

/// The maximum number of parent nodes
#define MV_MAX_XML_PARENTS_NUM 8

/// The length of the name of each unit that has been implemented
#define MV_MAX_XML_SYMBOLIC_STRLEN_C 64

enum MV_XML_Visibility
{
  V_Beginner = 0,   ///< Always visible
  V_Expert = 1,     ///< Visible for experts or Gurus
  V_Guru = 2,       ///< Visible for Gurus
  V_Invisible = 3,  ///< Not Visible
  V_Undefined = 99  ///< Object is not yet initialized
};

/// Single Node Basic Attributes
typedef struct _MV_XML_NODE_FEATURE_
{
  enum MV_XML_InterfaceType enType;     ///< Node Type
  enum MV_XML_Visibility enVisivility;  ///< Is visibility
  char strDescription
    [MV_MAX_XML_DISC_STRLEN_C];  ///< Node Description, NOT SUPPORT NOW
  char strDisplayName
    [MV_MAX_XML_NODE_STRLEN_C];               ///< Display Name
  char strName[MV_MAX_XML_NODE_STRLEN_C];     ///< Node Name
  char strToolTip[MV_MAX_XML_DISC_STRLEN_C];  ///< Notice

  unsigned int nReserved[4];
} MV_XML_NODE_FEATURE;

/// Node List
typedef struct _MV_XML_NODES_LIST_
{
  unsigned int nNodeNum;  ///< Node Number
  MV_XML_NODE_FEATURE stNodes[MV_MAX_XML_NODE_NUM_C];
} MV_XML_NODES_LIST;

typedef struct _MV_XML_FEATURE_Value_
{
  enum MV_XML_InterfaceType enType;  ///< Node Type
  char strDescription
    [MV_MAX_XML_DISC_STRLEN_C];  ///< Node Description, NOT SUPPORT NOW
  char strDisplayName
    [MV_MAX_XML_NODE_STRLEN_C];               ///< Display Name
  char strName[MV_MAX_XML_NODE_STRLEN_C];     ///< Node Name
  char strToolTip[MV_MAX_XML_DISC_STRLEN_C];  ///< Notice
  unsigned int nReserved[4];
} MV_XML_FEATURE_Value;

typedef struct _MV_XML_FEATURE_Base_
{
  enum MV_XML_AccessMode enAccessMode;  ///< Access Mode
} MV_XML_FEATURE_Base;

typedef struct _MV_XML_FEATURE_Integer_
{
  char strName[MV_MAX_XML_NODE_STRLEN_C];
  char strDisplayName[MV_MAX_XML_NODE_STRLEN_C];
  char strDescription
    [MV_MAX_XML_DISC_STRLEN_C];  ///< NOT SUPPORT NOW
  char strToolTip[MV_MAX_XML_DISC_STRLEN_C];

  enum MV_XML_Visibility enVisivility;  ///< Visible
  enum MV_XML_AccessMode enAccessMode;  ///< Access Mode
  int
    bIsLocked;  ///< Locked. 0-NO; 1-YES, NOT SUPPORT NOW
  int64_t nValue;      ///< Current Value
  int64_t nMinValue;   ///< Min Value
  int64_t nMaxValue;   ///< Max Value
  int64_t nIncrement;  ///< Increment

  unsigned int nReserved[4];

} MV_XML_FEATURE_Integer;

typedef struct _MV_XML_FEATURE_Boolean_
{
  char strName[MV_MAX_XML_NODE_STRLEN_C];
  char strDisplayName[MV_MAX_XML_NODE_STRLEN_C];
  char strDescription
    [MV_MAX_XML_DISC_STRLEN_C];  ///< NOT SUPPORT NOW
  char strToolTip[MV_MAX_XML_DISC_STRLEN_C];

  enum MV_XML_Visibility enVisivility;  ///< Visible
  enum MV_XML_AccessMode enAccessMode;  ///< Access Mode
  int
    bIsLocked;  ///< Locked. 0-NO; 1-YES, NOT SUPPORT NOW
  bool bValue;  ///< Current Value

  unsigned int nReserved[4];
} MV_XML_FEATURE_Boolean;

typedef struct _MV_XML_FEATURE_Command_
{
  char strName[MV_MAX_XML_NODE_STRLEN_C];
  char strDisplayName[MV_MAX_XML_NODE_STRLEN_C];
  char strDescription
    [MV_MAX_XML_DISC_STRLEN_C];  ///< NOT SUPPORT NOW
  char strToolTip[MV_MAX_XML_DISC_STRLEN_C];

  enum MV_XML_Visibility enVisivility;  ///< Visible
  enum MV_XML_AccessMode enAccessMode;  ///< Access Mode
  int
    bIsLocked;  ///< Locked. 0-NO; 1-YES, NOT SUPPORT NOW

  unsigned int nReserved[4];
} MV_XML_FEATURE_Command;

typedef struct _MV_XML_FEATURE_Float_
{
  char strName[MV_MAX_XML_NODE_STRLEN_C];
  char strDisplayName[MV_MAX_XML_NODE_STRLEN_C];
  char strDescription
    [MV_MAX_XML_DISC_STRLEN_C];  ///< NOT SUPPORT NOW
  char strToolTip[MV_MAX_XML_DISC_STRLEN_C];

  enum MV_XML_Visibility enVisivility;  ///< Visible
  enum MV_XML_AccessMode enAccessMode;  ///< Access Mode
  int
    bIsLocked;  ///< Locked. 0-NO; 1-YES, NOT SUPPORT NOW
  double dfValue;      ///< Current Value
  double dfMinValue;   ///< Min Value
  double dfMaxValue;   ///< Max Value
  double dfIncrement;  ///< Increment

  unsigned int nReserved[4];
} MV_XML_FEATURE_Float;

typedef struct _MV_XML_FEATURE_String_
{
  char strName[MV_MAX_XML_NODE_STRLEN_C];
  char strDisplayName[MV_MAX_XML_NODE_STRLEN_C];
  char strDescription
    [MV_MAX_XML_DISC_STRLEN_C];  ///< NOT SUPPORT NOW
  char strToolTip[MV_MAX_XML_DISC_STRLEN_C];

  enum MV_XML_Visibility enVisivility;  ///< Visible
  enum MV_XML_AccessMode enAccessMode;  ///< Access Mode
  int
    bIsLocked;  ///< Locked. 0-NO; 1-YES, NOT SUPPORT NOW
  char
    strValue[MV_MAX_XML_STRVALUE_STRLEN_C];  ///< Current Value

  unsigned int nReserved[4];
} MV_XML_FEATURE_String;

typedef struct _MV_XML_FEATURE_Register_
{
  char strName[MV_MAX_XML_NODE_STRLEN_C];
  char strDisplayName[MV_MAX_XML_NODE_STRLEN_C];
  char strDescription
    [MV_MAX_XML_DISC_STRLEN_C];  ///< NOT SUPPORT NOW
  char strToolTip[MV_MAX_XML_DISC_STRLEN_C];

  enum MV_XML_Visibility enVisivility;  ///< Visible
  enum MV_XML_AccessMode enAccessMode;  ///< Access Mode
  int
    bIsLocked;  ///< Locked. 0-NO; 1-YES, NOT SUPPORT NOW
  int64_t nAddrValue;  ///< Current Value

  unsigned int nReserved[4];
} MV_XML_FEATURE_Register;

typedef struct _MV_XML_FEATURE_Category_
{
  char strDescription
    [MV_MAX_XML_DISC_STRLEN_C];  ///< Node Description, NOT SUPPORT NOW
  char strDisplayName
    [MV_MAX_XML_NODE_STRLEN_C];               ///< Display Name
  char strName[MV_MAX_XML_NODE_STRLEN_C];     ///< Node Name
  char strToolTip[MV_MAX_XML_DISC_STRLEN_C];  ///< Notice

  enum MV_XML_Visibility enVisivility;  ///< Visible

  unsigned int nReserved[4];
} MV_XML_FEATURE_Category;

typedef struct _MV_XML_FEATURE_EnumEntry_
{
  char strName[MV_MAX_XML_NODE_STRLEN_C];
  char strDisplayName[MV_MAX_XML_NODE_STRLEN_C];
  char strDescription
    [MV_MAX_XML_DISC_STRLEN_C];  ///< NOT SUPPORT NOW
  char strToolTip[MV_MAX_XML_DISC_STRLEN_C];
  int bIsImplemented;
  int nParentsNum;
  MV_XML_NODE_FEATURE stParentsList[MV_MAX_XML_PARENTS_NUM];

  enum MV_XML_Visibility enVisivility;  ///< Visible
  int64_t nValue;                       ///< Current Value
  enum MV_XML_AccessMode enAccessMode;  ///< Access Mode
  int
    bIsLocked;  ///< Locked. 0-NO; 1-YES, NOT SUPPORT NOW
  int nReserved[8];

} MV_XML_FEATURE_EnumEntry;

typedef struct _MV_XML_FEATURE_Enumeration_
{
  enum MV_XML_Visibility enVisivility;  ///< Visible
  char strDescription
    [MV_MAX_XML_DISC_STRLEN_C];  ///< Node Description, NOT SUPPORT NOW
  char strDisplayName
    [MV_MAX_XML_NODE_STRLEN_C];               ///< Display Name
  char strName[MV_MAX_XML_NODE_STRLEN_C];     ///< Node Name
  char strToolTip[MV_MAX_XML_DISC_STRLEN_C];  ///< Notice

  int nSymbolicNum;  ///< Symbolic Number
  char strCurrentSymbolic
    [MV_MAX_XML_SYMBOLIC_STRLEN_C];  ///< Current Symbolic Index
  char strSymbolic[MV_MAX_XML_SYMBOLIC_NUM][MV_MAX_XML_SYMBOLIC_STRLEN_C];
  enum MV_XML_AccessMode enAccessMode;  ////< Access Mode
  int
    bIsLocked;  ///< Locked. 0-NO; 1-YES, NOT SUPPORT NOW
  int64_t nValue;  ///< Current Value

  unsigned int nReserved[4];
} MV_XML_FEATURE_Enumeration;

typedef struct _MV_XML_FEATURE_Port_
{
  enum MV_XML_Visibility enVisivility;  ///< Visible
  char strDescription
    [MV_MAX_XML_DISC_STRLEN_C];  ///< Node Description, NOT SUPPORT NOW
  char strDisplayName
    [MV_MAX_XML_NODE_STRLEN_C];               ///< Display Name
  char strName[MV_MAX_XML_NODE_STRLEN_C];     ///< Node Name
  char strToolTip[MV_MAX_XML_DISC_STRLEN_C];  ///< Notice

  enum MV_XML_AccessMode enAccessMode;  ///< Access Mode
  int
    bIsLocked;  ///< Locked. 0-NO; 1-YES, NOT SUPPORT NOW

  unsigned int nReserved[4];
} MV_XML_FEATURE_Port;

typedef struct _MV_XML_CAMERA_FEATURE_
{
  enum MV_XML_InterfaceType enType;
  union {
    MV_XML_FEATURE_Integer stIntegerFeature;
    MV_XML_FEATURE_Float stFloatFeature;
    MV_XML_FEATURE_Enumeration stEnumerationFeature;
    MV_XML_FEATURE_String stStringFeature;
  } SpecialFeature;

} MV_XML_CAMERA_FEATURE;

/// Save Image Parameters
typedef struct _MV_SAVE_IMAGE_PARAM_T_EX_
{
  unsigned char * pData;  ///< [IN]  Input Data Buffer
  unsigned int nDataLen;  ///< [IN]  Input Data length
  enum MvGvspPixelType
    enPixelType;  ///< [IN]  Input Data Pixel Format
  unsigned short nWidth;   ///< [IN]  Image Width
  unsigned short nHeight;  ///< [IN]  Image Height

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

} MV_SAVE_IMAGE_PARAM_EX;

/// Save Image Parameters
typedef struct _MV_SAVE_IMG_TO_FILE_PARAM_
{
  enum MvGvspPixelType
    enPixelType;  ///< [IN]  The pixel format of the input data
  unsigned char * pData;   ///< [IN]  Input Data Buffer
  unsigned int nDataLen;   ///< [IN]  Input Data length
  unsigned short nWidth;   ///< [IN]  Image Width
  unsigned short nHeight;  ///< [IN]  Image Height
  enum MV_SAVE_IAMGE_TYPE
    enImageType;  ///< [IN]  Input Image Format
  unsigned int
    nQuality;  ///< [IN]  JPG Encoding quality(50-99],PNG Encoding quality[0-9], Other formats are invalid
  char pImagePath[256];  ///< [IN]  Input file path

  int
    iMethodValue;  ///< [IN]  Bayer interpolation method  0-Fast 1-Equilibrium 2-Optimal

  unsigned int nReserved[8];  ///<       Reserved

} MV_SAVE_IMG_TO_FILE_PARAM;

// Pixel convert structure
typedef struct _MV_CC_PIXEL_CONVERT_PARAM_
{
  unsigned short nWidth;   ///< [IN]  Width
  unsigned short nHeight;  ///< [IN]  Height

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

} MV_CC_PIXEL_CONVERT_PARAM;

#endif /* _MV_OBSOLETE_CAM_PARAMS_H_ */
