//
//  driverSWINA228.h
//  
//
//  Created by Matt Cavins on 11/15/26.
//

#ifndef __DRIVERSWINA228_H
#define __DRIVERSWINA228_H

#include <stdint.h>
#include <stdbool.h>
#include "driverHWI2C2.h"
#include "driverHWI2C1.h"

// Keep the exact same bus & address definitions (note original "ADDRES" spelling if referenced elsewhere)
#define INA228_MASTER_ADDRES        0x40
#define INA228_MASTER_BUS           2
#define INA228_SHIELD_MAIN_ADDRES   0x44
#define INA228_SHIELD_MAIN_BUS      1
#define INA228_SHIELD_AUX_ADDRES    0x45
#define INA228_SHIELD_AUX_BUS       1

// INA228 CONFIG (0x00) ADCRANGE Bit [4]
typedef enum {
    ADCRANGE_163MV = 0,     // +/- 163.84mV full scale (312.5 nV/LSB) -> +/- 1638.4A on 100uOhm
    ADCRANGE_40MV  = 1      // +/- 40.96mV full scale  (78.125 nV/LSB) -> +/- 409.6A on 100uOhm
} driverSWINA228ADCRangeConfig;

// INA228 ADC_CONFIG (0x01) Conversion Time (Bits [11:9] VBUSCT, [8:6] VSHCT, [5:3] VTCT)
typedef enum {
    CT_50US   = 0x00,
    CT_84US   = 0x01,
    CT_150US  = 0x02,
    CT_280US  = 0x03,
    CT_540US  = 0x04,
    CT_1052US = 0x05,
    CT_2074US = 0x06,
    CT_4120US = 0x07
} driverSWINA228ConvTimeConfig;

// INA228 ADC_CONFIG (0x01) Sample Averaging (Bits [2:0] AVG)
typedef enum {
    AVG_1    = 0x00,
    AVG_4    = 0x01,
    AVG_16   = 0x02,
    AVG_64   = 0x03,
    AVG_128  = 0x04,
    AVG_256  = 0x05,
    AVG_512  = 0x06,
    AVG_1024 = 0x07
} driverSWINA228AvgConfig;

// INA228 ADC_CONFIG (0x01) Operating Mode (Bits [15:12] MODE)
typedef enum {
    INA228_MODE_SHUTDOWN                = 0x00,
    INA228_MODE_BUS_TRIG                = 0x01,
    INA228_MODE_SHUNT_TRIG              = 0x02,
    INA228_MODE_SHUNT_BUS_TRIG          = 0x03,
    INA228_MODE_TEMP_TRIG               = 0x04,
    INA228_MODE_TEMP_BUS_TRIG           = 0x05,
    INA228_MODE_TEMP_SHUNT_TRIG         = 0x06,
    INA228_MODE_TEMP_SHUNT_BUS_TRIG     = 0x07,
    INA228_MODE_BUS_CONTINUOUS          = 0x09,
    INA228_MODE_SHUNT_CONTINUOUS        = 0x0A,
    INA228_MODE_SHUNT_BUS_CONTINUOUS    = 0x0B,
    INA228_MODE_TEMP_CONTINUOUS         = 0x0C,
    INA228_MODE_TEMP_BUS_CONTINUOUS     = 0x0D,
    INA228_MODE_TEMP_SHUNT_CONTINUOUS   = 0x0E,
    INA228_MODE_ALL_CONTINUOUS          = 0x0F
} driverSWINA228ModeConfig;

// INA228 Register Map
typedef enum {
    INA228_REG_CONFIG          = 0x00, // 16-bit R/W
    INA228_REG_ADC_CONFIG      = 0x01, // 16-bit R/W
    INA228_REG_SHUNT_CAL       = 0x02, // 16-bit R/W
    INA228_REG_SHUNT_TEMPCO    = 0x03, // 16-bit R/W
    INA228_REG_VSHUNT          = 0x04, // 24-bit R   (Signed 20-bit in bits [23:4])
    INA228_REG_VBUS            = 0x05, // 24-bit R   (Signed 20-bit in bits [23:4])
    INA228_REG_DIETEMP         = 0x06, // 16-bit R
    INA228_REG_CURRENT         = 0x07, // 24-bit R   (Signed 20-bit in bits [23:4])
    INA228_REG_POWER           = 0x08, // 24-bit R   (Unsigned 24-bit)
    INA228_REG_ENERGY          = 0x09, // 40-bit R   (Unsigned 5-byte accumulator)
    INA228_REG_CHARGE          = 0x0A, // 40-bit R   (Signed 5-byte Coulomb accumulator)
    INA228_REG_DIAG_ALRT       = 0x0B, // 16-bit R/W
    INA228_REG_SOVL            = 0x0C, // 16-bit R/W
    INA228_REG_SUVL            = 0x0D, // 16-bit R/W
    INA228_REG_BOVL            = 0x0E, // 16-bit R/W
    INA228_REG_BUVL            = 0x0F, // 16-bit R/W
    INA228_REG_TEMP_LIMIT      = 0x10, // 16-bit R/W
    INA228_REG_PWR_LIMIT       = 0x11, // 16-bit R/W
    INA228_REG_MANUFACTURER_ID = 0x3E, // 16-bit R   (0x5449)
    INA228_REG_DEVICE_ID       = 0x3F  // 16-bit R   (0x2280)
} driverSWINA228Registers;

typedef struct {
    driverSWINA228ADCRangeConfig adcRange;
    driverSWINA228ConvTimeConfig busConvTime;
    driverSWINA228ConvTimeConfig shuntConvTime;
    driverSWINA228ConvTimeConfig tempConvTime;
    driverSWINA228AvgConfig      averaging;
    driverSWINA228ModeConfig     mode;
    uint16_t                     shuntCal; // Optional hardware calibration value
} driverSWINA228InitStruct;

// Matches your existing API pattern (with i2cAddres, i2cBus, offset, and scalar)
void driverSWINA228Init(uint8_t i2cAddres, uint8_t i2cBus, driverSWINA228InitStruct initStruct);
bool driverSWINA228GetBusCurrent(uint8_t i2cAddres, uint8_t i2cBus, float *busCurrent, int32_t offset, float scalar);
bool driverSWINA228GetBusVoltage(uint8_t i2cAddres, uint8_t i2cBus, float *busVoltage, float scalar);

// New INA228-specific functions (Die Temp & 40-bit Hardware Coulomb Counter)
bool driverSWINA228GetDieTemp(uint8_t i2cAddres, uint8_t i2cBus, float *dieTempC);
bool driverSWINA228GetChargeCoulombs(uint8_t i2cAddres, uint8_t i2cBus, double *coulombs, float currentLsb);
bool driverSWINA228ResetAccumulators(uint8_t i2cAddres, uint8_t i2cBus);

#endif /* !__DRIVERSWINA228_H */
