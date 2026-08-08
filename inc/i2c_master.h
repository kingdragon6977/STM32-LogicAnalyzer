#ifndef I2C_MASTER_H
#define I2C_MASTER_H

#include <stdint.h>

/* Use CH0 -> SDA and CH1 -> SCL by default. */
void i2c_master_init(void);

/* Do a 7-bit address scan, printing found addresses via uart */
void i2c_master_scan(void);

/* Write a single byte to addr (7-bit), returns 0 on NACK, 1 on ACK */
int i2c_master_write_byte(uint8_t addr7, uint8_t reg, uint8_t value);

/* Set bit delay to adjust speed (rough). Default set in init. */
void i2c_master_set_delay(uint32_t d);

#endif
