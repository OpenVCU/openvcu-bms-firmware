//
//  driverSWLTC681x.h
//  
//
//  Created by Matt Cavins on 11/15/26.
//

#ifndef __DRIVERSWLTC681X_H
#define __DRIVERSWLTC681X_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include "driverHWSPI1.h"

// Maximum supported topology (adjust if your pack has more than 16 daisy-chained modules)
#define LTC681X_MAX_ICS             16
#define LTC681X_MAX_CELLS_PER_IC    18

// Supported LTC / Analog Devices isoSPI Cell Monitors
typedef enum {
    LTC_CHIP_6810      = 6,   // 6 Cells per IC  (Cell Reg Groups A-B)
    LTC_CHIP_6804      = 12,  // 12 Cells per IC (Cell Reg Groups A-D)
    LTC_CHIP_6811      = 12,  // 12 Cells per IC (Cell Reg Groups A-D)
    LTC_CHIP_6812      = 15,  // 15 Cells per IC (Cell Reg Groups A-E)
    LTC_CHIP_6813      = 18,  // 18 Cells per IC (Cell Reg Groups A-F)
    LTC_CHIP_ADBMS1818 = 18   // 18 Cells per IC (Cell Reg Groups A-F)
} driverSWLTC681xChipType;

// 11-Bit Command Definitions for LTC6810 / 6804 / 6811 / 6812 / 6813 / ADBMS1818
typedef enum {
    LTC681X_CMD_WRCFGA   = 0x0001, // Write Config Register Group A
    LTC681X_CMD_WRCFGB   = 0x0024, // Write Config Register Group B (6812/6813/1818)
    LTC681X_CMD_RDCFGA   = 0x0002, // Read Config Register Group A
    LTC681X_CMD_RDCFGB   = 0x0026, // Read Config Register Group B
    LTC681X_CMD_RDCVA    = 0x0004, // Read Cell Voltages 1-3
    LTC681X_CMD_RDCVB    = 0x0006, // Read Cell Voltages 4-6   (LTC6810 stops here)
    LTC681X_CMD_RDCVC    = 0x0008, // Read Cell Voltages 7-9
    LTC681X_CMD_RDCVD    = 0x000A, // Read Cell Voltages 10-12 (LTC6804/6811 stops here)
    LTC681X_CMD_RDCVE    = 0x0009, // Read Cell Voltages 13-15 (LTC6812 stops here)
    LTC681X_CMD_RDCVF    = 0x000B, // Read Cell Voltages 16-18 (LTC6813/1818 stops here)
    LTC681X_CMD_RDAUXA   = 0x000C, // Read Aux Register Group A (GPIO1-3, or S0+GPIO1-2 on LTC6810)
    LTC681X_CMD_RDAUXB   = 0x000E, // Read Aux Register Group B (GPIO4-5, VREF2)
    LTC681X_CMD_RDSTATA  = 0x0010, // Read Status Register Group A (Sum of Cells, Internal Temp, Analog Supply)
    LTC681X_CMD_RDSTATB  = 0x0012, // Read Status Register Group B (Digital Supply, OV/UV Flags)
    LTC681X_CMD_CLRCELL  = 0x0711, // Clear Cell Voltage Registers
    LTC681X_CMD_CLRAUX   = 0x0712, // Clear Aux Registers
    LTC681X_CMD_CLRSTAT  = 0x0713, // Clear Status Registers
    LTC681X_CMD_ADCV_NORM_ALL      = 0x0260, // Start 7kHz Normal Cell ADC Conversion (All Cells, DCP=0)
    LTC681X_CMD_ADCV_NORM_ALL_DCP  = 0x0270, // Start 7kHz Normal Cell ADC Conversion (All Cells, DCP=1)
    LTC681X_CMD_ADAX_NORM_ALL      = 0x0460, // Start 7kHz Normal GPIO/Temp ADC Conversion
    LTC681X_CMD_ADSTAT_NORM_ALL    = 0x0468  // Start 7kHz Normal Status (Internal Temp/SC) Conversion
} driverSWLTC681xCommandTypedef;

// Matches existing cell struct used by modPowerElectronics.c
typedef struct {
    float cellVoltage;
    uint8_t cellNumber;
} driverLTC681xCellsTypedef;

