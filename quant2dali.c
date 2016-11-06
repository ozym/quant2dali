/*
 * Copyright (c) 2011 Institute of Geological & Nuclear Sciences Ltd.
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
 * $Id: quant2dali.c 4486 2011-05-09 02:12:20Z chadwick $
 *
 */

/* system includes */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <getopt.h>
#include <string.h>
#include <signal.h>
#include <syslog.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <errno.h>
#include <time.h>
#include <math.h>
#include <pwd.h>

/* libmseed library includes */
#include <libmseed.h>
#include <libdali.h>

/* lib330 library includes */
#include <libclient.h>
#include <libtypes.h>
#include <libmsgs.h>

#include "dsarchive.h"

#ifndef PACKAGE_NAME
#define PACKAGE_NAME "quant2dali" /* program name */
#endif // PACKAGE_NAME
#ifndef PACKAGE_VERSION
#define PACKAGE_VERSION "xx.x"
#endif // PACKAGE_VERSION

/*
 * q3link: collect and archive Q330 and optionally send it a ringserver via datalink
 *
 */

/* program variables */
static char *program_name = PACKAGE_NAME;
static char *program_version = PACKAGE_NAME " (" PACKAGE_VERSION ") $Id: quant2dali.c 4486 2011-05-09 02:12:20Z chadwick $ (c) GNS 2011 (m.chadwick@gns.cri.nz)";
static char *program_usage = PACKAGE_NAME " [options] <station> [<server>]";
static char *program_prefix = "[" PACKAGE_NAME "] ";

static int going = 1; /* are we running ... */
static int verbose = 0; /* program verbosity */
static time_t last = 0; /* keep us going */

static char *server = NULL; /* datalink server to use */
static DLCP *dlconn = NULL; /* datalink handle */
static int writeack = 0; /* request for write acks */

static DataStream datastream; /* archive it ... */

/* lib330 structures */
static tpar_register ri; /* registration info */
static tpar_create ci; /* creation info */
static tcontext sc; /* station context */

/* overall verbosity */
static int verbosity = VERB_SDUMP | VERB_REGMSG | VERB_LOGEXTRA;

/* q330 details */
static int lport = 1; /* the corresponding logical port */
static char *ipaddr = "127.0.0.1"; /* Q330 IP address */
static char *station = NULL; /* station code */
static char *serial = NULL; /* Q330 hex long serial number */
static char *authcode = "0x00"; /* Q330 hex auth code */
static char *continuity = NULL; /* plugin continuity file */

static int baseport = 5330; /* Q330 base port offset */
static int min_retry = 5;
static int max_retry = 100;
static int hiber_time = 1;
static int dead_time = 3600; /* if no data for a while ... stop */

static int cntl_attempts = 5; /* how many times to register */
static int cntl_wait = 120; /* how long to wait to register */

static int serial_retry = 3;
static int serial_wait = 5;

/* current running status */
static enum tlibstate lib_state = LIBSTATE_IDLE;

extern unsigned long long ping_for_serial(char *ipaddr, int port, int count, int timeout);

/* string to convert to upper case. */
static char *uc(char *string) {
	unsigned char c;
	char *result, *p;
	result = p = strdup(string);
	while (c = *p) *(p++) = islower(c) ? toupper(c) : c;
	return (result);
}

static char *lc(char *string) {
	unsigned char c;
	char *result, *p;
	result = p = strdup(string);
	while (c = *p) *(p++) = islower(c) ? c : tolower(c);
	return (result);
}

/* handle any KILL/TERM signals */
static void term_handler(int sig) {
	going = 0; return;
}

static void dummy_handler (int sig) {
	return;
}

static void log_print(char *message) {
	if (verbose)
		fprintf(stderr, "%s", message);
}

static void err_print(char *message) {
	fprintf(stderr, "error: %s", message);
}

