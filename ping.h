/*
 * Copyright (c) 2012 Institute of Geological & Nuclear Sciences Ltd.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *		notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *		notice, this list of conditions and the following disclaimer in the
 *		documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef _PING_H_
#define _PING_H_

#define QDP_VERSION 2
#define QDP_CRCSIZE 4
#define QDP_HEDSIZE 12
#define QDP_MAXSIZE 536
#define QDP_MAXDATA (QDP_MAXSIZE-4)
#define QDP_OFFSET 946684800

#ifndef CRC_POLYNOMIAL
#define CRC_POLYNOMIAL 1443300200
#endif /* CRC_POLYNOMIAL */

#define C1_PING 0x38

#define ST_GLOBAL 0
#define ST_GPS    1
#define ST_PWR    2
#define ST_BOOM   3
#define ST_THREAD 4
#define ST_PLL    5
#define ST_SATS   6
#define ST_ARP    7
#define ST_LP1    8
#define ST_LP2    9
#define ST_LP3    10
#define ST_LP4    11
#define ST_SP1    12
#define ST_SP2    13
#define ST_SP3    14
#define ST_ETH    15

#define CQ_LOCK 1 /* has been locked, else internal time */
#define CQ_2D   2 /* 2d lock */
#define CQ_3D   4 /* 3d lock */
#define CQ_1D   8 /* no fix, but should have time */
#define CQ_FILT 0x10 /* filtering in progress */

#ifndef PLL_OFF
#define PLL_OFF   0 /* not on */
#endif //PLL_OFF
#ifndef PLL_HOLD
#define PLL_HOLD  0x40
#endif // PLL_HOLD
#ifndef PLL_TRACK
#define PLL_TRACK 0x80
#endif // PLL_TRACK
#ifndef PLL_LOCK
#define PLL_LOCK  0xc0
#endif // PLL_LOCK

typedef struct qdp_ping_status {
	u_int16_t drifttol;
	u_int16_t usermsgcnt;
	u_int32_t lastreboot;
	u_int32_t spare[2];
	u_int32_t bitmap;
} qdp_ping_status;

typedef struct qdp_ping_info {
	u_int16_t version;
	u_int16_t flags;
	u_int32_t kmi;
	u_int32_t serial_high;
	u_int32_t serial_low;
	u_int32_t memory[8];
	u_int16_t interface[8];
	u_int16_t calerr;
	u_int16_t sysver;
} qdp_ping_info;

typedef struct qdp_stat_global {
  u_int16_t aqctr; /* acquisition control */
  u_int16_t clock_qual; /* current clock quality */
  u_int16_t clock_loss; /* current clock loss */
  u_int16_t current_voltage; /* current amb dac control value */
  u_int32_t sec_offset; /* seconds offset + data seq + 2000 = time */
  u_int32_t usec_offset; /* usec offset for data */
  u_int32_t total_time; /* total time in seconds */
  u_int32_t total_power; /* total power on time in seconds */
  u_int32_t last_resync; /* time of last resync */
  u_int32_t resyncs; /* total number of resyncs */
  u_int16_t gps_stat; /* gps status */
  u_int16_t cal_stat; /* calibrator status */
  u_int16_t sensor_map; /* sensor control bitmap */
  u_int16_t cur_vco; /* current vco value */
  u_int16_t data_seq; /* data sequence number */
  u_int16_t pll_flag; /* pll enabled */
  u_int16_t stat_inp; /* status inputs */
  u_int16_t misc_inp; /* misc. inputs */
  u_int32_t cur_sequence; /* latest digitizer sequence */
} qdp_stat_global;

typedef struct qdp_stat_gps {
  u_int16_t gpstime; /* gps power on/off time in seconds */
  u_int16_t gpson; /* gps on if non-zero */
  u_int16_t sat_used; /* number of satellites used */
  u_int16_t sat_view; /* number of satellites in view */
  char time[10]; /* gps time - pascal format */
  char date[12]; /* gps date - pascal format */
  char fix[6];  /* gps fix height - pascal format */
  char height[12]; /* gps height - pascal format */
  char lat[14]; /* gps latitude - pascal format */
  char lon[14]; /* gps longitude - pascal format */
  u_int32_t last_good; /* time of last good 1pps */
  u_int32_t check_err; /* checksum errors */
} qdp_stat_gps;

