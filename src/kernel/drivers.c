#include "kernel/graphics.h"

#include "kernel/drivers.h"

void InitializeDrivers(EFI_CONFIGURATION_TABLE *ConfigTable, UINTN NumberOfConfigTables)
{
    PrintString("Address of first table: 0x%lX\n", mainTextDisplaySettings.fontColor, mainTextDisplaySettings.backgroundColor, ConfigTable);
    PrintString("Number of tables: %lu\n", mainTextDisplaySettings.fontColor, mainTextDisplaySettings.backgroundColor, NumberOfConfigTables);
}