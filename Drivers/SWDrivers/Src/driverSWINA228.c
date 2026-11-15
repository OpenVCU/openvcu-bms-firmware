//
//  driverSWINA228.c
//  
//
//  Created by Matt Cavins on 11/15/26.
//

#include "driverSWINA228.h"

static float s_currentLsb = 0.002f; // Default 2mA/LSB
static uint8_t s_adcRange = INA228_ADCRANGE_0_163MV;

// Low-level helpers (adapt wrapper names to match your driverHWI2C implementation)
extern bool driverHWI2C1Write(uint8_t devAddr, uint8_t *txData, uint16_t len);
extern bool driverHWI2C1Read(uint8_t devAddr, uint8_t regAddr, uint8_t *rxData, uint16_t len);

static bool ina228WriteReg16(uint8_t addr, uint8_t reg, uint16_t val) {
    uint8_t buf[3];
    buf[0] = reg;
    buf[1] = (uint8_t)((val >> 8) & 0xFF);
    buf[2] = (uint8_t)(val & 0xFF);
    return driverHWI2C1Write(addr, buf, 3);
}

static bool ina228ReadReg16(uint8_t addr, uint8_t reg, uint16_t *val) {
    uint8_t buf[2] = {0};
    if (!driverHWI2C1Read(addr, reg, buf, 2)) return false;
    *val = ((uint16_t)buf[0] << 8) | buf[1];
    return true;
}

// Reads a 24-bit register and sign-extends bits [23:4] as a signed 20-bit integer
static bool ina228ReadReg24Signed20(uint8_t addr, uint8_t reg, int32_t *val) {
    uint8_t buf[3] = {0};
    if (!driverHWI2C1Read(addr, reg, buf, 3)) return false;
    uint32_t raw24 = ((uint32_t)buf[0] << 16) | ((uint32_t)buf[1] << 8) | buf[2];
    int32_t signed20 = (int32_t)(raw24 >> 4);
    if (signed20 & 0x80000) {
        signed20 |= 0xFFF00000; // Sign-extend 20-bit to 32-bit
    }
    *val = signed20;
    return true;
}

// Reads a 40-bit signed two's complement register (CHARGE)
static bool ina228ReadReg40Signed(uint8_t addr, uint8_t reg, int64_t *val) {
    uint8_t buf[5] = {0};
    if (!driverHWI2C1Read(addr, reg, buf, 5)) return false;
    uint64_t raw40 = ((uint64_t)buf[0] << 32) |
                     ((uint64_t)buf[1] << 24) |
                     ((uint64_t)buf[2] << 16) |
                     ((uint64_t)buf[3] << 8)  |
                     ((uint64_t)buf[4]);
    if (raw40 & 0x8000000000ULL) {
        raw40 |= 0xFFFFFF0000000000ULL; // Sign-extend 40-bit to 64-bit
    }
    *val = (int64_t)raw40;
    return true;
}

bool driverSWINA228Init(uint8_t i2cAddr, float rShuntOhms, float maxExpectedCurrentAmps, uint8_t adcRange) {
    uint16_t mfgId = 0;
    if (!ina228ReadReg16(i2cAddr, INA228_REG_MANUFACTURER_ID, &mfgId) || mfgId != 0x5449) {
        return false; // Verify TI Manufacturer ID ("TI" = 0x5449)
    }

    s_adcRange = (adcRange == INA228_ADCRANGE_1_40MV) ? 1 : 0;

    // 1. CONFIG (0x00): Reset accumulators (Bit 14 = RSTACC), set ADCRANGE (Bit 4)
    uint16_t config = (1U << 14) | ((uint16_t)s_adcRange << 4);
    if (!ina228WriteReg16(i2cAddr, INA228_REG_CONFIG, config)) return false;

    // 2. ADC_CONFIG (0x01):
    // MODE[15:12] = 0xB (Continuous Shunt + Bus) or 0xF (Continuous Shunt + Bus + Temp)
    // VBUSCT[11:9] = 0x4 (540us), VSHCT[8:6] = 0x4 (540us), VTCT[5:3] = 0x4 (540us), AVG[2:0] = 0x3 (64 samples)
    uint16_t adcConfig = (0xFU << 12) | (0x4U << 9) | (0x4U << 6) | (0x4U << 3) | 0x3U;
    if (!ina228WriteReg16(i2cAddr, INA228_REG_ADC_CONFIG, adcConfig)) return false;

    // 3. Calculate CURRENT_LSB = MaxCurrent / 2^19 (524288)
    s_currentLsb = maxExpectedCurrentAmps / 524288.0f;

    // 4. Calculate SHUNT_CAL = 13107.2 * 10^6 * CURRENT_LSB * R_SHUNT * (4 if ADCRANGE=1)
    float calFloat = 13107200000.0f * s_currentLsb * rShuntOhms;
    if (s_adcRange == INA228_ADCRANGE_1_40MV) {
        calFloat *= 4.0f;
    }
    uint16_t shuntCal = (uint16_t)calFloat;
    return ina228WriteReg16(i2cAddr, INA228_REG_SHUNT_CAL, shuntCal);
}

bool driverSWINA228ResetAccumulators(uint8_t i2cAddr) {
    uint16_t config = (1U << 14) | ((uint16_t)s_adcRange << 4);
    return ina228WriteReg16(i2cAddr, INA228_REG_CONFIG, config);
}

bool driverSWINA228GetBusVoltage(uint8_t i2cAddr, float *voltageVolts, float busDividerRatio) {
    int32_t rawVbus = 0;
    if (!ina228ReadReg24Signed20(i2cAddr, INA228_REG_VBUS, &rawVbus)) return false;
    *voltageVolts = (float)rawVbus * INA228_VBUS_LSB * busDividerRatio;
    return true;
}

bool driverSWINA228GetCurrent(uint8_t i2cAddr, float *currentAmps) {
    int32_t rawCurrent = 0;
    if (!ina228ReadReg24Signed20(i2cAddr, INA228_REG_CURRENT, &rawCurrent)) return false;
    *currentAmps = (float)rawCurrent * s_currentLsb;
    return true;
}

bool driverSWINA228GetChargeAh(uint8_t i2cAddr, double *ampHours) {
    int64_t rawCharge = 0;
    if (!ina228ReadReg40Signed(i2cAddr, INA228_REG_CHARGE, &rawCharge)) return false;
    // CHARGE LSB = 16 * CURRENT_LSB (in Coulombs / Amp-seconds)
    double coulombs = (double)rawCharge * (16.0 * (double)s_currentLsb);
    *ampHours = coulombs / 3600.0;
    return true;
}
