#include "uart.h"

// #define TEST_MODE

/* Open and read from GPIO pin
 */
static int GPIORead(int pin) {
#define PATH_LEN 30
  char path[PATH_LEN];
  char value_str[3];
  int fd;

  snprintf(path, PATH_LEN, "/sys/class/gpio/gpio%d/value", pin);
  fd = open(path, O_RDONLY);
  if (fd == -1) {
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

/* Set attributes for serial communication,
    from https://stackoverflow.com/questions/6947413/how-to-open-read-and-write-from-serial-port-in-c
 */
int set_interface_attribs(int fd, int speed) {
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

  // setup for non-canonical mode
  tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
  tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
  tty.c_oflag &= ~OPOST;

  // fetch bytes as they become available
  tty.c_cc[VMIN] = 1;
  tty.c_cc[VTIME] = 1;

  if (tcsetattr(fd, TCSANOW, &tty) != 0) {
    printf("Error from tcsetattr: %s\n", strerror(errno));
    return -1;
  }
  return 0;
}

/* Main function for data transfer over UART
 */
void* uart_main(void* arg) {
  int fd;
  char* portname = TERMINAL;
  struct risk_data* r_data = (struct risk_data*)arg;

  printf("Starting uart main loop\r\n");

  // open port for write only
  fd = open(portname, O_WRONLY);

  if (fd == -1) {
    fprintf(stderr, "Error opening %s: %s\n", portname, strerror(errno));
    fprintf(stderr, "Is broadcast device connected?\r\n");
    return 0;
  }

  // baudrate 115200, 8 bits, no parity, 1 stop bit
  set_interface_attribs(fd, B115200);

  while (1) {
    pthread_mutex_lock(&r_data->mutex);
    while (!r_data->request_ready) {
      pthread_cond_wait(&r_data->request_ready_cond, &r_data->mutex);
    }
    r_data->request_ready = 0;

#ifdef TEST_MODE

    // Set test data to sequential values
    uint8_t testarray[TEST_SIZE];
    for (int i = 0; i < TEST_SIZE; i++) {
      testarray[i]= i%255;
      // printf("%d", testarray[i]);
    }

    struct risk_data* r_data = (struct risk_data*)arg;

    int data_sent = 0;

    // write data to serial port in CHUNK_SIZE increments
    while (data_sent < TEST_SIZE) {
      tcflush(fd, TCIOFLUSH);
      // wait for ready signal from GPIO
      uint8_t ready = 0;
      while (ready == 0) {
        ready = GPIORead(PIN);
      }

      // write data
      tcflush(fd, TCIOFLUSH); // clear anything that might be in the buffer
      int wlen = write(fd, &testarray[data_sent], CHUNK_SIZE);
      if (wlen != CHUNK_SIZE) {
        fprintf(stderr, "Error from write: %d, %d\n", wlen, errno);
      }

      printf("uart sent %d bytes\r\n", wlen);

      tcdrain(fd); // ensure all bytes are trasnmitted before continue
      // prepare next chunk of data to send
      data_sent = data_sent + CHUNK_SIZE; // assumption: b_data_size mod CHUNK_SIZE = 0
      if (data_sent >= TEST_SIZE) {
        data_sent = 0;
      }
    }
#else

    int data_sent = 0;

    // write data to serial port in CHUNK_SIZE increments
    while (data_sent < r_data->data.size) {
      tcflush(fd, TCIOFLUSH);
      // wait for ready signal from GPIO
      uint8_t ready = 0;
      while (ready == 0) {
        ready = GPIORead(PIN);
      }

      // write data
      tcflush(fd, TCIOFLUSH); // clear anything that might be in the buffer
      int chunk_size = CHUNK_SIZE;
      int wlen = write(fd, &r_data->data.response[data_sent], chunk_size);
#define chunk ((uint8_t *)(&r_data->data.response[data_sent]))
      printf("data : 0x\r\n");
      for (int i = 0; i < CHUNK_SIZE; i++){
      	printf("%.2x ", chunk[i]);
	if (i % 16 == 15) {
		printf("\r\n");
	}
      }
      printf("\r\n");
      printf("%d bytes written\r\n", wlen);

      // write until entire chunk transferred
      while (wlen < CHUNK_SIZE) {
        if (wlen == -1) {
          fprintf(stderr, "Error from write: %d, %d\n", wlen, errno);
	  return 0;
	}
	printf("%d bytes written\r\n", wlen);
	chunk_size = chunk_size - wlen;
        int new_wlen = write(fd, &r_data->data.response[data_sent], chunk_size);
	wlen = wlen + new_wlen;
      }

      printf("uart sent %d bytes\r\n", wlen);
      tcdrain(fd);

      // prepare next chunk of data to send
      data_sent = data_sent + CHUNK_SIZE; // assumption: b_data_size mod CHUNK_SIZE = 0
      if (data_sent >= r_data->data.size) {
        data_sent = 0;
        if (r_data->data_ready == 1) {
          printf("Data ready, uart releasing lock\r\n");
	  r_data->uart_ready = 1;
          pthread_cond_signal(&r_data->uart_ready_cond);
          pthread_mutex_unlock(&r_data->mutex);
	  break;
        }
      }
    }
#endif
    pthread_mutex_unlock(&r_data->mutex);
  }
  close(fd); // should move inside while loop
  // should not reach here
  return 0;
}  
