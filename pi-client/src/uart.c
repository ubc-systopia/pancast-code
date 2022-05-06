#include "uart.h"

int fd;

int data_ready = 0;

void *receive_log(int fd)
{
  while (1) {
    char c[1];
    int len = read(fd, c, sizeof(char));
    if (len > 0) {
      printf("%c", c[0]);
    }
  }
}

// ====

typedef struct ble_pkt {
  uint8_t payload_data[MAX_PACKET_SIZE];
  uint8_t payload_size;
} ble_pkt;

typedef struct chunk {
  uint8_t pkt_arr_idx;
  uint8_t pkt_cnt;
} chunk;

typedef struct rpi_sl_buf {
  ble_pkt pkt_arr[MAX_PKTS];
  int pktidx_w;
  int chnkidx_w;
  int pktidx_r;
  int chnkidx_r;
  int curr_chnk_repcnt;
  int num_chunks;
  double last_req_time_s;
  chunk *chunk_arr;
} rpi_sl_buf;

static inline void init_rpi_sl_buf(rpi_sl_buf *rsb)
{
  if (!rsb)
    return;

  memset(rsb, 0, sizeof(rpi_sl_buf));
}

static void reset_rpi_sl_buf(rpi_sl_buf *rsb)
{
  if (!rsb)
    return;

  if (rsb->chunk_arr)
    free(rsb->chunk_arr);

  init_rpi_sl_buf(rsb);
}

void prep_next_pkt(rpi_sl_buf *rsb, char *inbuf, int inoff, int inlen,
    uint32_t chunkid, uint64_t chunklen, uint32_t pkt_seq)
{
  if (!rsb || !inbuf || !inlen)
    return;

  int idx = rsb->pktidx_w;
  uint8_t *ptr = rsb->pkt_arr[idx].payload_data;
  rsb->pkt_arr[idx].payload_size = inlen + sizeof(rpi_ble_hdr);

  rpi_ble_hdr *rbh = (rpi_ble_hdr *) ptr;
  rbh->pkt_seq = pkt_seq;
  rbh->chunkid = chunkid;
  rbh->chunklen = chunklen;
  ptr += sizeof(rpi_ble_hdr);
  memcpy(ptr, inbuf+inoff, inlen);

  idx = (idx + 1) % MAX_PKTS;
  rsb->pktidx_w = idx;
}

void prep_pkts_from_chunk(rpi_sl_buf *rsb, int chunk_id,
    char *chunk_data, uint64_t chunk_size)
{
#define MAX_PAYLOAD_SIZE (MAX_PACKET_SIZE - sizeof(rpi_ble_hdr))

  int woff = 0, wlen = 0, tot_len = 0;
  uint32_t seq = 0;

  rsb->chunk_arr[rsb->chnkidx_w].pkt_arr_idx = rsb->pktidx_w;
  while (tot_len < chunk_size) {
    wlen = (chunk_size - tot_len > MAX_PAYLOAD_SIZE) ? MAX_PAYLOAD_SIZE :
      (chunk_size - tot_len);
    prep_next_pkt(rsb, chunk_data, woff, wlen, chunk_id, chunk_size, seq);
    woff += wlen;
    tot_len += wlen;
    seq += 1;
  }
  rsb->chunk_arr[rsb->chnkidx_w].pkt_cnt = (rsb->pktidx_w -
      rsb->chunk_arr[rsb->chnkidx_w].pkt_arr_idx);
  rsb->chnkidx_w = (rsb->chnkidx_w+1) % rsb->num_chunks;
}

void make_request(rpi_sl_buf *rsb)
{
  reset_rpi_sl_buf(rsb);

  // get number of chunks in payload from backend
  struct req_data chunk_count_data = {0};
  handle_request_count(&chunk_count_data);

  rsb->num_chunks = ((uint32_t *) chunk_count_data.response)[0];

  if (rsb->num_chunks == 0)
    return;

  rsb->chunk_arr = (chunk *) malloc(sizeof(chunk) * rsb->num_chunks);
  memset(rsb->chunk_arr, 0, sizeof(chunk) * rsb->num_chunks);

  for (int i = 0; i < rsb->num_chunks; i++) {

    // make request to backend for the chunk
    struct req_data req_chunk = {0};
    handle_request_chunk(&req_chunk, i);

    chunk_hdr *chdr = (chunk_hdr *) req_chunk.response;
    uint64_t data_size = chdr->payload_len;

    // load chunk into payload_data[]
    char *risk_payload = req_chunk.response + sizeof(chunk_hdr);
    int chunkidx = rsb->chnkidx_w;
    prep_pkts_from_chunk(rsb, i, risk_payload, data_size);
    dprintf(LVL_EXP, "chunk size: %llu, pkt arr idx: %u cnt: %u\r\n", data_size,
        rsb->chunk_arr[chunkidx].pkt_arr_idx, rsb->chunk_arr[chunkidx].pkt_cnt);
  }

  rsb->last_req_time_s = ((double) clock()) / CLOCKS_PER_SEC;
  dprintf(LVL_EXP, "[%04.6f]: #chunks: %d\r\n", rsb->last_req_time_s,
      rsb->num_chunks);

  data_ready = 1;
}


