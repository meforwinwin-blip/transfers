//Author: Leonardo La Rocca
#include "Melopero_AMG8833.h"

Melopero_AMG8833::Melopero_AMG8833(uint8_t i2cAddr){
    this->i2cAddress = i2cAddr;
}

void Melopero_AMG8833::setWire(TwoWire *wire){
    _wire = wire ? wire : &Wire;
}

int Melopero_AMG8833::readBytes(uint8_t startRegister, uint8_t *buffer, size_t length){
    if (!buffer || length == 0)
        return (int) Melopero_AMG8833_ERROR_CODE::ARGUMENT_ERROR;

    _wire->beginTransmission(this->i2cAddress);
    _wire->write(startRegister);
    if (_wire->endTransmission(false) != 0)
        return (int) Melopero_AMG8833_ERROR_CODE::ERROR_READING;

    size_t received = _wire->requestFrom(this->i2cAddress, length);
    if (received != length)
        return (int) Melopero_AMG8833_ERROR_CODE::ERROR_READING;

    for (size_t i = 0; i < length; ++i) {
        if (!_wire->available())
            return (int) Melopero_AMG8833_ERROR_CODE::ERROR_READING;
        buffer[i] = (uint8_t)_wire->read();
    }

    return (int) Melopero_AMG8833_ERROR_CODE::NO_ERROR;
}

int Melopero_AMG8833::readByte(uint8_t registerAddress){
    uint8_t value = 0;
    int result = readBytes(registerAddress, &value, 1);
    return result < 0 ? result : (int)value;
}

int Melopero_AMG8833::writeByte(uint8_t registerAddress, uint8_t value){
    _wire->beginTransmission(this->i2cAddress);
    _wire->write(registerAddress);
    _wire->write(value);
    return _wire->endTransmission() == 0
        ? (int) Melopero_AMG8833_ERROR_CODE::NO_ERROR
        : (int) Melopero_AMG8833_ERROR_CODE::ERROR_WRITING;
}

/*Set's the device operating mode.
Note:
    Writing operations in Sleep mode are not permitted. Just the set_mode to NORMAL_MODE is permitted.
    Reading operations in Sleep mode are not permitted.*/
int Melopero_AMG8833::setMode(DEVICE_MODE mode){
    return this->writeByte(MODE_REG_ADDR, (uint8_t) mode);
}

int Melopero_AMG8833::updateStatus(){
    int status = this->readByte(STATUS_REG_ADDR);
    if (status < 0) return (int) status;

    this->interruptTriggered = (status & INTERRUPT_OCCURRED_MASK) > 0? true : false;
    this->pixelTemperatureOverflow = (status & PIXEL_TEMPERATURE_OVERFLOW_MASK) > 0? true : false;
    this->thermistorTemperatureOverflow = (status & THERMISTOR_TEMPERATURE_OVERFLOW_MASK) > 0? true : false;

    return (int) Melopero_AMG8833_ERROR_CODE::NO_ERROR;
}

/*Clears the specified flags.*/
int Melopero_AMG8833::clearFlags(bool clearInterrupt, bool clearPixelTempOF, bool clearThermTempOF){
    uint8_t value = clearThermTempOF << 3 | clearPixelTempOF << 2 | clearInterrupt << 1;
    return this->writeByte(CLEAR_STATUS_REG_ADDR, value);
}

/*Clears the Status Register, Interrupt Flag, and Interrupt Table.*/
int Melopero_AMG8833::resetFlags(){
    return this->writeByte(RESET_REG_ADDR, 0x30);
}

/*Resets flags and returns to initial setting.*/
int Melopero_AMG8833::resetFlagsAndSettings(){
    return this->writeByte(RESET_REG_ADDR, 0x3F);
}

int Melopero_AMG8833::setFPSMode(FPS_MODE mode){
    return this->writeByte(FPS_MODE_REGISTER, (uint8_t) mode);
}

int Melopero_AMG8833::getFPSMode(){
    int value = this->readByte(FPS_MODE_REGISTER);
    if (value < 0) return value;
    return value & 1;
}

int Melopero_AMG8833::enableInterrupt(bool enable, INT_MODE mode){
    uint8_t regValue = ((uint8_t) mode) << 1 | enable;
    return this->writeByte(INTERRUPT_CONTROL_REG_ADDR, regValue);
}

