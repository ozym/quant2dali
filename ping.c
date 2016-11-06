/* system includes */
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <byteswap.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <math.h>

#include "libmseed.h"
#include "ping.h"

#define swap16(x) __bswap_16((x));
#define swap32(x) __bswap_32((x));

static u_int32_t qdp_calc_crc (char *b, int len) {
  u_int16_t count, bits ;
  int32_t tdata, accum ;
  static u_int32_t crc_init = 0;
  static u_int32_t crc_table[256] ;

  union {
    unsigned char b[4];
    signed char sb[4];
    u_int16_t s[2];
    u_int32_t l;
    float f;
  } crc;

  if (!crc_init) {
    for (count = 0 ; count < 256 ; count++) {
      tdata = ((u_int32_t) count) << 24 ;
      accum = 0 ;
      for (bits = 1 ; bits <= 8 ; bits++) {
        if ((tdata ^ accum) < 0)
          accum = (accum << 1) ^ CRC_POLYNOMIAL;
        else
          accum = (accum << 1) ;
        tdata = tdata << 1 ;
      }
      crc_table[count] = accum ;
    }
    crc_init = 1;
  }

  crc.l = 0 ;
  while (len-- > 0) {
#ifndef WORDS_BIGENDIAN
    crc.l = (crc.l << 8) ^ crc_table[(crc.b[3] ^ *b++) & 255] ;
#else
    crc.l = (crc.l << 8) ^ crc_table[(crc.b[0] ^ *b++) & 255] ;
#endif
  }

  return crc.l ;
}

int send_qdp_ping(int sockfd, char *ipaddr, int ipport, int serial) {
  static int seqno = 1;
  struct sockaddr_in sin;

  int status;
  struct addrinfo hints, *res;

  qdp ping;
  int len = 0;

  memset(&ping, 0, sizeof(qdp));

  ping.crc = 0;
  ping.command = C1_PING;
  ping.version = QDP_VERSION;
  ping.datalength = (serial) ? 4 : 8;
  ping.sequence = seqno++;
  ping.acknowledge = 0;
  ping.ping_type = (serial) ? 4 : 2;
  ping.ping_id = 0;
  ping.data.bitmap = (serial) ? 0x0000 : 0x8f0b;

  len = QDP_HEDSIZE + ping.datalength;

#ifdef DEBUG
fprintf(stderr, "crc %u (%u)\n", ping.crc, qdp_calc_crc((char *) &ping.crc + 4, 12));
fprintf(stderr, "cmd %u\n", ping.command);
fprintf(stderr, "ver %u\n", ping.version);
fprintf(stderr, "dat %u\n", ping.datalength);
fprintf(stderr, "seq %u\n", ping.sequence);
fprintf(stderr, "ack %u\n", ping.acknowledge);
fprintf(stderr, "typ %u\n", ping.ping_type);
fprintf(stderr, "id  %u\n", ping.ping_id);
fprintf(stderr, "bit %x\n", ping.data.bitmap);
fprintf(stderr, "len %d\n", len);
#endif // DEBUG

#ifndef WORDS_BIGENDIAN
  ping.datalength = swap16(ping.datalength);
  ping.sequence = swap16(ping.sequence);
  ping.acknowledge = swap16(ping.acknowledge);
  ping.ping_type = swap16(ping.ping_type);
  ping.ping_id = swap16(ping.ping_id);
  ping.data.bitmap = swap32(ping.data.bitmap);
#endif /* WORDS_BIGENDIAN */

  ping.crc = qdp_calc_crc(&ping.command, len - QDP_CRCSIZE);

#ifndef WORDS_BIGENDIAN
  ping.crc = swap32(ping.crc);
#endif /* WORDS_BIGENDIAN */

#ifdef DEBUG
fprintf(stderr, "crc %u\n", ping.crc);
fprintf(stderr, "cmd %u\n", ping.command);
fprintf(stderr, "ver %u\n", ping.version);
fprintf(stderr, "dat %u\n", ping.datalength);
fprintf(stderr, "seq %u\n", ping.sequence);
fprintf(stderr, "ack %u\n", ping.acknowledge);
fprintf(stderr, "typ %u\n", ping.ping_type);
fprintf(stderr, "id  %u\n", ping.ping_id);
fprintf(stderr, "bit %x\n", ping.data.bitmap);
fprintf(stderr, "len %d\n", len);
#endif // DEBUG

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC; // AF_INET or AF_INET6 to force version
  hints.ai_socktype = SOCK_STREAM;

  if ((status = getaddrinfo(ipaddr, NULL, &hints, &res)) != 0) {
    ms_log(1, "getaddrinfo: %s\n", gai_strerror(status)); return -2;
  }

  memset((char *) &sin, 0, sizeof(struct sockaddr_in));
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = ((struct sockaddr_in *)(res->ai_addr))->sin_addr.s_addr;
  sin.sin_port = htons(ipport);
  
#ifdef DEBUG
fprintf(stderr, "ip address : %s\n", inet_ntoa(sin.sin_addr));
#endif // DEBUG

  if (sendto(sockfd, (char *) &ping, len, 0, (struct sockaddr *) &sin, sizeof(struct sockaddr_in)) != len)
    return -1;

  return 0;
}

