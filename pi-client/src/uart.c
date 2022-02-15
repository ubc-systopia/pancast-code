#include "uart.h"

int fd;

int data_ready = 0;

struct req_data request_data;
double last_request_time_sec;

uint32_t prev_seq_num = 0;
uint32_t seq_num = 0;
uint32_t chunk_num = 0;
uint8_t chunk_rep_count = 0;
uint32_t num_chunks;
uint32_t current_chunk_offset = 0;

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


void convert_chunk_to_pkts(char* chunk, uint32_t chunk_number, uint32_t offset, uint32_t len) {

    num_pkts = len / (PAYLOAD_SIZE - PACKET_HEADER_LEN);
    //printf("num_pkts: %d\r\n", num_pkts);

    uint32_t data_len = PAYLOAD_SIZE - PACKET_HEADER_LEN;

    for (int i = 0; i < num_pkts; i++) {
      // Add sequence number
      memcpy(payload_data + offset + i*PAYLOAD_SIZE, &i, sizeof(uint32_t));
      // Add chunk number
      memcpy(payload_data + offset + i*PAYLOAD_SIZE + sizeof(uint32_t), &chunk_number, sizeof(uint32_t));
      // Add chunk length
      memcpy(payload_data + offset + i*PAYLOAD_SIZE + 2*sizeof(uint32_t), &len, sizeof(uint64_t));

      // Add data
      memcpy(payload_data + offset + i*PAYLOAD_SIZE + PACKET_HEADER_LEN, &chunk[i*data_len], data_len);
    }
    
    // add last packet if data does not fit evenly 
    if (len % (PAYLOAD_SIZE - PACKET_HEADER_LEN) != 0) {
      // Add sequence number
      memcpy(payload_data + offset + (num_pkts)*PAYLOAD_SIZE, &num_pkts, sizeof(uint32_t));
      // Add chunk number
      memcpy(payload_data + offset + (num_pkts)*PAYLOAD_SIZE + sizeof(uint32_t), &chunk_number, sizeof(uint32_t));
      // Add chunk length
      memcpy(payload_data + offset + (num_pkts)*PAYLOAD_SIZE + 2*sizeof(uint32_t), &len, sizeof(uint64_t));
      
      uint32_t last_data_len = len - (num_pkts)*(PAYLOAD_SIZE - PACKET_HEADER_LEN);
      // Add data
      memcpy(payload_data + offset + (num_pkts)*PAYLOAD_SIZE + PACKET_HEADER_LEN, &chunk[data_len*num_pkts], last_data_len);
      num_pkts++;
    }
}

void make_request() {

  data_ready = 0;

  // get number of chunks in payload from backend
  struct req_data chunk_count_data = {0};
  handle_request_count(&chunk_count_data);

  memcpy(&num_chunks, chunk_count_data.response, sizeof(uint32_t));
  // printf("num chunks: %u\r\n", num_chunks);

  if (num_chunks == 0) {
    printf("num chunks is 0! No Request for data.\r\n");
    return;
  }

  for (int i = 0; i < num_chunks; i++) {
    // make request to backend for the chunk
    struct req_data req_chunk = {0};
    handle_request_chunk(&req_chunk, i);

    uint32_t data_size = req_chunk.size - REQ_HEADER_SIZE;

    // load chunk into payload_data[]
    convert_chunk_to_pkts(req_chunk.response + sizeof(uint64_t), i, i*PAYLOAD_SIZE*num_pkts, data_size);
    printf("total chunk size: %u\r\n", PAYLOAD_SIZE*num_pkts);	  
  }

  clock_t last_request_time = clock();
  last_request_time_sec = ((double)last_request_time)/CLOCKS_PER_SEC;
  printf("request at time: %f\r\n", last_request_time_sec);

  data_ready = 1;
}


void gpio_callback(int gpio, int level, uint32_t tick) {

  if (data_ready == 0) {
    return;
  }

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
     // make_request(); instead want to move some data pointer to the next chunk
     //  current_chunk_offset = current_chunk_offset + 1;
      chunk_rep_count = 0;
    }
  }
  else if (level == 0) {
    // no action
  }

  clock_t curr_time = clock();
  double curr_time_sec = ((double)curr_time)/CLOCKS_PER_SEC;

  if (curr_time_sec - last_request_time_sec > REQUEST_INTERVAL) {
    // request new data
    make_request();
    last_request_time_sec = curr_time_sec;	  
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
  // struct risk_data* r_data = (struct risk_data*)arg;

  // make request to backend and fill payload_data
  make_request();

  // for (int i = 0; i < PAYLOAD_SIZE*num_pkts; i++) {
  //     if (i % 16 == 0) {
  //       printf("\r\n");
  //     }
  //     printf("%x ", payload_data[i]);
  // }

  // open port for read and write over UART
  fd = open(portname, O_RDWR);

  if (fd == -1) {
    fprintf(stderr, "Error opening %s: %s\n", portname, strerror(errno));
    fprintf(stderr, "Is broadcast device connected?\r\n");
    return 0;
  }

  // baudrate 115200, 8 bits, no parity, 1 stop bit
  set_interface_attribs(fd, B115200);

  // init GPIO
  gpioCfgClock(1, 0, 0);

  if (gpioInitialise() == -1) {
    printf("Error initialising GPIO\r\n");
    return 0;
  }

  gpioSetMode(PIN, PI_INPUT);
  gpioSetAlertFunc(PIN, gpio_callback);

  // read logs from beacon
  receive_log();

  // should not get here
  return 0;

}