int Melopero_AMG8833::setInterruptThreshold(float lowThreshold, float highThreshold, float hysteresis){
    if (lowThreshold < MIN_THRESHOLD_TEMP || lowThreshold > MAX_THRESHOLD_TEMP ||
        highThreshold < MIN_THRESHOLD_TEMP || highThreshold > MAX_THRESHOLD_TEMP)
        return (int) Melopero_AMG8833_ERROR_CODE::ARGUMENT_ERROR;

    uint16_t lowThrRegFormat = this->to12bitFormat(lowThreshold);
    uint16_t highThrRegFormat = this->to12bitFormat(highThreshold);
    uint16_t hysteresisRegFormat = this->to12bitFormat(hysteresis);

    int result = this->writeByte(INT_THR_HIGH_L_REG_ADDRESS, highThrRegFormat & 0x00FF);
    if (result < 0) return result;
    result = this->writeByte(INT_THR_HIGH_H_REG_ADDRESS, (highThrRegFormat >> 8) & 0x00FF);
    if (result < 0) return result;
    result = this->writeByte(INT_THR_LOW_L_REG_ADDRESS, lowThrRegFormat & 0x00FF);
    if (result < 0) return result;
    result = this->writeByte(INT_THR_LOW_H_REG_ADDRESS, (lowThrRegFormat >> 8) & 0x00FF);
    if (result < 0) return result;
    result = this->writeByte(INT_HYSTERESIS_L_REG_ADDRESS, hysteresisRegFormat & 0x00FF);
    if (result < 0) return result;
    return this->writeByte(INT_HYSTERESIS_H_REG_ADDRESS, (hysteresisRegFormat >> 8) & 0x00FF);
}

uint16_t Melopero_AMG8833::to12bitFormat(float temp){
    uint16_t value = temp >= 0? (uint16_t) (temp * 4) : (uint16_t) (-temp * 4);
    if (temp < 0){
        value = ~value;
        value += 1;
        value &= 0x0FFF;
        value |= 0x0800;
    }
    return value;
}

int Melopero_AMG8833::updateInterruptMatrix(){
    int currRowAddress = INT_TABLE_FIRST_ROW;
    for (int rowIndex = 0; rowIndex < 8; rowIndex++){
        int rowValue = this->readByte(currRowAddress);
        if (rowValue < 0)
            return rowValue;
        for (int colIndex = 0; colIndex < 8; colIndex++)
            this->interruptMatrix[rowIndex][colIndex] = (rowValue & (1 << colIndex)) > 0;
        currRowAddress++;
    }
    return (int) Melopero_AMG8833_ERROR_CODE::NO_ERROR;
}



int Melopero_AMG8833::updatePixelMatrix(){
    // Read one 8-pixel row (16 bytes) per transaction. This avoids 128
    // individual register transactions while remaining comfortably below
    // Arduino Wire buffer limits on both ESP32 and nRF52.
    uint8_t row[16];

    for (uint8_t y = 0; y < 8; ++y) {
        const uint8_t startRegister = FIRST_PIXEL_REGISTER + (y * 16);
        int result = this->readBytes(startRegister, row, sizeof(row));
        if (result < 0) return result;

        for (uint8_t x = 0; x < 8; ++x) {
            pixelMatrix[y][x] = this->parsePixel(row[x * 2], row[x * 2 + 1]);
        }
    }

    return (int) Melopero_AMG8833_ERROR_CODE::NO_ERROR;
}

float Melopero_AMG8833::parsePixel(uint8_t lsb, uint8_t msb){
    int unified_no_sign = ((msb & 7) << 8) | lsb;
    int value = (msb & 8) == 0 ? 0 : - (1 << 11);
    value += unified_no_sign;
    return ((float) value) / 4.0;
}

int Melopero_AMG8833::updateThermistorTemperature(){
    uint8_t data[2];
    int result = this->readBytes(THERMISTOR_REGISTER, data, sizeof(data));
    if (result < 0) return result;

    const uint16_t magnitude = ((uint16_t)(data[1] & 0x07) << 8) | data[0];
    const int16_t signedRaw = (data[1] & 0x08)
        ? -(int16_t)magnitude
        : (int16_t)magnitude;

    // Thermistor data is 12-bit signed magnitude at 0.0625 C/LSB.
    this->thermistorTemperature = (float)signedRaw * 0.0625f;

    return (int) Melopero_AMG8833_ERROR_CODE::NO_ERROR;
}

String Melopero_AMG8833::getErrorDescription(int errorCode){
    if (errorCode >= 0)
        return "No Error";
    if (errorCode == -1)
        return "Error reading from device";
    if (errorCode == -2)
        return "Error writing to device";
    if (errorCode == -3)
        return "Argument error: argument value is out of valid range";
    return "coding error"; 
}