static char *decode_qdp_string(char *string) {
  int i, n;

  n = (int)(*string);
  for (i = 0; i < n; i++) {
    string[i] = string[i+1];
  }
  string[n] = '\0';

  return string;
}

static int decode_qdp_status(qdp *ping) {
  int i, j;
  int offset = 20;

  memset(&ping->global, 0, sizeof(qdp_stat_global));
  memset(&ping->gps, 0, sizeof(qdp_stat_gps));
  memset(&ping->ether, 0, sizeof(qdp_stat_ether));
  memset(&ping->boom, 0, sizeof(qdp_stat_boom));
  for (j = 0; j < 6; j++) {
    memset(&ping->lport[j], 0, sizeof(qdp_stat_log));
  }

  for (i = 0; i < 16; i++) {
    if (ping->data.status.bitmap & (0x01 << i)) {
      switch(i) {
      case ST_GLOBAL:
        memcpy(&ping->global, &ping->data.raw[offset], sizeof(qdp_stat_global));
#ifndef WORDS_BIGENDIAN
        ping->global.aqctr = swap16(ping->global.aqctr); /* acquisition control */
        ping->global.clock_qual = swap16(ping->global.clock_qual); /* current clock quality */
        ping->global.clock_loss = swap16(ping->global.clock_loss); /* current clock loss */
        ping->global.current_voltage = swap16(ping->global.current_voltage); /* current amb dac control value */
        ping->global.sec_offset = swap32(ping->global.sec_offset); /* seconds offset + data seq + 2000 = time */
        ping->global.usec_offset = swap32(ping->global.usec_offset); /* usec offset for data */
        ping->global.total_time = swap32(ping->global.total_time); /* total time in seconds */
        ping->global.total_power = swap32(ping->global.total_power); /* total power on time in seconds */
        ping->global.last_resync = swap32(ping->global.last_resync); /* time of last resync */
        ping->global.resyncs = swap32(ping->global.resyncs); /* total number of resyncs */
        ping->global.gps_stat = swap16(ping->global.gps_stat); /* gps status */
        ping->global.cal_stat = swap16(ping->global.cal_stat); /* calibrator status */
        ping->global.sensor_map = swap16(ping->global.sensor_map); /* sensor control bitmap */
        ping->global.cur_vco = swap16(ping->global.cur_vco); /* current vco value */
        ping->global.data_seq = swap16(ping->global.data_seq); /* data sequence number */
        ping->global.pll_flag = swap16(ping->global.pll_flag); /* pll enabled */
        ping->global.stat_inp = swap16(ping->global.stat_inp); /* status inputs */
        ping->global.misc_inp = swap16(ping->global.misc_inp); /* misc. inputs */
        ping->global.cur_sequence = swap32(ping->global.cur_sequence); /* latest digitizer sequence */
#endif /* WORDS_BIGENDIAN */
        offset += sizeof(qdp_stat_global);
        break;
      case ST_GPS:
        memcpy(&ping->gps, &ping->data.raw[offset], sizeof(qdp_stat_gps));
#ifndef WORDS_BIGENDIAN
        ping->gps.gpstime = swap16(ping->gps.gpstime); /* gps power on/off time in seconds */
        ping->gps.gpson = swap16(ping->gps.gpson); /* gps on if non-zero */
        ping->gps.sat_used = swap16(ping->gps.sat_used); /* number of satellites used */
        ping->gps.sat_view = swap16(ping->gps.sat_view); /* number of satellites in view */
        ping->gps.last_good = swap32(ping->gps.last_good); /* time of last good 1pps */
        ping->gps.check_err = swap32(ping->gps.check_err); /* checksum errors */
#endif /* WORDS_BIGENDIAN */
        decode_qdp_string(ping->gps.time);
        decode_qdp_string(ping->gps.date);
        decode_qdp_string(ping->gps.fix);
        decode_qdp_string(ping->gps.height);
        decode_qdp_string(ping->gps.lat);
        decode_qdp_string(ping->gps.lon);
        offset += sizeof(qdp_stat_gps);
        break;
      case ST_BOOM:
        memcpy(&ping->boom, &ping->data.raw[offset], sizeof(qdp_stat_boom));
#ifndef WORDS_BIGENDIAN
        for (j = 0; j < 6; j++) {
          ping->boom.booms[j] = swap16(ping->boom.booms[j]);
        }
        ping->boom.amb_pos = swap16(ping->boom.amb_pos); /* analog mother board positive - 10mv */
        ping->boom.amb_neg = swap16(ping->boom.amb_neg); /* analog mother board positive - 10mv */
        ping->boom.supply = swap16(ping->boom.supply); /* input voltage - 150mv */
        ping->boom.sys_temp = swap16(ping->boom.sys_temp); /* system temperature - celsius */
        ping->boom.main_cur = swap16(ping->boom.main_cur); /* main current - 1 ma */
        ping->boom.ant_cur = swap16(ping->boom.ant_cur); /* gps antenna current - 1 ma */
        ping->boom.seis1_temp = swap16(ping->boom.seis1_temp); /* seismo 1 temperature - celsius */
        ping->boom.seis2_temp = swap16(ping->boom.seis2_temp); /* seismo 2 temperature - celsius */
        ping->boom.cal_timeouts = swap32(ping->boom.cal_timeouts); /* calibrator timeouts */
#endif /* WORDS_BIGENDIAN */
        offset += sizeof(qdp_stat_boom);
        break;
      case ST_LP1:
        j = 0;
      case ST_LP2:
        j = 1;
      case ST_LP3:
        j = 2;
      case ST_LP4:
        j = 3;
        memcpy(&ping->lport[j], &ping->data.raw[offset], sizeof(qdp_stat_log));
#ifndef WORDS_BIGENDIAN
        ping->lport[j].sent = swap32(ping->lport[j].sent); /* total data packets sent */
        ping->lport[j].resends = swap32(ping->lport[j].resends); /* total packets re-sent */
        ping->lport[j].fill = swap32(ping->lport[j].fill); /* total fill packets sent */
        ping->lport[j].seq = swap32(ping->lport[j].seq); /* receive sequence errors */
        ping->lport[j].pack_used = swap32(ping->lport[j].pack_used); /* bytes of packet buffer used */
        ping->lport[j].last_ack = swap32(ping->lport[j].last_ack); /* time of last packet acked */
        ping->lport[j].phy_num = swap16(ping->lport[j].phy_num); /* physical port number used */
        ping->lport[j].log_num = swap16(ping->lport[j].log_num); /* logical port we are reporting */
        ping->lport[j].retran = swap16(ping->lport[j].retran); /* retransmission timer */
#endif /* WORDS_BIGENDIAN */
        offset += sizeof(qdp_stat_log);
        break;
      case ST_ETH:
        memcpy(&ping->ether, &ping->data.raw[offset], sizeof(qdp_stat_ether));
#ifndef WORDS_BIGENDIAN
        ping->ether.check = swap32(ping->ether.check); /* receive checksum errors */
        ping->ether.ioerrors = swap32(ping->ether.ioerrors); /* total i/o errors */
        ping->ether.phy_num = swap16(ping->ether.phy_num); /* physical port we are reporting */
        ping->ether.unreach = swap32(ping->ether.unreach); /* destination unreachable icmp packets received */
        ping->ether.quench = swap32(ping->ether.quench); /* source quench icmp packets received */
        ping->ether.echo = swap32(ping->ether.echo); /* echo request icmp packets received */
        ping->ether.redirect = swap32(ping->ether.redirect); /* redirect packets received */
        ping->ether.runt = swap32(ping->ether.runt); /* total runt frames */
        ping->ether.crc_err = swap32(ping->ether.crc_err); /* crc errors */
        ping->ether.bcast = swap32(ping->ether.bcast); /* broadcast frames */
        ping->ether.ucast = swap32(ping->ether.ucast); /* unicast frames */
        ping->ether.good = swap32(ping->ether.good); /* good frames */
        ping->ether.jabber = swap32(ping->ether.jabber); /* jabber errors */
        ping->ether.outwin = swap32(ping->ether.outwin); /* out the window */
        ping->ether.txok = swap32(ping->ether.txok); /* transmit okay */
        ping->ether.miss = swap32(ping->ether.miss); /* receive packets missed */
        ping->ether.collide = swap32(ping->ether.collide); /* transmit collisions */
        ping->ether.linkstat = swap16(ping->ether.linkstat); /* link status */
#endif /* WORDS_BIGENDIAN */
        offset += sizeof(qdp_stat_boom);
        break;
      default:
        ms_log(2, "error: unknown status messaage [%d]\n", i); exit(-1);
      }
    }
  }

  return 0;
}