#define CHRG_NOT    0 /* not charging */
#define CHRG_BULK   1 /* bulk */
#define CHRG_ABS    2 /* absorption */
#define CHRG_FLOAT  3 /* float */

typedef struct qdp_stat_pwr {
  u_int16_t phase; /* charging phase */
  int16_t battemp; /* battery temperature */
  u_int16_t capacity; /* battery capacity */
  u_int16_t depth; /* depth of discharge */
  u_int16_t batvolt; /* battery voltage */
  u_int16_t inpvolt; /* input voltage */
  int16_t batcur; /* battery current */
  u_int16_t absorption; /* absorption setpoint */
  u_int16_t float_; /* float setpoint */
  u_int16_t spare;
} qdp_stat_pwr;

typedef struct qdp_stat_boom {
  int16_t booms[6];
  u_int16_t amb_pos; /* analog mother board positive - 10mv */
  u_int16_t amb_neg; /* analog mother board positive - 10mv */
  u_int16_t supply; /* input voltage - 150mv */
  int16_t sys_temp; /* system temperature - celsius */
  int16_t main_cur; /* main current - 1 ma */
  int16_t ant_cur; /* gps antenna current - 1 ma */
  int16_t seis1_temp; /* seismo 1 temperature - celsius */
  int16_t seis2_temp; /* seismo 2 temperature - celsius */
  u_int32_t cal_timeouts; /* calibrator timeouts */
} qdp_stat_boom;

typedef struct qdp_stat_log {
  u_int32_t sent; /* total data packets sent */
  u_int32_t resends; /* total packets re-sent */
  u_int32_t fill; /* total fill packets sent */
  u_int32_t seq; /* receive sequence errors */
  u_int32_t pack_used; /* bytes of packet buffer used */
  u_int32_t last_ack; /* time of last packet acked */
  u_int16_t phy_num; /* physical port number used */
  u_int16_t log_num; /* logical port we are reporting */
  u_int16_t retran; /* retransmission timer */
  u_int16_t spare;
} qdp_stat_log;

typedef struct qdp_stat_ether {
  u_int32_t check; /* receive checksum errors */
  u_int32_t ioerrors; /* total i/o errors */
  u_int16_t phy_num; /* physical port we are reporting */
  u_int16_t spare;
  u_int32_t unreach; /* destination unreachable icmp packets received */
  u_int32_t quench; /* source quench icmp packets received */
  u_int32_t echo; /* echo request icmp packets received */
  u_int32_t redirect; /* redirect packets received */
  u_int32_t runt; /* total runt frames */
  u_int32_t crc_err; /* crc errors */
  u_int32_t bcast; /* broadcast frames */
  u_int32_t ucast; /* unicast frames */
  u_int32_t good; /* good frames */
  u_int32_t jabber; /* jabber errors */
  u_int32_t outwin; /* out the window */
  u_int32_t txok; /* transmit okay */
  u_int32_t miss; /* receive packets missed */
  u_int32_t collide; /* transmit collisions */
  u_int16_t linkstat; /* link status */
  u_int16_t spare2;
  u_int32_t spare3;
} qdp_stat_ether;

typedef struct _ping {
	int32_t crc; /* over all the packet */
	u_int8_t command; /* command */
	u_int8_t version; /* version */
	u_int16_t datalength; /* not including header */
	u_int16_t sequence; /* sender's sequence */
	u_int16_t acknowledge; /* and acknowledge */
	u_int16_t ping_type;
	u_int16_t ping_id;
	union {
		u_int32_t data;
		u_int32_t bitmap;
		qdp_ping_info info;
		qdp_ping_status status;
		u_int8_t raw[QDP_MAXDATA]; /* raw bytes */
	} data;
	qdp_stat_global global;
	qdp_stat_gps gps;
	qdp_stat_pwr pwr; /* not used */
	qdp_stat_boom boom;
	qdp_stat_log lport[4];
	qdp_stat_ether ether;
} qdp;

//int make_qdp_socket(void);
//int qdp_clock_quality(qdp *ping);
//int qdp_gps_datetime(qdp *qdp, time_t *datetime);
//int qdp_gps_height(qdp *qdp, double *hgt);
//int qdp_gps_latitude(qdp *qdp, double *lat);
//int qdp_gps_longitude(qdp *qdp, double *lon);

int send_qdp_ping(int sockfd, char *ipaddr, int ipport, int serial);
int recv_qdp_ping(int sockfd, qdp *ping); // char **ipaddr, int *ipport);

#endif // _PING_H_
