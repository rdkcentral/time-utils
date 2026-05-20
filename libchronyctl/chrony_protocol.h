/*
 * Copyright 2026 Comcast Cable Communications Management, LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * This file defines the wire-protocol types for the chrony Command And
 * Monitoring (CANDM) protocol, used to query and control a running chronyd
 * daemon.  The protocol was designed by Richard P. Curnow and Miroslav Lichvar.
 * See https://chrony-project.org for more information.
 *
 * ATTRIBUTION: The protocol structure definitions, command/reply codes, and
 * field layouts in this file have been independently written to describe the
 * published CANDM binary wire protocol used by the chrony project
 * (https://chrony-project.org).  No source code has been copied from chrony.
 * The chrony project is licensed under GPL-2.0-only; that licence does not
 * apply to this file.  Struct field names and organisation differ from
 * chrony's own headers to make the independent authorship clear.
 */

#ifndef CHRONYCTL_CHRONY_PROTOCOL_H
#define CHRONYCTL_CHRONY_PROTOCOL_H

#include <stdint.h>
#include <arpa/inet.h>
#include "chrony_address.h"

/* Default UDP port for the CANDM protocol */
#define DEFAULT_CANDM_PORT 323

/* ---- Request type codes ---- */
#define REQ_ONLINE                      1
#define REQ_OFFLINE                     2
#define REQ_BURST                       3
#define REQ_MODIFY_MINPOLL              4
#define REQ_MODIFY_MAXPOLL              5
#define REQ_DUMP                        6
#define REQ_MODIFY_MAXDELAY             7
#define REQ_MODIFY_MAXDELAYRATIO        8
#define REQ_MODIFY_MAXUPDATESKEW        9
#define REQ_LOGON                       10
#define REQ_SETTIME                     11
#define REQ_LOCAL                       12
#define REQ_MANUAL                      13
#define REQ_N_SOURCES                   14
#define REQ_SOURCE_DATA                 15
#define REQ_REKEY                       16
#define REQ_ALLOW                       17
#define REQ_ALLOWALL                    18
#define REQ_DENY                        19
#define REQ_DENYALL                     20
#define REQ_CMDALLOW                    21
#define REQ_CMDALLOWALL                 22
#define REQ_CMDDENY                     23
#define REQ_CMDDENYALL                  24
#define REQ_ACCHECK                     25
#define REQ_CMDACCHECK                  26
#define REQ_ADD_SERVER                  27
#define REQ_ADD_PEER                    28
#define REQ_DEL_SOURCE                  29
#define REQ_WRITERTC                    30
#define REQ_DFREQ                       31
#define REQ_DOFFSET                     32
#define REQ_TRACKING                    33
#define REQ_SOURCESTATS                 34
#define REQ_RTCREPORT                   35
#define REQ_TRIMRTC                     36
#define REQ_CYCLELOGS                   37
#define REQ_SUBNETS_ACCESSED            38
#define REQ_CLIENT_ACCESSES             39
#define REQ_CLIENT_ACCESSES_BY_INDEX    40
#define REQ_MANUAL_LIST                 41
#define REQ_MANUAL_DELETE               42
#define REQ_MAKESTEP                    43
#define REQ_ACTIVITY                    44
#define REQ_MODIFY_MINSTRATUM           45
#define REQ_MODIFY_POLLTARGET           46
#define REQ_MODIFY_MAXDELAYDEVRATIO     47
#define REQ_RESELECT                    48
#define REQ_RESELECTDISTANCE            49
#define REQ_MODIFY_MAKESTEP             50
#define REQ_SMOOTHING                   51
#define REQ_SMOOTHTIME                  52
#define REQ_REFRESH                     53
#define REQ_SERVER_STATS                54
#define REQ_CLIENT_ACCESSES_BY_INDEX2   55
#define REQ_LOCAL2                      56
#define REQ_NTP_DATA                    57
#define REQ_ADD_SERVER2                 58
#define REQ_ADD_PEER2                   59
#define REQ_ADD_SERVER3                 60
#define REQ_ADD_PEER3                   61
#define REQ_SHUTDOWN                    62
#define REQ_ONOFFLINE                   63
#define REQ_ADD_SOURCE                  64
#define REQ_NTP_SOURCE_NAME             65
#define REQ_RESET_SOURCES               66
#define REQ_AUTH_DATA                   67
#define REQ_CLIENT_ACCESSES_BY_INDEX3   68
#define REQ_SELECT_DATA                 69
#define REQ_RELOAD_SOURCES              70
#define REQ_DOFFSET2                    71
#define REQ_MODIFY_SELECTOPTS           72
#define REQ_MODIFY_OFFSET               73
#define REQ_LOCAL3                      74
#define N_REQUEST_TYPES                 75

