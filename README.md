# custom_packio

A sample C++ asynchronous RPC server/client project based on [packio](https://github.com/qchateau/packio), designed for cross-building environments such as Raspberry Pi.

## Structure

- `main.cpp`: Sample implementation of server and client
- `CMakeLists.txt`: CMake build configuration
- `llhttp_build.sh`: Build script for the llhttp HTTP parser
- `packio/`: packio submodule
- `json/`: nlohmann/json submodule
- `spdlog/`: spdlog submodule
- `llhttp/`: HTTP parser llhttp
- `custom_rpc/`: Custom RPC implementation for packio extension

## Initialize Submodules

```sh
git submodule update --init --recursive
```

## Build Instructions

### Normal Build

```sh
mkdir -p build
cd build
cmake ..
cmake --build .
```

### Cross Build (e.g., for Raspberry Pi)

Example toolchain file: `rpi-toolchain.cmake`

```sh
mkdir -p build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../rpi-toolchain.cmake
cmake --build .
```

#### About llhttp Build

The `llhttp_build.sh` script is automatically invoked to generate `llhttp/build/libllhttp.a`.

## Example Execution

```sh
./custom_packio
```

The server starts, and the client calls the `add` RPC and prints the result.

## Dependencies

- [packio](https://github.com/qchateau/packio)
- [nlohmann/json](https://github.com/nlohmann/json)
- [spdlog](https://github.com/gabime/spdlog)
- [llhttp](https://github.com/nodejs/llhttp)
- [Node.js](https://nodejs.org/) (Required for building llhttp)

### Pre-build Requirements

Make sure Node.js is installed on your system before building the project, as it is required to build the llhttp dependency. You can verify your Node.js installation by running:

```sh
node --version
npm --version
```

For more detailed information about llhttp build requirements and configuration options, please refer to the [llhttp repository](https://github.com/nodejs/llhttp).

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

Note: The submodules included in this project are subject to their respective licenses.
