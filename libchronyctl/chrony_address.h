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
 * This file provides address representation types used when communicating
 * with a chrony daemon over its command and monitoring (CANDM) protocol.
 * The chrony protocol was designed by Richard P. Curnow and Miroslav Lichvar.
 * See https://chrony-project.org for more information.
 */

#ifndef CHRONYCTL_CHRONY_ADDRESS_H
#define CHRONYCTL_CHRONY_ADDRESS_H

#include <stdint.h>
#include <arpa/inet.h>
#include <sys/types.h>

/* Values for the family field of IPAddr */
#define IPADDR_UNSPEC 0
#define IPADDR_INET4  1
#define IPADDR_INET6  2
#define IPADDR_ID     3

/*
 * Protocol address container.  Holds an IPv4 address, an IPv6 address, or an
 * unresolved-source numeric ID.  All multi-byte quantities are stored in host
 * byte order.
 */
typedef struct {
  union {
    uint32_t in4;
    uint8_t  in6[16];
    uint32_t id;
  } addr;
  uint16_t family;
  uint16_t _pad;
} IPAddr;

/* Pairs an IP address with a UDP port number */
typedef struct {
  IPAddr   ip_address;
  uint16_t port;
} IPSockAddr;

/* Alias for the remote NTP peer address */
typedef IPSockAddr NTP_Remote_Address;

/* Sentinel meaning no network interface has been selected */
#define INVALID_IF_INDEX (-1)

/* Identifies the local socket used for an outgoing NTP exchange */
typedef struct {
  IPAddr ip_address;
  int    interface_index;
  int    socket_fd;
} NTP_Local_Address;

#endif /* CHRONYCTL_CHRONY_ADDRESS_H */
