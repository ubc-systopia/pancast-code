import serial


def main():
    sp = serial.Serial(
        port="COM5",
        baudrate=115200,
        bytesize=8,
        stopbits=serial.STOPBITS_ONE,
        parity=serial.PARITY_NONE
    )
    while True:
        output = str(sp.readline())
        print(output)


if __name__ == '__main__':
    main()
