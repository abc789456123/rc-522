#include "rc522.h"

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

#define CommandReg      0x01
#define ComIEnReg       0x02
#define ComIrqReg       0x04
#define DivIrqReg       0x05
#define ErrorReg        0x06
#define FIFODataReg     0x09
#define FIFOLevelReg    0x0A
#define ControlReg      0x0C
#define BitFramingReg   0x0D
#define ModeReg         0x11
#define TxControlReg    0x14
#define TxASKReg        0x15
#define TModeReg        0x2A
#define TPrescalerReg   0x2B
#define TReloadRegH     0x2C
#define TReloadRegL     0x2D

#define PCD_IDLE        0x00
#define PCD_TRANSCEIVE  0x0C
#define PCD_SOFTRESET   0x0F

#define PICC_REQIDL     0x26
#define PICC_ANTICOLL   0x93

#define MI_OK           0
#define MI_NOTAGERR     1
#define MI_ERR          2

static int spi_fd = -1;

static uint8_t spi_mode = SPI_MODE_0;
static uint8_t spi_bits = 8;
static uint32_t spi_speed = 100000; // 처음엔 낮게

int rc522_open(const char *device)
{
    spi_fd = open(device, O_RDWR);
    if (spi_fd < 0) {
        perror("open spi");
        return -1;
    }

    if (ioctl(spi_fd, SPI_IOC_WR_MODE, &spi_mode) < 0) {
        perror("SPI_IOC_WR_MODE");
        return -1;
    }

    if (ioctl(spi_fd, SPI_IOC_WR_BITS_PER_WORD, &spi_bits) < 0) {
        perror("SPI_IOC_WR_BITS_PER_WORD");
        return -1;
    }

    if (ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &spi_speed) < 0) {
        perror("SPI_IOC_WR_MAX_SPEED_HZ");
        return -1;
    }

    return 0;
}

void rc522_close(void)
{
    if (spi_fd >= 0) {
        close(spi_fd);
        spi_fd = -1;
    }
}

uint8_t rc522_read_reg(uint8_t reg)
{
    uint8_t tx[2];
    uint8_t rx[2];

    tx[0] = ((reg << 1) & 0x7E) | 0x80;
    tx[1] = 0x00;

    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = (unsigned long)rx,
        .len = 2,
        .speed_hz = spi_speed,
        .bits_per_word = spi_bits,
    };

    if (ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr) < 0) {
        perror("SPI_IOC_MESSAGE read");
        return 0;
    }

    return rx[1];
}

void rc522_write_reg(uint8_t reg, uint8_t value)
{
    uint8_t tx[2];

    tx[0] = (reg << 1) & 0x7E;
    tx[1] = value;

    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = 0,
        .len = 2,
        .speed_hz = spi_speed,
        .bits_per_word = spi_bits,
    };

    if (ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr) < 0) {
        perror("SPI_IOC_MESSAGE write");
    }
}

void rc522_reset(void)
{
    // CommandReg = 0x01, SoftReset = 0x0F
    rc522_write_reg(0x01, 0x0F);
    usleep(50000);
}

void rc522_init(void)
{
    rc522_reset();

    // Timer / mode 기본 설정
    rc522_write_reg(0x2A, 0x8D); // TModeReg
    rc522_write_reg(0x2B, 0x3E); // TPrescalerReg
    rc522_write_reg(0x2D, 30);   // TReloadRegL
    rc522_write_reg(0x2C, 0);    // TReloadRegH

    rc522_write_reg(0x15, 0x40); // TxASKReg
    rc522_write_reg(0x11, 0x3D); // ModeReg

    // 안테나 ON
    uint8_t tx_control = rc522_read_reg(0x14); // TxControlReg
    if (!(tx_control & 0x03)) {
        rc522_write_reg(0x14, tx_control | 0x03);
    }
}

void rc522_set_bit_mask(uint8_t reg, uint8_t mask)
{
    uint8_t tmp = rc522_read_reg(reg);
    rc522_write_reg(reg, tmp | mask);
}

void rc522_clear_bit_mask(uint8_t reg, uint8_t mask)
{
    uint8_t tmp = rc522_read_reg(reg);
    rc522_write_reg(reg, tmp & (~mask));
}

static int rc522_to_card(uint8_t command, uint8_t *send_data, int send_len,
                         uint8_t *back_data, int *back_len)
{
    int status = MI_ERR;
    uint8_t irq_en = 0x00;
    uint8_t wait_irq = 0x00;
    uint8_t n;
    int i;

    if (command == PCD_TRANSCEIVE) {
        irq_en = 0x77;
        wait_irq = 0x30;
    }

    rc522_write_reg(ComIEnReg, irq_en | 0x80);
    rc522_clear_bit_mask(ComIrqReg, 0x80);
    rc522_set_bit_mask(FIFOLevelReg, 0x80);

    rc522_write_reg(CommandReg, PCD_IDLE);

    for (i = 0; i < send_len; i++) {
        rc522_write_reg(FIFODataReg, send_data[i]);
    }

    rc522_write_reg(CommandReg, command);

    if (command == PCD_TRANSCEIVE) {
        rc522_set_bit_mask(BitFramingReg, 0x80);
    }

    i = 2000;
    do {
        n = rc522_read_reg(ComIrqReg);
        i--;
    } while (i && !(n & 0x01) && !(n & wait_irq));

    rc522_clear_bit_mask(BitFramingReg, 0x80);

    if (i != 0) {
        if (!(rc522_read_reg(ErrorReg) & 0x1B)) {
            status = MI_OK;

            if (n & irq_en & 0x01) {
                status = MI_NOTAGERR;
            }

            if (command == PCD_TRANSCEIVE) {
                n = rc522_read_reg(FIFOLevelReg);
                uint8_t last_bits = rc522_read_reg(ControlReg) & 0x07;

                if (last_bits) {
                    *back_len = (n - 1) * 8 + last_bits;
                } else {
                    *back_len = n * 8;
                }

                if (n == 0) n = 1;
                if (n > 16) n = 16;

                for (i = 0; i < n; i++) {
                    back_data[i] = rc522_read_reg(FIFODataReg);
                }
            }
        } else {
            status = MI_ERR;
        }
    }

    return status;
}

int rc522_request(uint8_t *tag_type)
{
    int status;
    int back_bits;
    uint8_t req = PICC_REQIDL;

    rc522_write_reg(BitFramingReg, 0x07);

    status = rc522_to_card(PCD_TRANSCEIVE, &req, 1, tag_type, &back_bits);

    if ((status != MI_OK) || (back_bits != 0x10)) {
        return MI_ERR;
    }

    return MI_OK;
}

int rc522_anticoll(uint8_t *uid)
{
    int status;
    int back_bits;
    uint8_t ser_num_check = 0;
    uint8_t buffer[2];

    rc522_write_reg(BitFramingReg, 0x00);

    buffer[0] = PICC_ANTICOLL;
    buffer[1] = 0x20;

    status = rc522_to_card(PCD_TRANSCEIVE, buffer, 2, uid, &back_bits);

    if (status == MI_OK) {
        for (int i = 0; i < 4; i++) {
            ser_num_check ^= uid[i];
        }

        if (ser_num_check != uid[4]) {
            return MI_ERR;
        }
    }

    return status;
}