# fuzzing

**fastgltf** has multiple fuzzing targets that are built usings LLVM's libFuzzer. These can be configured through CMake by passing `FASTGLTF_ENABLE_FUZZING=ON` while configuring which will expose CMake targets called `*_fuzzer`.

> [!NOTE]
> Since these fuzzing targets are written using libFuzzer they can only be run on full LLVM installations. Also, AppleClang does not ship libFuzzer. macOS users therefore need to use Homebrew's LLVM.

## Running gltf_fuzzer

As a first step it is recommended to let libFuzzer merge all of the glTF-Sample-Assets (and other samples such as benchmark assets, Blender exports, ...) to generate a compact and effective set of fuzzing data by passing `-merge=1` like so:

```sh
./build/fuzzing/tests/fuzz/gltf_fuzzer -merge=1 -max_len=65536. ./build/fuzzing/tests/fuzz/corpus/gltf_fuzzer ./tests/gltf
```

> [!NOTE]
> The CMake script will automatically create a `${CMAKE_CURRENT_BINARY_DIR}/corpus/*_fuzzer` directory meant to store any sample or merged data as well as a `${CMAKE_CURRENT_BINARY_DIR}/artifacts` directory for storing files that produced crashes, OOMs, ...
