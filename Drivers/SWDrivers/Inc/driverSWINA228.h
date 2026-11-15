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
#include <math.h>

// Default 7-bit I2C Address (A0 = GND, A1 = GND)
#define INA228_I2C_ADDR                 0x40

// INA228 Register Map
#define INA228_REG_CONFIG               0x00 // 16-bit
#define INA228_REG_ADC_CONFIG           0x01 // 16-bit
#define INA228_REG_SHUNT_CAL            0x02 // 16-bit
#define INA228_REG_VSHUNT               0x04 // 24-bit (signed 20-bit in [23:4])
#define INA228_REG_VBUS                 0x05 // 24-bit (signed 20-bit in [23:4])
#define INA228_REG_DIETEMP              0x06 // 16-bit
#define INA228_REG_CURRENT              0x07 // 24-bit (signed 20-bit in [23:4])
#define INA228_REG_POWER                0x08 // 24-bit (unsigned)
#define INA228_REG_ENERGY               0x09 // 40-bit (unsigned, 5 bytes)
#define INA228_REG_CHARGE               0x0A // 40-bit (signed two's complement, 5 bytes)
#define INA228_REG_DIAG_ALRT            0x0B // 16-bit
#define INA228_REG_MANUFACTURER_ID      0x3E // Reads 0x5449 ("TI")
#define INA228_REG_DEVICE_ID            0x3F // Reads 0x2280 or 0x2281

// Hardware Constants for R29 = CSM2F-8518-L100J01 (100 uOhm)
#define INA228_RSHUNT_OHMS              0.0001f   // 100 micro-Ohm
#define INA228_ADCRANGE_0_163MV         0         // +/- 163.84mV (+/- 1638.4A)
#define INA228_ADCRANGE_1_40MV          1         // +/- 40.96mV  (+/- 409.6A)

// Scaling LSBs
#define INA228_VBUS_LSB                 0.0001953125f   // 195.3125 uV/LSB
#define INA228_VSHUNT_LSB_RANGE0        0.0000003125f   // 312.5 nV/LSB
#define INA228_VSHUNT_LSB_RANGE1        0.000000078125f // 78.125 nV/LSB
#define INA228_DIETEMP_LSB              0.0078125f      // 7.8125 mC/LSB

typedef struct {
    float busVoltageVolts;      // Pack voltage (scaled by external divider if applicable)
    float shuntVoltageVolts;    // Raw voltage across R29
    float currentAmps;          // Instantaneous pack current (A)
    float powerWatts;           // Instantaneous power (W)
    double chargeCoulombs;      // Hardware accumulated Coulombs (A*s)
    double chargeAmpHours;      // Hardware accumulated Amp-Hours (Ah)
    float dieTempC;             // Internal INA228 die temperature (C)
} INA228_MeasureStruct;

bool driverSWINA228Init(uint8_t i2cAddr, float rShuntOhms, float maxExpectedCurrentAmps, uint8_t adcRange);
bool driverSWINA228ResetAccumulators(uint8_t i2cAddr);
bool driverSWINA228GetBusVoltage(uint8_t i2cAddr, float *voltageVolts, float busDividerRatio);
bool driverSWINA228GetCurrent(uint8_t i2cAddr, float *currentAmps);
bool driverSWINA228GetChargeAh(uint8_t i2cAddr, double *ampHours);
bool driverSWINA228ReadAll(uint8_t i2cAddr, INA228_MeasureStruct *meas, float busDividerRatio);

#endif
