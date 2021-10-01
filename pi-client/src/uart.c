#include "uart.h"

int fd;

struct req_data* request_data;

uint32_t prev_seq_num = 0;
uint32_t seq_num = 0;
uint32_t chunk_num = 0;
uint8_t chunk_rep_count = 0;
uint32_t num_chunks;

uint8_t payload_data[MAX_PAYLOAD_SIZE];
uint32_t num_pkts;

void* receive_log() {
  while (1) {
    char c[1];
    int len = read(fd, c, sizeof(char));
    if (len > 0) {
      printf("%c", c[0]);
    }
  }
}


void convert_chunk_to_pkts(char* chunk, uint32_t len) {

    num_pkts = len / (PAYLOAD_SIZE - PACKET_HEADER_LEN);

    printf("num_pkts: %d\r\n", num_pkts);
    printf("len: %lu\r\n", len);

    uint32_t data_len = PAYLOAD_SIZE - PACKET_HEADER_LEN;

    for (int i = 0; i < num_pkts; i++) {
      // Add sequence number
      memcpy(payload_data + i*PAYLOAD_SIZE, &i, sizeof(uint32_t));
      // Add chunk number
      memcpy(payload_data + i*PAYLOAD_SIZE + sizeof(uint32_t), &chunk_num, sizeof(uint32_t));
      // Add chunk length
      memcpy(payload_data + i*PAYLOAD_SIZE + 2*sizeof(uint32_t), &len, sizeof(uint64_t));

      // Add data
      memcpy(payload_data + i*PAYLOAD_SIZE + PACKET_HEADER_LEN, &chunk[i*data_len], data_len);
    }
    
    if (len % (PAYLOAD_SIZE - PACKET_HEADER_LEN) != 0) {
      // Add last packet which has less data
      //num_pkts++;
      // Add sequence number
      memcpy(payload_data + (num_pkts-1)*PAYLOAD_SIZE, &num_pkts, sizeof(uint32_t));
      // Add chunk number
      memcpy(payload_data + (num_pkts-1)*PAYLOAD_SIZE + sizeof(uint32_t), &chunk_num, sizeof(uint32_t));
      // Add chunk length
      memcpy(payload_data + (num_pkts-1)*PAYLOAD_SIZE + 2*sizeof(uint32_t), &len, sizeof(uint64_t));
      
      uint32_t last_data_len = len - (num_pkts-1)*(PAYLOAD_SIZE - PACKET_HEADER_LEN);
      // Add data
      memcpy(payload_data + (num_pkts-1)*PAYLOAD_SIZE + PACKET_HEADER_LEN, &chunk[data_len*num_pkts-1], last_data_len);
    }

}

void make_request() {

  struct req_data* new_data = malloc(sizeof(struct req_data));

  chunk_num++;

  if (chunk_num >= num_chunks) {
    chunk_num = 0;
  }

  handle_request_chunk(new_data, chunk_num);
  
  uint32_t real_data_size = new_data->size - REQ_HEADER_SIZE;

  // get len from the response data

  convert_chunk_to_pkts(new_data->response + sizeof(uint64_t), real_data_size); 
  //convert_chunk_to_pkts(new_data.response, new_data.size);

  free(new_data);
  
}


void gpio_callback(int gpio, int level, uint32_t tick) {

  if (level == 1) {

    int wlen = write(fd, &payload_data[seq_num*PAYLOAD_SIZE], PAYLOAD_SIZE);
    if (wlen != PAYLOAD_SIZE) {
      fprintf(stderr, "Full len not written, len:%d\r\n", wlen);
    }
    printf("wrote %d bytes\r\n", wlen);

    // update sequence
    prev_seq_num = seq_num;
    seq_num = (seq_num + 1) % num_pkts;
    if (prev_seq_num > seq_num) {
      chunk_rep_count++;
      printf("chunk_rep_count: %d\r\n", chunk_rep_count);
    }
    if (chunk_rep_count > CHUNK_REPLICATION) {
      make_request();
      chunk_rep_count = 0;
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

  char* portname = TERMINAL;
  struct risk_data* r_data = (struct risk_data*)arg;

  struct req_data count_data = {0};
  handle_request_count(&count_data);

  //max_chunks = (uint32_t)count_data.response;
  memcpy(&num_chunks, count_data.response, sizeof(uint32_t));
  printf("max chunks: %u\r\n", num_chunks);
  
 // printf("size: %d\r\n", payload->data.size);
//  for (int i = 0; i < payload->data.size; i++) {
//    printf("%d ", payload->data.response[i]);
//  }
//  printf("\r\n");

  request_data = malloc(sizeof(struct req_data));
  handle_request_chunk(request_data, 0);

  uint32_t real_data_size = request_data->size - REQ_HEADER_SIZE;

  printf("req_data_size: %lu\r\n", request_data->size);

  printf("real_data_size: %lu\r\n", real_data_size);

  convert_chunk_to_pkts(request_data->response + sizeof(uint64_t), real_data_size); 

  for (int i = 0; i < MAX_PAYLOAD_SIZE; i++) {
      if (i % 250 == 0) {
        printf("\r\n");
      }
      printf("0x%x, ", payload_data[i]);
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
