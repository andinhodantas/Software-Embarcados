#include "mpu6050_handler.h"
#include <stdio.h>

// Endereços dos Registradores
static const uint8_t MPU6050_ADDR         = 0x68;
static const uint8_t PWR_MGMT_1_REG     = 0x6B;
static const uint8_t GYRO_CONFIG_REG    = 0x1B;
static const uint8_t ACCEL_CONFIG_REG   = 0x1C;
static const uint8_t ACCEL_XOUT_H_REG   = 0x3B;

// Variáveis estáticas para guardar o estado da biblioteca
static i2c_inst_t *i2c_port;
static float accel_divisor;
static float gyro_divisor;

// Função interna para acordar o sensor
static void mpu6050_wake_up() {
    uint8_t buf[] = {PWR_MGMT_1_REG, 0x00};
    i2c_write_blocking(i2c_port, MPU6050_ADDR, buf, 2, false);
}

// --- Funções Públicas ---

bool mpu6050_init(i2c_inst_t *i2c, gyro_fs_range_t gyro_range, accel_fs_range_t accel_range) {
    i2c_port = i2c;
    mpu6050_wake_up();

    // 1. Configurar o Giroscópio
    // O valor a ser escrito no registrador é o enum (0, 1, 2 ou 3) deslocado 3 bits para a esquerda
    uint8_t gyro_config_val = gyro_range << 3;
    uint8_t gyro_buf[] = {GYRO_CONFIG_REG, gyro_config_val};
    i2c_write_blocking(i2c_port, MPU6050_ADDR, gyro_buf, 2, false);

    // 2. Configurar o Acelerômetro
    uint8_t accel_config_val = accel_range << 3;
    uint8_t accel_buf[] = {ACCEL_CONFIG_REG, accel_config_val};
    i2c_write_blocking(i2c_port, MPU6050_ADDR, accel_buf, 2, false);
    
    // 3. Armazenar os divisores corretos com base na configuração
    switch (gyro_range) {
        case GYRO_FS_250_DPS:  gyro_divisor = 131.0f; break;
        case GYRO_FS_500_DPS:  gyro_divisor = 65.5f;  break;
        case GYRO_FS_1000_DPS: gyro_divisor = 32.8f;  break;
        case GYRO_FS_2000_DPS: gyro_divisor = 16.4f;  break;
    }
    
    switch (accel_range) {
        case ACCEL_FS_2G:  accel_divisor = 16384.0f; break;
        case ACCEL_FS_4G:  accel_divisor = 8192.0f;  break;
        case ACCEL_FS_8G:  accel_divisor = 4096.0f;  break;
        case ACCEL_FS_16G: accel_divisor = 2048.0f;  break;
    }
    
    // Testa se o dispositivo ainda está presente após a configuração
    uint8_t temp;
    return i2c_read_blocking(i2c_port, MPU6050_ADDR, &temp, 1, false) >= 0;
}

bool mpu6050_read_data(mpu6050_data_t *data) {
    uint8_t buffer[14];
    
    i2c_write_blocking(i2c_port, MPU6050_ADDR, &ACCEL_XOUT_H_REG, 1, true);
    int bytes_read = i2c_read_blocking(i2c_port, MPU6050_ADDR, buffer, 14, false);

    if (bytes_read != 14) {
        return false;
    }

    int16_t accel_raw[3], gyro_raw[3], temp_raw;

    accel_raw[0] = (buffer[0] << 8) | buffer[1];
    accel_raw[1] = (buffer[2] << 8) | buffer[3];
    accel_raw[2] = (buffer[4] << 8) | buffer[5];
    temp_raw     = (buffer[6] << 8) | buffer[7];
    gyro_raw[0]  = (buffer[8] << 8) | buffer[9];
    gyro_raw[1]  = (buffer[10] << 8) | buffer[11];
    gyro_raw[2]  = (buffer[12] << 8) | buffer[13];

    // ATUALIZADO: Usa as variáveis de divisor em vez de números fixos
    data->accel_x = accel_raw[0] / accel_divisor;
    data->accel_y = accel_raw[1] / accel_divisor;
    data->accel_z = accel_raw[2] / accel_divisor;
    
    data->gyro_x  = gyro_raw[0] / gyro_divisor;
    data->gyro_y  = gyro_raw[1] / gyro_divisor;
    data->gyro_z  = gyro_raw[2] / gyro_divisor;
    
    // A fórmula da temperatura permanece a mesma
    data->temperature = (temp_raw / 340.0f) + 36.53f;

    return true;
}