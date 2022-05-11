project(emeraldint CXX)

set(SOURCE_FILES src/main.cpp)

add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES})
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)
