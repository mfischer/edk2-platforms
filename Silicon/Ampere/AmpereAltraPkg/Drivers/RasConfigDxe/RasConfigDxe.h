/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _RAS_CONFIG_DXE_H_
#define _RAS_CONFIG_DXE_H_

#include <Uefi.h>
#include <Guid/MdeModuleHii.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
#include <Library/DevicePathLib.h>
#include <Library/DebugLib.h>
#include <Library/PrintLib.h>
#include <Library/HiiLib.h>
#include <Library/DevicePathLib.h>
#include <Library/HobLib.h>
#include <Library/NVParamLib.h>
#include <Library/AmpereCpuLib.h>
#include <Protocol/HiiConfigAccess.h>
#include <Protocol/HiiConfigRouting.h>
#include <PlatformInfoHob.h>
#include <NVParamDef.h>

#include "NVDataStruc.h"

//
// This is the generated IFR binary data for each formset defined in VFR.
//
extern UINT8  VfrBin[];

//
// This is the generated String package data for all .UNI files.
//
extern UINT8  RasConfigDxeStrings[];

#define RAS_2P_CE_THRESHOLD  OFFSET_OF (RAS_CONFIG_VARSTORE_DATA, Ras2pCeThreshold)

#define RAS_CONFIG_PRIVATE_SIGNATURE SIGNATURE_32 ('R', 'A', 'S', 'C')

typedef struct {
  UINTN                            Signature;

  EFI_HANDLE                       DriverHandle;
  EFI_HII_HANDLE                   HiiHandle;
  RAS_CONFIG_VARSTORE_DATA         Configuration;

  //
  // Consumed protocol
  //
  EFI_HII_CONFIG_ROUTING_PROTOCOL  *HiiConfigRouting;

  //
  // Produced protocol
  //
  EFI_HII_CONFIG_ACCESS_PROTOCOL   ConfigAccess;
} RAS_CONFIG_PRIVATE_DATA;

#define RAS_CONFIG_PRIVATE_FROM_THIS(a)  CR (a, RAS_CONFIG_PRIVATE_DATA, ConfigAccess, RAS_CONFIG_PRIVATE_SIGNATURE)

#pragma pack(1)

///
/// HII specific Vendor Device Path definition.
///
typedef struct {
  VENDOR_DEVICE_PATH             VendorDevicePath;
  EFI_DEVICE_PATH_PROTOCOL       End;
} HII_VENDOR_DEVICE_PATH;

#pragma pack()

#endif /* _RAS_CONFIG_DXE_H_ */
