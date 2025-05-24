// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#ifndef CUSTOM_RPC_NL_JSON_RPC_RPC_H
#define CUSTOM_RPC_NL_JSON_RPC_RPC_H

#include <array>
#include <optional>

#include <nlohmann/json.hpp>

#include "packio/arg.h"
#include "packio/args_specs.h"
#include "packio/internal/config.h"
#include "packio/internal/expected.h"
#include "packio/internal/log.h"
#include "packio/internal/rpc.h"
#include "packio/nl_json_rpc/incremental_buffers.h"


#include "llhttp.h"
#include <iostream>
#include <string>
#include <optional>


typedef struct {
    std::string body;
} parser_context_t;

// on_body コールバック
int on_body(llhttp_t* parser, const char* at, size_t length) {
    parser_context_t* context = (parser_context_t*) parser->data;
    context->body += std::string(at, length);
    return 0;
}

class http_parser {
    public:
        std::optional<std::string> parse(std::string request) {
            llhttp_t parser;
            llhttp_settings_t settings;
            parser_context_t context = {};

            llhttp_settings_init(&settings);
            settings.on_body = on_body;
            llhttp_init(&parser, HTTP_BOTH, &settings);
            parser.data = &context;

            enum llhttp_errno err = llhttp_execute(&parser, request.c_str(), request.length());
            if (err != HPE_OK) {
                // fprintf(stderr, "Parse error: %s %s\n", llhttp_errno_name(err), llhttp_get_error_reason(&parser));
                return std::optional<std::string>();
            }

            return std::optional<std::string>(context.body);
        }
};


namespace custom_rpc {
namespace nl_json_rpc {
namespace internal {

template <typename... Args>
constexpr bool positional_args_v = (!packio::is_arg_v<Args> && ...);

template <typename... Args>
constexpr bool named_args_v = sizeof...(Args) > 0 && (packio::is_arg_v<Args> && ...);

using id_type = nlohmann::json;
using native_type = nlohmann::json;
using packio::internal::expected;
using packio::internal::unexpected;

//! The incremental parser for JSON-RPC objects
class incremental_parser {
public:
    expected<packio::nl_json_rpc::internal::request, std::string> get_request()
    {
        try_parse_object();
        if (!parsed_) {
            return unexpected{"no request parsed"};
        }
        auto object = std::move(*parsed_);
        parsed_.reset();
        return parse_request(std::move(object));
    }

    expected<packio::nl_json_rpc::internal::response, std::string> get_response()
    {
        try_parse_object();
        if (!parsed_) {
            return unexpected{"no response parsed"};
        }
        auto object = std::move(*parsed_);
        parsed_.reset();
        return parse_response(std::move(object));
    }

    char* buffer()
    { //
        return incremental_buffers_.in_place_buffer();
    }

    std::size_t buffer_capacity() const
    { //
        return incremental_buffers_.in_place_buffer_capacity();
    }

    void buffer_consumed(std::size_t bytes)
    { //
        incremental_buffers_.in_place_buffer_consumed(bytes);
    }

    void reserve_buffer(std::size_t bytes)
    { //
        incremental_buffers_.reserve_in_place_buffer(bytes);
    }

private:
    void try_parse_object()
    {
        if (parsed_) {
            PACKIO_TRACE("already parsed");
            return;
        }
        auto buffer = incremental_buffers_.get_parsed_buffer();
        PACKIO_TRACE("try parse object: {}", buffer ? *buffer : "null");
        if (buffer) {
            auto http_parsed_ = http_parser_.parse(*buffer);
            if(http_parsed_) {
                parsed_ = nlohmann::json::parse(*http_parsed_);
            } else {
                parsed_ = nlohmann::json::parse(*buffer);
            }
        }
    }

    static expected<packio::nl_json_rpc::internal::response, std::string> parse_response(nlohmann::json&& res)
    {
        auto id_it = res.find("id");
        auto result_it = res.find("result");
        auto error_it = res.find("error");

        if (id_it == end(res)) {
            return unexpected{"missing id field"};
        }
        if (result_it == end(res) && error_it == end(res)) {
            return unexpected{"missing error and result field"};
        }

        packio::nl_json_rpc::internal::response parsed;
        parsed.id = std::move(*id_it);
        if (error_it != end(res)) {
            parsed.error = std::move(*error_it);
        }
        if (result_it != end(res)) {
            parsed.result = std::move(*result_it);
        }
        return {std::move(parsed)};
    }

