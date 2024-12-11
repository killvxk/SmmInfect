#include "communication.h"
#include "windows.h"
#include "memory.h"
#include <Library/UefiLib.h>
#include <Protocol/SmmCpu.h>
#include <Uefi.h>
#include <Protocol/SmmCpu.h>
#include <PiSmm.h>

UINT64 SmiCountIndex = 0;

UINT64 GetCommunicationProcess()
{
  return GetEProcess("SmiUM.exe");
}

EFI_STATUS PerformCommunication()
{
  SmiCountIndex++;

  // Get ntoskrnl base and the kernel context
  UINT64 kernel = GetKernelBase();
  UINT64 cr3 = GetKernelCr3();
  if (!kernel || !cr3)
  {
    return EFI_SUCCESS;
  }
  
  // Get the process we write our communication buffer to
  UINT64 cprocess = GetCommunicationProcess();
  if (cprocess)
  {
    UINT64 base = GetBaseAddressModuleX64(cprocess, L"SmiUM.exe");

    if (base)
    {
      UINT64 section = GetSectionBaseAddressX64(cprocess, base, (unsigned char*)".ZEPTA");
  //UINT8* ReadVirtual(UINT64 address, UINT64 cr3, UINT8* buffer, UINT64 length);

      if (section)
      {
        SmmCommunicationProtocol protocol = { 0 };
        ReadVirtual(section + 0b0, GetProcessCr3(cprocess), (UINT8*)&protocol, (UINT64)sizeof(SmmCommunicationProtocol));
        if (protocol.magic != SMM_PROTOCOL_MAGIC)
        {
          return EFI_SUCCESS;
        }

        UINT64 tprocess = GetEProcess((const char*)protocol.process_name);

        if (tprocess == 0)
        {
          return EFI_SUCCESS;
        }

        UINT64 tbase = GetBaseAddressModuleX64(tprocess, protocol.module_name);

        if (tbase == 0)
        {
          return EFI_SUCCESS;
        }

        *(UINT64*)(TranslateVirtualToPhysical(GetProcessCr3(cprocess), section + SMI_COUNT_OFFSET)) = SmiCountIndex;

        ReadVirtual(tbase + protocol.offset, GetProcessCr3(tprocess), protocol.read_buffer, protocol.read_size);

        // Section starts at new frame and struct is not bigger then a page size. So we can get away with only translating one time
        UINT64 temp = TranslateVirtualToPhysical(GetProcessCr3(cprocess), section + READ_BUFFER_OFFSET);

        for (UINT64 i = 0; i < protocol.read_size; i++)
        {
          *(UINT8*)(temp + i) = protocol.read_buffer[i];
        }
      }
    }
  }
  return EFI_SUCCESS;
}
