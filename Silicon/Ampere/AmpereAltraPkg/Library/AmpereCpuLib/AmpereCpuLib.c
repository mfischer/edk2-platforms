/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Base.h>
#include <PiPei.h>
#include <Library/BaseMemoryLib.h>
#include <Library/IoLib.h>
#include <Library/HobLib.h>
#include <Library/AmpereCpuLib.h>
#include <Library/NVParamLib.h>
#include <Platform/Ac01.h>
#include <PlatformInfoHob.h>
#include <NVParamDef.h>

#define MONOLITIC_NUM_OF_REGION        1
#define HEMISPHERE_NUM_OF_REGION       2
#define QUADRANT_NUM_OF_REGION         4
#define SUBNUMA_CPM_REGION_SIZE        4
#define NUM_OF_CPM_PER_MESH_ROW        8

STATIC PlatformInfoHob_V2*
GetPlatformHob (VOID)
{
  VOID                *Hob;
  CONST EFI_GUID      PlatformHobGuid = PLATFORM_INFO_HOB_GUID_V2;

  Hob = GetFirstGuidHob (&PlatformHobGuid);
  if (Hob == NULL) {
    return NULL;
  }

  return (PlatformInfoHob_V2 *) GET_GUID_HOB_DATA (Hob);
}

/**
  Get the SubNUMA mode.

  @return   UINT8      The SubNUMA mode.

**/
UINT8
EFIAPI
CPUGetSubNumaMode (VOID)
{
  PlatformInfoHob_V2 *PlatformHob;

  PlatformHob = GetPlatformHob ();
  if (PlatformHob == NULL) {
    return SUBNUMA_MODE_MONOLITHIC;
  }

  return PlatformHob->SubNumaMode[0];
}

/**
  Get the number of SubNUMA region.

  @return   UINT8      The number of SubNUMA region.

**/
UINT8
EFIAPI
CPUGetNumOfSubNuma (VOID)
{
  UINT8 SubNumaMode = CPUGetSubNumaMode();

  switch (SubNumaMode) {
  case SUBNUMA_MODE_MONOLITHIC:
    return MONOLITIC_NUM_OF_REGION;
  case SUBNUMA_MODE_HEMISPHERE:
    return HEMISPHERE_NUM_OF_REGION;
  case SUBNUMA_MODE_QUADRANT:
    return QUADRANT_NUM_OF_REGION;
  }

  return QUADRANT_NUM_OF_REGION;
}

/**
  Get the SubNUMA node of a CPM.

  @param    SocketId    Socket index.
  @param    Cpm         CPM index.
  @return   UINT8       The SubNUMA node of a CPM.

**/
UINT8
EFIAPI
CPUGetSubNumNode (
  UINT8  SocketId,
  UINT32 Cpm
  )
{
  UINT8               MaxNumOfCPM = GetMaximumNumberCPMs();
  UINT8               SubNumaMode = CPUGetSubNumaMode();
  INTN                Ret = 0;
  UINT8               AsymMesh = 0;
  UINT8               AsymMeshRow = 0;

  switch (SubNumaMode) {
  case SUBNUMA_MODE_MONOLITHIC:
    if (SocketId == 0) {
      Ret = 0;
    } else {
      Ret = 1;
    }
    break;

  case SUBNUMA_MODE_HEMISPHERE:
    if (((Cpm % NUM_OF_CPM_PER_MESH_ROW) / SUBNUMA_CPM_REGION_SIZE) != 0) {
      Ret = 1;
    } else {
      Ret = 0;
    }
    if (SocketId == 1) {
      Ret += HEMISPHERE_NUM_OF_REGION;
    }
    break;

  case SUBNUMA_MODE_QUADRANT:
    // For asymmetric mesh, the CPM at the middle row is distributed
    // equally to each node. As each mesh row has 8 CPMs,
    //   First pair of CPMs: Node 0
    //   Second pair of CPMs: Node 1
    //   Third pair of CPMs: Node 3
    //   Forth paif of CPMs: Node 2
    AsymMesh = (MaxNumOfCPM / NUM_OF_CPM_PER_MESH_ROW) % 2;
    if (AsymMesh != 0) {
      AsymMeshRow = (MaxNumOfCPM / NUM_OF_CPM_PER_MESH_ROW) / 2;
    }
    if ((AsymMesh != 0) && ((Cpm / NUM_OF_CPM_PER_MESH_ROW) == AsymMeshRow)) {
      switch ((Cpm % NUM_OF_CPM_PER_MESH_ROW) / 2) {
      case 0:
        Ret = 0;
        break;
      case 1:
        Ret = 1;
        break;
      case 2:
        Ret = 3;
        break;
      case 3:
        Ret = 2;
        break;
      }
    } else {
      if (Cpm < (MaxNumOfCPM / 2)) {
        if (((Cpm % NUM_OF_CPM_PER_MESH_ROW) / SUBNUMA_CPM_REGION_SIZE) != 0) {
          Ret = 2;
        } else {
          Ret = 0;
        }
      } else {
        if (((Cpm % NUM_OF_CPM_PER_MESH_ROW) / SUBNUMA_CPM_REGION_SIZE) != 0) {
          Ret = 3;
        } else {
          Ret = 1;
        }
      }
    }

    if (SocketId == 1) {
      Ret += QUADRANT_NUM_OF_REGION;
    }
    break;
  }

  return Ret;
}