// Matches existing config struct used by modPowerElectronics.c + adds chipType
typedef struct {
    bool WatchDogFlag;
    bool GPIO1;
    bool GPIO2;
    bool LevelPolling;
    uint8_t CDCMode;
    uint32_t DisChargeEnableMask;       // Upgraded to 32-bit to support up to 18 cells per IC
    uint8_t noOfCells;                  // Cells per IC (e.g. 6 for LTC6810, 12 for LTC6811, 18 for LTC6813)
    driverSWLTC681xChipType chipType;   // Select LTC_CHIP_6810, LTC_CHIP_6811, LTC_CHIP_6813, etc.
    uint16_t CellVoltageConversionMode;
    float CellUnderVoltageLimit;
    float CellOverVoltageLimit;
} driverLTC681xConfigStructTypedef;

// Core API
void driverSWLTC681xInit(driverLTC681xConfigStructTypedef configStruct, uint8_t totalNumberOfLTCs);
void driverSWLTC681xReInit(void);
void driverSWLTC681xStartCellVoltageConversion(void);
void driverSWLTC681xStartTemperatureVoltageConversion(void);
void driverSWLTC681xResetCellVoltageRegisters(void);
bool driverSWLTC681xReadCellVoltages(driverLTC681xCellsTypedef cellVoltages[]);
bool driverSWLTC681xReadTempVoltages(uint16_t tempVoltages[3]);
void driverSWLTC681xWriteConfig(driverLTC681xConfigStructTypedef configStruct);
bool driverSWLTC681xReadConfig(driverLTC681xConfigStructTypedef *configStruct);
bool driverSWLTC681xReadVoltageFlags(uint16_t *underVoltageFlags, uint16_t *overVoltageFlags);
void driverSWLTC681xEnableBalanceResistors(uint32_t balanceEnableMask);

// Low-level isoSPI & 15-bit PEC functions
void driverSWLTC681xWakeup(void);
uint16_t driverSWLTC681xCalcPEC15(uint8_t len, const uint8_t *data);
void driverSWLTC681xSendCommand(uint16_t command);
void driverSWLTC681xWrite(uint8_t *writeBytes, uint16_t writeLength);
void driverSWLTC681xWriteRead(uint8_t *writeBytes, uint16_t writeLength, uint8_t *readBytes, uint16_t readLength);

// Temperature conversion (Updated for 16-bit 100uV/LSB ADC and -45C cold weather operation!)
float driverSWLTC681xConvertTemperatureExt(uint16_t inputValue, uint32_t ntcNominal, uint32_t ntcSeriesResistance, uint16_t ntcBetaFactor, float ntcNominalTemp);
float driverSWLTC681xConvertTemperatureInt(uint16_t inputValue);

// Backward-compatible aliases so modPowerElectronics.c compiles cleanly during migration
typedef driverLTC681xCellsTypedef        driverLTC6803CellsTypedef;
typedef driverLTC681xConfigStructTypedef driverLTC6803ConfigStructTypedef;
#define LTC6803StartCellVoltageADCConversionAll driverSWLTC681x_ADCV_DEFAULT
#define driverSWLTC681x_ADCV_DEFAULT            LTC681X_CMD_ADCV_NORM_ALL
#define driverSWLTC6803Init                     driverSWLTC681xInit
#define driverSWLTC6803ReInit                   driverSWLTC681xReInit
#define driverSWLTC6803StartCellVoltageConversion        driverSWLTC681xStartCellVoltageConversion
#define driverSWLTC6803StartTemperatureVoltageConversion driverSWLTC681xStartTemperatureVoltageConversion
#define driverSWLTC6803ResetCellVoltageRegisters         driverSWLTC681xResetCellVoltageRegisters
#define driverSWLTC6803ReadCellVoltages         driverSWLTC681xReadCellVoltages
#define driverSWLTC6803ReadTempVoltages         driverSWLTC681xReadTempVoltages
#define driverSWLTC6803ReadConfig               driverSWLTC681xReadConfig
#define driverSWLTC6803ReadVoltageFlags         driverSWLTC681xReadVoltageFlags
#define driverSWLTC6803EnableBalanceResistors   driverSWLTC681xEnableBalanceResistors
#define driverSWLTC6803ConvertTemperatureExt    driverSWLTC681xConvertTemperatureExt
#define driverSWLTC6803ConvertTemperatureInt    driverSWLTC681xConvertTemperatureInt

#endif /* !__DRIVERSWLTC681X_H */
