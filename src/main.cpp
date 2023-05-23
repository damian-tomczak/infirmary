#include "server.h"

#include "boost/program_options.hpp"
#include "fmt/core.h"
#include "fmt/color.h"
#include "fmt/ostream.h"

#include <iostream>

namespace po = boost::program_options;

int main(int argc, char** argv)
{
    try
    {
        std::string mailLogin;
        std::string mailPassword;

        po::options_description desc("Allowed options");
        desc.add_options()
            ("help,h", "produce help message")
            ("mailLogin,l", po::value<std::string>(&mailLogin)->required(), "set mail login")
            ("mailPassword,p", po::value<std::string>(&mailPassword)->required(), "set mail password");

        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);

        if (vm.count("help"))
        {
            std::cout << desc << "\n";
            return EXIT_SUCCESS;
        }

        po::notify(vm);

        tsrpp::Server server{mailLogin, mailPassword};
        server.run();
    }
    catch(const std::exception& e)
    {
        fmt::print(std::cerr, fmt::format(fmt::fg(fmt::color::red), "main::exception {}\n", e.what()));
        return EXIT_FAILURE;
    }
    catch(...)
    {
        fmt::print(std::cerr, fmt::format(fmt::fg(fmt::color::red), "unknown error\n"));
    }

    return EXIT_SUCCESS;
}