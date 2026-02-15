// tools cimple_cli main - invoke CLI handlers
#include <iostream>
#include <string>

// forward declarations from cli_commands.cpp
void handle_build(const std::string& path);
void handle_cli(int argc, char** argv);

int main(int argc, char** argv) {
    handle_cli(argc, argv);
    return 0;
}
