FROM gcr.io/oss-fuzz-base/base-builder@sha256:be3e169f430d9ddec9190194d8fd95e4694547765c6ce9d80efd821f20464940

HEALTHCHECK NONE

RUN apt-get update && apt-get upgrade -y --no-install-recommends \
        ninja-build \
 && rm -rf /var/lib/apt/lists/*

COPY . $SRC/amp-embedded-infra-lib
COPY .clusterfuzzlite/build.sh $SRC/

WORKDIR $SRC/amp-embedded-infra-lib
