#include "uart.h"

// #define TEST_MODE

struct risk_data* payload;
int data_index;
int fd;
uint32_t seq_num = 0;

uint8_t test_data[TEST_SIZE];

void* receive_log() {
  while (1) {
    char c[1];
    int len = read(fd, c, sizeof(char));
    if (len > 0) {
      printf("%c", c[0]);
    }
  }
}

void gpio_callback(int gpio, int level, uint32_t tick) {

  if (level == 1) {
    memcpy(&test_data[0], &seq_num, sizeof(uint32_t));
    printf("seq num: %d\r\n", seq_num);
    printf("payload: %d\r\n", test_data[0]);
    int wlen = write(fd, &test_data[0], CHUNK_SIZE);
    if (wlen != CHUNK_SIZE) {
      fprintf(stderr, "Full len not written, len:%d\r\n", wlen);
    }
    seq_num++;
    printf("wrote %d bytes\r\n", wlen);
    data_index = data_index + CHUNK_SIZE;
    if (data_index >= TEST_SIZE) {
      data_index = 0;
    }
  }
  else if (level == 0) {
    // no action
  }
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

  for (int i = 0; i < TEST_SIZE; i++) {
    test_data[i] = i;
  }

  char* portname = TERMINAL;
  struct risk_data* r_data = (struct risk_data*)arg;

  if (!payload) {
    payload = (struct risk_data*)malloc(sizeof(r_data));
    payload = r_data;
  }

  printf("Starting uart main loop\r\n");

  // open port for read and write
  fd = open(portname, O_RDWR);

  if (fd == -1) {
    fprintf(stderr, "Error opening %s: %s\n", portname, strerror(errno));
    fprintf(stderr, "Is broadcast device connected?\r\n");
    return 0;
  }

  // baudrate 115200, 8 bits, no parity, 1 stop bit
  set_interface_attribs(fd, B115200);

  // Init GPIO

  gpioCfgClock(1, 0, 0);

  if (gpioInitialise() == -1) {
    printf("Error initialising GPIO\r\n");
    return 0;
  }

  gpioSetMode(PIN, PI_INPUT);
  gpioSetAlertFunc(PIN, gpio_callback);

  receive_log();

  return 0;

}
