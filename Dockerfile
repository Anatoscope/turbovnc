from ubuntu:latest

COPY turbovnc-*.deb /

RUN admindir=/turbovnc/var/lib/dpkg/ && \
    for file in status available statoverride diversions dpkg.log; do \
        touch "$admindir/$file"; \
    done && \
    for dir in info methods parts updates; do \
        mkdir "$admindir/$dir"; \
    done && \
    dpkg --instdir=/turbovnc --admindir="$admindir" --force-depends -i turbovnc-*.deb && \
    rm turbovnc-*.deb
