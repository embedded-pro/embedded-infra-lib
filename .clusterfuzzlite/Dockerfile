FROM gcr.io/oss-fuzz-base/base-builder@sha256:92d00ddf87671c5b03ffdb8a2f965eef4f51035b715ed6ba3688afcc376a3d1d

HEALTHCHECK NONE

RUN apt-get update && apt-get upgrade -y --no-install-recommends \
        ninja-build \
 && rm -rf /var/lib/apt/lists/*

COPY . $SRC/amp-embedded-infra-lib
COPY .clusterfuzzlite/build.sh $SRC/

WORKDIR $SRC/amp-embedded-infra-lib