    static expected<packio::nl_json_rpc::internal::request, std::string> parse_request(nlohmann::json&& req)
    {
        auto id_it = req.find("id");
        auto method_it = req.find("method");
        auto params_it = req.find("params");

        if (method_it == end(req)) {
            return unexpected{"missing method field"};
        }
        if (!method_it->is_string()) {
            return unexpected{"method field is not a string"};
        }

        packio::nl_json_rpc::internal::request parsed;
        parsed.method = method_it->get<std::string>();
        if (params_it == end(req) || params_it->is_null()) {
            parsed.args = nlohmann::json::array();
        }
        else if (!params_it->is_array() && !params_it->is_object()) {
            return unexpected{"non-structured arguments are not supported"};
        }
        else {
            parsed.args = std::move(*params_it);
        }

        if (id_it == end(req) || id_it->is_null()) {
            parsed.type = packio::call_type::notification;
        }
        else {
            parsed.type = packio::call_type::request;
            parsed.id = std::move(*id_it);
        }
        return {std::move(parsed)};
    }

    std::optional<nlohmann::json> parsed_;
    packio::nl_json_rpc::incremental_buffers incremental_buffers_;
    http_parser http_parser_;
};

} // internal

//! The JSON-RPC protocol implementation
class rpc {
public:
    //! Type of the call ID
    using id_type = internal::id_type;

    //! The native type of the serialization library
    using native_type = internal::native_type;

    //! The type of the parsed request object
    using request_type = packio::nl_json_rpc::internal::request;

    //! The type of the parsed response object
    using response_type = packio::nl_json_rpc::internal::response;

    //! The incremental parser type
    using incremental_parser_type = internal::incremental_parser;

    static std::string format_id(const id_type& id)
    { //
        return id.dump();
    }

    template <typename... Args>
    static auto serialize_notification(std::string_view method, Args&&... args)
        -> std::enable_if_t<internal::positional_args_v<Args...>, std::string>
    {
        return nlohmann::json({
                                  {"jsonrpc", "2.0"},
                                  {"method", method},
                                  {"params",
                                   nlohmann::json::array({nlohmann::json(
                                       std::forward<Args>(args))...})},
                              })
            .dump();
    }

    template <typename... Args>
    static auto serialize_notification(std::string_view method, Args&&... args)
        -> std::enable_if_t<internal::named_args_v<Args...>, std::string>
    {
        return nlohmann::json({
                                  {"jsonrpc", "2.0"},
                                  {"method", method},
                                  {"params", {{args.name, args.value}...}},
                              })
            .dump();
    }

    template <typename... Args>
    static auto serialize_notification(std::string_view, Args&&...) -> std::enable_if_t<
        !internal::positional_args_v<Args...> && !internal::named_args_v<Args...>,
        std::string>
    {
        static_assert(
            internal::positional_args_v<Args...> || internal::named_args_v<Args...>,
            "JSON-RPC does not support mixed named and unnamed arguments");
    }

    template <typename... Args>
    static auto serialize_request(
        const id_type& id,
        std::string_view method,
        Args&&... args)
        -> std::enable_if_t<internal::positional_args_v<Args...>, std::string>
    {
        return nlohmann::json({
                                  {"jsonrpc", "2.0"},
                                  {"method", method},
                                  {"params",
                                   nlohmann::json::array({nlohmann::json(
                                       std::forward<Args>(args))...})},
                                  {"id", id},
                              })
            .dump();
    }

    template <typename... Args>
    static auto serialize_request(
        const id_type& id,
        std::string_view method,
        Args&&... args)
        -> std::enable_if_t<internal::named_args_v<Args...>, std::string>
    {
        return nlohmann::json({
                                  {"jsonrpc", "2.0"},
                                  {"method", method},
                                  {"params", {{args.name, args.value}...}},
                                  {"id", id},
                              })
            .dump();
    }

    template <typename... Args>
    static auto serialize_request(const id_type&, std::string_view, Args&&...)
        -> std::enable_if_t<
            !internal::positional_args_v<Args...> && !internal::named_args_v<Args...>,
            std::string>
    {
        static_assert(
            internal::positional_args_v<Args...> || internal::named_args_v<Args...>,
            "JSON-RPC does not support mixed named and unnamed arguments");
    }

    static std::string serialize_response(const id_type& id)
    {
        return serialize_response(id, nlohmann::json{});
    }

    template <typename T>
    static std::string serialize_response(const id_type& id, T&& value)
    {
        auto jsonstring = nlohmann::json{
            {"jsonrpc", "2.0"},
            {"id", id},
            {"result", std::forward<T>(value)},
        }
        .dump();

        std::stringstream stream_;
        stream_ << "HTTP/1.1 200 OK\r\n";
        stream_ << "Content-Length: " << jsonstring.size() << "\r\n";
        stream_ << "Content-Type: application/json\r\n";
        stream_ << "\r\n";
        stream_ << jsonstring;

        return stream_.str();
    }

