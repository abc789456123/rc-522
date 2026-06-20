#ifndef RC522_H
#define RC522_H

#include <stdint.h>

int rc522_open(const char *device);
void rc522_close(void);

uint8_t rc522_read_reg(uint8_t reg);
void rc522_write_reg(uint8_t reg, uint8_t value);

void rc522_set_bit_mask(uint8_t reg, uint8_t mask);
void rc522_clear_bit_mask(uint8_t reg, uint8_t mask);

void rc522_reset(void);
void rc522_init(void);

int rc522_request(uint8_t *tag_type);
int rc522_anticoll(uint8_t *uid);

#endif