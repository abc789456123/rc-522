# rc522_test.py
import spidev
import RPi.GPIO as GPIO
import time

RST_PIN = 25  # 물리핀 22 = BCM GPIO25

# MFRC522 register
CommandReg = 0x01
VersionReg = 0x37
SoftReset = 0x0F

bus = 0
device = 0  # CE0, 물리핀 24

spi = spidev.SpiDev()

def write_reg(reg, value):
    # write: address format = (reg << 1) & 0x7E
    spi.xfer2([(reg << 1) & 0x7E, value])

def read_reg(reg):
    # read: address format = ((reg << 1) & 0x7E) | 0x80
    result = spi.xfer2([((reg << 1) & 0x7E) | 0x80, 0x00])
    return result[1]

def hard_reset():
    GPIO.output(RST_PIN, GPIO.LOW)
    time.sleep(0.05)
    GPIO.output(RST_PIN, GPIO.HIGH)
    time.sleep(0.05)

try:
    GPIO.setmode(GPIO.BCM)
    GPIO.setup(RST_PIN, GPIO.OUT)

    spi.open(bus, device)
    spi.max_speed_hz = 1000000
    spi.mode = 0
    spi.bits_per_word = 8

    hard_reset()

    write_reg(CommandReg, SoftReset)
    time.sleep(0.05)

    version = read_reg(VersionReg)

    print(f"MFRC522 VersionReg: 0x{version:02X}")

    if version in (0x91, 0x92):
        print("RC522 연결 성공!")
    elif version == 0x00 or version == 0xFF:
        print("통신 실패 가능성 높음: 배선, SPI 활성화, 전원 확인")
    else:
        print("응답은 있음. 클론 칩이거나 다른 버전일 수 있음.")

finally:
    spi.close()
    GPIO.cleanup()