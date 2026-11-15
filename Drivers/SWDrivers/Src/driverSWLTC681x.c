//
//  driverSWLTC681x.c
//  
//
//  Created by Matt Cavins on 11/15/26.
//

#include "driverSWLTC681x.h"
#include <string.h>

uint8_t driverSWLTC681xTotalNumberOfICs = 1;
driverLTC681xConfigStructTypedef driverSWLTC681xConfigStruct;

void driverSWLTC681xInit(driverLTC681xConfigStructTypedef configStruct, uint8_t totalNumberOfLTCs) {
    driverSWLTC681xTotalNumberOfICs = (totalNumberOfLTCs > 0 && totalNumberOfLTCs <= LTC681X_MAX_ICS) ? totalNumberOfLTCs : 1;
    driverSWLTC681xConfigStruct = configStruct;

    // Auto-detect chipType from noOfCells if chipType wasn't explicitly set
    if (driverSWLTC681xConfigStruct.chipType == 0) {
        if (configStruct.noOfCells <= 6) {
            driverSWLTC681xConfigStruct.chipType = LTC_CHIP_6810;
        } else if (configStruct.noOfCells <= 12) {
            driverSWLTC681xConfigStruct.chipType = LTC_CHIP_6811;
        } else if (configStruct.noOfCells <= 15) {
            driverSWLTC681xConfigStruct.chipType = LTC_CHIP_6812;
        } else {
            driverSWLTC681xConfigStruct.chipType = LTC_CHIP_6813;
        }
    }

    driverHWSPI1Init(LTC_CS_GPIO_Port, LTC_CS_Pin);
    driverSWLTC681xWakeup();
    driverSWLTC681xWriteConfig(driverSWLTC681xConfigStruct);
}

void driverSWLTC681xReInit(void) {
    driverSWLTC681xWakeup();
    driverSWLTC681xWriteConfig(driverSWLTC681xConfigStruct);
}

// Wakes up LTC6820 isoSPI transceiver and all daisy-chained slave ICs
void driverSWLTC681xWakeup(void) {
    uint8_t dummy = 0xFF;
    for (uint8_t i = 0; i < driverSWLTC681xTotalNumberOfICs; i++) {
        driverSWLTC681xWrite(&dummy, 1);
    }
}

void driverSWLTC681xStartCellVoltageConversion(void) {
    uint16_t cmd = (driverSWLTC681xConfigStruct.CellVoltageConversionMode != 0)
                   ? driverSWLTC681xConfigStruct.CellVoltageConversionMode
                   : LTC681X_CMD_ADCV_NORM_ALL;
    driverSWLTC681xWakeup();
    driverSWLTC681xSendCommand(cmd);
}

void driverSWLTC681xStartTemperatureVoltageConversion(void) {
    driverSWLTC681xWakeup();
    driverSWLTC681xSendCommand(LTC681X_CMD_ADAX_NORM_ALL);
    driverSWLTC681xSendCommand(LTC681X_CMD_ADSTAT_NORM_ALL);
}

void driverSWLTC681xResetCellVoltageRegisters(void) {
    driverSWLTC681xWakeup();
    driverSWLTC681xSendCommand(LTC681X_CMD_CLRCELL);
}

