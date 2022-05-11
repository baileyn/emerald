#include <spdlog/spdlog.h>

int main(int argc, char** argv)
{
    spdlog::info("Hello! We got {} args!", argc);

    return 0;
}
