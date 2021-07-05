#include "uart.h"

// #define TEST_MODE

static int GPIORead(int pin)
{
#define VALUE_MAX 30
	char path[VALUE_MAX];
	char value_str[3];
	int fd;

	snprintf(path, VALUE_MAX, "/sys/class/gpio/gpio%d/value", pin);
	fd = open(path, O_RDONLY);
	if (-1 == fd) {
		fprintf(stderr, "Failed to open gpio value for reading!\n");
		return(-1);
	}

	if (-1 == read(fd, value_str, 3)) {
		fprintf(stderr, "Failed to read value!\n");
		return(-1);
	}

	close(fd);
	return(atoi(value_str));
}

static int GPIO_open(int pin)
{
    // TODO: open GPIO
    return 0;
}

static int GPIO_read(int pin)
{
    // TODO: read GPIO
    return 0;
}

/* Set attributes for serial communication, 
    from https://stackoverflow.com/questions/6947413/how-to-open-read-and-write-from-serial-port-in-c 
*/
int set_interface_attribs(int fd, int speed)
{
    struct termios tty;

    if (tcgetattr(fd, &tty) < 0) {
        printf("Error from tcgetattr: %s\n", strerror(errno));
        return -1;
    }

    cfsetospeed(&tty, (speed_t)speed);
    cfsetispeed(&tty, (speed_t)speed);

    tty.c_cflag |= (CLOCAL | CREAD);    /* ignore modem controls */
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;         /* 8-bit characters */
    tty.c_cflag |= PARENB;     /* parity bit (even by default) */
    tty.c_cflag &= ~CSTOPB;     /* only need 1 stop bit */
    tty.c_cflag |= CRTSCTS;    /* enable hardware flowcontrol */

    /* setup for non-canonical mode */
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    tty.c_oflag &= ~OPOST;

    /* fetch bytes as they become available */
    tty.c_cc[VMIN] = 1;
    tty.c_cc[VTIME] = 1;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        printf("Error from tcsetattr: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

void* uart_main(void* arg) {
    int fd; 
    char* portname = TERMINAL;

    struct risk_data* r_data = (struct risk_data*)arg;

    printf("Data size: %d\r\n", r_data->data.size);

    while (1) {
    // open port for write only
    if (!fd) {
        fd = open(portname, O_WRONLY);
    }

    if (fd < 0) {
     //  fprintf(stderr, "Error opening %s: %s\n", portname, strerror(errno));
       // continue;
    }

    /*baudrate 115200, 8 bits, no parity, 1 stop bit */
    set_interface_attribs(fd, B115200);

    #ifdef TEST_MODE

    // Set test data to sequential values
    uint8_t testarray[TEST_SIZE];
    for (int i = 0; i < TEST_SIZE; i++) {
        testarray[i]= i%sizeof(uint8_t);
    }

    struct risk_data* r_data = (struct risk_data*)arg;

    // lock data
    pthread_mutex_lock(&r_data->mutex);
    int data_sent = 0;

    // write data to serial port in CHUNK_SIZE increments
    while (data_sent < TEST_SIZE) {
        tcflush(fd, TCIOFLUSH);
        // wait for ready signal from GPIO
        uint8_t ready = 0;
        while (ready == 0) {
            int ready = GPIORead(PIN);
        }

        // write data 
        tcflush(fd, TCIOFLUSH); // clear anything that might be in the buffer
        int wlen = write(fd, &testarray[data_sent], CHUNK_SIZE);
        if (wlen != CHUNK_SIZE) {
            fprintf(stderr, "Error from write: %d, %d\n", wlen, errno);
        }
        tcdrain(fd); // ensure all bytes are trasnmitted before continue

        // prepare next chunk of data to send
        data_sent = data_sent + CHUNK_SIZE; // assumption: b_data_size mod CHUNK_SIZE = 0
        if (data_sent >= TEST_SIZE) {
            data_sent = 0;
        }
        // should not reach here yet, TODO
        pthread_mutex_unlock(&r_data->mutex);
        close(fd);
    }
    #else

    int data_sent = 0;

    // write data to serial port in CHUNK_SIZE increments
    while (data_sent < r_data->data.size) {
        tcflush(fd, TCIOFLUSH);
        // wait for ready signal from GPIO
        uint8_t ready = 0;
        while (ready == 0) {
            int ready = GPIORead(PIN);
        }

        // write data 
        tcflush(fd, TCIOFLUSH); // clear anything that might be in the buffer
        int wlen = write(fd, &r_data->data.response[data_sent], CHUNK_SIZE);
        if (wlen != CHUNK_SIZE) {
           fprintf(stderr, "Error from write: %d, %d\n", wlen, errno);
        }
        tcdrain(fd);

        // prepare next chunk of data to send
        data_sent = data_sent + CHUNK_SIZE; // assumption: b_data_size mod CHUNK_SIZE = 0
        if (data_sent >= r_data->data.size) {
            data_sent = 0;
        }
    }
    #endif
}

    return 0;
}