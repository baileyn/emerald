project(winx)

set(SOURCE_FILES src/process.cpp include/winx/process.h)

add_library(${PROJECT_NAME} ${SOURCE_FILES})
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)
target_include_directories(${PROJECT_NAME} PUBLIC include)