static int sendrecord (char *record, int reclen) {
  static MSRecord *msr = NULL;
  hptime_t endtime;
  char streamid[100];
  int rv;

	if ((dlconn == NULL) || (dlconn->link == -1))
		return -1;

  /* Parse Mini-SEED header */
  if ((rv = msr_unpack (record, reclen, &msr, 0, 0)) != MS_NOERROR) {
    ms_recsrcname (record, streamid, 0);
    ms_log (2, "error unpacking %s: %s", streamid, ms_errorstr(rv)); return -1;
	}

  /* Generate stream ID for this record: NET_STA_LOC_CHAN/MSEED */
  msr_srcname (msr, streamid, 0);
  strcat (streamid, "/MSEED");

  /* Determine high precision end time */
  endtime = msr_endtime (msr);

  /* Send record to server */
  if (dl_write (dlconn, record, reclen, streamid, msr->starttime, endtime, writeack) < 0) {
		return -1;
	}

  return 0;
}  /* End of sendrecord() */

/* lib330 error messages ... */
void q330_error(int status, enum tliberr errcode) {
	string63 errmsg;

	lib_get_errstr(errcode, &errmsg);
	ms_log((status) ? 2 : 0, "%s\n", errmsg);
	going = ((status) ? 0 : going);
}

void q330_state_callback(pointer p) {
	string63 new_state_name;

	if (((tstate_call *) p)->state_type == ST_STATE) {
		lib_get_statestr((enum tlibstate)((tstate_call *) p)->info, &new_state_name);
		if (verbose)
			ms_log(0, "changing state to %s\n", new_state_name);
		lib_state = (enum tlibstate)((tstate_call *) p)->info;
	}
}

void q330_message_callback(pointer p) {
	tmsg_call *msg = (tmsg_call *) p;
	string95 msg_text;
	char data_time[32];

	/* decode the message */
	lib_get_msg(msg->code, &msg_text);
	jul_string((msg->datatime) ? msg->datatime : msg->timestamp, &data_time);

	if (verbose)
		ms_log(0, "%s {%03d} %s %s\n", data_time, msg->code, msg_text, msg->suffix);
}

void q330_minidata_callback(pointer p) {
  int rc;
  char srcname[100];
  static MSRecord *msr = NULL;

	tminiseed_call *data = (tminiseed_call *) p;
  pq330 q330 = (pq330) ((tminiseed_call *)p)->context;

	/* archive it perhaps ... */
	if (datastream.path != NULL) {
  	ms_recsrcname ((char *) data->data_address, srcname, 0);

  	/* Parse Mini-SEED header */
  	if ((rc = msr_unpack ((char *) data->data_address, data->data_size, &msr, 0, 0)) != MS_NOERROR) {
    	ms_log (1, "error unpacking %s: %s", srcname, ms_errorstr(rc)); going = 0; return;
  	}

		if (ds_streamproc (&datastream, msr, 0, verbose - 1) < 0) {
   		ms_log (1, "error archiving packet\n"); going = 0; return;
		}
	}
	
	/* send it off to a datalink server */
	if (dlconn != NULL) {
		while (sendrecord ((char *) data->data_address, data->data_size)) {
			if (verbose > 0)
				ms_log (1, "re-connecting to datalink server\n");

			if (dlconn->link != -1)
				dl_disconnect(dlconn);

			if (dl_connect(dlconn) < 0) {
				ms_log (1, "error re-connecting to datalink server, sleeping 10 seconds\n"); sleep (10);
			}

			if (q330->terminate)
				break;
		}
	}

	/* got it ... */
	last = (time_t) time((time_t *) 0);
}