// Reads cell voltages across all daisy-chained ICs (LTC6810=6, LTC6811=12, LTC6812=15, LTC6813/1818=18)
bool driverSWLTC681xReadCellVoltages(driverLTC681xCellsTypedef cellVoltages[]) {
    static const uint16_t cellRegCmds[6] = {
        LTC681X_CMD_RDCVA, // Cells 1-3
        LTC681X_CMD_RDCVB, // Cells 4-6  (LTC6810 uses groups A-B)
        LTC681X_CMD_RDCVC, // Cells 7-9
        LTC681X_CMD_RDCVD, // Cells 10-12 (LTC6804/6811 uses groups A-D)
        LTC681X_CMD_RDCVE, // Cells 13-15 (LTC6812 uses groups A-E)
        LTC681X_CMD_RDCVF  // Cells 16-18 (LTC6813/ADBMS1818 uses groups A-F)
    };

    uint8_t cellsPerIC = (driverSWLTC681xConfigStruct.noOfCells > 0)
                         ? driverSWLTC681xConfigStruct.noOfCells
                         : (uint8_t)driverSWLTC681xConfigStruct.chipType;
    uint8_t numGroups = ((uint8_t)driverSWLTC681xConfigStruct.chipType + 2U) / 3U;
    bool dataValid = true;

    driverSWLTC681xWakeup();

    for (uint8_t grp = 0; grp < numGroups; grp++) {
        uint8_t cmd[4];
        uint8_t rx_data[8 * LTC681X_MAX_ICS] = {0};

        cmd[0] = (uint8_t)((cellRegCmds[grp] >> 8) & 0xFFU);
        cmd[1] = (uint8_t)(cellRegCmds[grp] & 0xFFU);
        uint16_t cmdPec = driverSWLTC681xCalcPEC15(2, cmd);
        cmd[2] = (uint8_t)((cmdPec >> 8) & 0xFFU);
        cmd[3] = (uint8_t)(cmdPec & 0xFFU);

        driverSWLTC681xWriteRead(cmd, 4, rx_data, 8U * driverSWLTC681xTotalNumberOfICs);

        for (uint8_t ic = 0; ic < driverSWLTC681xTotalNumberOfICs; ic++) {
            uint8_t *frame = &rx_data[ic * 8U];
            uint16_t receivedPec = ((uint16_t)frame[6] << 8) | frame[7];
            uint16_t calcPec = driverSWLTC681xCalcPEC15(6, frame);

            if (receivedPec != calcPec) {
                dataValid = false;
            }

            for (uint8_t c = 0; c < 3; c++) {
                uint8_t cellInIC = (grp * 3U) + c;
                if (cellInIC < cellsPerIC) {
                    uint16_t raw16 = ((uint16_t)frame[(c * 2U) + 1U] << 8) | frame[c * 2U];
                    float voltage = (float)raw16 * 0.0001f; // 100 uV/LSB on all LTC681x chips
                    uint16_t globalCellIdx = (ic * cellsPerIC) + cellInIC;

                    if (voltage < 5.5f && voltage > 0.1f) {
                        cellVoltages[globalCellIdx].cellVoltage = voltage;
                    } else {
                        dataValid = false;
                    }
                    cellVoltages[globalCellIdx].cellNumber = globalCellIdx;
                }
            }
        }
    }

    return dataValid;
}

// Reads GPIO1, GPIO2 (external NTCs) from AUX Group A and Internal Die Temp (ITMP) from Status Group A
bool driverSWLTC681xReadTempVoltages(uint16_t tempVoltages[3]) {
    uint8_t cmd[4];
    uint8_t rx_aux[8 * LTC681X_MAX_ICS] = {0};
    uint8_t rx_stat[8 * LTC681X_MAX_ICS] = {0};
    bool pecOk = true;

    driverSWLTC681xWakeup();

    // 1. Read AUX Group A (0x000C)
    cmd[0] = (uint8_t)((LTC681X_CMD_RDAUXA >> 8) & 0xFFU);
    cmd[1] = (uint8_t)(LTC681X_CMD_RDAUXA & 0xFFU);
    uint16_t pec = driverSWLTC681xCalcPEC15(2, cmd);
    cmd[2] = (uint8_t)((pec >> 8) & 0xFFU);
    cmd[3] = (uint8_t)(pec & 0xFFU);
    driverSWLTC681xWriteRead(cmd, 4, rx_aux, 8U * driverSWLTC681xTotalNumberOfICs);

    uint16_t rxPecAux = ((uint16_t)rx_aux[6] << 8) | rx_aux[7];
    if (rxPecAux != driverSWLTC681xCalcPEC15(6, rx_aux)) {
        pecOk = false;
    }

    // Note: On LTC6810, AUXA holds [S0, GPIO1, GPIO2]; on LTC6804/11/12/13 it holds [GPIO1, GPIO2, GPIO3]
    if (driverSWLTC681xConfigStruct.chipType == LTC_CHIP_6810) {
        tempVoltages[0] = ((uint16_t)rx_aux[3] << 8) | rx_aux[2]; // GPIO1
        tempVoltages[1] = ((uint16_t)rx_aux[5] << 8) | rx_aux[4]; // GPIO2
    } else {
        tempVoltages[0] = ((uint16_t)rx_aux[1] << 8) | rx_aux[0]; // GPIO1
        tempVoltages[1] = ((uint16_t)rx_aux[3] << 8) | rx_aux[2]; // GPIO2
    }

    // 2. Read STATUS Group A (0x0010) for Internal Die Temperature (ITMP in bytes 2-3)
    cmd[0] = (uint8_t)((LTC681X_CMD_RDSTATA >> 8) & 0xFFU);
    cmd[1] = (uint8_t)(LTC681X_CMD_RDSTATA & 0xFFU);
    pec = driverSWLTC681xCalcPEC15(2, cmd);
    cmd[2] = (uint8_t)((pec >> 8) & 0xFFU);
    cmd[3] = (uint8_t)(pec & 0xFFU);
    driverSWLTC681xWriteRead(cmd, 4, rx_stat, 8U * driverSWLTC681xTotalNumberOfICs);

    uint16_t rxPecStat = ((uint16_t)rx_stat[6] << 8) | rx_stat[7];
    if (rxPecStat != driverSWLTC681xCalcPEC15(6, rx_stat)) {
        pecOk = false;
    }
    tempVoltages[2] = ((uint16_t)rx_stat[3] << 8) | rx_stat[2]; // ITMP

    return pecOk;
}

