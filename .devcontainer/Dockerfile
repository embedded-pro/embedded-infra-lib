FROM gabrielfrasantos/embedded-devcontainer-cpp:latest@sha256:5fe0d1ae0ab020db875964752d4e9e07e54718ea54733b5f7a8c81753cd48027

RUN xwin --accept-license --sdk-version 10.0.26100 --crt-version 14.44.17.14 --timeout 120s splat --preserve-ms-arch-notation --include-debug-libs --output /winsdk

HEALTHCHECK NONE
