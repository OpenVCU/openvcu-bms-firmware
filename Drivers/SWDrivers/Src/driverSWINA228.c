//
//  driverSWINA228.c
//  
//
//  Created by Matt Cavins on 11/15/26.
//

#include "driverSWINA228.h"

// Internal helper to write a 16-bit register over I2C1 or I2C2
static void ina228WriteReg16(uint8_t i2cAddres, uint8_t i2cBus, uint8_t reg, uint16_t value) {
    uint8_t writeData[3];
    writeData[0] = reg;
    writeData[1] = (uint8_t)((value >> 8) & 0xFF);
    writeData[2] = (uint8_t)(value & 0xFF);

    if (i2cBus == 1) {
        driverHWI2C1Write(i2cAddres, false, writeData, 3);
    } else if (i2cBus == 2) {
        driverHWI2C2Write(i2cAddres, false, writeData, 3);
    }
}

// Internal helper to read N bytes from a register over I2C1 or I2C2
static void ina228ReadRegN(uint8_t i2cAddres, uint8_t i2cBus, uint8_t reg, uint8_t *readBuf, uint8_t len) {
    uint8_t writeReg[1] = {reg};

    if (i2cBus == 1) {
        driverHWI2C1Write(i2cAddres, false, writeReg, 1);
        driverHWI2C1Read(i2cAddres, readBuf, len);
    } else if (i2cBus == 2) {
        driverHWI2C2Write(i2cAddres, false, writeReg, 1);
        driverHWI2C2Read(i2cAddres, readBuf, len);
    }
}

void driverSWINA228Init(uint8_t i2cAddres, uint8_t i2cBus, driverSWINA228InitStruct initStruct) {
    if (i2cBus == 1) {
        driverHWI2C1Init();
    } else if (i2cBus == 2) {
        driverHWI2C2Init();
    }

    // 1. Build CONFIG register (0x00):
    // Bit 14 = RSTACC (clear charge/energy accumulators on init)
    // Bit 4  = ADCRANGE (0 = +/-163.84mV, 1 = +/-40.96mV)
    uint16_t configReg = (1U << 14) | (((uint16_t)initStruct.adcRange & 0x01U) << 4);
    ina228WriteReg16(i2cAddres, i2cBus, INA228_REG_CONFIG, configReg);

    // 2. Build ADC_CONFIG register (0x01):
    // If initStruct.mode is 0 (uninitialized struct), default to Continuous All (0xF), 540us, 64x avg (0xFB6B)
    uint16_t adcConfigReg;
    if (initStruct.mode == 0) {
        adcConfigReg = (INA228_MODE_ALL_CONTINUOUS << 12) |
                       (CT_540US << 9) |
                       (CT_540US << 6) |
                       (CT_540US << 3) |
                       AVG_64;
    } else {
        adcConfigReg = (((uint16_t)initStruct.mode & 0x0FU) << 12) |
                       (((uint16_t)initStruct.busConvTime & 0x07U) << 9) |
                       (((uint16_t)initStruct.shuntConvTime & 0x07U) << 6) |
                       (((uint16_t)initStruct.tempConvTime & 0x07U) << 3) |
                       ((uint16_t)initStruct.averaging & 0x07U);
    }
    ina228WriteReg16(i2cAddres, i2cBus, INA228_REG_ADC_CONFIG, adcConfigReg);

    // 3. Write SHUNT_CAL (0x02) so hardware CURRENT, POWER, ENERGY, and CHARGE registers work:
    // Default to 2621 (0x0A3D) for R29 = 100uOhm and CURRENT_LSB = 2mA/LSB if shuntCal == 0
    uint16_t shuntCal = (initStruct.shuntCal != 0) ? initStruct.shuntCal : 2621U;
    ina228WriteReg16(i2cAddres, i2cBus, INA228_REG_SHUNT_CAL, shuntCal);
}

