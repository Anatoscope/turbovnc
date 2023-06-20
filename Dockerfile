ARG BUILD_IMAGE

from ${BUILD_IMAGE} as builder
COPY turbovnc_*.deb libjpeg-turbo-official_*.deb virtualgl_*.deb /
RUN admindir=/turbovnc/var/lib/dpkg/ && \
    mkdir -p "$admindir" && \
    for file in status available statoverride diversions dpkg.log; do \
        touch "$admindir/$file"; \
    done && \
    for dir in info methods parts updates; do \
        mkdir "$admindir/$dir"; \
    done && \
    dpkg --instdir=/turbovnc --admindir="$admindir" --force-depends -i /*.deb

from scratch
COPY --from=builder /turbovnc /turbovnc
CMD /usr/bin/true
