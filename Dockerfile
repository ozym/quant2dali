FROM alpine:3.4
MAINTAINER "Mark Chadwick" <m.chadwick@gns.cri.nz>

ENV LIBMSEED_VERSION 2.15
ENV LIBSLINK_VERSION 2.6
ENV LIBDALI_VERSION 1.7

ADD . /src

RUN apk --update add curl make tar gcc libc-dev && \
        curl -o /tmp/libmseed-${LIBMSEED_VERSION}.tar.gz https://seiscode.iris.washington.edu/attachments/download/653/libmseed-${LIBMSEED_VERSION}.tar.gz && \
        cd /tmp && tar xvfz libmseed-${LIBMSEED_VERSION}.tar.gz && \
        cd /tmp/libmseed && make && \
        cp -a libmseed.a /usr/lib && \
        cp -a libmseed.h /usr/include && \
        cp -a lmplatform.h /usr/include && \
        rm -rf /tmp/libmseed && \
        curl -o /tmp/libslink-${LIBSLINK_VERSION}.tar.gz http://ds.iris.edu/pub/programs/SeedLink/libslink-${LIBSLINK_VERSION}.tar.gz && \
        cd /tmp && tar xvfz libslink-${LIBSLINK_VERSION}.tar.gz && \
        cd /tmp/libslink && make && \
        cp -a libslink.a /usr/lib && \
        cp -a libslink.h /usr/include && \
        cp -a slplatform.h /usr/include && \
        rm -rf /tmp/libslink && \
        curl -o /tmp/libdali-${LIBDALI_VERSION}.tar.gz ftp://ftp.iris.washington.edu/pub/programs/ringserver/libdali-${LIBDALI_VERSION}.tar.gz && \
        cd /tmp && tar xvfz libdali-${LIBDALI_VERSION}.tar.gz && \
        cd /tmp/libdali && make && \
        cp -a libdali.a /usr/lib && \
        cp -a libdali.h /usr/include && \
        cp -a portable.h /usr/include && \
        rm -rf /tmp/libdali && \
        cd /src && make && \
        cp -a quant2dali /usr/bin && \
        make clean

ENTRYPOINT ["/usr/bin/quant2dali"]