int recv_qdp_ping(int sockfd, qdp *ping) { // char **ipaddr, int *ipport) {
  int i;
  int len;

  int slen;
  int norecv;
  struct sockaddr_in sin;
  slen = sizeof(sin);

  memset(ping, sizeof(qdp), 0);

  do {

    /* now pull in a block ... */
    if ((norecv = recvfrom(sockfd, (char *) ping, sizeof(qdp), 0, (struct sockaddr *) &sin, &slen)) < 0) {
      return((errno == EWOULDBLOCK) ? 0 : -1);
    }

    /* check returned size .. */
    if (norecv < QDP_HEDSIZE)
      continue;

    len = ping->datalength;
#ifndef WORDS_BIGENDIAN
    len = swap16(len);
#endif /* WORDS_BIGENDIAN */

    /* now check the payload */
    if (norecv < (QDP_HEDSIZE + len))
      continue;

#ifndef WORDS_BIGENDIAN
    ping->crc = swap32(ping->crc);
#endif /* WORDS_BIGENDIAN */

    if (ping->crc != qdp_calc_crc(&ping->command, QDP_HEDSIZE + len - QDP_CRCSIZE))
      continue;

#ifdef DEBUG
fprintf(stderr, "bit %x\n", ping->data.status.bitmap);
#endif // DEBUG

#ifndef WORDS_BIGENDIAN
    ping->datalength = swap16(ping->datalength);
    ping->sequence = swap16(ping->sequence);
    ping->acknowledge = swap16(ping->acknowledge);
    ping->ping_type = swap16(ping->ping_type);
    ping->ping_id = swap16(ping->ping_id);

    if (ping->ping_type == 3) {
      ping->data.status.drifttol = swap16(ping->data.status.drifttol);
      ping->data.status.usermsgcnt = swap16(ping->data.status.usermsgcnt);
      ping->data.status.lastreboot = swap32(ping->data.status.lastreboot);
      ping->data.status.bitmap = swap32(ping->data.status.bitmap);
    }
    else if (ping->ping_type == 5) {
      ping->data.info.version = swap16(ping->data.info.version);
      ping->data.info.flags = swap16(ping->data.info.flags);
      ping->data.info.kmi = swap32(ping->data.info.kmi);
      ping->data.info.serial_low = swap32(ping->data.info.serial_low);
      ping->data.info.serial_high = swap32(ping->data.info.serial_high);
      for (i = 0; i < 8; i++) {
        ping->data.info.memory[i] = swap32(ping->data.info.memory[i]);
      }
      for (i = 0; i < 8; i++) {
        ping->data.info.interface[i] = swap16(ping->data.info.interface[i]);
      }
      ping->data.info.calerr = swap16(ping->data.info.calerr);
      ping->data.info.sysver = swap16(ping->data.info.sysver);
    }
#endif /* WORDS_BIGENDIAN */

#ifdef DEBUG
fprintf(stderr, "bit %x\n", ping->data.status.bitmap);
#endif // DEBUG

    if (ping->ping_type == 3)
      decode_qdp_status(ping);

  } while (norecv < 0);

  return norecv;
}

