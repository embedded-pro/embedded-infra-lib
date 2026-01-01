FROM gcr.io/oss-fuzz-base/base-builder@sha256:1a80bb5e8f084613f4a1de7fa09729f4dd8c8df3c55c5b40c15cf49afd886dad

HEALTHCHECK NONE

RUN apt-get update && apt-get upgrade -y --no-install-recommends \
        ninja-build \
 && rm -rf /var/lib/apt/lists/*

COPY . $SRC/amp-embedded-infra-lib
COPY .clusterfuzzlite/build.sh $SRC/

WORKDIR $SRC/amp-embedded-infra-lib