void driverSWLTC681xWriteConfig(driverLTC681xConfigStructTypedef configStruct) {
    uint8_t txBuf[4 + (8 * LTC681X_MAX_ICS)] = {0};

    // VUV = (UnderVoltage / (16 * 100uV)) - 1; VOV = (OverVoltage / (16 * 100uV))
    uint16_t vuv = (configStruct.CellUnderVoltageLimit > 0.1f)
                   ? (uint16_t)((configStruct.CellUnderVoltageLimit / 0.0016f) - 1.0f) : 0;
    uint16_t vov = (configStruct.CellOverVoltageLimit > 0.1f)
                   ? (uint16_t)(configStruct.CellOverVoltageLimit / 0.0016f) : 0;

    txBuf[0] = (uint8_t)((LTC681X_CMD_WRCFGA >> 8) & 0xFFU);
    txBuf[1] = (uint8_t)(LTC681X_CMD_WRCFGA & 0xFFU);
    uint16_t cmdPec = driverSWLTC681xCalcPEC15(2, txBuf);
    txBuf[2] = (uint8_t)((cmdPec >> 8) & 0xFFU);
    txBuf[3] = (uint8_t)(cmdPec & 0xFFU);

    for (uint8_t i = 0; i < driverSWLTC681xTotalNumberOfICs; i++) {
        uint8_t *cfg = &txBuf[4U + (i * 8U)];
        cfg[0] = 0xFCU; // GPIO1-5 pull-downs off, REFON=1 (keep reference awake), ADCOPT=0
        cfg[1] = (uint8_t)(vuv & 0xFFU);
        cfg[2] = (uint8_t)(((vuv >> 8) & 0x0FU) | ((vov & 0x0FU) << 4));
        cfg[3] = (uint8_t)((vov >> 4) & 0xFFU);
        cfg[4] = (uint8_t)(configStruct.DisChargeEnableMask & 0xFFU);        // DCC1..DCC8
        cfg[5] = (uint8_t)((configStruct.DisChargeEnableMask >> 8) & 0x0FU); // DCC9..DCC12

        uint16_t dataPec = driverSWLTC681xCalcPEC15(6, cfg);
        cfg[6] = (uint8_t)((dataPec >> 8) & 0xFFU);
        cfg[7] = (uint8_t)(dataPec & 0xFFU);
    }

    driverSWLTC681xWakeup();
    driverSWLTC681xWrite(txBuf, 4U + (8U * driverSWLTC681xTotalNumberOfICs));
}

bool driverSWLTC681xReadConfig(driverLTC681xConfigStructTypedef *configStruct) {
    uint8_t cmd[4];
    uint8_t rx_data[8 * LTC681X_MAX_ICS] = {0};

    cmd[0] = (uint8_t)((LTC681X_CMD_RDCFGA >> 8) & 0xFFU);
    cmd[1] = (uint8_t)(LTC681X_CMD_RDCFGA & 0xFFU);
    uint16_t cmdPec = driverSWLTC681xCalcPEC15(2, cmd);
    cmd[2] = (uint8_t)((cmdPec >> 8) & 0xFFU);
    cmd[3] = (uint8_t)(cmdPec & 0xFFU);

    driverSWLTC681xWakeup();
    driverSWLTC681xWriteRead(cmd, 4, rx_data, 8U * driverSWLTC681xTotalNumberOfICs);

    uint16_t rxPec = ((uint16_t)rx_data[6] << 8) | rx_data[7];
    uint16_t calcPec = driverSWLTC681xCalcPEC15(6, rx_data);

    uint16_t vuv = ((uint16_t)(rx_data[2] & 0x0FU) << 8) | rx_data[1];
    uint16_t vov = ((uint16_t)rx_data[3] << 4) | ((rx_data[2] >> 4) & 0x0FU);

    configStruct->DisChargeEnableMask = ((uint32_t)(rx_data[5] & 0x0FU) << 8) | rx_data[4];
    configStruct->CellUnderVoltageLimit = (float)(vuv + 1U) * 0.0016f;
    configStruct->CellOverVoltageLimit  = (float)vov * 0.0016f;

    return (rxPec == calcPec);
}