/**
  Get the value of CLIDR register.

  @return   UINT64      The value of CLIDR register.

**/
UINT64
EFIAPI
AArch64ReadCLIDRReg (VOID)
{
  UINT64 Value;

  asm volatile("mrs %x0, clidr_el1 " : "=r" (Value));

  return Value;
}

/**
  Get the value of CCSID register.

  @param    Level       Cache level.
  @return   UINT64      The value of CCSID register.

**/
UINT64
EFIAPI
AArch64ReadCCSIDRReg (
  UINT64 Level
  )
{
  UINT64 Value;

  asm volatile("msr csselr_el1, %x0 " : : "rZ" (Level));
  asm volatile("mrs %x0, ccsidr_el1 " : "=r" (Value));

  return Value;
}

/**
  Get the associativity of cache.

  @param    Level       Cache level.
  @return   UINT32      Associativity of cache.

**/
UINT32
EFIAPI
CpuGetAssociativity (
  UINTN Level
  )
{
  UINT64 CacheCCSIDR;
  UINT64 CacheCLIDR = AArch64ReadCLIDRReg ();
  UINT32  Value = 0x2; /* Unknown Set-Associativity */

  if (!CLIDR_CTYPE (CacheCLIDR, Level)) {
    return Value;
  }

  CacheCCSIDR = AArch64ReadCCSIDRReg (Level);
  switch (CCSIDR_ASSOCIATIVITY (CacheCCSIDR)) {
  case 0:
    /* Direct mapped */
    Value = 0x3;
    break;
  case 1:
    /* 2-way Set-Associativity */
    Value = 0x4;
    break;
  case 3:
    /* 4-way Set-Associativity */
    Value = 0x5;
    break;
  case 7:
    /* 8-way Set-Associativity */
    Value = 0x7;
    break;
  case 15:
    /* 16-way Set-Associativity */
    Value = 0x8;
    break;
  case 11:
    /* 12-way Set-Associativity */
    Value = 0x9;
    break;
  case 23:
    /* 24-way Set-Associativity */
    Value = 0xA;
    break;
  case 31:
    /* 32-way Set-Associativity */
    Value = 0xB;
    break;
  case 47:
    /* 48-way Set-Associativity */
    Value = 0xC;
    break;
  case 63:
    /* 64-way Set-Associativity */
    Value = 0xD;
    break;
  case 19:
    /* 20-way Set-Associativity */
    Value = 0xE;
    break;
  }

  return Value;
}

/**
  Get the cache size.

  @param    Level       Cache level.
  @return   UINT32      Cache size.

**/
UINT32
EFIAPI
CpuGetCacheSize (
  UINTN Level
  )
{
  UINT32 CacheLineSize;
  UINT32 Count;
  UINT64 CacheCCSIDR;
  UINT64 CacheCLIDR = AArch64ReadCLIDRReg ();

  if (!CLIDR_CTYPE (CacheCLIDR, Level)) {
    return 0;
  }

  CacheCCSIDR = AArch64ReadCCSIDRReg (Level);
  CacheLineSize = 1;
  Count = CCSIDR_LINE_SIZE (CacheCCSIDR) + 4;
  while (Count-- > 0) {
    CacheLineSize *= 2;
  }

  return ((CCSIDR_NUMSETS (CacheCCSIDR) + 1) *
          (CCSIDR_ASSOCIATIVITY (CacheCCSIDR) + 1) *
          CacheLineSize);
}

