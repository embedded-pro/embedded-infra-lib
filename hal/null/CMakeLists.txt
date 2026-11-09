add_library(hal.null ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_link_libraries(hal.null PUBLIC
    hal.interfaces
    hal.synchronous_interfaces
)

target_sources(hal.null PRIVATE
    AdcMultiChannelNull.cpp
    AdcMultiChannelNull.hpp
    CanNull.cpp
    CanNull.hpp
    GpioPinNull.cpp
    GpioPinNull.hpp
    SerialCommunicationNull.cpp
    SerialCommunicationNull.hpp
    SynchronousQuadratureEncoderNull.cpp
    SynchronousQuadratureEncoderNull.hpp
    ThreeChannelsPwmNull.cpp
    ThreeChannelsPwmNull.hpp
)

add_subdirectory(test)
