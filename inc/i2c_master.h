#ifndef I2C_MASTER_H
#define I2C_MASTER_H

#include <stdint.h>

/*
 * External bit-banged I2C master:
 * PC9  = SDA
 * PC12 = SCL
 *
 * The four logic-analyzer inputs PA0-PA3 remain available for passive capture.
 */
void i2c_master_init(void);

/* Scan normal 7-bit I2C addresses 0x08-0x77 and print ACKing devices. */
void i2c_master_scan(void);

/* Write one register byte. addr7 is a 7-bit address. Returns 1 on ACK. */
int i2c_master_write_byte(uint8_t addr7, uint8_t reg, uint8_t value);

/* Generate a known-good I2C transaction for analyzer loopback testing. */
int i2c_master_test_transaction(void);

/*
 * Synchronized I2C/analyzer self-test.
 *
 * Starts DMA capture on PA0-PA3, generates the known transaction on PC9/PC12,
 * waits for DMA completion, then decodes PA0=SDA and PA1=SCL.
 */
int i2c_master_capture_test(void);

/* Adjust the conservative bit-bang delay. Larger values = slower bus. */
void i2c_master_set_delay(uint32_t d);

#endif
