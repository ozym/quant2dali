
# Build environment can be configured the following
# environment variables:
#   CC : Specify the C compiler to use
#   CFLAGS : Specify compiler options to use
#
#
VPATH = libq330

CFLAGS += -I. -I./lib330 -DPACKAGE_VERSION=\"1.3.0\"

Q330_HDRS = libarchive.h libclient.h libcmds.h libcompress.h libcont.h libctrldet.h libcvrt.h \
	    libdetect.h libdss.h libfilters.h liblogs.h libmd5.h libmsgs.h libnetserv.h libopaque.h \
	    libpoc.h libsampcfg.h libsampglob.h libsample.h libseed.h libslider.h libstats.h\
	    libstrucs.h libsupport.h libtokens.h libtypes.h libverbose.h pascal.h platform.h\
	    q330cvrt.h q330io.h q330types.h

Q330_FILES = libarchive.c libclient.c libcmds.c libcompress.c libcont.c libctrldet.c libcvrt.c\
	    libdetect.c libdss.c libfilters.c liblogs.c libmd5.c libmsgs.c libnetserv.c libopaque.c\
	    libpoc.c libsampcfg.c libsample.c libseed.c libslider.c libstats.c libstrucs.c libsupport.c\
	    libtokens.c libtypes.c libverbose.c q330cvrt.c q330io.c

Q330_SRCS = $(Q330_FILES:%.c=lib330/%.c)
Q330_OBJS = $(Q330_FILES:%.c=lib330/%.o)

LDFLAGS =
LDLIBS = -ldali -lslink -lmseed -lpthread -lrt -lm -lc

all: quant2dali

quant2dali: quant2dali.o dsarchive.h dsarchive.o ping.h ping.o $(Q330_OBJS)
	$(CC) $(CFLAGS) -o $@ quant2dali.o dsarchive.o ping.o $(Q330_OBJS) $(LDLIBS)

clean:
	rm -f quant2dali.o quant2dali dsarchive.o ping.o $(Q330_OBJS)

$(Q330_OBJS): %.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Implicit rule for building object files
%.o: %.c
	$(CC) $(CFLAGS) -c $<

install:
	@echo
	@echo "No install target, copy the executable(s) yourself"
	@echo