unsigned long long ping_for_serial(char *ipaddr, int port, int count, int timeout) {

  int n;
  qdp ping;

  int state = 0;
  int tries = 0;
  char buf[256];

  int sockfd = 0;
  struct sockaddr_in sin;

  unsigned long long serial = 0LL;

  /* bind local socket ... */
  memset((char *) &sin, 0, sizeof(struct sockaddr_in));
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = htons(INADDR_ANY);
  sin.sin_port = htons(0);

  /* build a local control socket .... */
  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    ms_log(2, "can't create local socket [%s]\n", strerror(errno)); exit(-1);
  }

  /* make it non-blocking? - get old flags then set new flags */
  if ((state = fcntl(sockfd, F_GETFL, 0)) < 0) {
    ms_log(2, "could not get socket options [%s]\n", strerror(errno)); exit(-1);
  }
  if (fcntl(sockfd, F_SETFL, state | O_NONBLOCK) < 0) {
    ms_log(2, "could not set socket non-blocking [%s]\n", strerror(errno)); exit(-1);
  }
  if (bind(sockfd, (struct sockaddr *) &sin, sizeof(struct sockaddr_in)) < 0) {
    ms_log(2, "can't bind local socket address [%s]\n", strerror(errno)); exit(-1);
  }

  while (count > 0) {
    if (send_qdp_ping(sockfd, ipaddr, port, 1) < 0) {
      ms_log(0, "unable to send ping [%s:%d] %s\n", ipaddr, port, strerror(errno)); exit(-1);
    }
    for (tries = timeout; tries > 0; tries--) {
      if ((n = recv_qdp_ping(sockfd, &ping)) < 0) {
        return (unsigned long long) -1;
      }
      if (n > 0) {
        snprintf(buf, sizeof(buf) - 1, "0x0%08x%08x", ping.data.info.serial_high, ping.data.info.serial_low);
        serial = (unsigned long long) strtoll(buf, (char **) NULL, 0);
        count = 0;
        break;
      }
      sleep(1);
    }
    count--;
  }

  close(sockfd);

  return serial;
}
