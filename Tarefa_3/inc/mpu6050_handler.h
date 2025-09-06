#ifndef MPU6050_HANDLER_H
#define MPU6050_HANDLER_H

#include "hardware/i2c.h"
#include <stdbool.h>

// Enum para a faixa de medição do Giroscópio (Full-Scale Range)
// Baseado na Tabela 6.1, página 12 do datasheet [cite: 184]
typedef enum {
    GYRO_FS_250_DPS,  // ±250 °/s
    GYRO_FS_500_DPS,  // ±500 °/s
    GYRO_FS_1000_DPS, // ±1000 °/s
    GYRO_FS_2000_DPS  // ±2000 °/s
} gyro_fs_range_t;

// Enum para a faixa de medição do Acelerômetro (Full-Scale Range)
// Baseado na Tabela 6.2, página 13 do datasheet [cite: 194]
typedef enum {
    ACCEL_FS_2G,  // ±2g
    ACCEL_FS_4G,  // ±4g
    ACCEL_FS_8G,  // ±8g
    ACCEL_FS_16G  // ±16g
} accel_fs_range_t;


// Estrutura para guardar os dados lidos do sensor
typedef struct {
    float accel_x, accel_y, accel_z; // em g
    float gyro_x, gyro_y, gyro_z;   // em °/s
    float temperature;              // em °C
} mpu6050_data_t;

/**
 * @brief Inicializa o sensor MPU-6050 com as configurações de sensibilidade desejadas.
 * @param i2c O ponteiro da instância I2C (ex: i2c0).
 * @param gyro_range A faixa de medição desejada para o giroscópio.
 * @param accel_range A faixa de medição desejada para o acelerômetro.
 * @return true se a inicialização for bem-sucedida.
 */
bool mpu6050_init(i2c_inst_t *i2c, gyro_fs_range_t gyro_range, accel_fs_range_t accel_range);

/**
 * @brief Lê os dados de aceleração, giroscópio e temperatura do sensor.
 * @param data Ponteiro para a struct onde os dados lidos serão armazenados.
 * @return true se a leitura for bem-sucedida.
 */
bool mpu6050_read_data(mpu6050_data_t *data);

#endif // MPU6050_HANDLER_H