/**
  Get the number of supported socket.

  @return   UINT32      Number of supported socket.

**/
UINT32
EFIAPI
GetNumberSupportedSockets (VOID)
{
  PlatformInfoHob_V2   *PlatformHob;

  PlatformHob = GetPlatformHob ();
  if (PlatformHob == NULL) {
      return 0;
  }

  return (sizeof (PlatformHob->ClusterEn) / sizeof (PlatformClusterEn));
}

/**
  Get the number of active socket.

  @return   UINT32      Number of active socket.

**/
UINT32
EFIAPI
GetNumberActiveSockets (VOID)
{
  UINTN                NumberActiveSockets, Count, Index, Index1;
  PlatformClusterEn    *Socket;
  PlatformInfoHob_V2   *PlatformHob;

  PlatformHob = GetPlatformHob ();
  if (PlatformHob == NULL) {
      return 0;
  }

  NumberActiveSockets = 0;

  for (Index = 0; Index < GetNumberSupportedSockets (); Index++) {
    Socket = &PlatformHob->ClusterEn[Index];
    Count = sizeof (Socket->EnableMask) / sizeof (Socket->EnableMask[0]);
    for (Index1 = 0; Index1 < Count; Index1++) {
      if (Socket->EnableMask[Index1]) {
        NumberActiveSockets++;
        break;
      }
    }
  }

  return NumberActiveSockets;
}

/**
  Get the number of active CPM per socket.

  @param    SocketId    Socket index.
  @return   UINT32      Number of CPM.

**/
UINT32
EFIAPI
GetNumberActiveCPMsPerSocket (
  UINT32 SocketId
  )
{
  UINTN                NumberCPMs, Count, Index;
  UINT32               Val32;
  PlatformClusterEn    *Socket;
  PlatformInfoHob_V2   *PlatformHob;

  PlatformHob = GetPlatformHob ();
  if (PlatformHob == NULL) {
      return 0;
  }

  if (SocketId >= GetNumberSupportedSockets ()) {
    return 0;
  }

  NumberCPMs = 0;
  Socket = &PlatformHob->ClusterEn[SocketId];
  Count = sizeof (Socket->EnableMask) / sizeof (Socket->EnableMask[0]);
  for (Index = 0; Index < Count; Index++) {
    Val32 = Socket->EnableMask[Index];
    while (Val32) {
      if (Val32 & 0x1) {
        NumberCPMs++;
      }
      Val32 >>= 1;
    }
  }

  return NumberCPMs;
}

/**
  Get the configured number of CPM per socket. This number
  should be the same for all sockets.

  @param    SocketId    Socket index.
  @return   UINT32      Configured number of CPM.

**/
UINT32
EFIAPI
GetConfiguredNumberCPMs (
  UINTN SocketId
  )
{
  EFI_STATUS  Status;
  UINT32      Value;
  UINT32      Param, ParamStart, ParamEnd;
  INTN        Count;

  Count = 0;
  ParamStart = NV_SI_S0_PCP_ACTIVECPM_0_31 + SocketId * NV_PARAM_ENTRYSIZE * (PLATFORM_CPU_MAX_CPM / 32);
  ParamEnd = ParamStart + NV_PARAM_ENTRYSIZE * (PLATFORM_CPU_MAX_CPM / 32);
  for (Param = ParamStart; Param < ParamEnd; Param += NV_PARAM_ENTRYSIZE) {
    Status = NVParamGet (
               Param,
               NV_PERM_ATF | NV_PERM_BIOS | NV_PERM_MANU | NV_PERM_BMC,
               &Value
               );
    if (EFI_ERROR (Status)) {
      break;
    }
    while (Value) {
      if (Value & 1) {
        Count++;
      }
      Value >>= 1;
    }
  }

  return Count;
}