bool driverSWINA228GetBusCurrent(uint8_t i2cAddres, uint8_t i2cBus, float *busCurrent, int32_t offset, float scalar) {
    uint8_t readDataC[3] = {0, 0, 0};
    int32_t shuntRaw20;

    // INA228 VSHUNT (0x04) is a 24-bit register (3 bytes); bits [23:4] hold the 20-bit signed value
    ina228ReadRegN(i2cAddres, i2cBus, INA228_REG_VSHUNT, readDataC, 3);

    uint32_t raw24 = ((uint32_t)readDataC[0] << 16) |
                     ((uint32_t)readDataC[1] << 8)  |
                     ((uint32_t)readDataC[2]);

    // Shift right by 4 and sign-extend 20-bit two's complement to 32-bit int32_t
    shuntRaw20 = (int32_t)(raw24 >> 4);
    if (shuntRaw20 & 0x00080000) {
        shuntRaw20 |= 0xFFF00000;
    }

    *busCurrent = scalar * (float)(shuntRaw20 + offset);
    return false;
}

bool driverSWINA228GetBusVoltage(uint8_t i2cAddres, uint8_t i2cBus, float *busVoltage, float scalar) {
    uint8_t readDataV[3] = {0, 0, 0};
    int32_t busVoltageRaw20;

    // INA228 VBUS (0x05) is a 24-bit register (3 bytes); bits [23:4] hold the 20-bit signed value
    ina228ReadRegN(i2cAddres, i2cBus, INA228_REG_VBUS, readDataV, 3);

    uint32_t raw24 = ((uint32_t)readDataV[0] << 16) |
                     ((uint32_t)readDataV[1] << 8)  |
                     ((uint32_t)readDataV[2]);

    busVoltageRaw20 = (int32_t)(raw24 >> 4);
    if (busVoltageRaw20 & 0x00080000) {
        busVoltageRaw20 |= 0xFFF00000;
    }

    *busVoltage = scalar * (float)busVoltageRaw20;
    return false;
}

bool driverSWINA228GetDieTemp(uint8_t i2cAddres, uint8_t i2cBus, float *dieTempC) {
    uint8_t readDataT[2] = {0, 0};
    ina228ReadRegN(i2cAddres, i2cBus, INA228_REG_DIETEMP, readDataT, 2);

    int16_t rawTemp = (int16_t)(((uint16_t)readDataT[0] << 8) | readDataT[1]);
    *dieTempC = (float)rawTemp * 0.0078125f; // 7.8125 mC/LSB
    return false;
}

bool driverSWINA228GetChargeCoulombs(uint8_t i2cAddres, uint8_t i2cBus, double *coulombs, float currentLsb) {
    uint8_t readDataQ[5] = {0, 0, 0, 0, 0};
    ina228ReadRegN(i2cAddres, i2cBus, INA228_REG_CHARGE, readDataQ, 5);

    uint64_t raw40 = ((uint64_t)readDataQ[0] << 32) |
                     ((uint64_t)readDataQ[1] << 24) |
                     ((uint64_t)readDataQ[2] << 16) |
                     ((uint64_t)readDataQ[3] << 8)  |
                     ((uint64_t)readDataQ[4]);

    if (raw40 & 0x8000000000ULL) {
        raw40 |= 0xFFFFFF0000000000ULL; // Sign-extend 40-bit to 64-bit
    }

    int64_t signedCharge = (int64_t)raw40;
    *coulombs = (double)signedCharge * (16.0 * (double)currentLsb);
    return false;
}

bool driverSWINA228ResetAccumulators(uint8_t i2cAddres, uint8_t i2cBus) {
    uint8_t readCfg[2] = {0, 0};
    ina228ReadRegN(i2cAddres, i2cBus, INA228_REG_CONFIG, readCfg, 2);
    uint16_t cfg = (((uint16_t)readCfg[0] << 8) | readCfg[1]) | (1U << 14);
    ina228WriteReg16(i2cAddres, i2cBus, INA228_REG_CONFIG, cfg);
    return false;
}
