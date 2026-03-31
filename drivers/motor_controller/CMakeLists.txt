add_library(drivers.motor_controller ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_link_libraries(drivers.motor_controller PUBLIC
    hal.interfaces
    infra.util
)

target_sources(drivers.motor_controller PRIVATE
    DirectPwmStepperMotorDrv8711Decorator.cpp
    DirectPwmStepperMotorDrv8711Decorator.hpp
    StepDirStepperMotorDrv8711Decorator.cpp
    StepDirStepperMotorDrv8711Decorator.hpp
    StepperMotorControllerDrv8711.cpp
    StepperMotorControllerDrv8711.hpp
)

add_subdirectory(test)