bool driverSWLTC681xReadVoltageFlags(uint16_t *underVoltageFlags, uint16_t *overVoltageFlags) {
    uint8_t cmd[4];
    uint8_t rx_statB[8 * LTC681X_MAX_ICS] = {0};

    *underVoltageFlags = 0;
    *overVoltageFlags  = 0;

    cmd[0] = (uint8_t)((LTC681X_CMD_RDSTATB >> 8) & 0xFFU);
    cmd[1] = (uint8_t)(LTC681X_CMD_RDSTATB & 0xFFU);
    uint16_t cmdPec = driverSWLTC681xCalcPEC15(2, cmd);
    cmd[2] = (uint8_t)((cmdPec >> 8) & 0xFFU);
    cmd[3] = (uint8_t)(cmdPec & 0xFFU);

    driverSWLTC681xWakeup();
    driverSWLTC681xWriteRead(cmd, 4, rx_statB, 8U * driverSWLTC681xTotalNumberOfICs);

    uint16_t rxPec = ((uint16_t)rx_statB[6] << 8) | rx_statB[7];
    if (rxPec != driverSWLTC681xCalcPEC15(6, rx_statB)) {
        return false;
    }

    // STATB bytes 2, 3, 4 hold C1UV/C1OV .. C12UV/C12OV pairs
    uint32_t combined = ((uint32_t)rx_statB[4] << 16) | ((uint32_t)rx_statB[3] << 8) | rx_statB[2];
    for (uint8_t bit = 0; bit < driverSWLTC681xConfigStruct.noOfCells && bit < 12; bit++) {
        if (combined & (1UL << (bit * 2U))) {
            *underVoltageFlags |= (1U << bit);
        }
        if (combined & (1UL << ((bit * 2U) + 1U))) {
            *overVoltageFlags |= (1U << bit);
        }
    }
    return true;
}

void driverSWLTC681xEnableBalanceResistors(uint32_t balanceEnableMask) {
    driverSWLTC681xConfigStruct.DisChargeEnableMask = balanceEnableMask;
    driverSWLTC681xWriteConfig(driverSWLTC681xConfigStruct);
}

// 15-bit PEC (CRC15) calculation for all LTC681x / ADBMS1818 chips
uint16_t driverSWLTC681xCalcPEC15(uint8_t len, const uint8_t *data) {
    uint16_t remainder = 16; // PEC15 seed = 0x0010
    for (uint8_t i = 0; i < len; i++) {
        uint8_t byte = data[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            uint16_t din = ((byte >> (7 - bit)) & 0x01U) ^ ((remainder >> 14) & 0x01U);
            remainder = (remainder << 1);
            if (din) {
                remainder ^= 0x4599U;
            }
            remainder &= 0x7FFFU;
        }
    }
    return (remainder << 1);
}

void driverSWLTC681xSendCommand(uint16_t command) {
    uint8_t cmd[4];
    cmd[0] = (uint8_t)((command >> 8) & 0xFFU);
    cmd[1] = (uint8_t)(command & 0xFFU);
    uint16_t pec = driverSWLTC681xCalcPEC15(2, cmd);
    cmd[2] = (uint8_t)((pec >> 8) & 0xFFU);
    cmd[3] = (uint8_t)(pec & 0xFFU);
    driverSWLTC681xWrite(cmd, 4);
}

void driverSWLTC681xWrite(uint8_t *writeBytes, uint16_t writeLength) {
    driverHWSPI1Write(writeBytes, (uint8_t)writeLength, LTC_CS_GPIO_Port, LTC_CS_Pin);
}

void driverSWLTC681xWriteRead(uint8_t *writeBytes, uint16_t writeLength, uint8_t *readBytes, uint16_t readLength) {
    driverHWSPI1WriteRead(writeBytes, (uint8_t)writeLength, readBytes, (uint8_t)readLength, LTC_CS_GPIO_Port, LTC_CS_Pin);
}

// Updated for 16-bit 100uV/LSB ADC (VREF2 = 3.0V = 30000 counts) and safe down to -45C!
float driverSWLTC681xConvertTemperatureExt(uint16_t inputValue, uint32_t ntcNominal, uint32_t ntcSeriesResistance, uint16_t ntcBetaFactor, float ntcNominalTemp) {
    if (inputValue == 0 || inputValue >= 30000U) {
        return 200.0f; // Open or shorted sensor fault
    }
    float scalar = (30000.0f / (float)inputValue) - 1.0f;
    float rNtc = (float)ntcSeriesResistance / scalar;
    float steinhart = logf(rNtc / (float)ntcNominal) / (float)ntcBetaFactor;
    steinhart += 1.0f / (ntcNominalTemp + 273.15f);
    steinhart = (1.0f / steinhart) - 273.15f;

    // Lowered cutoff from -30.0f to -45.0f so -30C cold weather operation never triggers a false 200C fault!
    if (steinhart < -45.0f) {
        steinhart = 200.0f;
    }
    return steinhart;
}

// Updated for LTC681x internal die temperature sensor: ITMP * 100uV / (7.5mV/K) - 273.15
float driverSWLTC681xConvertTemperatureInt(uint16_t inputValue) {
    return ((float)inputValue * 0.0133333f) - 273.15f;
}