int main(int argc, char **argv) {

	int i;
	char *c;

	char buf[128];
	unsigned long long s, a;

	int rc;
	int option_index = 0;
	struct option long_options[] = {
		{"help", 0, 0, 'h'},
		{"verbose", 0, 0, 'v'},
		{"ack", 0, 0, 'w'},
		{"authcode", 1, 0, 'a'},
		{"serial", 1, 0, 's'},
		{"ipaddress", 1, 0, 'i'},
		{"lport", 1, 0, 'l'},
		{"baseport", 1, 0, 'p'},
    {"timeout", 1, 0, 'k'},
		{"retries", 1, 0, 'r'},
    {"deadtime", 1, 0, 'd'},
    {"attempts", 1, 0, 'n'},
    {"continuity", 1, 0, 'x'},
    {"format", 1, 0, 'f'},
		{0, 0, 0, 0}
	};

	string63 errmsg;
	topstat retopstat;
	enum tliberr errcode;
	tslidestat slidecopy;

	/* posix signal handling */
	struct sigaction sa;

	sa.sa_handler = dummy_handler;
	sa.sa_flags	= SA_RESTART;
	sigemptyset (&sa.sa_mask);
	sigaction (SIGALRM, &sa, NULL);

	sa.sa_handler = term_handler;
	sigaction (SIGINT, &sa, NULL);
	sigaction (SIGQUIT, &sa, NULL);
	sigaction (SIGTERM, &sa, NULL);

	sa.sa_handler = SIG_IGN;
	sigaction (SIGHUP, &sa, NULL);
	sigaction (SIGPIPE, &sa, NULL);

	/* adjust output logging ... -> syslog maybe? */
	ms_loginit (log_print, program_prefix, err_print, program_prefix);

  memset(&datastream, 0, sizeof(DataStream *));
  datastream.path = NULL;
  datastream.idletimeout = 60;
  datastream.grouproot = NULL;

	while ((rc = getopt_long(argc, argv, "hvwr:p:d:a:i:s:l:k:n:x:f:", long_options, &option_index)) != EOF) {
		switch(rc) {
		case '?':
			(void) fprintf(stderr, "usage: %s\n", program_usage);
			exit(-1); /*NOTREACHED*/
		case 'h':
			(void) fprintf(stderr, "\n[%s] Q330 miniseed collector\n\n", program_name);
			(void) fprintf(stderr, "usage:\n\t%s\n", program_usage);
			(void) fprintf(stderr, "version:\n\t%s\n", program_version);
			(void) fprintf(stderr, "options:\n");
			(void) fprintf(stderr, "\t-h --help\tcommand line help (this)\n");
			(void) fprintf(stderr, "\t-v --verbose\trun program in verbose mode\n");
			(void) fprintf(stderr, "\t-d --deadtime\thalt processing if no data received [%ds]\n", dead_time);
			(void) fprintf(stderr, "\t-r --retries\tmax connection retries [%d]\n", max_retry);
			(void) fprintf(stderr, "\t-w --ack\trequest write acks [%s]\n", (writeack) ? "on" : "off");
			(void) fprintf(stderr, "\t-i --ipaddress\tprovide the q330 ip address [%s]\n", ipaddr);
			(void) fprintf(stderr, "\t-s --serial\tprovide the q330 serial number [%s]\n", serial);
			(void) fprintf(stderr, "\t-a --authcode\tprovide the q330 authority code [%s]\n", authcode);
			(void) fprintf(stderr, "\t-l --lport\tprovide the q330 logical port number [%d]\n", lport);
			(void) fprintf(stderr, "\t-p --baseport\tprovide the q330 base port number [%d]\n", baseport);
			(void) fprintf(stderr, "\t-k --timeout\tregistration wait time [%d]\n", cntl_wait);
      (void) fprintf(stderr, "\t-n --attempts\tnumber of registration attempts [%d]\n", cntl_attempts);
      (void) fprintf(stderr, "\t-x --continuity\tprovide a continuity file [%s]\n", (continuity) ? continuity : "<null>");
      (void) fprintf(stderr, "\t-f --format\toptional miniseed archive format [%s]\n", (datastream.path) ? datastream.path : "<null>");
			exit(0); /*NOTREACHED*/
		case 'v':
			verbose++;
			break;
		case 'w':
			writeack++;
			break;
		case 's':
			serial = optarg;
			break;
		case 'a':
			authcode = optarg;
			break;
		case 'i':
			ipaddr = optarg;
			break;
		case 'l':
			lport = atoi(optarg);
			break;
		case 'p':
			baseport = atoi(optarg);
			break;
    case 'n':
      cntl_attempts = atoi(optarg);
      break;
    case 'd':
      dead_time = atoi(optarg);
      break;
		case 'r':
			max_retry = atoi(optarg);
			break;
    case 'k':
      cntl_wait = atoi(optarg);
      break;
    case 'x':
      continuity = optarg;
      break;
    case 'f':
      datastream.path = optarg;
      break;
		}
	}

	/* who to connect to ... */
	station = ((optind < argc) ? argv[optind++] : station);
	server = ((optind < argc) ? argv[optind++] : server);

	/* report the program version */
	if (verbose)
		ms_log (0, "%s\n", program_version);

	if (!station) {
		ms_log (2, "no station code given\n"); exit(-1);
	}

	/* what to recover ... */
	verbosity |= ((verbose > 0) ? VERB_RETRY : 0);
	verbosity |= ((verbose > 1) ? VERB_PACKET : 0);

	if (verbosity > 1)
     ms_log(0, "configuring q330 [%s] ... \n", station);

  if ((serial == NULL) || (strtoll(serial, (char **) NULL, 0) == 0)) {
	  if (verbosity > 1)
     ms_log(0, "discover q330 serial number [%s] ... \n", station);
    if ((s = ping_for_serial(ipaddr, baseport, serial_retry, serial_wait)) < 0) {
      ms_log(2, "error finding serial number: %s\n", strerror(errno)); exit(-1);
    }
    if (s == 0LL) {
      ms_log(2, "unable to determine serial number\n"); exit(-1);
    }
  }
  else {
	  /* box serial number */
	  s = (unsigned long long) strtoll(serial, (char **) NULL, 0);
  }

	if (verbose > 1)
		ms_log(0, "filling creation information structure [%s]\n", station);

	/* box auth number */
	a = (unsigned long long) strtoll(authcode, (char **) NULL, 0);

	/* q330 connection details */
	memcpy(ci.q330id_serial, &s, sizeof(long long));
	switch(lport) {
	case 1:
		ci.q330id_dataport = LP_TEL1;
		break;
	case 2:
		ci.q330id_dataport = LP_TEL2;
 		break;
	case 3:
		ci.q330id_dataport = LP_TEL3;
  		break;
	case 4:
		ci.q330id_dataport = LP_TEL4;
		break;
	}
	strncpy(ci.q330id_station, uc(station), 5);
	ci.host_timezone = 0;
	strncpy(ci.host_software, PACKAGE_NAME, 95);
	strncpy(ci.opt_contfile, (continuity) ? continuity : "", 250);
	ci.opt_verbose = verbosity;
	ci.opt_zoneadjust = 1;
	ci.opt_secfilter = 0;
	ci.opt_minifilter = OMF_ALL;
	ci.opt_aminifilter = 0;
	ci.amini_exponent = 0;
	ci.amini_512highest = 0;
	ci.mini_embed = 1;
	ci.mini_separate = 1;
	ci.mini_firchain = 0;
	ci.call_minidata = q330_minidata_callback;
	ci.call_aminidata = NULL;
	ci.resp_err = LIBERR_NOERR;
	ci.call_state = q330_state_callback;
	ci.call_messages = q330_message_callback;
	ci.call_secdata = NULL;
	ci.call_lowlatency = NULL;

	if (verbose > 1)
		ms_log(0, "filling registration structure [%s]\n", station);
	memcpy(ri.q330id_auth, &a, sizeof(long long));
	strncpy(ri.q330id_address, ipaddr, 250);
	ri.q330id_baseport = baseport;
	ri.host_mode = HOST_ETH;
	strcpy(ri.host_interface, "");
	ri.host_mincmdretry = min_retry;
	ri.host_maxcmdretry = max_retry;
	ri.host_ctrlport = 0;
	ri.host_dataport = 0;
	ri.opt_latencytarget = 0;
	ri.opt_closedloop = 0;
	ri.opt_dynamic_ip = 0;
	ri.opt_hibertime = hiber_time;
	ri.opt_conntime = 0;
	ri.opt_connwait = 0;
	ri.opt_regattempts = cntl_attempts;
	ri.opt_ipexpire = 0;
	ri.opt_buflevel = 0;

	if (verbose > 1)
		ms_log(0, "creating station thread [%s]\n", station);

	lib_create_context(&sc, &ci);
	if (ci.resp_err != LIBERR_NOERR) {
		lib_get_errstr(errcode, &errmsg);
		ms_log(2, "unable to create context for %s skipping [%s]\n", station, errmsg);
		exit(-1);
	}

  ds_maxopenfiles = 50; /* just in case ... */

	/* first off .. */
	last = (time_t) time((time_t *) 0);

	if (server) {
		/* provide user tag */
		(void) snprintf(buf, sizeof(buf) - 1, "%s:%s", (strrchr(argv[0], '/')) ? strrchr(argv[0], '/') + 1 : argv[0], station);

		if (verbose)
     	ms_log(0, "connecting to datalink server %s as \"%s\"\n", server, buf);

		/* allocate and initialize datalink connection description */
		if ((dlconn = dl_newdlcp (server, buf)) == NULL) {
     	ms_log(2, "cannot allocation datalink descriptor\n"); exit (-1);
  	}
		/* connect to datalink server */
		if (dl_connect(dlconn) < 0) {
     	ms_log(2, "error connecting to datalink server: server\n"); exit(-1);
  	}
		if (dlconn->writeperm != 1) {
     	ms_log(2, "datalink server is non-writable\n"); exit(-1);
		}
	}

	if (verbose > 1)
		ms_log (0, "connecting to q330 %s::%s@%s::%s/%d\n", station, authcode, serial, ipaddr, lport);

	while (going) {

		lib_state = lib_get_state(sc, &errcode, &retopstat);

		/* ready to go ... */
		switch(lib_state) {
		case LIBSTATE_IDLE:
			if (verbose)
				ms_log(0, "pinging the q330 [%s]\n", ipaddr);
			lib_unregistered_ping(sc, &ri);
			if (verbose)
				ms_log(0, "registering with the q330 [0x%0xll %s]\n", s, authcode);
			errcode = lib_register(sc, &ri);
			if (errcode != LIBERR_NOERR)
				q330_error(1, errcode);
      /* wait for a change ... */
      for (i = 0; i < (cntl_wait * 10); i++) {
				/* find the library state */
        if (lib_state == LIBSTATE_RUNWAIT) break;
        (void) usleep((going) ? 100000 : 0);
      }
			break;
		case LIBSTATE_RUNWAIT:
			if (verbose)
				ms_log(0, "registered, starting data flowing\n");
			lib_change_state(sc, LIBSTATE_RUN, LIBERR_NOERR);
      /* wait for a change ... */
      for (i = 0; i < (cntl_wait * 10); i++) {
        if (lib_state == LIBSTATE_RUN) break;
        (void) usleep((going) ? 100000 : 0);
      }
			break;
		case LIBSTATE_WAIT:
			if (verbose)
				ms_log(0, "waiting ... \n");
			lib_change_state(sc, (going) ? LIBSTATE_IDLE : LIBSTATE_WAIT, LIBERR_NOERR);
      for (i = 0; i < (cntl_wait * 10); i++) {
        if (lib_state == LIBSTATE_IDLE) break;
        (void) usleep((going) ? 100000 : 0);
      }
      break;
		case LIBSTATE_RUN:
		case LIBSTATE_PING:
		case LIBSTATE_REG:
		case LIBSTATE_READCFG:
		case LIBSTATE_READTOK:
		case LIBSTATE_DECTOK:
			break;
		case LIBSTATE_TERM:
		case LIBSTATE_DEALLOC:
		case LIBSTATE_DEREG:
			going = 0;
			break;
		}

		if ((int)(time((time_t *) 0) - last) > dead_time) {
			going = 0; continue;
		}

		/* have a short nap */
		(void) usleep((going) ? 100000: 0);
	}

	/* de-register etc */
	if (verbose)
		ms_log (0, "de-registering\n");

	lib_state = lib_get_state(sc, &errcode, &retopstat);
  if ((lib_state != LIBSTATE_TERM) && (lib_state != LIBSTATE_WAIT)) {
    lib_change_state(sc, LIBSTATE_WAIT, LIBERR_NOERR);
    for (i = 0; i < (cntl_wait * 10); i++) {
      if (lib_state == LIBSTATE_WAIT) break;
      (void) usleep(100000);
		}
	}

  if (lib_state != LIBSTATE_TERM) {
    lib_change_state(sc, LIBSTATE_TERM, LIBERR_CLOSED);
    for (i = 0; i < (cntl_wait * 10); i++) {
      if (lib_state == LIBSTATE_TERM) break;
      (void) usleep(100000);
    }
  }

	/* closing down */
	if (verbose)
		ms_log (0, "destroying station thread\n");
 	errcode = lib_destroy_context(&sc);
	if (errcode != LIBERR_NOERR)
		q330_error(1, errcode);

 	if ((dlconn) && (dlconn->link != -1))
 	 dl_disconnect (dlconn);

	/* closing down */
	if (verbose)
		ms_log (0, "terminated\n");

	/* done */
	return(0);
}