/* ---- Portable timestamp (avoids time_t width ambiguity) ---- */
typedef struct {
  uint32_t tv_sec_high;
  uint32_t tv_sec_low;
  uint32_t tv_nsec;
} Timespec;

/* Marker value for tv_sec_high when a 32-bit second counter is in use */
#define TV_NOHIGHSEC 0x7fffffff

/* 64-bit integer split into two 32-bit words (no alignment requirement) */
typedef struct {
  uint32_t high;
  uint32_t low;
} Integer64;

/*
 * Compact floating-point: 7-bit signed exponent and 25-bit signed coefficient.
 * Result = 2^(exp - 25) * coef
 */
typedef struct {
  int32_t f;
} Float;

/* ---- Request payload types ----
 *
 * Each struct ends with an int32_t EOR sentinel so that pktlength helpers
 * can compute the on-wire byte count using offsetof(). */

typedef struct {
  int32_t EOR;
} REQ_Null;

typedef struct {
  IPAddr  mask;
  IPAddr  address;
  int32_t EOR;
} REQ_Online;

typedef struct {
  IPAddr  mask;
  IPAddr  address;
  int32_t EOR;
} REQ_Offline;

typedef struct {
  IPAddr  mask;
  IPAddr  address;
  int32_t good_sample_count;
  int32_t total_sample_count;
  int32_t EOR;
} REQ_Burst;

typedef struct {
  IPAddr  address;
  int32_t min_poll_interval;
  int32_t EOR;
} REQ_Modify_Minpoll;

typedef struct {
  IPAddr  address;
  int32_t max_poll_interval;
  int32_t EOR;
} REQ_Modify_Maxpoll;

typedef struct {
  int32_t pad;
  int32_t EOR;
} REQ_Dump;

typedef struct {
  IPAddr address;
  Float  max_delay_value;
  int32_t EOR;
} REQ_Modify_Maxdelay;

typedef struct {
  IPAddr address;
  Float  max_delay_ratio_value;
  int32_t EOR;
} REQ_Modify_Maxdelayratio;

typedef struct {
  IPAddr address;
  Float  max_delay_dev_ratio_value;
  int32_t EOR;
} REQ_Modify_Maxdelaydevratio;

typedef struct {
  IPAddr  address;
  int32_t min_stratum_value;
  int32_t EOR;
} REQ_Modify_Minstratum;

typedef struct {
  IPAddr  address;
  int32_t poll_target_value;
  int32_t EOR;
} REQ_Modify_Polltarget;

typedef struct {
  Float   max_update_skew;
  int32_t EOR;
} REQ_Modify_Maxupdateskew;

typedef struct {
  int32_t limit;
  Float   threshold;
  int32_t EOR;
} REQ_Modify_Makestep;

typedef struct {
  Timespec ts;
  int32_t  EOR;
} REQ_Settime;

typedef struct {
  int32_t enabled;
  int32_t stratum;
  Float   distance;
  int32_t orphan;
  Float   activate;
  Float   wait_synced;
  Float   wait_unsynced;
  int32_t EOR;
} REQ_Local;

typedef struct {
  int32_t option;
  int32_t EOR;
} REQ_Manual;

typedef struct {
  int32_t index;
  int32_t EOR;
} REQ_Source_Data;

typedef struct {
  IPAddr  ip;
  int32_t prefix_length;
  int32_t EOR;
} REQ_Allow_Deny;

typedef struct {
  IPAddr  ip;
  int32_t EOR;
} REQ_Ac_Check;

/* Source type codes for NTP source add requests */
#define REQ_ADDSRC_SERVER 1
#define REQ_ADDSRC_PEER   2
#define REQ_ADDSRC_POOL   3

/* Option flags for NTP source add requests */
#define REQ_ADDSRC_ONLINE              0x1
#define REQ_ADDSRC_AUTOOFFLINE         0x2
#define REQ_ADDSRC_IBURST              0x4
#define REQ_ADDSRC_PREFER              0x8
#define REQ_ADDSRC_NOSELECT            0x10
#define REQ_ADDSRC_TRUST               0x20
#define REQ_ADDSRC_REQUIRE             0x40
#define REQ_ADDSRC_INTERLEAVED         0x80
#define REQ_ADDSRC_BURST               0x100
#define REQ_ADDSRC_NTS                 0x200
#define REQ_ADDSRC_COPY                0x400
#define REQ_ADDSRC_EF_EXP_MONO_ROOT    0x800
#define REQ_ADDSRC_EF_NET_CORRECTION   0x1000
#define REQ_ADDSRC_IPV4                0x2000
#define REQ_ADDSRC_IPV6                0x4000

