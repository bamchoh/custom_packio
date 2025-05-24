#include <iostream>

#include <packio/packio.h>
#include "custom_rpc/nl_json_rpc.h"
#include "custom_rpc/rpc.h"

using packio::allow_extra_arguments;
using packio::arg;
using packio::nl_json_rpc::completion_handler;
using custom_rpc::nl_json_rpc::make_client;
using custom_rpc::nl_json_rpc::make_server;
using custom_rpc::nl_json_rpc::rpc;
using namespace packio::arg_literals;

int main(int, char**)
{
    // spdlog::set_level(spdlog::level::trace); // Set global log level to debug

    using namespace packio::arg_literals;

    // Declare a server and a client, sharing the same io_context
    packio::net::io_context io;
    packio::net::ip::tcp::endpoint bind_ep{
        packio::net::ip::make_address("0.0.0.0"), 43925};
    auto server = make_server(packio::net::ip::tcp::acceptor{io, bind_ep});

    // Declare a synchronous callback with named arguments
    server->dispatcher()->add(
        "add", {"a", "b"}, [](int a, int b) { return a + b; });

    server->dispatcher()->add(
        "shutdown",
        [&io]() { io.stop(); });

    // Accept connections
    server->async_serve_forever();
    // Run the io_context
    std::thread thread{[&] { io.run(); }};

    std::cout << "Server listening on port "
              << server->acceptor().local_endpoint().port() << std::endl;

    /*
    auto client = make_client(packio::net::ip::tcp::socket{io});
    client->socket().connect(server->acceptor().local_endpoint());
    std::promise<int> add1_result;
    client->async_call(
        "add",
        std::tuple{arg("a") = 42, "b"_arg = 24},
        [&](packio::error_code, const rpc::response_type& r) {
            add1_result.set_value(r.result.get<int>());
        });
    std::cout << "42 + 24 = " << add1_result.get_future().get() << std::endl;    
    */

    thread.join();

    return 0;
}