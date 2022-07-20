#include "uart.h"
#include "common.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

int fd;

int data_ready = 0;
time_t t = 0, t2 = 0;
struct tm tm;
struct tm tm1, tm2;

#define YEAR_OFF 1900
#define MON_OFF 1

#if 0
static void set_next_update_time(void)
{
  t = time(NULL);
  tm = *localtime(&t);

  tm1.tm_year = tm.tm_year;
  tm1.tm_mon = tm.tm_mon;
  tm1.tm_mday = tm.tm_mday;
  tm1.tm_hour = -1;
  tm1.tm_min = 0;
  tm1.tm_sec = 0;

  tm2.tm_year = tm1.tm_year;
  tm2.tm_mon = tm1.tm_mon;
  tm2.tm_mday = tm1.tm_mday+1;
  tm2.tm_hour = tm1.tm_hour;
  tm2.tm_min = tm1.tm_min;
  tm2.tm_sec = tm1.tm_sec;

  t2 = mktime(&tm2);
  dprintf(LVL_EXP, "=== now: [%lu] %d-%02d-%02d %02d:%02d:%02d, "
      "SOD: [%lu] %d-%02d-%02d %02d:%02d:%02d, "
      "NxD: [%lu] %d-%02d-%02d %02d:%02d:%02d ===\n",
      t, tm.tm_year+YEAR_OFF, tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
      mktime(&tm1), tm1.tm_year+YEAR_OFF, tm1.tm_mon+1, tm1.tm_mday, tm1.tm_hour, tm1.tm_min, tm1.tm_sec,
      t2, tm2.tm_year+YEAR_OFF, tm2.tm_mon+1, tm2.tm_mday, tm2.tm_hour, tm2.tm_min, tm2.tm_sec);
}
#endif

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
  uint8_t payload_data[PER_ADV_SIZE];
  uint8_t payload_size;
} ble_pkt;

typedef struct chunk {
  uint32_t pkt_arr_idx;
  uint32_t pkt_cnt;
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
  rbh->numchunks = rsb->num_chunks;
  ptr += sizeof(rpi_ble_hdr);
  memcpy(ptr, inbuf+inoff, inlen);

  idx = (idx + 1) % MAX_PKTS;
  rsb->pktidx_w = idx;
}

void prep_pkts_from_chunk(rpi_sl_buf *rsb, int chunk_id,
    char *chunk_data, uint64_t chunk_size)
{
#define MAX_PAYLOAD_SIZE (PER_ADV_SIZE - sizeof(rpi_ble_hdr))

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

//    hexdump(risk_payload, data_size);
//    bitdump(risk_payload, data_size);
    int chunkidx = rsb->chnkidx_w;
    prep_pkts_from_chunk(rsb, i, risk_payload, data_size);
    dprintf(LVL_EXP, "[%d:%d] chunk size: %llu, pkt arr idx: %u cnt: %u\r\n",
        i, rsb->num_chunks, data_size,
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
    dprintf(LVL_DBG, "G: %d, T: %u, chnk w: %u r: %u "
        "pkt w: %u r: %u rep: %u\r\n",
        gpio, tick, rsb->chnkidx_w, rsb->chnkidx_r,
        rsb->pktidx_w, rsb->pktidx_r, rsb->curr_chnk_repcnt);
    goto make_req;
  }

  uint32_t chunkidx = rsb->chnkidx_r;
  chunk *chnk = &rsb->chunk_arr[chunkidx];
  int pktidx = rsb->pktidx_r;
  ble_pkt *pkt = &rsb->pkt_arr[pktidx];
  uint8_t *ptr = pkt->payload_data;
//  int outlen = pkt->payload_size;
  int outlen = PER_ADV_SIZE;

//  hexdump((char *) ptr, outlen);

  int wlen = write(fd, ptr, outlen);
  if (wlen != outlen) {
    fprintf(stderr, "write error, len: %d wlen: %d\r\n", outlen, wlen);
  }
  dprintf(LVL_DBG, "[%u:%d]/%u,%u len: %u wlen: %d chnk w: %u r: %u "
      "pkt w: %u r: %u cidx: %u, +1: %u, mod: %u pktidx[mod]: %u "
      "chnk[idx,cnt]: [%u,%u] rep: %u\r\n",
      ((uint32_t *) ptr)[1], ((uint32_t *) ptr)[0], ((uint32_t *) ptr)[3],
      rsb->num_chunks,
      ((uint32_t *) ptr)[2], wlen, rsb->chnkidx_w, rsb->chnkidx_r,
      rsb->pktidx_w, rsb->pktidx_r, chunkidx, (chunkidx+1),
      (chunkidx+1) % rsb->num_chunks,
      rsb->chunk_arr[((chunkidx+1)%rsb->num_chunks)].pkt_arr_idx,
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

//  time_t currtime = time(NULL);
//  if (currtime < t2)
//    return;
//
//  set_next_update_time();

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

  cfsetospeed(&tty, (speed_t) speed);
  cfsetispeed(&tty, (speed_t) speed);

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

//  set_next_update_time();

  // make request to backend and fill payload_data
  make_request(&rsb);

  // read logs from beacon
  receive_log(fd);

  // should not get here
  return 0;

}