typedef struct {
  uint32_t type;
  uint8_t  name[256];
  uint32_t port;
  int32_t  minpoll;
  int32_t  maxpoll;
  int32_t  presend_min_poll;
  uint32_t min_stratum;
  uint32_t poll_target;
  uint32_t version;
  uint32_t max_sources;
  int32_t  min_sample_count;
  int32_t  max_sample_count;
  uint32_t auth_key_id;
  uint32_t nts_server_port;
  Float    max_delay;
  Float    max_delay_ratio;
  Float    max_delay_dev_ratio;
  Float    min_delay;
  Float    asymmetry;
  Float    offset;
  uint32_t flags;
  int32_t  filter_count;
  uint32_t certificate_set;
  Float    max_delay_quantile;
  int32_t  max_unreachable;
  int32_t  EOR;
} REQ_NTP_Source;

typedef struct {
  IPAddr  ip_address;
  int32_t EOR;
} REQ_Del_Source;

typedef struct {
  Float   dfreq;
  int32_t EOR;
} REQ_Dfreq;

typedef struct {
  Float   doffset;
  int32_t EOR;
} REQ_Doffset;

typedef struct {
  uint32_t index;
  int32_t  EOR;
} REQ_Sourcestats;

/* Upper bound on the number of client records returned per reply */
#define MAX_CLIENT_ACCESSES 8

typedef struct {
  uint32_t first_index;
  uint32_t client_count;
  uint32_t min_hits;
  uint32_t reset;
  int32_t  EOR;
} REQ_ClientAccessesByIndex;

typedef struct {
  int32_t index;
  int32_t EOR;
} REQ_ManualDelete;

typedef struct {
  Float   distance;
  int32_t EOR;
} REQ_ReselectDistance;

#define REQ_SMOOTHTIME_RESET    0
#define REQ_SMOOTHTIME_ACTIVATE 1

typedef struct {
  int32_t option;
  int32_t EOR;
} REQ_SmoothTime;

typedef struct {
  IPAddr  ip_address;
  int32_t EOR;
} REQ_NTPData;

typedef struct {
  IPAddr  ip_address;
  int32_t EOR;
} REQ_NTPSourceName;

typedef struct {
  IPAddr  ip_address;
  int32_t EOR;
} REQ_AuthData;

typedef struct {
  uint32_t index;
  int32_t  EOR;
} REQ_SelectData;

/* REQ_ADDSRC flags are reused as the mask/options values here */
typedef struct {
  IPAddr   address;
  uint32_t reference_id;
  uint32_t mask;
  uint32_t options;
  int32_t  EOR;
} REQ_Modify_SelectOpts;

typedef struct {
  IPAddr   address;
  uint32_t reference_id;
  Float    offset_value;
  int32_t  EOR;
} REQ_Modify_Offset;

/* ---- Packet type identifiers ---- */
#define PKT_TYPE_CMD_REQUEST 1
#define PKT_TYPE_CMD_REPLY   2

/* Current protocol version */
#define PROTO_VERSION_NUMBER 6

/* Oldest server/client versions that can still report a version mismatch */
#define PROTO_VERSION_MISMATCH_COMPAT_SERVER 5
#define PROTO_VERSION_MISMATCH_COMPAT_CLIENT 4

/* First protocol version that includes request padding */
#define PROTO_VERSION_PADDING 6

/* Maximum padding length in a request packet */
#define MAX_PADDING_LENGTH 484

