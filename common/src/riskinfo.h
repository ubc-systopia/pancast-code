/*
 * riskinfo.h
 *
 *     author: aasthakm
 * created on: May 06, 2022
 *
 * common data structure for risk broadcast on bluetooth and wifi
 */

#ifndef __RISKINFO_H__
#define __RISKINFO_H__

typedef struct chunk_hdr {
  uint64_t payload_len;
} chunk_hdr;

typedef struct rpi_ble_hdr {
  uint32_t pkt_seq;
  uint32_t chunkid;
  uint64_t chunklen;
} rpi_ble_hdr;

#endif /* __RISKINFO_H__ */