/**
  Set the configured number of CPM per socket.

  @param    SocketId        Socket index.
  @param    Number          Number of CPM to be configured.
  @return   EFI_SUCCESS     Operation succeeded.
  @return   Others          An error has occurred.

**/
EFI_STATUS
EFIAPI
SetConfiguredNumberCPMs (
  UINTN Socket,
  UINTN Number
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;
  UINT32      Value;
  UINT32      Param, ParamStart, ParamEnd;
  BOOLEAN     IsClear = FALSE;

  if (Number == 0) {
    IsClear = TRUE;
  }

  ParamStart = NV_SI_S0_PCP_ACTIVECPM_0_31 + Socket * NV_PARAM_ENTRYSIZE * (PLATFORM_CPU_MAX_CPM / 32);
  ParamEnd = ParamStart + NV_PARAM_ENTRYSIZE * (PLATFORM_CPU_MAX_CPM / 32);
  for (Param = ParamStart; Param < ParamEnd; Param += NV_PARAM_ENTRYSIZE) {
    if (Number >= 32) {
      Value = 0xffffffff;
      Number -= 32;
    } else {
      Value = 0;
      while (Number > 0) {
        Value |= (1 << (--Number));
      }
    }
    if (IsClear) {
      /* Clear this param */
      Status = NVParamClr (
                 Param,
                 NV_PERM_BIOS | NV_PERM_MANU
                 );
    } else {
      Status = NVParamSet (
                 Param,
                 NV_PERM_ATF | NV_PERM_BIOS | NV_PERM_MANU | NV_PERM_BMC,
                 NV_PERM_BIOS | NV_PERM_MANU,
                 Value
                 );
    }
  }

  return Status;
}

/**
  Get the maximum number of core per socket.

  @return   UINT32      Maximum number of core.

**/
UINT32
EFIAPI
GetMaximumNumberOfCores (VOID)
{

  PlatformInfoHob_V2   *PlatformHob;

  PlatformHob = GetPlatformHob ();
  if (PlatformHob == NULL) {
      return 0;
  }

  return PlatformHob->MaxNumOfCore[0];
}

/**
  Get the maximum number of CPM per socket. This number
  should be the same for all sockets.

  @return   UINT32      Maximum number of CPM.

**/
UINT32
EFIAPI
GetMaximumNumberCPMs (VOID)
{
  return GetMaximumNumberOfCores() / PLATFORM_CPU_NUM_CORES_PER_CPM;
}

/**
  Get the number of active cores of a sockets.

  @param    SocketId    Socket Index.
  @return   UINT32      Number of active core.

**/
UINT32
EFIAPI
GetNumberActiveCoresPerSocket (
  UINT32 SocketId
  )
{
  return GetNumberActiveCPMsPerSocket (SocketId) * PLATFORM_CPU_NUM_CORES_PER_CPM;
}

/**
  Get the number of active cores of all sockets.

  @return   UINT32      Number of active core.

**/
UINT32
EFIAPI
GetNumberActiveCores (VOID)
{
  UINTN   NumberActiveCores;
  UINTN   Index;

  NumberActiveCores = 0;

  for (Index = 0; Index < GetNumberSupportedSockets (); Index++) {
    NumberActiveCores += GetNumberActiveCoresPerSocket (Index);
  }

  return NumberActiveCores;
}

/**
  Check if the logical CPU is enabled or not.

  @param    CpuId       The logical Cpu ID. Started from 0.
  @return   BOOLEAN     TRUE if the Cpu enabled
                        FALSE if the Cpu disabled/

**/
BOOLEAN
EFIAPI
IsCpuEnabled (
  UINTN CpuId
  )
{
  PlatformClusterEn    *Socket;
  PlatformInfoHob_V2   *PlatformHob;
  UINT32               SocketId = CpuId / (PLATFORM_CPU_MAX_CPM * PLATFORM_CPU_NUM_CORES_PER_CPM);
  UINT32               ClusterId = (CpuId / PLATFORM_CPU_NUM_CORES_PER_CPM) % PLATFORM_CPU_MAX_CPM;

  PlatformHob = GetPlatformHob ();
  if (PlatformHob == NULL) {
      return FALSE;
  }

  if (SocketId >= GetNumberSupportedSockets ()) {
    return FALSE;
  }

  Socket = &PlatformHob->ClusterEn[SocketId];
  if (Socket->EnableMask[ClusterId / 32] & (1 << (ClusterId % 32))) {
    return TRUE;
  }

  return FALSE;
}

/**
  Check if the slave socket is present

  @return   BOOLEAN     TRUE if the Slave Cpu present
                        FALSE if the Slave Cpu present

**/
BOOLEAN
EFIAPI
PlatSlaveSocketPresent (VOID)
{
  UINT32 Value;

  Value = MmioRead32 (SMPRO_EFUSE_SHADOW0 + CFG2P_OFFSET);
  return (Value & SLAVE_PRESENT_N)? FALSE : TRUE;
}