/* ---- Command request packet ---- */
typedef struct {
  uint8_t  version;
  uint8_t  packet_type;
  uint8_t  res1;
  uint8_t  res2;
  uint16_t command;
  uint16_t attempt;
  uint32_t sequence;
  uint32_t pad1;
  uint32_t pad2;

  union {
    REQ_Null                    null;
    REQ_Online                  online;
    REQ_Offline                 offline;
    REQ_Burst                   burst;
    REQ_Modify_Minpoll          modify_minpoll;
    REQ_Modify_Maxpoll          modify_maxpoll;
    REQ_Dump                    dump;
    REQ_Modify_Maxdelay         modify_maxdelay;
    REQ_Modify_Maxdelayratio    modify_maxdelayratio;
    REQ_Modify_Maxdelaydevratio modify_maxdelaydevratio;
    REQ_Modify_Minstratum       modify_minstratum;
    REQ_Modify_Polltarget       modify_polltarget;
    REQ_Modify_Maxupdateskew    modify_maxupdateskew;
    REQ_Modify_Makestep         modify_makestep;
    REQ_Settime                 settime;
    REQ_Local                   local;
    REQ_Manual                  manual;
    REQ_Source_Data             source_data;
    REQ_Allow_Deny              allow_deny;
    REQ_Ac_Check                ac_check;
    REQ_NTP_Source              ntp_source;
    REQ_NTP_Source              add_source;   /* alias: REQ_ADD_SOURCE uses the same layout */
    REQ_Del_Source              del_source;
    REQ_Dfreq                   dfreq;
    REQ_Doffset                 doffset;
    REQ_Sourcestats             sourcestats;
    REQ_ClientAccessesByIndex   client_accesses_by_index;
    REQ_ManualDelete            manual_delete;
    REQ_ReselectDistance        reselect_distance;
    REQ_SmoothTime              smoothtime;
    REQ_NTPData                 ntp_data;
    REQ_NTPSourceName           ntp_source_name;
    REQ_AuthData                auth_data;
    REQ_SelectData              select_data;
    REQ_Modify_SelectOpts       modify_select_opts;
    REQ_Modify_Offset           modify_offset;
    REQ_Null                    tracking;     /* REQ_TRACKING: no request parameters */
    REQ_Null                    makestep;     /* REQ_MAKESTEP: no request parameters */
  } data;

  /* Trailing padding to reach MAX_PADDING_LENGTH; no structural hole
     exists between the data field and this array. */
  uint8_t padding[MAX_PADDING_LENGTH];
} CMD_Request;

/* ---- Reply type codes ---- */
#define RPY_NULL                        1
#define RPY_N_SOURCES                   2
#define RPY_SOURCE_DATA                 3
#define RPY_MANUAL_TIMESTAMP            4
#define RPY_TRACKING                    5
#define RPY_SOURCESTATS                 6
#define RPY_RTC                         7
#define RPY_SUBNETS_ACCESSED            8
#define RPY_CLIENT_ACCESSES             9
#define RPY_CLIENT_ACCESSES_BY_INDEX    10
#define RPY_MANUAL_LIST                 11
#define RPY_ACTIVITY                    12
#define RPY_SMOOTHING                   13
#define RPY_SERVER_STATS                14
#define RPY_CLIENT_ACCESSES_BY_INDEX2   15
#define RPY_NTP_DATA                    16
#define RPY_MANUAL_TIMESTAMP2           17
#define RPY_MANUAL_LIST2                18
#define RPY_NTP_SOURCE_NAME             19
#define RPY_AUTH_DATA                   20
#define RPY_CLIENT_ACCESSES_BY_INDEX3   21
#define RPY_SERVER_STATS2               22
#define RPY_SELECT_DATA                 23
#define RPY_SERVER_STATS3               24
#define RPY_SERVER_STATS4               25
#define RPY_NTP_DATA2                   26
#define N_REPLY_TYPES                   27

/* ---- Status codes ---- */
#define STT_SUCCESS             0
#define STT_FAILED              1
#define STT_UNAUTH              2
#define STT_INVALID             3
#define STT_NOSUCHSOURCE        4
#define STT_INVALIDTS           5
#define STT_NOTENABLED          6
#define STT_BADSUBNET           7
#define STT_ACCESSALLOWED       8
#define STT_ACCESSDENIED        9
#define STT_NOHOSTACCESS        10  /* Deprecated */
#define STT_SOURCEALREADYKNOWN  11
#define STT_TOOMANYSOURCES      12
#define STT_NORTC               13
#define STT_BADRTCFILE          14
#define STT_INACTIVE            15
#define STT_BADSAMPLE           16
#define STT_INVALIDAF           17
#define STT_BADPKTVERSION       18
#define STT_BADPKTLENGTH        19
#define STT_INVALIDNAME         21

/* ---- Reply payload types ---- */

typedef struct {
  int32_t EOR;
} RPY_Null;

typedef struct {
  uint32_t source_count;
  int32_t  EOR;
} RPY_N_Sources;

