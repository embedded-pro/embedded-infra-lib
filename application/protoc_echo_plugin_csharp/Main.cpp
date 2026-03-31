#include "application/protoc_echo_plugin_csharp/ProtoCEchoPluginCSharp.hpp"
#include "google/protobuf/compiler/plugin.h"

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

int main(int argc, char* argv[])
{
#ifdef _MSC_VER
    //_CrtDbgBreak();
#endif
    application::CSharpEchoCodeGenerator generator;
    return google::protobuf::compiler::PluginMain(argc, argv, &generator);
}
