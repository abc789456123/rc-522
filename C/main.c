#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include "rc522.h"

#define LOST_THRESHOLD 5

static volatile int running = 1;
int lost_count = 0;
int card_present = 0;

void handle_sigint(int sig)
{
    (void)sig;
    running = 0;
}

int main(void)
{
    uint8_t version;
    uint8_t tag_type[2];
    uint8_t uid[5];
    int card_present = 0;

    signal(SIGINT, handle_sigint);

    if (rc522_open("/dev/spidev0.0") < 0) {
        return 1;
    }

    rc522_init();

    version = rc522_read_reg(0x37);
    printf("MFRC522 VersionReg: 0x%02X\n", version);

    if (version == 0x91 || version == 0x92 || version == 0x18) {
        printf("RC522 정상 인식\n");
    } else {
        printf("VersionReg 값이 예상과 다름\n");
    }

    printf("카드를 RC522에 가까이 대세요. 종료: Ctrl+C\n");

    while (running) {
        int detected = 0;

        if (rc522_request(tag_type) == 0) {
            if (rc522_anticoll(uid) == 0) {
                detected = 1;
            }
        }

        if (detected) {
            lost_count = 0;

            if (!card_present) {
                printf("카드 감지됨 UID: %02X %02X %02X %02X %02X\n",
                    uid[0], uid[1], uid[2], uid[3], uid[4]);
                card_present = 1;
            }
        } else {
            if (card_present) {
                lost_count++;

                if (lost_count >= LOST_THRESHOLD) {
                    printf("카드 제거됨\n");
                    card_present = 0;
                    lost_count = 0;
                }
            }
        }

        usleep(100000);
    }

    printf("\n종료합니다.\n");
    rc522_close();
    return 0;
}