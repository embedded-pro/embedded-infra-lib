#include "application/fsm_validator/FsmValidator.hpp"
#include "args.hxx"
#include "infra/stream/IoOutputStream.hpp"
#include "infra/stream/StdStringOutputStream.hpp"
#include <exception>
#include <fstream>
#include <iostream>
#include <string>

namespace
{
    bool WriteMermaidFile(const application::FsmRegistration& registration, const std::string& directory)
    {
        infra::StdStringOutputStream::WithStorage diagram;
        registration.WriteMermaid(diagram);

        std::ofstream file(directory + "/" + registration.Name() + ".mmd");
        file << diagram.Storage();
        return static_cast<bool>(file);
    }

    bool WriteMermaidFiles(const std::string& directory)
    {
        bool written = true;

        for (const auto& registration : application::FsmRegistration::Registrations())
            if (!WriteMermaidFile(registration, directory))
            {
                std::cerr << "cannot write " << registration.Name() << ".mmd to " << directory << std::endl;
                written = false;
            }

        return written;
    }

    application::FsmValidatorOptions Options(bool strict, bool info, const std::vector<std::string>& names)
    {
        application::FsmValidatorOptions options;
        options.minimum = info ? services::Severity::info : services::Severity::warning;
        options.failAt = strict ? services::Severity::warning : services::Severity::error;
        options.names = names;
        return options;
    }
}

int main(int argc, char* argv[])
{
    std::string toolname = argv[0];
    args::ArgumentParser parser(toolname + " validates the transition tables of the state machines linked into it.");
    args::HelpFlag help(parser, "help", "Show this help", { 'h', "help" });
    args::Flag strict(parser, "strict", "Fail on warnings as well as on errors", { "strict" });
    args::Flag info(parser, "info", "Also report informational findings", { "info" });
    args::Flag list(parser, "list", "List the registered state machines", { "list" });
    args::ValueFlag<std::string> mermaid(parser, "directory", "Write a <name>.mmd diagram for every state machine to directory", { "mermaid" });
    args::PositionalList<std::string> names(parser, "names", "State machines to validate; all when omitted");

    try
    {
        parser.Prog(toolname);
        parser.ParseCLI(argc, argv);
    }
    catch (const args::Help&)
    {
        std::cout << parser;
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << ex.what() << std::endl;
        return 1;
    }

    infra::IoOutputStream report;
    application::FsmValidator validator(application::FsmRegistration::Registrations(), report);

    if (list)
    {
        validator.List();
        return 0;
    }

    if (mermaid && !WriteMermaidFiles(args::get(mermaid)))
        return 1;

    return validator.Validate(Options(strict, info, args::get(names))) ? 0 : 1;
}
