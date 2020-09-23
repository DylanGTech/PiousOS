#ifndef _Drivers_H
#define _Drivers_H 1

#include <efi.h>

void InitializeDrivers(EFI_CONFIGURATION_TABLE *ConfigTable, UINTN NumberOfConfigTables);

#endif