    template <typename T>
    static std::string serialize_error_response(const id_type& id, T&& value)
    {
        auto jsonstring = nlohmann::json{
            {"jsonrpc", "2.0"},
            {"id", id},
            {"error",
             [&]() {
                 nlohmann::json error = {
                     {"code", -32000}, // -32000 is an implementation-defined error
                     {"data", std::forward<T>(value)},
                 };
                 if (error["data"].is_string()) {
                     error["message"] = error["data"];
                 }
                 else {
                     error["message"] = "unknown error";
                 }
                 return error;
             }()},
        }.dump();

        std::stringstream stream_;
        stream_ << "HTTP/1.1 200 OK\r\n";
        stream_ << "Content-Length: " << jsonstring.size() << "\r\n";
        stream_ << "Content-Type: application/json\r\n";
        stream_ << "\r\n";
        stream_ << jsonstring;

        return stream_.str();
    }

    static packio::net::const_buffer buffer(const std::string& buf)
    {
        return packio::net::const_buffer(buf.data(), buf.size());
    }

    template <typename T, typename F>
    static internal::expected<T, std::string> extract_args(
        const nlohmann::json& args,
        const packio::internal::args_specs<F>& specs)
    {
        try {
            if (args.is_array()) {
                return convert_positional_args<T>(args, specs);
            }
            else if (args.is_object()) {
                return convert_named_args<T>(args, specs);
            }
            else {
                throw std::runtime_error{"arguments are not a structured type"};
            }
        }
        catch (const std::exception& exc) {
            return internal::unexpected{
                std::string{"cannot convert arguments: "} + exc.what()};
        }
    }

private:
    template <typename T, typename F>
    static constexpr T convert_positional_args(
        const nlohmann::json& array,
        const packio::internal::args_specs<F>& specs)
    {
        return convert_positional_args<T>(
            array, specs, std::make_index_sequence<packio::internal::args_specs<F>::size()>());
    }

    template <typename T, typename F, std::size_t... Idxs>
    static constexpr T convert_positional_args(
        const nlohmann::json& array,
        const packio::internal::args_specs<F>& specs,
        std::index_sequence<Idxs...>)
    {
        if (!specs.options().allow_extra_arguments
            && array.size() > std::tuple_size_v<T>) {
            throw std::runtime_error{"too many arguments"};
        }
        return {[&]() {
            if (Idxs < array.size()) {
                try {
                    return array.at(Idxs).get<std::tuple_element_t<Idxs, T>>();
                }
                catch (const ::nlohmann::json::type_error&) {
                    throw std::runtime_error{
                        "invalid type for argument "
                        + specs.template get<Idxs>().name()};
                }
            }
            if (const auto& value = specs.template get<Idxs>().default_value()) {
                return *value;
            }
            throw std::runtime_error{
                "no value for argument " + specs.template get<Idxs>().name()};
        }()...};
    }

    template <typename T, typename F>
    static T convert_named_args(const nlohmann::json& args, const packio::internal::args_specs<F>& specs)
    {
        return convert_named_args<T>(
            args, specs, std::make_index_sequence<packio::internal::args_specs<F>::size()>{});
    }

    template <typename T, typename F, std::size_t... Idxs>
    static T convert_named_args(
        const nlohmann::json& args,
        const packio::internal::args_specs<F>& specs,
        std::index_sequence<Idxs...>)
    {
        if (!specs.options().allow_extra_arguments) {
            const std::array<const std::string*, sizeof...(Idxs)>
                available_arguments = {&specs.template get<Idxs>().name()...};
            for (auto it = args.begin(); it != args.end(); ++it) {
                auto arg_it = std::find_if(
                    available_arguments.begin(),
                    available_arguments.end(),
                    [&](const std::string* arg) { return *arg == it.key(); });
                if (arg_it == available_arguments.end()) {
                    throw std::runtime_error{"unexpected argument " + it.key()};
                }
            }
        }

        return T{[&]() {
            auto it = args.find(specs.template get<Idxs>().name());
            if (it != args.end()) {
                try {
                    return it->template get<std::tuple_element_t<Idxs, T>>();
                }
                catch (const ::nlohmann::json::type_error&) {
                    throw std::runtime_error{
                        "invalid type for argument "
                        + specs.template get<Idxs>().name()};
                }
            }
            if (const auto& value = specs.template get<Idxs>().default_value()) {
                return *value;
            }
            throw std::runtime_error{
                "no value for argument " + specs.template get<Idxs>().name()};
        }()...};
    }
};

} // nl_json_rpc
} // packio

#endif // PACKIO_NL_JSON_RPC_RPC_H