void gpio_callback(int gpio, int level, uint32_t tick, void *rsb_p)
{
  if (data_ready == 0) {
    return;
  }

  rpi_sl_buf *rsb = (rpi_sl_buf *) rsb_p;
  double curr_time_sec;

  if (level == 0) {
    dprintf(LVL_DBG, "G: %d, T: %u, chnk w: %u r: %u pkt w: %u r: %u rep: %u\r\n",
        gpio, tick, rsb->chnkidx_w, rsb->chnkidx_r, rsb->pktidx_w, rsb->pktidx_r,
        rsb->curr_chnk_repcnt);
    goto make_req;
  }

  int chunkidx = rsb->chnkidx_r;
  chunk *chnk = &rsb->chunk_arr[chunkidx];
  int pktidx = rsb->pktidx_r;
  ble_pkt *pkt = &rsb->pkt_arr[pktidx];
  uint8_t *ptr = pkt->payload_data;
//  int outlen = pkt->payload_size;
  int outlen = MAX_PACKET_SIZE;
  int wlen = write(fd, ptr, outlen);
  if (wlen != outlen) {
    fprintf(stderr, "write error, len: %d wlen: %d\r\n", outlen, wlen);
  }
  dprintf(LVL_EXP, "[%u:%d] len: %llu wlen: %d "
      "chnk w: %u r: %u pkt w: %u r: %u chnk[idx,cnt]: [%u,%u] rep: %u\r\n",
      ((uint32_t *) ptr)[1], ((uint32_t *) ptr)[0], ((uint64_t *) ptr)[1], wlen,
      rsb->chnkidx_w, rsb->chnkidx_r, rsb->pktidx_w, rsb->pktidx_r,
      chnk->pkt_arr_idx, chnk->pkt_cnt,
      rsb->curr_chnk_repcnt);

  // next packet
  pktidx = (pktidx+1) % MAX_PKTS;

  // repeat chunk
  if (pktidx >= ((chnk->pkt_arr_idx + chnk->pkt_cnt) % MAX_PKTS)) {
    rsb->curr_chnk_repcnt++;
    pktidx = chnk->pkt_arr_idx;
  }

  // move to next chunk
  if (rsb->curr_chnk_repcnt >= CHUNK_REPLICATION) {
    chunkidx = (chunkidx+1) % rsb->num_chunks;
    rsb->curr_chnk_repcnt = 0;

    pktidx = rsb->chunk_arr[chunkidx].pkt_arr_idx;
  }

  rsb->pktidx_r = pktidx;
  rsb->chnkidx_r = chunkidx;

make_req:

  curr_time_sec = ((double) clock()) / CLOCKS_PER_SEC;
  if (curr_time_sec - rsb->last_req_time_s < REQUEST_INTERVAL)
    return;

  // request new risk data from backend after INTERVAL
  make_request(rsb);
  rsb->last_req_time_s = curr_time_sec;
}

/*
 * Set attributes for serial communication, from
 * https://stackoverflow.com/questions/6947413/how-to-open-read-and-write-from-serial-port-in-c
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

/*
 * Main function for data transfer over UART
 */
void *uart_main(void *arg)
{
  rpi_sl_buf rsb;
  init_rpi_sl_buf(&rsb);

  char *portname = TERMINAL;

  // open port for read and write over UART
  fd = open(portname, O_RDWR);

  if (fd == -1) {
    fprintf(stderr, "Error opening %s: %s\n", portname, strerror(errno));
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

  data_ready = 0;

  gpioSetMode(PIN, PI_INPUT);
  gpioSetAlertFuncEx(PIN, gpio_callback, (void *) &rsb);

  // make request to backend and fill payload_data
  make_request(&rsb);

  // read logs from beacon
  receive_log(fd);

  // should not get here
  return 0;

}
