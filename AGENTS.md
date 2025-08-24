## Build Instructions

This project now uses an intelligent build system with automatic caching for faster builds. AIs can build the project with just two simple commands:

### Quick Build (Recommended for AIs)

```bash
# Build project with cached compilled files (should be always used for speed up builds, unless full build is necessary for some reason)
./pre-build.sh
```
### Advanced Options

```bash
# Force rebuild (clears cache)
./pre-build.sh --rebuild

# Custom build parameters
./pre-build.sh --build-params "-DUSE_CEF=ON -DCMAKE_BUILD_TYPE=Debug"

# Show help
./pre-build.sh --help
```

**Note:** The new system is much faster and handles all the complexity automatically. Use it for the best development experience!