FROM gcr.io/oss-fuzz-base/base-builder@sha256:b2a8815966df04e361c8e3ef74c5a526fa0d2b973206ed492ae9c77a9dd4f988

HEALTHCHECK NONE

RUN apt-get update && apt-get upgrade -y --no-install-recommends \
        ninja-build \
 && rm -rf /var/lib/apt/lists/*

COPY . $SRC/amp-embedded-infra-lib
COPY .clusterfuzzlite/build.sh $SRC/

WORKDIR $SRC/amp-embedded-infra-lib