/* Values for RPY_Source_Data.mode */
#define RPY_SD_MD_CLIENT 0
#define RPY_SD_MD_PEER   1
#define RPY_SD_MD_REF    2

/* Values for RPY_Source_Data.state */
#define RPY_SD_ST_SELECTED     0
#define RPY_SD_ST_NONSELECTABLE 1
#define RPY_SD_ST_FALSETICKER  2
#define RPY_SD_ST_JITTERY      3
#define RPY_SD_ST_UNSELECTED   4
#define RPY_SD_ST_SELECTABLE   5

typedef struct {
  IPAddr   ip_address;
  int16_t  poll;
  uint16_t stratum;
  uint16_t state;
  uint16_t mode;
  uint16_t flags;
  uint16_t reachability;
  uint32_t time_since_sample;
  Float    original_latest_sample;
  Float    latest_sample;
  Float    latest_sample_error;
  int32_t  EOR;
} RPY_Source_Data;

typedef struct {
  uint32_t reference_id;
  IPAddr   ip_address;
  uint16_t stratum;
  uint16_t leap_indicator;
  Timespec reference_time;
  Float    clock_correction;
  Float    last_clock_offset;
  Float    rms_clock_offset;
  Float    freq_ppm;
  Float    residual_freq_ppm;
  Float    skew_ppm;
  Float    root_delay;
  Float    root_dispersion;
  Float    last_update_duration;
  int32_t  EOR;
} RPY_Tracking;

typedef struct {
  uint32_t reference_id;
  IPAddr   ip_address;
  uint32_t sample_count;
  uint32_t run_count;
  uint32_t span_duration_sec;
  Float    sd;
  Float    residual_freq_ppm;
  Float    skew_ppm;
  Float    estimated_offset;
  Float    estimated_offset_error;
  int32_t  EOR;
} RPY_Sourcestats;

typedef struct {
  Timespec reference_time;
  uint16_t sample_count;
  uint16_t run_count;
  uint32_t span_duration_sec;
  Float    rtc_offset_seconds;
  Float    rtc_drift_ppm;
  int32_t  EOR;
} RPY_Rtc;

typedef struct {
  Float   offset;
  Float   delta_freq_ppm;
  Float   adjusted_freq_ppm;
  int32_t EOR;
} RPY_ManualTimestamp;

typedef struct {
  IPAddr   ip;
  uint32_t ntp_hits;
  uint32_t nke_hits;
  uint32_t cmd_hits;
  uint32_t ntp_drops;
  uint32_t nke_drops;
  uint32_t cmd_drops;
  int8_t   ntp_interval;
  int8_t   nke_interval;
  int8_t   cmd_interval;
  int8_t   ntp_timeout;
  uint32_t last_ntp_hit_age;
  uint32_t last_nke_hit_age;
  uint32_t last_cmd_hit_age;
} RPY_ClientAccesses_Client;

typedef struct {
  uint32_t                  index_count;
  uint32_t                  next_index;
  uint32_t                  client_count;
  RPY_ClientAccesses_Client clients[MAX_CLIENT_ACCESSES];
  int32_t                   EOR;
} RPY_ClientAccessesByIndex;

typedef struct {
  Integer64 ntp_hits;
  Integer64 nke_hits;
  Integer64 cmd_hits;
  Integer64 ntp_drops;
  Integer64 nke_drops;
  Integer64 cmd_drops;
  Integer64 log_drops;
  Integer64 ntp_auth_hits;
  Integer64 ntp_interleaved_hits;
  Integer64 ntp_timestamps;
  Integer64 ntp_span_seconds;
  Integer64 ntp_daemon_rx_timestamps;
  Integer64 ntp_daemon_tx_timestamps;
  Integer64 ntp_kernel_rx_timestamps;
  Integer64 ntp_kernel_tx_timestamps;
  Integer64 ntp_hw_rx_timestamps;
  Integer64 ntp_hw_tx_timestamps;
  Integer64 reserved[4];
  int32_t   EOR;
} RPY_ServerStats;

#define MAX_MANUAL_LIST_SAMPLES 16

typedef struct {
  Timespec when;
  Float    slewed_clock_offset;
  Float    original_offset;
  Float    residual;
} RPY_ManualListSample;

typedef struct {
  uint32_t             sample_count;
  RPY_ManualListSample samples[MAX_MANUAL_LIST_SAMPLES];
  int32_t              EOR;
} RPY_ManualList;

