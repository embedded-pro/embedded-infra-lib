FROM gcr.io/oss-fuzz-base/base-builder@sha256:a2ed92c50ff6788a6569b9ae3d3860f38104860933475eb05798df96fc1ead5f

HEALTHCHECK NONE

RUN apt-get update && apt-get upgrade -y --no-install-recommends \
        ninja-build \
 && rm -rf /var/lib/apt/lists/*

COPY . $SRC/amp-embedded-infra-lib
COPY .clusterfuzzlite/build.sh $SRC/

WORKDIR $SRC/amp-embedded-infra-lib
