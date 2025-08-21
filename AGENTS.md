## Build Instructions
To build this project, run:

```bash
mkdir -p build && cd build
cmake .. -DUSE_CEF=ON
make -j$(nproc)