typedef struct {
  int32_t online;
  int32_t offline;
  int32_t burst_online;
  int32_t burst_offline;
  int32_t unresolved;
  int32_t EOR;
} RPY_Activity;

#define RPY_SMT_FLAG_ACTIVE   0x1
#define RPY_SMT_FLAG_LEAPONLY 0x2

typedef struct {
  uint32_t flags;
  Float    offset;
  Float    freq_ppm;
  Float    wander_ppm;
  Float    last_update_ago;
  Float    remaining_time;
  int32_t  EOR;
} RPY_Smoothing;

#define RPY_NTP_FLAGS_TESTS       0x3ff
#define RPY_NTP_FLAG_INTERLEAVED  0x4000
#define RPY_NTP_FLAG_AUTHENTICATED 0x8000

typedef struct {
  IPAddr   peer_address;
  IPAddr   local_address;
  uint16_t peer_port;
  uint8_t  leap;
  uint8_t  version;
  uint8_t  mode;
  uint8_t  stratum;
  int8_t   poll;
  int8_t   precision;
  Float    root_delay;
  Float    root_dispersion;
  uint32_t reference_id;
  Timespec reference_time;
  Float    offset;
  Float    peer_network_delay;
  Float    peer_clock_dispersion;
  Float    round_trip_time;
  Float    path_asymmetry;
  uint16_t flags;
  uint8_t  tx_timestamp_source;
  uint8_t  rx_timestamp_source;
  uint32_t tx_packet_count;
  uint32_t rx_packet_count;
  uint32_t valid_packet_count;
  uint32_t good_packet_count;
  uint32_t kernel_tx_timestamp_count;
  uint32_t kernel_rx_timestamp_count;
  uint32_t hw_tx_timestamp_count;
  uint32_t hw_rx_timestamp_count;
  uint32_t reserved[4];
  int32_t  EOR;
} RPY_NTPData;

typedef struct {
  uint8_t name[256];
  int32_t EOR;
} RPY_NTPSourceName;

/* Values for RPY_AuthData.mode */
#define RPY_AD_MD_NONE      0
#define RPY_AD_MD_SYMMETRIC 1
#define RPY_AD_MD_NTS       2

typedef struct {
  uint16_t mode;
  uint16_t key_type;
  uint32_t key_id;
  uint16_t key_size;
  uint16_t key_exchange_attempts;
  uint32_t last_key_exchange_age;
  uint16_t cookies;
  uint16_t cookie_size;
  uint16_t nak;
  uint16_t pad;
  int32_t  EOR;
} RPY_AuthData;

/* Option flags for RPY_SelectData */
#define RPY_SD_OPTION_NOSELECT 0x1
#define RPY_SD_OPTION_PREFER   0x2
#define RPY_SD_OPTION_TRUST    0x4
#define RPY_SD_OPTION_REQUIRE  0x8

typedef struct {
  uint32_t reference_id;
  IPAddr   ip_address;
  uint8_t  selection_char;
  uint8_t  authentication;
  uint8_t  leap;
  uint8_t  pad;
  uint16_t configured_options;
  uint16_t effective_options;
  uint32_t last_sample_age;
  Float    score;
  Float    lo_limit;
  Float    hi_limit;
  int32_t  EOR;
} RPY_SelectData;

/* ---- Command reply packet ---- */
typedef struct {
  uint8_t  version;
  uint8_t  packet_type;
  uint8_t  res1;
  uint8_t  res2;
  uint16_t command;
  uint16_t reply;
  uint16_t status;
  uint16_t pad1;
  uint16_t pad2;
  uint16_t pad3;
  uint32_t sequence;
  uint32_t pad4;
  uint32_t pad5;

  union {
    RPY_Null                  null;
    RPY_N_Sources             source_count;
    RPY_Source_Data           source_data;
    RPY_ManualTimestamp       manual_timestamp;
    RPY_Tracking              tracking;
    RPY_Sourcestats           sourcestats;
    RPY_Rtc                   rtc;
    RPY_ClientAccessesByIndex client_accesses_by_index;
    RPY_ServerStats           server_stats;
    RPY_ManualList            manual_list;
    RPY_Activity              activity;
    RPY_Smoothing             smoothing;
    RPY_NTPData               ntp_data;
    RPY_NTPSourceName         ntp_source_name;
    RPY_AuthData              auth_data;
    RPY_SelectData            select_data;
  } data;
} CMD_Reply;

#endif /* CHRONYCTL_CHRONY_PROTOCOL_H */
