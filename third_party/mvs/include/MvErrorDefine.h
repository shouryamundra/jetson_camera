
#ifndef _MV_ERROR_DEFINE_H_
#define _MV_ERROR_DEFINE_H_

#include "MvISPErrorDefine.h"

/********************************************************************/
///  \~english
///  \name Definition of correct code
///  @{
#define MV_OK 0x00000000  ///< Successed, no error
/// @}

/********************************************************************/
///  \~english
///  \name Definition of General error code
///  @{
#define MV_E_HANDLE \
  0x80000000  ///< Error or invalid handle
#define MV_E_SUPPORT \
  0x80000001  ///< Not supported function
#define MV_E_BUFOVER 0x80000002  ///< Buffer overflow
#define MV_E_CALLORDER \
  0x80000003  ///< Function calling order error
#define MV_E_PARAMETER \
  0x80000004  ///< Incorrect parameter
#define MV_E_RESOURCE \
  0x80000006  ///< Applying resource failed
#define MV_E_NODATA 0x80000007  ///< No data
#define MV_E_PRECONDITION \
  0x80000008  ///< Precondition error, or running environment changed
#define MV_E_VERSION \
  0x80000009  ///< Version mismatches
#define MV_E_NOENOUGH_BUF \
  0x8000000A  ///< Insufficient memory
#define MV_E_ABNORMAL_IMAGE \
  0x8000000B  ///< Abnormal image, maybe incomplete image because of lost packet
#define MV_E_LOAD_LIBRARY \
  0x8000000C  ///< Load library failed
#define MV_E_NOOUTBUF \
  0x8000000D  ///< No Avaliable Buffer
#define MV_E_ENCRYPT 0x8000000E   ///< Encryption error
#define MV_E_OPENFILE 0x8000000F  ///< open file error
#define MV_E_UNKNOW 0x800000FF    ///< Unknown error
/// @}

/********************************************************************/
///  \~english
///  \name GenICam Series Error Codes: Range from 0x80000100 to 0x800001FF
///  @{
#define MV_E_GC_GENERIC 0x80000100  ///< General error
#define MV_E_GC_ARGUMENT \
  0x80000101  ///< Illegal parameters
#define MV_E_GC_RANGE \
  0x80000102  ///< The value is out of range
#define MV_E_GC_PROPERTY 0x80000103  ///< Property
#define MV_E_GC_RUNTIME \
  0x80000104  ///< Running environment error
#define MV_E_GC_LOGICAL 0x80000105  ///< Logical error
#define MV_E_GC_ACCESS \
  0x80000106  ///< Node accessing condition error
#define MV_E_GC_TIMEOUT 0x80000107  ///< Timeout
#define MV_E_GC_DYNAMICCAST \
  0x80000108  ///< Transformation exception
#define MV_E_GC_UNKNOW \
  0x800001FF  ///< GenICam unknown error
/// @}

/********************************************************************/
///  \~english
///  \name GigE_STATUS Error Codes: Range from 0x80000200 to 0x800002FF
///  @{
#define MV_E_NOT_IMPLEMENTED \
  0x80000200  ///< The command is not supported by device
#define MV_E_INVALID_ADDRESS \
  0x80000201  ///< The target address being accessed does not exist
#define MV_E_WRITE_PROTECT \
  0x80000202  ///< The target address is not writable
#define MV_E_ACCESS_DENIED \
  0x80000203  ///< No permission
#define MV_E_BUSY \
  0x80000204  ///< Device is busy, or network disconnected
#define MV_E_PACKET \
  0x80000205  ///< Network data packet error
#define MV_E_NETER 0x80000206  ///< Network error
#define MV_E_IP_CONFLICT \
  0x80000221  ///< Device IP conflict
/// @}

/********************************************************************/
///  \~english
///  \name USB_STATUS Error Codes: Range from 0x80000300 to 0x800003FF
///  @{
#define MV_E_USB_READ \
  0x80000300  ///< Reading USB error
#define MV_E_USB_WRITE \
  0x80000301  ///< Writing USB error
#define MV_E_USB_DEVICE \
  0x80000302  ///< Device exception
#define MV_E_USB_GENICAM 0x80000303  ///< GenICam error
#define MV_E_USB_BANDWIDTH \
  0x80000304  ///< Insufficient bandwidth
#define MV_E_USB_DRIVER \
  0x80000305  ///< Driver mismatch or unmounted drive
#define MV_E_USB_UNKNOW \
  0x800003FF  ///< USB unknown error
/// @}

/********************************************************************/
///  \~english
///  \name Upgrade Error Codes: Range from 0x80000400 to 0x800004FF
///  @{
#define MV_E_UPG_FILE_MISMATCH \
  0x80000400  ///< Firmware mismatches
#define MV_E_UPG_LANGUSGE_MISMATCH \
  0x80000401  ///< Firmware language mismatches
#define MV_E_UPG_CONFLICT \
  0x80000402  ///< Upgrading conflicted (repeated upgrading requests during device upgrade)
#define MV_E_UPG_INNER_ERR \
  0x80000403  ///< Camera internal error during upgrade
#define MV_E_UPG_UNKNOW \
  0x800004FF  ///< Unknown error during upgrade
/// @}

#endif  //_MV_ERROR_DEFINE_H_
