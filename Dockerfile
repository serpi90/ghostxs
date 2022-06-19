FROM ubuntu:14.04

RUN set -eux; \
    apt-get update; \
    DEBIAN_FRONTEND=noninteractive apt-get install --assume-yes \
        build-essential \
        libboost-all-dev \
        libbz2-dev \
        libgmp3-dev \
        libmysql++-dev \
        zlib1g-dev \
        ;

COPY source /app/source
COPY runfiles /app/runfiles

WORKDIR /app/source/bncsutil/src/bncsutil/
RUN make && make install && ldconfig

WORKDIR /app/source/StormLib/stormlib/
RUN make && make install && ldconfig

WORKDIR /app/source/ghost/
RUN make

WORKDIR /app/runfiles

RUN set -eux; \
    mkdir -p \
        /app/data \
        /app/data/mapcfgs/ \
        /app/data/maps/ \
        /app/data/replays/ \
        /app/data/savegames/ \
        /app/data/war3/ \
        ; \
    cp /app/runfiles/default.cfg /app/data/ghost.cfg; \
    cp /app/runfiles/default.cfg /app/data/default.cfg; \
	ln -sf /app/data/ghost.dbs    /app/runfiles/ghost.dbs ; \
    ln -sf /app/data/default.cfg  /app/runfiles/default.cfg ; \
    ln -sf /app/data/ghost.cfg    /app/runfiles/ghost.cfg ; \
    ln -sf /app/data/mapcfgs/     /app/runfiles/mapcfgs ; \
    ln -sf /app/data/maps/        /app/runfiles/maps ; \
    ln -sf /app/data/replays/     /app/runfiles/replays ; \
    ln -sf /app/data/savegames/   /app/runfiles/savegames ; \
    ln -sf /app/data/war3/        /app/runfiles/war3 ; \
    true

VOLUME /app/data/

STOPSIGNAL SIGINT

CMD ["./ghost++"]
