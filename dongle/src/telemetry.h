#ifndef DONGLE_TELEMETRY__H
#define DONGLE_TELEMETRY__H

// The applications can be configured to provide a verbose real-time
// telemetry stream. Output is filtered using the corresponding log
// level macro.

// Each line of output is a single telemetry record. Telemetry records
// are comma-delimited and are all of the form
//
//        <type>,<data>,...,
//
// where <type> is an 8-bit integer (expressed) in hexadecimal format
// which indicates the type of record i.e. schema of the following data
// fields and their semantics. The type codes are enumerated below.
//
// Application Start
// This record contains no data but simply indicates that a system start
// has occurred. Useful for differentiating app executions in the
// resulting logs.
#define TELEM_TYPE_RESTART 0x00
//
// Scan Result
// A bluetooth packet was received whose length matches what is expected
// from the beacon broadcast.
#define TELEM_TYPE_SCAN_RESULT 0x01
// Data is of the form
//
//          <t_d>,<i>,<signal_id>,<addr>,<rssi>
//
// Where
// - t_d is the dongle clock expressed as a base-10 integer
// - i is the number of the current dongle epoch, also base 10
// - signal_id is an identifier for the associated packet, unique to the
//   given epoch
// - addr is the bluetooth address of the advertising device
// - rssi is the RSSI value associated with the packet
//
// Broadcast UID Mismatch
// The service ID contained within an advertisement packet does not
// match the predetermined value. Hence, the packet will be discarded.
#define TELEM_TYPE_BROADCAST_ID_MISMATCH 0x02
// Schema:
//
//          <t_d>,<i>,<signal_id>
//
//
// New Ephemeral ID Tracked
// An ephemeral ID has been successfully decoded and addded to the set
// of tracked IDs.
#define TELEM_TYPE_BROADCAST_TRACK_NEW 0x03
// Schema:
//
//          <t_d>,<i>,<signal_id>,<beacon_id>,<t_b>
//
// Where
// - beacon_id is the ID of the broadcasting beacon
// - t_b is the beacon's clock
//
// Tracked Ephemeral ID Observed
// The received eph. ID matches one currently tracked.
#define TELEM_TYPE_BROADCAST_TRACK_MATCH 0x04
// Schema matches that of the previous type.
//
// Beacon Encounter
// An encounter with a particular ephemeral ID has been detected.
#define TELEM_TYPE_ENCOUNTER 0x05
// Schema:
//
//          <t_d>,<i>,<signal_id>,<beacon_id>,<t_b>,<duration>
//
// Where
// - duration is the number of device time units for which the
//   ID was observed.

//
// Download Telemetry
//

// Periodic Synchronization Lost
#define TELEM_TYPE_PERIODIC_SYNC_LOST 0x06
// Schema:
//
//          <t>
//
// where t is a non-persistent time-stamp for when the loss was
// detected

// Truncated Periodic Packet
#define TELEM_TYPE_PERIODIC_PKT_ERROR 0x07
// Schema:
//
//          <t>
//
// where t is a non-persistent time-stamp for the packet

#define TELEM_TYPE_PERIODIC_PKT_DATA 0x08
// Schema:
//
//          <t>,<rssi>,<len>,<seq?>,<chunk?>
//
// where
// - t is a non-persistent time-stamp for the packet
// - rssi is the receiver RSSI value for the packet
// - len is the length of the packet in bytes
// - seq is the sequence number decoded from the packet, provided
//   that enough data is present
// - chunk is the chunk (filter) number decoded from the packet,
//   provided that enough data is present

#endif
