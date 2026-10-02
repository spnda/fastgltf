window.BENCHMARK_DATA = {
  "lastUpdate": 1790965307400,
  "repoUrl": "https://github.com/spnda/fastgltf",
  "entries": {
    "fastgltf (Linux x64)": [
      {
        "commit": {
          "author": {
            "email": "sean165@outlook.de",
            "name": "sean",
            "username": "spnda"
          },
          "committer": {
            "email": "sean165@outlook.de",
            "name": "sean",
            "username": "spnda"
          },
          "distinct": true,
          "id": "586d6859601b4871f11763b2f5bcb3daa5a0cb0f",
          "message": "Disable long & demanding crc benchmarks in benchmark runner",
          "timestamp": "2026-09-29T05:25:46+02:00",
          "tree_id": "79842415b4da4aca0731a87da3ca100d2e984a57",
          "url": "https://github.com/spnda/fastgltf/commit/586d6859601b4871f11763b2f5bcb3daa5a0cb0f"
        },
        "date": 1790652534826,
        "tool": "catch2",
        "benches": [
          {
            "name": "Minify Sponza.gltf",
            "value": 36.7707,
            "range": "± 6.47512",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with normal JSON",
            "value": 339.046,
            "range": "± 65.3237",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with minified JSON",
            "value": 327.642,
            "range": "± 59.6666",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf",
            "value": 331.85,
            "range": "± 65.396",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Default 1-byte tabular algorithm",
            "value": 19.1516,
            "range": "± 1.1776",
            "unit": "ns",
            "extra": "50 samples\n1529 iterations"
          },
          {
            "name": "SSE4 hardware algorithm",
            "value": 6.63743,
            "range": "± 0.546325",
            "unit": "ns",
            "extra": "50 samples\n8406 iterations"
          },
          {
            "name": "Parse MetalRoughSpheres and decode base64",
            "value": 3.93005,
            "range": "± 145.328",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's fallback base64 decoder",
            "value": 1.11065,
            "range": "± 125.566",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's SSE4 base64 decoder",
            "value": 180.119,
            "range": "± 69.4973",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's AVX2 base64 decoder",
            "value": 99.5763,
            "range": "± 37.3489",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "sean165@outlook.de",
            "name": "sean",
            "username": "spnda"
          },
          "committer": {
            "email": "sean165@outlook.de",
            "name": "sean",
            "username": "spnda"
          },
          "distinct": true,
          "id": "d11e7b7622031c60d0413f876a3aaaf0f43a3f6c",
          "message": "Add CI for C++20 modules, rename workflow files",
          "timestamp": "2026-09-29T06:46:31+02:00",
          "tree_id": "49a363e71628bafcafa8dcbcba1b8318363db9c1",
          "url": "https://github.com/spnda/fastgltf/commit/d11e7b7622031c60d0413f876a3aaaf0f43a3f6c"
        },
        "date": 1790657502892,
        "tool": "catch2",
        "benches": [
          {
            "name": "Minify Sponza.gltf",
            "value": 8.23962,
            "range": "± 354.552",
            "unit": "us",
            "extra": "50 samples\n3 iterations"
          },
          {
            "name": "Parse Sponza.gltf with normal JSON",
            "value": 220.164,
            "range": "± 8.84185",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with minified JSON",
            "value": 220.243,
            "range": "± 8.25242",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf",
            "value": 222.894,
            "range": "± 8.0058",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Default 1-byte tabular algorithm",
            "value": 16.2611,
            "range": "± 0.808329",
            "unit": "ns",
            "extra": "50 samples\n1486 iterations"
          },
          {
            "name": "SSE4 hardware algorithm",
            "value": 2.7279,
            "range": "± 0.145542",
            "unit": "ns",
            "extra": "50 samples\n8810 iterations"
          },
          {
            "name": "Parse MetalRoughSpheres and decode base64",
            "value": 2.10582,
            "range": "± 27.3857",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's fallback base64 decoder",
            "value": 0.8733630000000001,
            "range": "± 0.006281310000000001",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's SSE4 base64 decoder",
            "value": 118.97,
            "range": "± 3.13458",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's AVX2 base64 decoder",
            "value": 66.2434,
            "range": "± 4.72696",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "sean165@outlook.de",
            "name": "sean",
            "username": "spnda"
          },
          "committer": {
            "email": "sean165@outlook.de",
            "name": "sean",
            "username": "spnda"
          },
          "distinct": true,
          "id": "b46c2ff86cdc6ad6222f83f831c872ebbf0e29fd",
          "message": "Declare global variables inline constexpr, use VS2026 for C++ modules",
          "timestamp": "2026-09-29T07:01:07+02:00",
          "tree_id": "3820a168e9a82efe2424b2f94b11f79c27c7c0be",
          "url": "https://github.com/spnda/fastgltf/commit/b46c2ff86cdc6ad6222f83f831c872ebbf0e29fd"
        },
        "date": 1790658250741,
        "tool": "catch2",
        "benches": [
          {
            "name": "Parse MetalRoughSpheres and decode base64",
            "value": 2.90174,
            "range": "± 550.375",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Default 1-byte tabular algorithm",
            "value": 12.493,
            "range": "± 0.060036",
            "unit": "ns",
            "extra": "50 samples\n1339 iterations"
          },
          {
            "name": "SSE4 hardware algorithm",
            "value": 1.68693,
            "range": "± 0.0963512",
            "unit": "ns",
            "extra": "50 samples\n9207 iterations"
          },
          {
            "name": "Minify Sponza.gltf",
            "value": 6.54347,
            "range": "± 1.11296",
            "unit": "us",
            "extra": "50 samples\n3 iterations"
          },
          {
            "name": "Parse Sponza.gltf with normal JSON",
            "value": 167.213,
            "range": "± 11.983",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with minified JSON",
            "value": 166.653,
            "range": "± 11.1554",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf",
            "value": 168.991,
            "range": "± 11.6834",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's fallback base64 decoder",
            "value": 0.679756,
            "range": "± 0.0717101",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's SSE4 base64 decoder",
            "value": 126.866,
            "range": "± 18.5819",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's AVX2 base64 decoder",
            "value": 125.332,
            "range": "± 18.5985",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "sean165@outlook.de",
            "name": "sean",
            "username": "spnda"
          },
          "committer": {
            "email": "sean165@outlook.de",
            "name": "sean",
            "username": "spnda"
          },
          "distinct": true,
          "id": "9003cdd1e9e50ea07d2d11dc82f4051bc05a0fb6",
          "message": "Fix #86: Move ordinary includes before the module purview",
          "timestamp": "2026-09-29T07:29:42+02:00",
          "tree_id": "c2ca30dbda58ee2d0df6ab581f49640432a08a8b",
          "url": "https://github.com/spnda/fastgltf/commit/9003cdd1e9e50ea07d2d11dc82f4051bc05a0fb6"
        },
        "date": 1790659970326,
        "tool": "catch2",
        "benches": [
          {
            "name": "Minify Sponza.gltf",
            "value": 9.05936,
            "range": "± 4.10777",
            "unit": "us",
            "extra": "50 samples\n3 iterations"
          },
          {
            "name": "Parse Sponza.gltf with normal JSON",
            "value": 269.021,
            "range": "± 21.2759",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with minified JSON",
            "value": 267.543,
            "range": "± 19.4455",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse MetalRoughSpheres and decode base64",
            "value": 4.38296,
            "range": "± 523.391",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Default 1-byte tabular algorithm",
            "value": 17.6153,
            "range": "± 0.562709",
            "unit": "ns",
            "extra": "50 samples\n1059 iterations"
          },
          {
            "name": "SSE4 hardware algorithm",
            "value": 2.87418,
            "range": "± 0.00412389",
            "unit": "ns",
            "extra": "50 samples\n5954 iterations"
          },
          {
            "name": "Parse Sponza.gltf",
            "value": 267.818,
            "range": "± 18.9439",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's fallback base64 decoder",
            "value": 0.944121,
            "range": "± 0.00856622",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's SSE4 base64 decoder",
            "value": 188.661,
            "range": "± 20.2269",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's AVX2 base64 decoder",
            "value": 127.275,
            "range": "± 25.3342",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "sean165@outlook.de",
            "name": "sean",
            "username": "spnda"
          },
          "committer": {
            "email": "sean165@outlook.de",
            "name": "sean",
            "username": "spnda"
          },
          "distinct": true,
          "id": "baeea08a25f547fa21510453ced87ad4654e2a54",
          "message": "Reorder includes in modules test file because of GCC (?)",
          "timestamp": "2026-09-29T07:34:03+02:00",
          "tree_id": "cee516cc44dbbb38080d5b75bd830579eb81e44c",
          "url": "https://github.com/spnda/fastgltf/commit/baeea08a25f547fa21510453ced87ad4654e2a54"
        },
        "date": 1790660321005,
        "tool": "catch2",
        "benches": [
          {
            "name": "Parse MetalRoughSpheres and decode base64",
            "value": 2.84605,
            "range": "± 593.686",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf",
            "value": 213.467,
            "range": "± 6.73092",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Default 1-byte tabular algorithm",
            "value": 13.4456,
            "range": "± 0.459706",
            "unit": "ns",
            "extra": "50 samples\n1436 iterations"
          },
          {
            "name": "SSE4 hardware algorithm",
            "value": 3.65917,
            "range": "± 0.139996",
            "unit": "ns",
            "extra": "50 samples\n5151 iterations"
          },
          {
            "name": "Minify Sponza.gltf",
            "value": 7.96497,
            "range": "± 563.823",
            "unit": "us",
            "extra": "50 samples\n3 iterations"
          },
          {
            "name": "Parse Sponza.gltf with normal JSON",
            "value": 213.566,
            "range": "± 10.4026",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with minified JSON",
            "value": 218.378,
            "range": "± 13.0971",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's fallback base64 decoder",
            "value": 0.770167,
            "range": "± 0.0262033",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's SSE4 base64 decoder",
            "value": 130.264,
            "range": "± 26.4741",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's AVX2 base64 decoder",
            "value": 116.283,
            "range": "± 24.1482",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "sean165@outlook.de",
            "name": "sean",
            "username": "spnda"
          },
          "committer": {
            "email": "sean165@outlook.de",
            "name": "sean",
            "username": "spnda"
          },
          "distinct": true,
          "id": "1ac090d0eca56a7164aa1350673070dcc0b40e91",
          "message": "Disable NEON base64 funcs on Windows temporarily (see #154)",
          "timestamp": "2026-09-29T10:37:44+02:00",
          "tree_id": "df5e99af8f9ef1e59e945bb3ece69c36a524e9f1",
          "url": "https://github.com/spnda/fastgltf/commit/1ac090d0eca56a7164aa1350673070dcc0b40e91"
        },
        "date": 1790671233841,
        "tool": "catch2",
        "benches": [
          {
            "name": "Parse Sponza.gltf",
            "value": 153.669,
            "range": "± 7.74375",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse MetalRoughSpheres and decode base64",
            "value": 1.66217,
            "range": "± 50.941",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's fallback base64 decoder",
            "value": 0.66966,
            "range": "± 0.00381323",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's SSE4 base64 decoder",
            "value": 92.9089,
            "range": "± 2.61187",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's AVX2 base64 decoder",
            "value": 63.4481,
            "range": "± 4.57555",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Default 1-byte tabular algorithm",
            "value": 12.3407,
            "range": "± 0.461098",
            "unit": "ns",
            "extra": "50 samples\n1646 iterations"
          },
          {
            "name": "SSE4 hardware algorithm",
            "value": 1.60988,
            "range": "± 0.0575766",
            "unit": "ns",
            "extra": "50 samples\n12177 iterations"
          },
          {
            "name": "Minify Sponza.gltf",
            "value": 6.11306,
            "range": "± 324.621",
            "unit": "us",
            "extra": "50 samples\n4 iterations"
          },
          {
            "name": "Parse Sponza.gltf with normal JSON",
            "value": 161.914,
            "range": "± 10.8843",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with minified JSON",
            "value": 161.292,
            "range": "± 7.20877",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "sean165@outlook.de",
            "name": "sean",
            "username": "spnda"
          },
          "committer": {
            "email": "sean165@outlook.de",
            "name": "sean",
            "username": "spnda"
          },
          "distinct": true,
          "id": "6da0b3d4bf29655708d20c7331219f14497c3797",
          "message": "Fix 1ac090d: Use correct feature-test macros in tests",
          "timestamp": "2026-09-29T10:46:53+02:00",
          "tree_id": "f90711dcd05622435ed7326024e1fc49d93e51d1",
          "url": "https://github.com/spnda/fastgltf/commit/6da0b3d4bf29655708d20c7331219f14497c3797"
        },
        "date": 1790671804994,
        "tool": "catch2",
        "benches": [
          {
            "name": "Minify Sponza.gltf",
            "value": 29.531,
            "range": "± 1.72622",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with normal JSON",
            "value": 305.628,
            "range": "± 8.34882",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with minified JSON",
            "value": 306.74,
            "range": "± 18.3294",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf",
            "value": 307.383,
            "range": "± 12.167",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse MetalRoughSpheres and decode base64",
            "value": 3.02311,
            "range": "± 141.594",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's fallback base64 decoder",
            "value": 1.05421,
            "range": "± 8.92292",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's SSE4 base64 decoder",
            "value": 148.631,
            "range": "± 12.0955",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's AVX2 base64 decoder",
            "value": 78.9523,
            "range": "± 8.46683",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Default 1-byte tabular algorithm",
            "value": 18.4017,
            "range": "± 0.788225",
            "unit": "ns",
            "extra": "50 samples\n1543 iterations"
          },
          {
            "name": "SSE4 hardware algorithm",
            "value": 3.41461,
            "range": "± 0.164587",
            "unit": "ns",
            "extra": "50 samples\n8469 iterations"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "43609023+spnda@users.noreply.github.com",
            "name": "Sean Apeler",
            "username": "spnda"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "8cd13ff1794159d447fdd8923ea2b04a85fc2578",
          "message": "Merge pull request #155 from 1runeberg/fix/android-glb-directory-check\n\n[Android] Skip dir check on loadGltfBinary to match loadGltfJson",
          "timestamp": "2026-10-01T20:39:47+02:00",
          "tree_id": "67e393f15257637852b4390a0ab939fe2cef81a9",
          "url": "https://github.com/spnda/fastgltf/commit/8cd13ff1794159d447fdd8923ea2b04a85fc2578"
        },
        "date": 1790880134975,
        "tool": "catch2",
        "benches": [
          {
            "name": "Run fastgltf's fallback base64 decoder",
            "value": 0.677329,
            "range": "± 0.0050123400000000005",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's SSE4 base64 decoder",
            "value": 95.4648,
            "range": "± 5.41077",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's AVX2 base64 decoder",
            "value": 63.7189,
            "range": "± 4.79932",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Minify Sponza.gltf",
            "value": 6.13305,
            "range": "± 253.597",
            "unit": "us",
            "extra": "50 samples\n4 iterations"
          },
          {
            "name": "Parse Sponza.gltf with normal JSON",
            "value": 158.617,
            "range": "± 7.85506",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with minified JSON",
            "value": 158.108,
            "range": "± 9.49468",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf",
            "value": 161.658,
            "range": "± 9.34925",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse MetalRoughSpheres and decode base64",
            "value": 1.63218,
            "range": "± 75.4321",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Default 1-byte tabular algorithm",
            "value": 13.244,
            "range": "± 1.58772",
            "unit": "ns",
            "extra": "50 samples\n1680 iterations"
          },
          {
            "name": "SSE4 hardware algorithm",
            "value": 1.97815,
            "range": "± 1.21812",
            "unit": "ns",
            "extra": "50 samples\n12239 iterations"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "sean165@outlook.de",
            "name": "sean",
            "username": "spnda"
          },
          "committer": {
            "email": "sean165@outlook.de",
            "name": "sean",
            "username": "spnda"
          },
          "distinct": true,
          "id": "c8d28ee7506176dc2bf5a5aa763cb5638d70e545",
          "message": "fix alias templates not having proper deduction",
          "timestamp": "2026-10-02T20:06:42+02:00",
          "tree_id": "ffeff287bf0ce3ccb5c9837382dd88a78a220b77",
          "url": "https://github.com/spnda/fastgltf/commit/c8d28ee7506176dc2bf5a5aa763cb5638d70e545"
        },
        "date": 1790964600900,
        "tool": "catch2",
        "benches": [
          {
            "name": "Minify Sponza.gltf",
            "value": 8.45383,
            "range": "± 3.124",
            "unit": "us",
            "extra": "50 samples\n3 iterations"
          },
          {
            "name": "Parse Sponza.gltf with normal JSON",
            "value": 264.963,
            "range": "± 16.5244",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with minified JSON",
            "value": 266.258,
            "range": "± 19.7409",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse MetalRoughSpheres and decode base64",
            "value": 4.51852,
            "range": "± 447.852",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf",
            "value": 267.064,
            "range": "± 20.7688",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's fallback base64 decoder",
            "value": 0.945012,
            "range": "± 0.0105725",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's SSE4 base64 decoder",
            "value": 155.376,
            "range": "± 22.4477",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's AVX2 base64 decoder",
            "value": 128.983,
            "range": "± 27.6268",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Default 1-byte tabular algorithm",
            "value": 17.6403,
            "range": "± 0.714576",
            "unit": "ns",
            "extra": "50 samples\n1050 iterations"
          },
          {
            "name": "SSE4 hardware algorithm",
            "value": 3.0347,
            "range": "± 0.110451",
            "unit": "ns",
            "extra": "50 samples\n6026 iterations"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "sean165@outlook.de",
            "name": "sean",
            "username": "spnda"
          },
          "committer": {
            "email": "sean165@outlook.de",
            "name": "sean",
            "username": "spnda"
          },
          "distinct": true,
          "id": "b41b79f1bf1cfb5874a0e97642356c39662a247f",
          "message": "compare resources instead of relying on the conversion operator from allocators",
          "timestamp": "2026-10-02T20:15:51+02:00",
          "tree_id": "b43ea192e88ef8a7a83756c5aa8a75e2ca461b3e",
          "url": "https://github.com/spnda/fastgltf/commit/b41b79f1bf1cfb5874a0e97642356c39662a247f"
        },
        "date": 1790965105296,
        "tool": "catch2",
        "benches": [
          {
            "name": "Default 1-byte tabular algorithm",
            "value": 11.7991,
            "range": "± 0.0149285",
            "unit": "ns",
            "extra": "50 samples\n1550 iterations"
          },
          {
            "name": "SSE4 hardware algorithm",
            "value": 1.77425,
            "range": "± 0.0571541",
            "unit": "ns",
            "extra": "50 samples\n12161 iterations"
          },
          {
            "name": "Parse MetalRoughSpheres and decode base64",
            "value": 1.43558,
            "range": "± 24.8002",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf",
            "value": 147.487,
            "range": "± 2.74896",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's fallback base64 decoder",
            "value": 0.634309,
            "range": "± 0.0026077400000000003",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's SSE4 base64 decoder",
            "value": 88.0215,
            "range": "± 2.25058",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's AVX2 base64 decoder",
            "value": 47.5895,
            "range": "± 1.47101",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Minify Sponza.gltf",
            "value": 5.72626,
            "range": "± 305.117",
            "unit": "us",
            "extra": "50 samples\n4 iterations"
          },
          {
            "name": "Parse Sponza.gltf with normal JSON",
            "value": 148.483,
            "range": "± 3.06038",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with minified JSON",
            "value": 148.759,
            "range": "± 5.60008",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          }
        ]
      }
    ],
    "fastgltf (Linux arm64)": [
      {
        "commit": {
          "author": {
            "email": "sean165@outlook.de",
            "name": "sean",
            "username": "spnda"
          },
          "committer": {
            "email": "sean165@outlook.de",
            "name": "sean",
            "username": "spnda"
          },
          "distinct": true,
          "id": "586d6859601b4871f11763b2f5bcb3daa5a0cb0f",
          "message": "Disable long & demanding crc benchmarks in benchmark runner",
          "timestamp": "2026-09-29T05:25:46+02:00",
          "tree_id": "79842415b4da4aca0731a87da3ca100d2e984a57",
          "url": "https://github.com/spnda/fastgltf/commit/586d6859601b4871f11763b2f5bcb3daa5a0cb0f"
        },
        "date": 1790652721984,
        "tool": "catch2",
        "benches": [
          {
            "name": "Parse MetalRoughSpheres and decode base64",
            "value": 7.92212,
            "range": "± 61.0475",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's fallback base64 decoder",
            "value": 728.297,
            "range": "± 4.97839",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's Neon base64 decoder",
            "value": 490.3,
            "range": "± 5.57751",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Minify Sponza.gltf",
            "value": 33.8813,
            "range": "± 850.938",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with normal JSON",
            "value": 281.794,
            "range": "± 4.73418",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with minified JSON",
            "value": 283.864,
            "range": "± 7.13823",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf",
            "value": 282.827,
            "range": "± 4.08567",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Default 1-byte tabular algorithm",
            "value": 15.4641,
            "range": "± 0.174375",
            "unit": "ns",
            "extra": "50 samples\n2041 iterations"
          },
          {
            "name": "ARMv8 hardware CRC32-C algorithm",
            "value": 2.06186,
            "range": "± 0.0160556",
            "unit": "ns",
            "extra": "50 samples\n15231 iterations"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "sean165@outlook.de",
            "name": "sean",
            "username": "spnda"
          },
          "committer": {
            "email": "sean165@outlook.de",
            "name": "sean",
            "username": "spnda"
          },
          "distinct": true,
          "id": "d11e7b7622031c60d0413f876a3aaaf0f43a3f6c",
          "message": "Add CI for C++20 modules, rename workflow files",
          "timestamp": "2026-09-29T06:46:31+02:00",
          "tree_id": "49a363e71628bafcafa8dcbcba1b8318363db9c1",
          "url": "https://github.com/spnda/fastgltf/commit/d11e7b7622031c60d0413f876a3aaaf0f43a3f6c"
        },
        "date": 1790657695948,
        "tool": "catch2",
        "benches": [
          {
            "name": "Parse MetalRoughSpheres and decode base64",
            "value": 8.03969,
            "range": "± 93.2824",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's fallback base64 decoder",
            "value": 731.715,
            "range": "± 14.1246",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's Neon base64 decoder",
            "value": 491.919,
            "range": "± 7.05197",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Minify Sponza.gltf",
            "value": 33.812,
            "range": "± 462.329",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with normal JSON",
            "value": 284.572,
            "range": "± 7.00443",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with minified JSON",
            "value": 284.978,
            "range": "± 7.37226",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf",
            "value": 284.605,
            "range": "± 6.7906",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Default 1-byte tabular algorithm",
            "value": 15.4969,
            "range": "± 0.105428",
            "unit": "ns",
            "extra": "50 samples\n2034 iterations"
          },
          {
            "name": "ARMv8 hardware CRC32-C algorithm",
            "value": 2.3421,
            "range": "± 0.0560999",
            "unit": "ns",
            "extra": "50 samples\n15160 iterations"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "sean165@outlook.de",
            "name": "sean",
            "username": "spnda"
          },
          "committer": {
            "email": "sean165@outlook.de",
            "name": "sean",
            "username": "spnda"
          },
          "distinct": true,
          "id": "b46c2ff86cdc6ad6222f83f831c872ebbf0e29fd",
          "message": "Declare global variables inline constexpr, use VS2026 for C++ modules",
          "timestamp": "2026-09-29T07:01:07+02:00",
          "tree_id": "3820a168e9a82efe2424b2f94b11f79c27c7c0be",
          "url": "https://github.com/spnda/fastgltf/commit/b46c2ff86cdc6ad6222f83f831c872ebbf0e29fd"
        },
        "date": 1790658427067,
        "tool": "catch2",
        "benches": [
          {
            "name": "Parse MetalRoughSpheres and decode base64",
            "value": 7.86368,
            "range": "± 98.1848",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf",
            "value": 281.665,
            "range": "± 3.55781",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Minify Sponza.gltf",
            "value": 33.862,
            "range": "± 546.715",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with normal JSON",
            "value": 285.217,
            "range": "± 5.82196",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with minified JSON",
            "value": 285.685,
            "range": "± 3.77884",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Default 1-byte tabular algorithm",
            "value": 15.8684,
            "range": "± 0.167092",
            "unit": "ns",
            "extra": "50 samples\n1990 iterations"
          },
          {
            "name": "ARMv8 hardware CRC32-C algorithm",
            "value": 2.06452,
            "range": "± 0.0216345",
            "unit": "ns",
            "extra": "50 samples\n15218 iterations"
          },
          {
            "name": "Run fastgltf's fallback base64 decoder",
            "value": 727.713,
            "range": "± 5.45029",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's Neon base64 decoder",
            "value": 486.265,
            "range": "± 4.61983",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "sean165@outlook.de",
            "name": "sean",
            "username": "spnda"
          },
          "committer": {
            "email": "sean165@outlook.de",
            "name": "sean",
            "username": "spnda"
          },
          "distinct": true,
          "id": "9003cdd1e9e50ea07d2d11dc82f4051bc05a0fb6",
          "message": "Fix #86: Move ordinary includes before the module purview",
          "timestamp": "2026-09-29T07:29:42+02:00",
          "tree_id": "c2ca30dbda58ee2d0df6ab581f49640432a08a8b",
          "url": "https://github.com/spnda/fastgltf/commit/9003cdd1e9e50ea07d2d11dc82f4051bc05a0fb6"
        },
        "date": 1790660165165,
        "tool": "catch2",
        "benches": [
          {
            "name": "Run fastgltf's fallback base64 decoder",
            "value": 726.354,
            "range": "± 3.67494",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's Neon base64 decoder",
            "value": 487.248,
            "range": "± 4.26901",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Default 1-byte tabular algorithm",
            "value": 15.7905,
            "range": "± 0.15005",
            "unit": "ns",
            "extra": "50 samples\n2000 iterations"
          },
          {
            "name": "ARMv8 hardware CRC32-C algorithm",
            "value": 2.06421,
            "range": "± 0.024666",
            "unit": "ns",
            "extra": "50 samples\n15262 iterations"
          },
          {
            "name": "Parse MetalRoughSpheres and decode base64",
            "value": 7.7481,
            "range": "± 30.0976",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Minify Sponza.gltf",
            "value": 34.0305,
            "range": "± 1.46314",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with normal JSON",
            "value": 285.723,
            "range": "± 4.56377",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with minified JSON",
            "value": 284.319,
            "range": "± 5.07497",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf",
            "value": 285.409,
            "range": "± 3.44822",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "sean165@outlook.de",
            "name": "sean",
            "username": "spnda"
          },
          "committer": {
            "email": "sean165@outlook.de",
            "name": "sean",
            "username": "spnda"
          },
          "distinct": true,
          "id": "baeea08a25f547fa21510453ced87ad4654e2a54",
          "message": "Reorder includes in modules test file because of GCC (?)",
          "timestamp": "2026-09-29T07:34:03+02:00",
          "tree_id": "cee516cc44dbbb38080d5b75bd830579eb81e44c",
          "url": "https://github.com/spnda/fastgltf/commit/baeea08a25f547fa21510453ced87ad4654e2a54"
        },
        "date": 1790660509384,
        "tool": "catch2",
        "benches": [
          {
            "name": "Minify Sponza.gltf",
            "value": 33.8671,
            "range": "± 927.299",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with normal JSON",
            "value": 281.389,
            "range": "± 5.66509",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with minified JSON",
            "value": 282.172,
            "range": "± 5.72044",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Default 1-byte tabular algorithm",
            "value": 15.782,
            "range": "± 0.415685",
            "unit": "ns",
            "extra": "50 samples\n2003 iterations"
          },
          {
            "name": "ARMv8 hardware CRC32-C algorithm",
            "value": 2.06308,
            "range": "± 0.0201567",
            "unit": "ns",
            "extra": "50 samples\n15216 iterations"
          },
          {
            "name": "Parse Sponza.gltf",
            "value": 281.834,
            "range": "± 4.45169",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse MetalRoughSpheres and decode base64",
            "value": 7.78558,
            "range": "± 81.3078",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's fallback base64 decoder",
            "value": 729.067,
            "range": "± 7.96638",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's Neon base64 decoder",
            "value": 486.041,
            "range": "± 6.65735",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "sean165@outlook.de",
            "name": "sean",
            "username": "spnda"
          },
          "committer": {
            "email": "sean165@outlook.de",
            "name": "sean",
            "username": "spnda"
          },
          "distinct": true,
          "id": "1ac090d0eca56a7164aa1350673070dcc0b40e91",
          "message": "Disable NEON base64 funcs on Windows temporarily (see #154)",
          "timestamp": "2026-09-29T10:37:44+02:00",
          "tree_id": "df5e99af8f9ef1e59e945bb3ece69c36a524e9f1",
          "url": "https://github.com/spnda/fastgltf/commit/1ac090d0eca56a7164aa1350673070dcc0b40e91"
        },
        "date": 1790671434100,
        "tool": "catch2",
        "benches": [
          {
            "name": "Parse Sponza.gltf",
            "value": 282.516,
            "range": "± 4.12337",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse MetalRoughSpheres and decode base64",
            "value": 7.96148,
            "range": "± 84.2288",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Minify Sponza.gltf",
            "value": 33.825,
            "range": "± 365.692",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with normal JSON",
            "value": 281.96,
            "range": "± 5.85829",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with minified JSON",
            "value": 283.119,
            "range": "± 4.37011",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's fallback base64 decoder",
            "value": 729.202,
            "range": "± 4.8178",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's Neon base64 decoder",
            "value": 488.176,
            "range": "± 8.04322",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Default 1-byte tabular algorithm",
            "value": 15.8163,
            "range": "± 0.194734",
            "unit": "ns",
            "extra": "50 samples\n2003 iterations"
          },
          {
            "name": "ARMv8 hardware CRC32-C algorithm",
            "value": 2.06508,
            "range": "± 0.0229975",
            "unit": "ns",
            "extra": "50 samples\n15234 iterations"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "sean165@outlook.de",
            "name": "sean",
            "username": "spnda"
          },
          "committer": {
            "email": "sean165@outlook.de",
            "name": "sean",
            "username": "spnda"
          },
          "distinct": true,
          "id": "6da0b3d4bf29655708d20c7331219f14497c3797",
          "message": "Fix 1ac090d: Use correct feature-test macros in tests",
          "timestamp": "2026-09-29T10:46:53+02:00",
          "tree_id": "f90711dcd05622435ed7326024e1fc49d93e51d1",
          "url": "https://github.com/spnda/fastgltf/commit/6da0b3d4bf29655708d20c7331219f14497c3797"
        },
        "date": 1790672002698,
        "tool": "catch2",
        "benches": [
          {
            "name": "Parse Sponza.gltf",
            "value": 282.79,
            "range": "± 5.39541",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse MetalRoughSpheres and decode base64",
            "value": 8.42114,
            "range": "± 86.9203",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's fallback base64 decoder",
            "value": 734.081,
            "range": "± 6.45846",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's Neon base64 decoder",
            "value": 488.165,
            "range": "± 7.2778",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Default 1-byte tabular algorithm",
            "value": 15.7307,
            "range": "± 0.193013",
            "unit": "ns",
            "extra": "50 samples\n2012 iterations"
          },
          {
            "name": "ARMv8 hardware CRC32-C algorithm",
            "value": 2.08971,
            "range": "± 0.0581229",
            "unit": "ns",
            "extra": "50 samples\n15245 iterations"
          },
          {
            "name": "Minify Sponza.gltf",
            "value": 33.8633,
            "range": "± 408.019",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with normal JSON",
            "value": 283.348,
            "range": "± 5.67347",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with minified JSON",
            "value": 282.145,
            "range": "± 3.38396",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "43609023+spnda@users.noreply.github.com",
            "name": "Sean Apeler",
            "username": "spnda"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "8cd13ff1794159d447fdd8923ea2b04a85fc2578",
          "message": "Merge pull request #155 from 1runeberg/fix/android-glb-directory-check\n\n[Android] Skip dir check on loadGltfBinary to match loadGltfJson",
          "timestamp": "2026-10-01T20:39:47+02:00",
          "tree_id": "67e393f15257637852b4390a0ab939fe2cef81a9",
          "url": "https://github.com/spnda/fastgltf/commit/8cd13ff1794159d447fdd8923ea2b04a85fc2578"
        },
        "date": 1790880948397,
        "tool": "catch2",
        "benches": [
          {
            "name": "Run fastgltf's fallback base64 decoder",
            "value": 727.878,
            "range": "± 5.99878",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's Neon base64 decoder",
            "value": 487.905,
            "range": "± 5.55261",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse MetalRoughSpheres and decode base64",
            "value": 7.80157,
            "range": "± 93.3246",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Minify Sponza.gltf",
            "value": 33.9268,
            "range": "± 577.33",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with normal JSON",
            "value": 285.436,
            "range": "± 8.63586",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with minified JSON",
            "value": 282.387,
            "range": "± 3.6825",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf",
            "value": 283.231,
            "range": "± 4.61486",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Default 1-byte tabular algorithm",
            "value": 15.7435,
            "range": "± 0.156962",
            "unit": "ns",
            "extra": "50 samples\n2002 iterations"
          },
          {
            "name": "ARMv8 hardware CRC32-C algorithm",
            "value": 2.06388,
            "range": "± 0.019388",
            "unit": "ns",
            "extra": "50 samples\n15252 iterations"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "sean165@outlook.de",
            "name": "sean",
            "username": "spnda"
          },
          "committer": {
            "email": "sean165@outlook.de",
            "name": "sean",
            "username": "spnda"
          },
          "distinct": true,
          "id": "c8d28ee7506176dc2bf5a5aa763cb5638d70e545",
          "message": "fix alias templates not having proper deduction",
          "timestamp": "2026-10-02T20:06:42+02:00",
          "tree_id": "ffeff287bf0ce3ccb5c9837382dd88a78a220b77",
          "url": "https://github.com/spnda/fastgltf/commit/c8d28ee7506176dc2bf5a5aa763cb5638d70e545"
        },
        "date": 1790964800740,
        "tool": "catch2",
        "benches": [
          {
            "name": "Parse MetalRoughSpheres and decode base64",
            "value": 8.06948,
            "range": "± 128.74",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Default 1-byte tabular algorithm",
            "value": 15.9667,
            "range": "± 0.435036",
            "unit": "ns",
            "extra": "50 samples\n1987 iterations"
          },
          {
            "name": "ARMv8 hardware CRC32-C algorithm",
            "value": 2.06367,
            "range": "± 0.0207917",
            "unit": "ns",
            "extra": "50 samples\n15232 iterations"
          },
          {
            "name": "Parse Sponza.gltf",
            "value": 281.016,
            "range": "± 4.37108",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Minify Sponza.gltf",
            "value": 33.8913,
            "range": "± 476.062",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with normal JSON",
            "value": 281.942,
            "range": "± 7.19725",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with minified JSON",
            "value": 281.352,
            "range": "± 4.45139",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's fallback base64 decoder",
            "value": 732.355,
            "range": "± 9.4355",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's Neon base64 decoder",
            "value": 488.996,
            "range": "± 5.34928",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "sean165@outlook.de",
            "name": "sean",
            "username": "spnda"
          },
          "committer": {
            "email": "sean165@outlook.de",
            "name": "sean",
            "username": "spnda"
          },
          "distinct": true,
          "id": "b41b79f1bf1cfb5874a0e97642356c39662a247f",
          "message": "compare resources instead of relying on the conversion operator from allocators",
          "timestamp": "2026-10-02T20:15:51+02:00",
          "tree_id": "b43ea192e88ef8a7a83756c5aa8a75e2ca461b3e",
          "url": "https://github.com/spnda/fastgltf/commit/b41b79f1bf1cfb5874a0e97642356c39662a247f"
        },
        "date": 1790965306665,
        "tool": "catch2",
        "benches": [
          {
            "name": "Default 1-byte tabular algorithm",
            "value": 15.7428,
            "range": "± 0.153903",
            "unit": "ns",
            "extra": "50 samples\n2001 iterations"
          },
          {
            "name": "ARMv8 hardware CRC32-C algorithm",
            "value": 2.10958,
            "range": "± 0.0706903",
            "unit": "ns",
            "extra": "50 samples\n15269 iterations"
          },
          {
            "name": "Minify Sponza.gltf",
            "value": 33.9768,
            "range": "± 1.08794",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with normal JSON",
            "value": 284.004,
            "range": "± 7.45251",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with minified JSON",
            "value": 283.729,
            "range": "± 5.13262",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse MetalRoughSpheres and decode base64",
            "value": 8.21549,
            "range": "± 102.879",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's fallback base64 decoder",
            "value": 728.631,
            "range": "± 5.07642",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's Neon base64 decoder",
            "value": 490.417,
            "range": "± 5.16675",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf",
            "value": 284.005,
            "range": "± 7.22209",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          }
        ]
      }
    ]
  }
}