# fastgltf

A superfast SIMD powered glTF 2.0 parser written in C++17 with no additional dependencies.
Unlike other glTF parsers, it does not automatically load textures and load GLB binaries to allow
the user to optimise to their liking.

## Acknowledgments

The major speedup comes from the awesome [`simdjson`](https://github.com/simdjson/simdjson) JSON
library which can use a multitude of different SIMD intrinsics to increase JSON decoding speed.

The SIMD-based base64 decoding algorithms come from
[Wojciech Muła](http://0x80.pl/notesen/2016-01-17-sse-base64-decoding.html#avx2-version), which can
usually nearly triple the decoding speed with AVX2.

## License

The **fastgltf** library is licensed under the MIT License.

----

Third party licenses:
- [simdjson](https://github.com/simdjson/simdjson): Licensed under Apache 2.0.
