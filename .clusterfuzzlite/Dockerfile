FROM gcr.io/oss-fuzz-base/base-builder@sha256:5655cb17ab592e6b539e515de384f9b709a574f234ce21176d84cf95b1a63a1b

HEALTHCHECK NONE

RUN apt-get update && apt-get upgrade -y --no-install-recommends \
        ninja-build \
 && rm -rf /var/lib/apt/lists/*

COPY . $SRC/amp-embedded-infra-lib
COPY .clusterfuzzlite/build.sh $SRC/

WORKDIR $SRC/amp-embedded-infra-lib
