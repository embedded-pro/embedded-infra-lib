FROM gabrielfrasantos/embedded-devcontainer-cpp:v7.3.0@sha256:4f92e9c86dfc5a9688b6134944e7a40c9eb3c658022ea62dc9e57e4c3b056940

RUN xwin --accept-license --sdk-version 10.0.26100 --crt-version 14.44.17.14 --timeout 120s splat --preserve-ms-arch-notation --include-debug-libs --output /winsdk

HEALTHCHECK NONE
