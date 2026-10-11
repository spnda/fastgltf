window.BENCHMARK_DATA = {
  "lastUpdate": 1791687253365,
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
          "id": "4cff6c82b51ab010bae58bec91c24e9a42c57b8e",
          "message": "compare resources instead of relying on the conversion operator from allocators",
          "timestamp": "2026-10-02T22:12:49+02:00",
          "tree_id": "314cd5063a6de9722440e53b6f329f4428cf24ba",
          "url": "https://github.com/spnda/fastgltf/commit/4cff6c82b51ab010bae58bec91c24e9a42c57b8e"
        },
        "date": 1790972169892,
        "tool": "catch2",
        "benches": [
          {
            "name": "Parse MetalRoughSpheres and decode base64",
            "value": 3.02061,
            "range": "± 34.6742",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Default 1-byte tabular algorithm",
            "value": 20.7602,
            "range": "± 0.723383",
            "unit": "ns",
            "extra": "50 samples\n1484 iterations"
          },
          {
            "name": "SSE4 hardware algorithm",
            "value": 3.50887,
            "range": "± 0.145634",
            "unit": "ns",
            "extra": "50 samples\n8798 iterations"
          },
          {
            "name": "Minify Sponza.gltf",
            "value": 32.3031,
            "range": "± 3.17155",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with normal JSON",
            "value": 309.369,
            "range": "± 11.5941",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with minified JSON",
            "value": 309.314,
            "range": "± 13.646",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf",
            "value": 309.129,
            "range": "± 14.8954",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's fallback base64 decoder",
            "value": 1.12928,
            "range": "± 5.22999",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's SSE4 base64 decoder",
            "value": 151.727,
            "range": "± 6.21826",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's AVX2 base64 decoder",
            "value": 86.1308,
            "range": "± 6.96259",
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
          "id": "a03222c024e8a96bc00bbd675be1252769c3fb3c",
          "message": "Allow complex types in flagged_optional through flag traits\n\nflagged_optional now uses optional_flag_value<T>::is_empty and optional_flag_value<T>::set_empty to check for or set the flags. This allows the type to store more complex types such as TextureInfo, where we allow the user to store the empty flag somewhere internally.",
          "timestamp": "2026-10-03T00:48:40+02:00",
          "tree_id": "e9fcc2f0ee41116f2c6fb2ef6f6b1887687aa80d",
          "url": "https://github.com/spnda/fastgltf/commit/a03222c024e8a96bc00bbd675be1252769c3fb3c"
        },
        "date": 1790981500985,
        "tool": "catch2",
        "benches": [
          {
            "name": "Parse Sponza.gltf",
            "value": 313.822,
            "range": "± 14.4365",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Default 1-byte tabular algorithm",
            "value": 18.4285,
            "range": "± 0.727846",
            "unit": "ns",
            "extra": "50 samples\n1543 iterations"
          },
          {
            "name": "SSE4 hardware algorithm",
            "value": 3.41179,
            "range": "± 0.145384",
            "unit": "ns",
            "extra": "50 samples\n8460 iterations"
          },
          {
            "name": "Minify Sponza.gltf",
            "value": 29.106,
            "range": "± 1.35269",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with normal JSON",
            "value": 319.652,
            "range": "± 13.5037",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with minified JSON",
            "value": 313.869,
            "range": "± 6.97578",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse MetalRoughSpheres and decode base64",
            "value": 2.99374,
            "range": "± 39.7855",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's fallback base64 decoder",
            "value": 1.0529,
            "range": "± 3.22316",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's SSE4 base64 decoder",
            "value": 132.186,
            "range": "± 4.0497",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's AVX2 base64 decoder",
            "value": 78.308,
            "range": "± 7.17393",
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
          "id": "2571de8a0d7055aeaddd99214e9e7b4cc046078b",
          "message": "Use _mm_crc32_u64 instead of only using the 32-bit instructions",
          "timestamp": "2026-10-03T02:09:06+02:00",
          "tree_id": "46d3d9232361d6f2f406f4a9639da1177c251e92",
          "url": "https://github.com/spnda/fastgltf/commit/2571de8a0d7055aeaddd99214e9e7b4cc046078b"
        },
        "date": 1790986424457,
        "tool": "catch2",
        "benches": [
          {
            "name": "Parse MetalRoughSpheres and decode base64",
            "value": 2.63392,
            "range": "± 87.3125",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Default 1-byte tabular algorithm",
            "value": 19.2934,
            "range": "± 1.02936",
            "unit": "ns",
            "extra": "50 samples\n1458 iterations"
          },
          {
            "name": "SSE4 hardware algorithm",
            "value": 2.56958,
            "range": "± 0.126956",
            "unit": "ns",
            "extra": "50 samples\n10890 iterations"
          },
          {
            "name": "Run fastgltf's fallback base64 decoder",
            "value": 1.03979,
            "range": "± 8.08438",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's SSE4 base64 decoder",
            "value": 135.551,
            "range": "± 4.20958",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's AVX2 base64 decoder",
            "value": 78.9612,
            "range": "± 7.41782",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf",
            "value": 263.096,
            "range": "± 8.53735",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Minify Sponza.gltf",
            "value": 9.6698,
            "range": "± 416.181",
            "unit": "us",
            "extra": "50 samples\n3 iterations"
          },
          {
            "name": "Parse Sponza.gltf with normal JSON",
            "value": 261.3,
            "range": "± 10.8112",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with minified JSON",
            "value": 264.546,
            "range": "± 21.5897",
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
          "id": "270c90758529b4e0b9ed381de7e03ff92373890c",
          "message": "Simplify AccessorBoundsArray and make compatible with flagged_optional",
          "timestamp": "2026-10-03T04:42:19+02:00",
          "tree_id": "d622bf7c8245832cd6d8381d2aa6226dc8ee962e",
          "url": "https://github.com/spnda/fastgltf/commit/270c90758529b4e0b9ed381de7e03ff92373890c"
        },
        "date": 1790995525048,
        "tool": "catch2",
        "benches": [
          {
            "name": "Minify Sponza.gltf",
            "value": 29.582,
            "range": "± 2.95409",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with normal JSON",
            "value": 310.95,
            "range": "± 7.51269",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with minified JSON",
            "value": 310.747,
            "range": "± 8.30664",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Default 1-byte tabular algorithm",
            "value": 19.4358,
            "range": "± 1.97468",
            "unit": "ns",
            "extra": "50 samples\n1540 iterations"
          },
          {
            "name": "SSE4 hardware algorithm",
            "value": 3.1965,
            "range": "± 0.527639",
            "unit": "ns",
            "extra": "50 samples\n9352 iterations"
          },
          {
            "name": "Parse MetalRoughSpheres and decode base64",
            "value": 2.88766,
            "range": "± 47.2313",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf",
            "value": 311.689,
            "range": "± 9.93749",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's fallback base64 decoder",
            "value": 1.05343,
            "range": "± 6.80934",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's SSE4 base64 decoder",
            "value": 145.344,
            "range": "± 5.10682",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's AVX2 base64 decoder",
            "value": 75.9028,
            "range": "± 3.9274",
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
          "id": "ac38e7a8d619594d77f784525d85f8fcbe75e04e",
          "message": "Simplify AccessorBoundsArray and make compatible with flagged_optional",
          "timestamp": "2026-10-03T04:45:56+02:00",
          "tree_id": "6abe5c40169678f41ae28103940dc4ee6f66a2dc",
          "url": "https://github.com/spnda/fastgltf/commit/ac38e7a8d619594d77f784525d85f8fcbe75e04e"
        },
        "date": 1790995871973,
        "tool": "catch2",
        "benches": [
          {
            "name": "Parse MetalRoughSpheres and decode base64",
            "value": 2.9227,
            "range": "± 565.505",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's fallback base64 decoder",
            "value": 0.6942240000000001,
            "range": "± 0.07795189999999999",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's SSE4 base64 decoder",
            "value": 127.328,
            "range": "± 18.782",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's AVX2 base64 decoder",
            "value": 125.984,
            "range": "± 15.6816",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Default 1-byte tabular algorithm",
            "value": 11.3812,
            "range": "± 0.572533",
            "unit": "ns",
            "extra": "50 samples\n1462 iterations"
          },
          {
            "name": "SSE4 hardware algorithm",
            "value": 1.20294,
            "range": "± 0.0587453",
            "unit": "ns",
            "extra": "50 samples\n13906 iterations"
          },
          {
            "name": "Parse Sponza.gltf",
            "value": 165.491,
            "range": "± 24.1942",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Minify Sponza.gltf",
            "value": 6.48591,
            "range": "± 412.676",
            "unit": "us",
            "extra": "50 samples\n3 iterations"
          },
          {
            "name": "Parse Sponza.gltf with normal JSON",
            "value": 163.635,
            "range": "± 14.6195",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with minified JSON",
            "value": 162.294,
            "range": "± 11.0502",
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
          "id": "4101ad522049e8a7583244d993ea88aa138be816",
          "message": "Add for_overwrite_t tag to avoid large memsets with static_vector",
          "timestamp": "2026-10-03T05:35:34+02:00",
          "tree_id": "05e8177cf6b088993aa87035912d0124784b4504",
          "url": "https://github.com/spnda/fastgltf/commit/4101ad522049e8a7583244d993ea88aa138be816"
        },
        "date": 1790998731525,
        "tool": "catch2",
        "benches": [
          {
            "name": "Default 1-byte tabular algorithm",
            "value": 18.4872,
            "range": "± 0.707467",
            "unit": "ns",
            "extra": "50 samples\n1546 iterations"
          },
          {
            "name": "SSE4 hardware algorithm",
            "value": 3.1981,
            "range": "± 0.246848",
            "unit": "ns",
            "extra": "50 samples\n9292 iterations"
          },
          {
            "name": "Parse MetalRoughSpheres and decode base64",
            "value": 3.28053,
            "range": "± 84.8221",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Minify Sponza.gltf",
            "value": 28.9433,
            "range": "± 1.11234",
            "unit": "us",
            "extra": "50 samples\n2 iterations"
          },
          {
            "name": "Parse Sponza.gltf with normal JSON",
            "value": 302.718,
            "range": "± 11.0098",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with minified JSON",
            "value": 306.583,
            "range": "± 15.238",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf",
            "value": 301.412,
            "range": "± 11.5589",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's fallback base64 decoder",
            "value": 1.0543,
            "range": "± 8.31158",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's SSE4 base64 decoder",
            "value": 147.82,
            "range": "± 6.87333",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's AVX2 base64 decoder",
            "value": 82.907,
            "range": "± 16.0041",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "timothee.denizou@epita.fr",
            "name": "Timothee Denizou",
            "username": "tim-tim707"
          },
          "committer": {
            "email": "sean165@outlook.de",
            "name": "sean",
            "username": "spnda"
          },
          "distinct": true,
          "id": "075c41a57eac64c607443435ba7e760a632d4e3a",
          "message": "a '%' without two hex digits after it read past the string and erase()d past its end, which throws from this noexcept function.",
          "timestamp": "2026-10-03T08:08:09+02:00",
          "tree_id": "3dc22ccb776a28a34f06a7850aa0033549a65a34",
          "url": "https://github.com/spnda/fastgltf/commit/075c41a57eac64c607443435ba7e760a632d4e3a"
        },
        "date": 1791007851278,
        "tool": "catch2",
        "benches": [
          {
            "name": "Parse MetalRoughSpheres and decode base64",
            "value": 2.08676,
            "range": "± 27.7798",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's fallback base64 decoder",
            "value": 0.874847,
            "range": "± 0.00537169",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's SSE4 base64 decoder",
            "value": 118.863,
            "range": "± 6.15639",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's AVX2 base64 decoder",
            "value": 65.9026,
            "range": "± 5.03065",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf",
            "value": 216.006,
            "range": "± 8.7011",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Default 1-byte tabular algorithm",
            "value": 16.2156,
            "range": "± 0.64193",
            "unit": "ns",
            "extra": "50 samples\n1489 iterations"
          },
          {
            "name": "SSE4 hardware algorithm",
            "value": 2.18061,
            "range": "± 0.104341",
            "unit": "ns",
            "extra": "50 samples\n11014 iterations"
          },
          {
            "name": "Minify Sponza.gltf",
            "value": 8.35351,
            "range": "± 341.15",
            "unit": "us",
            "extra": "50 samples\n3 iterations"
          },
          {
            "name": "Parse Sponza.gltf with normal JSON",
            "value": 215.91,
            "range": "± 9.08968",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with minified JSON",
            "value": 216.085,
            "range": "± 9.45033",
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
          "id": "0801a4f91da79b4aff4e82ce65d156af0a77ebb3",
          "message": "Rewrite URI parser, fix decode percents",
          "timestamp": "2026-10-03T09:21:47+02:00",
          "tree_id": "2b2aff230092421474d48a40e9ae96eee0c53fee",
          "url": "https://github.com/spnda/fastgltf/commit/0801a4f91da79b4aff4e82ce65d156af0a77ebb3"
        },
        "date": 1791012297088,
        "tool": "catch2",
        "benches": [
          {
            "name": "Run fastgltf's fallback base64 decoder",
            "value": 1.12435,
            "range": "± 6.54855",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's SSE4 base64 decoder",
            "value": 151.628,
            "range": "± 5.71682",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's AVX2 base64 decoder",
            "value": 87.7099,
            "range": "± 9.16761",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Minify Sponza.gltf",
            "value": 31.8887,
            "range": "± 2.19433",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with normal JSON",
            "value": 300.308,
            "range": "± 10.9525",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with minified JSON",
            "value": 301.414,
            "range": "± 12.6407",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse MetalRoughSpheres and decode base64",
            "value": 3.00613,
            "range": "± 36.6718",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Default 1-byte tabular algorithm",
            "value": 20.8082,
            "range": "± 1.01026",
            "unit": "ns",
            "extra": "50 samples\n1483 iterations"
          },
          {
            "name": "SSE4 hardware algorithm",
            "value": 2.83365,
            "range": "± 0.216787",
            "unit": "ns",
            "extra": "50 samples\n10980 iterations"
          },
          {
            "name": "Parse Sponza.gltf",
            "value": 300.031,
            "range": "± 13.038",
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
          "id": "fb48bb4fd9822ed48e8418f403de41a255c246ce",
          "message": "Cast URI string to u8string_view on Windows, try and avoid allocations",
          "timestamp": "2026-10-03T09:36:52+02:00",
          "tree_id": "2736fa219b756963a34b79e7d854b0bc87344b1b",
          "url": "https://github.com/spnda/fastgltf/commit/fb48bb4fd9822ed48e8418f403de41a255c246ce"
        },
        "date": 1791013167248,
        "tool": "catch2",
        "benches": [
          {
            "name": "Default 1-byte tabular algorithm",
            "value": 11.4377,
            "range": "± 0.616951",
            "unit": "ns",
            "extra": "50 samples\n1475 iterations"
          },
          {
            "name": "SSE4 hardware algorithm",
            "value": 1.20396,
            "range": "± 0.0524497",
            "unit": "ns",
            "extra": "50 samples\n13710 iterations"
          },
          {
            "name": "Parse MetalRoughSpheres and decode base64",
            "value": 2.78055,
            "range": "± 492.104",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's fallback base64 decoder",
            "value": 0.6814070000000001,
            "range": "± 0.0191437",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's SSE4 base64 decoder",
            "value": 125.803,
            "range": "± 18.8103",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's AVX2 base64 decoder",
            "value": 123.273,
            "range": "± 12.9516",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf",
            "value": 164.596,
            "range": "± 12.3507",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Minify Sponza.gltf",
            "value": 6.58369,
            "range": "± 918.736",
            "unit": "us",
            "extra": "50 samples\n3 iterations"
          },
          {
            "name": "Parse Sponza.gltf with normal JSON",
            "value": 165.229,
            "range": "± 13.797",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with minified JSON",
            "value": 167.524,
            "range": "± 15.4802",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "timothee.denizou@epita.fr",
            "name": "Timothee Denizou",
            "username": "tim-tim707"
          },
          "committer": {
            "email": "sean165@outlook.de",
            "name": "sean",
            "username": "spnda"
          },
          "distinct": true,
          "id": "a9f4ead8f990f006f70cca6c549c78fecb108dbc",
          "message": " getPadding / getOutputSize and the decoders assume a positive multiple of 4 characters from the base64 alphabet and only assert it, so malformed data URIs read out of bounds or overflow the output.",
          "timestamp": "2026-10-03T19:52:42+02:00",
          "tree_id": "500dcfa4015926a1d139bd5d18e486546b6db856",
          "url": "https://github.com/spnda/fastgltf/commit/a9f4ead8f990f006f70cca6c549c78fecb108dbc"
        },
        "date": 1791050121659,
        "tool": "catch2",
        "benches": [
          {
            "name": "Minify Sponza.gltf",
            "value": 8.0216,
            "range": "± 351.098",
            "unit": "us",
            "extra": "50 samples\n4 iterations"
          },
          {
            "name": "Parse Sponza.gltf with normal JSON",
            "value": 217.719,
            "range": "± 8.63422",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with minified JSON",
            "value": 217.633,
            "range": "± 7.81585",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Default 1-byte tabular algorithm",
            "value": 16.2416,
            "range": "± 0.573809",
            "unit": "ns",
            "extra": "50 samples\n1485 iterations"
          },
          {
            "name": "SSE4 hardware algorithm",
            "value": 2.17704,
            "range": "± 0.0808967",
            "unit": "ns",
            "extra": "50 samples\n10997 iterations"
          },
          {
            "name": "Run fastgltf's fallback base64 decoder",
            "value": 0.877112,
            "range": "± 0.0241689",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's SSE4 base64 decoder",
            "value": 116.822,
            "range": "± 3.53914",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's AVX2 base64 decoder",
            "value": 65.2972,
            "range": "± 5.07315",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf",
            "value": 222.864,
            "range": "± 9.87177",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse MetalRoughSpheres and decode base64",
            "value": 2.1506,
            "range": "± 59.759",
            "unit": "ms",
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
          "id": "4474db7cfc6c22e6a716fe585a603ba73a7a716e",
          "message": "Check error returned from parsing material extensions",
          "timestamp": "2026-10-05T04:24:43+02:00",
          "tree_id": "4d013fa3d5cb5babf9c633059b8ebd20b40b08a5",
          "url": "https://github.com/spnda/fastgltf/commit/4474db7cfc6c22e6a716fe585a603ba73a7a716e"
        },
        "date": 1791167300890,
        "tool": "catch2",
        "benches": [
          {
            "name": "Minify Sponza.gltf",
            "value": 29.5377,
            "range": "± 1.35512",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with normal JSON",
            "value": 297.572,
            "range": "± 7.6657",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with minified JSON",
            "value": 298.255,
            "range": "± 9.69267",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Default 1-byte tabular algorithm",
            "value": 18.8337,
            "range": "± 0.722309",
            "unit": "ns",
            "extra": "50 samples\n1530 iterations"
          },
          {
            "name": "SSE4 hardware algorithm",
            "value": 3.09954,
            "range": "± 0.119436",
            "unit": "ns",
            "extra": "50 samples\n9290 iterations"
          },
          {
            "name": "Parse Sponza.gltf",
            "value": 300.915,
            "range": "± 7.66838",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse MetalRoughSpheres and decode base64",
            "value": 2.90414,
            "range": "± 39.0012",
            "unit": "ms",
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
          "id": "ee8672f7ebe51deecdc87abdd05d3088059cdd10",
          "message": "Re-add forgotten export marker for Error & Expected",
          "timestamp": "2026-10-05T04:28:05+02:00",
          "tree_id": "047ac84faf2d947dc0188392071548b636d2b59e",
          "url": "https://github.com/spnda/fastgltf/commit/ee8672f7ebe51deecdc87abdd05d3088059cdd10"
        },
        "date": 1791167973213,
        "tool": "catch2",
        "benches": [
          {
            "name": "Minify Sponza.gltf",
            "value": 29.0774,
            "range": "± 1.72645",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with normal JSON",
            "value": 297.979,
            "range": "± 9.55315",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with minified JSON",
            "value": 297.76,
            "range": "± 16.6118",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse MetalRoughSpheres and decode base64",
            "value": 3.51863,
            "range": "± 242.54",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Default 1-byte tabular algorithm",
            "value": 18.859,
            "range": "± 0.782469",
            "unit": "ns",
            "extra": "50 samples\n1533 iterations"
          },
          {
            "name": "SSE4 hardware algorithm",
            "value": 3.26922,
            "range": "± 0.236728",
            "unit": "ns",
            "extra": "50 samples\n9303 iterations"
          },
          {
            "name": "Parse Sponza.gltf",
            "value": 299.297,
            "range": "± 16.0857",
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
          "id": "8e9d0a01ed035fc6debf1723969ac5600d02600e",
          "message": "Fix #160: Guard against invalid accessor indices when generating mesh indices",
          "timestamp": "2026-10-08T21:14:54+02:00",
          "tree_id": "115b0228b2186cd6b1ccaf41ab19203f7a62a59e",
          "url": "https://github.com/spnda/fastgltf/commit/8e9d0a01ed035fc6debf1723969ac5600d02600e"
        },
        "date": 1791487080220,
        "tool": "catch2",
        "benches": [
          {
            "name": "Parse MetalRoughSpheres and decode base64",
            "value": 3.12357,
            "range": "± 325.567",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Minify Sponza.gltf",
            "value": 8.91454,
            "range": "± 472.939",
            "unit": "us",
            "extra": "50 samples\n3 iterations"
          },
          {
            "name": "Parse Sponza.gltf with normal JSON",
            "value": 237.783,
            "range": "± 14.0387",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with minified JSON",
            "value": 236.773,
            "range": "± 8.85387",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf",
            "value": 235.695,
            "range": "± 6.96219",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Default 1-byte tabular algorithm",
            "value": 15.6439,
            "range": "± 0.480929",
            "unit": "ns",
            "extra": "50 samples\n1423 iterations"
          },
          {
            "name": "SSE4 hardware algorithm",
            "value": 3.89297,
            "range": "± 0.11682",
            "unit": "ns",
            "extra": "50 samples\n5935 iterations"
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
          "id": "9b115fa8679cd59ecb13c98844832e69ff9d782d",
          "message": "Implement a boxed_optional type as a heap allocated optional",
          "timestamp": "2026-10-09T05:57:29+02:00",
          "tree_id": "b8cfc8c25367b05198ab2c1002424b0335d7a928",
          "url": "https://github.com/spnda/fastgltf/commit/9b115fa8679cd59ecb13c98844832e69ff9d782d"
        },
        "date": 1791518606615,
        "tool": "catch2",
        "benches": [
          {
            "name": "Default 1-byte tabular algorithm",
            "value": 19.2669,
            "range": "± 0.705145",
            "unit": "ns",
            "extra": "50 samples\n1532 iterations"
          },
          {
            "name": "SSE4 hardware algorithm",
            "value": 3.10106,
            "range": "± 0.115934",
            "unit": "ns",
            "extra": "50 samples\n9301 iterations"
          },
          {
            "name": "Parse MetalRoughSpheres and decode base64",
            "value": 3.04002,
            "range": "± 71.6548",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf",
            "value": 311.359,
            "range": "± 7.94558",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Minify Sponza.gltf",
            "value": 28.8305,
            "range": "± 1.56149",
            "unit": "us",
            "extra": "50 samples\n2 iterations"
          },
          {
            "name": "Parse Sponza.gltf with normal JSON",
            "value": 311.208,
            "range": "± 8.47328",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with minified JSON",
            "value": 312.073,
            "range": "± 10.7298",
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
          "id": "36e05da6ac6658d58e49fa6e672309db25a19420",
          "message": "Merge pull request #159 from spnda/simdutf_base64\n\nReplace custom base64 decoders/encoders with simdutf",
          "timestamp": "2026-10-10T02:13:12+02:00",
          "tree_id": "5af53a4bd4acaa84d1ce62e72d67df9326078850",
          "url": "https://github.com/spnda/fastgltf/commit/36e05da6ac6658d58e49fa6e672309db25a19420"
        },
        "date": 1791591390661,
        "tool": "catch2",
        "benches": [
          {
            "name": "Minify Sponza.gltf",
            "value": 30.0568,
            "range": "± 2.32939",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with normal JSON",
            "value": 315.14,
            "range": "± 12.6574",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with minified JSON",
            "value": 319.678,
            "range": "± 21.4466",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse MetalRoughSpheres and decode base64",
            "value": 3.12072,
            "range": "± 61.647",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Default 1-byte tabular algorithm",
            "value": 19.3719,
            "range": "± 0.943291",
            "unit": "ns",
            "extra": "50 samples\n1540 iterations"
          },
          {
            "name": "SSE4 hardware algorithm",
            "value": 3.11175,
            "range": "± 0.127873",
            "unit": "ns",
            "extra": "50 samples\n9345 iterations"
          },
          {
            "name": "Parse Sponza.gltf",
            "value": 314.798,
            "range": "± 10.6479",
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
          "id": "2a9e939359a997dd795a96839b6643a0319b8d18",
          "message": "Bump fetched simdjson version to 5.0.3",
          "timestamp": "2026-10-10T02:24:13+02:00",
          "tree_id": "e868cd555f3ca4a5d3ecb70d125476448a54e1eb",
          "url": "https://github.com/spnda/fastgltf/commit/2a9e939359a997dd795a96839b6643a0319b8d18"
        },
        "date": 1791592069383,
        "tool": "catch2",
        "benches": [
          {
            "name": "Parse MetalRoughSpheres and decode base64",
            "value": 3.53664,
            "range": "± 75.909",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Default 1-byte tabular algorithm",
            "value": 18.4701,
            "range": "± 0.728785",
            "unit": "ns",
            "extra": "50 samples\n1549 iterations"
          },
          {
            "name": "SSE4 hardware algorithm",
            "value": 3.13866,
            "range": "± 0.163991",
            "unit": "ns",
            "extra": "50 samples\n9337 iterations"
          },
          {
            "name": "Minify Sponza.gltf",
            "value": 36.2146,
            "range": "± 5.35115",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with normal JSON",
            "value": 302.349,
            "range": "± 12.112",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with minified JSON",
            "value": 304.708,
            "range": "± 19.1317",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf",
            "value": 303.988,
            "range": "± 16.6473",
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
          "id": "9debfe516901d5e4a57035d6fa5797a0d372a164",
          "message": "Replace pi constant with std::numbers",
          "timestamp": "2026-10-11T04:29:51+02:00",
          "tree_id": "407e5f8f2384271230b7fb816b3b31bd46ce0648",
          "url": "https://github.com/spnda/fastgltf/commit/9debfe516901d5e4a57035d6fa5797a0d372a164"
        },
        "date": 1791685975161,
        "tool": "catch2",
        "benches": [
          {
            "name": "Default 1-byte tabular algorithm",
            "value": 18.2385,
            "range": "± 0.99605",
            "unit": "ns",
            "extra": "50 samples\n1588 iterations"
          },
          {
            "name": "SSE4 hardware algorithm",
            "value": 3.13411,
            "range": "± 0.167632",
            "unit": "ns",
            "extra": "50 samples\n9306 iterations"
          },
          {
            "name": "Parse MetalRoughSpheres and decode base64",
            "value": 3.1747,
            "range": "± 52.3349",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Minify Sponza.gltf",
            "value": 29.0128,
            "range": "± 1.7165",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with normal JSON",
            "value": 307.175,
            "range": "± 10.6022",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with minified JSON",
            "value": 308.014,
            "range": "± 14.9799",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf",
            "value": 313.007,
            "range": "± 17.0083",
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
          "id": "6202dd980673d5bd7aa63000c84203223ff067bd",
          "message": "Add missing <numbers> include in fastgltf.ixx",
          "timestamp": "2026-10-11T04:47:52+02:00",
          "tree_id": "fdcf703a6da9e1c84e3d9ef431823392d5a1e6f4",
          "url": "https://github.com/spnda/fastgltf/commit/6202dd980673d5bd7aa63000c84203223ff067bd"
        },
        "date": 1791687058595,
        "tool": "catch2",
        "benches": [
          {
            "name": "Default 1-byte tabular algorithm",
            "value": 18.1938,
            "range": "± 0.983787",
            "unit": "ns",
            "extra": "50 samples\n1595 iterations"
          },
          {
            "name": "SSE4 hardware algorithm",
            "value": 3.24427,
            "range": "± 0.182773",
            "unit": "ns",
            "extra": "50 samples\n9321 iterations"
          },
          {
            "name": "Minify Sponza.gltf",
            "value": 29.5957,
            "range": "± 2.11724",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with normal JSON",
            "value": 309.45,
            "range": "± 15.5631",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with minified JSON",
            "value": 308.768,
            "range": "± 11.2738",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf",
            "value": 305.596,
            "range": "± 14.0978",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse MetalRoughSpheres and decode base64",
            "value": 3.43556,
            "range": "± 76.7652",
            "unit": "ms",
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
          "id": "4cff6c82b51ab010bae58bec91c24e9a42c57b8e",
          "message": "compare resources instead of relying on the conversion operator from allocators",
          "timestamp": "2026-10-02T22:12:49+02:00",
          "tree_id": "314cd5063a6de9722440e53b6f329f4428cf24ba",
          "url": "https://github.com/spnda/fastgltf/commit/4cff6c82b51ab010bae58bec91c24e9a42c57b8e"
        },
        "date": 1790972359511,
        "tool": "catch2",
        "benches": [
          {
            "name": "Minify Sponza.gltf",
            "value": 33.8722,
            "range": "± 572.133",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with normal JSON",
            "value": 287.579,
            "range": "± 5.94714",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with minified JSON",
            "value": 285.93,
            "range": "± 4.08684",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse MetalRoughSpheres and decode base64",
            "value": 7.84266,
            "range": "± 47.5685",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's fallback base64 decoder",
            "value": 729.327,
            "range": "± 4.75573",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's Neon base64 decoder",
            "value": 487.988,
            "range": "± 6.05137",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf",
            "value": 285.965,
            "range": "± 5.70666",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Default 1-byte tabular algorithm",
            "value": 15.7419,
            "range": "± 0.136135",
            "unit": "ns",
            "extra": "50 samples\n2002 iterations"
          },
          {
            "name": "ARMv8 hardware CRC32-C algorithm",
            "value": 2.06136,
            "range": "± 0.0142178",
            "unit": "ns",
            "extra": "50 samples\n15251 iterations"
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
          "id": "a03222c024e8a96bc00bbd675be1252769c3fb3c",
          "message": "Allow complex types in flagged_optional through flag traits\n\nflagged_optional now uses optional_flag_value<T>::is_empty and optional_flag_value<T>::set_empty to check for or set the flags. This allows the type to store more complex types such as TextureInfo, where we allow the user to store the empty flag somewhere internally.",
          "timestamp": "2026-10-03T00:48:40+02:00",
          "tree_id": "e9fcc2f0ee41116f2c6fb2ef6f6b1887687aa80d",
          "url": "https://github.com/spnda/fastgltf/commit/a03222c024e8a96bc00bbd675be1252769c3fb3c"
        },
        "date": 1790981678772,
        "tool": "catch2",
        "benches": [
          {
            "name": "Minify Sponza.gltf",
            "value": 33.8834,
            "range": "± 887.01",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with normal JSON",
            "value": 280.197,
            "range": "± 5.86311",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with minified JSON",
            "value": 281.994,
            "range": "± 5.24776",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse MetalRoughSpheres and decode base64",
            "value": 7.80018,
            "range": "± 74.8997",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's fallback base64 decoder",
            "value": 728.466,
            "range": "± 5.34029",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's Neon base64 decoder",
            "value": 486.733,
            "range": "± 4.82404",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Default 1-byte tabular algorithm",
            "value": 15.7552,
            "range": "± 0.161",
            "unit": "ns",
            "extra": "50 samples\n2005 iterations"
          },
          {
            "name": "ARMv8 hardware CRC32-C algorithm",
            "value": 2.1456,
            "range": "± 0.070872",
            "unit": "ns",
            "extra": "50 samples\n15293 iterations"
          },
          {
            "name": "Parse Sponza.gltf",
            "value": 282.898,
            "range": "± 6.28112",
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
          "id": "2571de8a0d7055aeaddd99214e9e7b4cc046078b",
          "message": "Use _mm_crc32_u64 instead of only using the 32-bit instructions",
          "timestamp": "2026-10-03T02:09:06+02:00",
          "tree_id": "46d3d9232361d6f2f406f4a9639da1177c251e92",
          "url": "https://github.com/spnda/fastgltf/commit/2571de8a0d7055aeaddd99214e9e7b4cc046078b"
        },
        "date": 1790986619954,
        "tool": "catch2",
        "benches": [
          {
            "name": "Parse MetalRoughSpheres and decode base64",
            "value": 7.93163,
            "range": "± 112.7",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's fallback base64 decoder",
            "value": 729.166,
            "range": "± 5.7424",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's Neon base64 decoder",
            "value": 486.57,
            "range": "± 4.7695",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Minify Sponza.gltf",
            "value": 33.8473,
            "range": "± 416.18",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with normal JSON",
            "value": 280.457,
            "range": "± 6.47258",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with minified JSON",
            "value": 286.756,
            "range": "± 16.5353",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Default 1-byte tabular algorithm",
            "value": 15.7229,
            "range": "± 0.168673",
            "unit": "ns",
            "extra": "50 samples\n2005 iterations"
          },
          {
            "name": "ARMv8 hardware CRC32-C algorithm",
            "value": 2.10063,
            "range": "± 0.0668951",
            "unit": "ns",
            "extra": "50 samples\n15261 iterations"
          },
          {
            "name": "Parse Sponza.gltf",
            "value": 282.172,
            "range": "± 5.5048",
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
          "id": "270c90758529b4e0b9ed381de7e03ff92373890c",
          "message": "Simplify AccessorBoundsArray and make compatible with flagged_optional",
          "timestamp": "2026-10-03T04:42:19+02:00",
          "tree_id": "d622bf7c8245832cd6d8381d2aa6226dc8ee962e",
          "url": "https://github.com/spnda/fastgltf/commit/270c90758529b4e0b9ed381de7e03ff92373890c"
        },
        "date": 1790995716491,
        "tool": "catch2",
        "benches": [
          {
            "name": "Run fastgltf's fallback base64 decoder",
            "value": 730.007,
            "range": "± 5.3946",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's Neon base64 decoder",
            "value": 490.146,
            "range": "± 5.89727",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf",
            "value": 290.264,
            "range": "± 5.26217",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse MetalRoughSpheres and decode base64",
            "value": 7.90523,
            "range": "± 93.1908",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Default 1-byte tabular algorithm",
            "value": 15.5069,
            "range": "± 0.214334",
            "unit": "ns",
            "extra": "50 samples\n2106 iterations"
          },
          {
            "name": "ARMv8 hardware CRC32-C algorithm",
            "value": 2.08023,
            "range": "± 0.0906278",
            "unit": "ns",
            "extra": "50 samples\n15793 iterations"
          },
          {
            "name": "Minify Sponza.gltf",
            "value": 33.8167,
            "range": "± 506.484",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with normal JSON",
            "value": 291.473,
            "range": "± 8.50308",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with minified JSON",
            "value": 294.324,
            "range": "± 35.7742",
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
          "id": "ac38e7a8d619594d77f784525d85f8fcbe75e04e",
          "message": "Simplify AccessorBoundsArray and make compatible with flagged_optional",
          "timestamp": "2026-10-03T04:45:56+02:00",
          "tree_id": "6abe5c40169678f41ae28103940dc4ee6f66a2dc",
          "url": "https://github.com/spnda/fastgltf/commit/ac38e7a8d619594d77f784525d85f8fcbe75e04e"
        },
        "date": 1790996049102,
        "tool": "catch2",
        "benches": [
          {
            "name": "Default 1-byte tabular algorithm",
            "value": 15.5066,
            "range": "± 0.165499",
            "unit": "ns",
            "extra": "50 samples\n2035 iterations"
          },
          {
            "name": "ARMv8 hardware CRC32-C algorithm",
            "value": 2.06314,
            "range": "± 0.0205833",
            "unit": "ns",
            "extra": "50 samples\n15199 iterations"
          },
          {
            "name": "Run fastgltf's fallback base64 decoder",
            "value": 728.703,
            "range": "± 5.09079",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's Neon base64 decoder",
            "value": 490.265,
            "range": "± 5.06881",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse MetalRoughSpheres and decode base64",
            "value": 7.96479,
            "range": "± 115.717",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf",
            "value": 288.328,
            "range": "± 4.83596",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Minify Sponza.gltf",
            "value": 33.8562,
            "range": "± 485.065",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with normal JSON",
            "value": 290.174,
            "range": "± 6.70158",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with minified JSON",
            "value": 291.175,
            "range": "± 5.0446",
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
          "id": "4101ad522049e8a7583244d993ea88aa138be816",
          "message": "Add for_overwrite_t tag to avoid large memsets with static_vector",
          "timestamp": "2026-10-03T05:35:34+02:00",
          "tree_id": "05e8177cf6b088993aa87035912d0124784b4504",
          "url": "https://github.com/spnda/fastgltf/commit/4101ad522049e8a7583244d993ea88aa138be816"
        },
        "date": 1790998960294,
        "tool": "catch2",
        "benches": [
          {
            "name": "Run fastgltf's fallback base64 decoder",
            "value": 727.665,
            "range": "± 3.89646",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's Neon base64 decoder",
            "value": 489.886,
            "range": "± 4.54615",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf",
            "value": 284.344,
            "range": "± 5.22711",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Minify Sponza.gltf",
            "value": 34.08,
            "range": "± 995.204",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with normal JSON",
            "value": 285.191,
            "range": "± 6.00565",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with minified JSON",
            "value": 284.761,
            "range": "± 4.72154",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Default 1-byte tabular algorithm",
            "value": 15.8532,
            "range": "± 0.261804",
            "unit": "ns",
            "extra": "50 samples\n2004 iterations"
          },
          {
            "name": "ARMv8 hardware CRC32-C algorithm",
            "value": 2.06316,
            "range": "± 0.0199548",
            "unit": "ns",
            "extra": "50 samples\n15246 iterations"
          },
          {
            "name": "Parse MetalRoughSpheres and decode base64",
            "value": 7.78093,
            "range": "± 74.1825",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "timothee.denizou@epita.fr",
            "name": "Timothee Denizou",
            "username": "tim-tim707"
          },
          "committer": {
            "email": "sean165@outlook.de",
            "name": "sean",
            "username": "spnda"
          },
          "distinct": true,
          "id": "075c41a57eac64c607443435ba7e760a632d4e3a",
          "message": "a '%' without two hex digits after it read past the string and erase()d past its end, which throws from this noexcept function.",
          "timestamp": "2026-10-03T08:08:09+02:00",
          "tree_id": "3dc22ccb776a28a34f06a7850aa0033549a65a34",
          "url": "https://github.com/spnda/fastgltf/commit/075c41a57eac64c607443435ba7e760a632d4e3a"
        },
        "date": 1791008045744,
        "tool": "catch2",
        "benches": [
          {
            "name": "Default 1-byte tabular algorithm",
            "value": 15.7228,
            "range": "± 0.16821",
            "unit": "ns",
            "extra": "50 samples\n2003 iterations"
          },
          {
            "name": "ARMv8 hardware CRC32-C algorithm",
            "value": 2.09413,
            "range": "± 0.0622177",
            "unit": "ns",
            "extra": "50 samples\n15228 iterations"
          },
          {
            "name": "Parse MetalRoughSpheres and decode base64",
            "value": 7.82606,
            "range": "± 100.34",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf",
            "value": 281.954,
            "range": "± 5.54654",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Minify Sponza.gltf",
            "value": 33.9918,
            "range": "± 1.24416",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with normal JSON",
            "value": 283.363,
            "range": "± 5.06607",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with minified JSON",
            "value": 283.796,
            "range": "± 5.70738",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's fallback base64 decoder",
            "value": 729.33,
            "range": "± 5.80277",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's Neon base64 decoder",
            "value": 489.283,
            "range": "± 4.06318",
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
          "id": "0801a4f91da79b4aff4e82ce65d156af0a77ebb3",
          "message": "Rewrite URI parser, fix decode percents",
          "timestamp": "2026-10-03T09:21:47+02:00",
          "tree_id": "2b2aff230092421474d48a40e9ae96eee0c53fee",
          "url": "https://github.com/spnda/fastgltf/commit/0801a4f91da79b4aff4e82ce65d156af0a77ebb3"
        },
        "date": 1791012483427,
        "tool": "catch2",
        "benches": [
          {
            "name": "Parse MetalRoughSpheres and decode base64",
            "value": 7.81699,
            "range": "± 106.007",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's fallback base64 decoder",
            "value": 732.192,
            "range": "± 6.23555",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's Neon base64 decoder",
            "value": 487.413,
            "range": "± 6.06581",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf",
            "value": 280.723,
            "range": "± 4.29412",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Minify Sponza.gltf",
            "value": 33.8359,
            "range": "± 547.161",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with normal JSON",
            "value": 283.148,
            "range": "± 6.90235",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with minified JSON",
            "value": 281.904,
            "range": "± 4.04463",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Default 1-byte tabular algorithm",
            "value": 15.8779,
            "range": "± 0.154878",
            "unit": "ns",
            "extra": "50 samples\n1989 iterations"
          },
          {
            "name": "ARMv8 hardware CRC32-C algorithm",
            "value": 2.06593,
            "range": "± 0.0451283",
            "unit": "ns",
            "extra": "50 samples\n15258 iterations"
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
          "id": "fb48bb4fd9822ed48e8418f403de41a255c246ce",
          "message": "Cast URI string to u8string_view on Windows, try and avoid allocations",
          "timestamp": "2026-10-03T09:36:52+02:00",
          "tree_id": "2736fa219b756963a34b79e7d854b0bc87344b1b",
          "url": "https://github.com/spnda/fastgltf/commit/fb48bb4fd9822ed48e8418f403de41a255c246ce"
        },
        "date": 1791013362549,
        "tool": "catch2",
        "benches": [
          {
            "name": "Parse MetalRoughSpheres and decode base64",
            "value": 7.93276,
            "range": "± 100.374",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf",
            "value": 279.955,
            "range": "± 6.4383",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Default 1-byte tabular algorithm",
            "value": 15.7117,
            "range": "± 0.12831",
            "unit": "ns",
            "extra": "50 samples\n2006 iterations"
          },
          {
            "name": "ARMv8 hardware CRC32-C algorithm",
            "value": 2.06376,
            "range": "± 0.0206384",
            "unit": "ns",
            "extra": "50 samples\n15274 iterations"
          },
          {
            "name": "Minify Sponza.gltf",
            "value": 33.798,
            "range": "± 560.202",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with normal JSON",
            "value": 282.487,
            "range": "± 5.53883",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with minified JSON",
            "value": 282.766,
            "range": "± 6.02236",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's fallback base64 decoder",
            "value": 728.168,
            "range": "± 4.19005",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's Neon base64 decoder",
            "value": 486.84,
            "range": "± 5.36329",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "timothee.denizou@epita.fr",
            "name": "Timothee Denizou",
            "username": "tim-tim707"
          },
          "committer": {
            "email": "sean165@outlook.de",
            "name": "sean",
            "username": "spnda"
          },
          "distinct": true,
          "id": "a9f4ead8f990f006f70cca6c549c78fecb108dbc",
          "message": " getPadding / getOutputSize and the decoders assume a positive multiple of 4 characters from the base64 alphabet and only assert it, so malformed data URIs read out of bounds or overflow the output.",
          "timestamp": "2026-10-03T19:52:42+02:00",
          "tree_id": "500dcfa4015926a1d139bd5d18e486546b6db856",
          "url": "https://github.com/spnda/fastgltf/commit/a9f4ead8f990f006f70cca6c549c78fecb108dbc"
        },
        "date": 1791050310781,
        "tool": "catch2",
        "benches": [
          {
            "name": "Default 1-byte tabular algorithm",
            "value": 15.7976,
            "range": "± 0.139167",
            "unit": "ns",
            "extra": "50 samples\n1999 iterations"
          },
          {
            "name": "ARMv8 hardware CRC32-C algorithm",
            "value": 2.06374,
            "range": "± 0.0226622",
            "unit": "ns",
            "extra": "50 samples\n15280 iterations"
          },
          {
            "name": "Run fastgltf's fallback base64 decoder",
            "value": 733.618,
            "range": "± 7.20936",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Run fastgltf's Neon base64 decoder",
            "value": 488.173,
            "range": "± 8.68578",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf",
            "value": 280.366,
            "range": "± 4.92207",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse MetalRoughSpheres and decode base64",
            "value": 8.41496,
            "range": "± 137.529",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Minify Sponza.gltf",
            "value": 33.8702,
            "range": "± 527.334",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with normal JSON",
            "value": 281.797,
            "range": "± 7.40222",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with minified JSON",
            "value": 280.119,
            "range": "± 5.22481",
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
          "id": "4474db7cfc6c22e6a716fe585a603ba73a7a716e",
          "message": "Check error returned from parsing material extensions",
          "timestamp": "2026-10-05T04:24:43+02:00",
          "tree_id": "4d013fa3d5cb5babf9c633059b8ebd20b40b08a5",
          "url": "https://github.com/spnda/fastgltf/commit/4474db7cfc6c22e6a716fe585a603ba73a7a716e"
        },
        "date": 1791167485790,
        "tool": "catch2",
        "benches": [
          {
            "name": "Minify Sponza.gltf",
            "value": 33.9298,
            "range": "± 769.099",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with normal JSON",
            "value": 284.612,
            "range": "± 4.14416",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with minified JSON",
            "value": 285.402,
            "range": "± 4.83918",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf",
            "value": 285.459,
            "range": "± 7.1109",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Default 1-byte tabular algorithm",
            "value": 15.4112,
            "range": "± 0.109275",
            "unit": "ns",
            "extra": "50 samples\n2045 iterations"
          },
          {
            "name": "ARMv8 hardware CRC32-C algorithm",
            "value": 2.06405,
            "range": "± 0.020719",
            "unit": "ns",
            "extra": "50 samples\n15226 iterations"
          },
          {
            "name": "Parse MetalRoughSpheres and decode base64",
            "value": 8.21161,
            "range": "± 128.335",
            "unit": "ms",
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
          "id": "ee8672f7ebe51deecdc87abdd05d3088059cdd10",
          "message": "Re-add forgotten export marker for Error & Expected",
          "timestamp": "2026-10-05T04:28:05+02:00",
          "tree_id": "047ac84faf2d947dc0188392071548b636d2b59e",
          "url": "https://github.com/spnda/fastgltf/commit/ee8672f7ebe51deecdc87abdd05d3088059cdd10"
        },
        "date": 1791168152069,
        "tool": "catch2",
        "benches": [
          {
            "name": "Parse MetalRoughSpheres and decode base64",
            "value": 7.79205,
            "range": "± 93.5657",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Minify Sponza.gltf",
            "value": 33.7866,
            "range": "± 396.727",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with normal JSON",
            "value": 283.593,
            "range": "± 3.72351",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with minified JSON",
            "value": 284.881,
            "range": "± 6.97899",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf",
            "value": 283.846,
            "range": "± 4.45832",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Default 1-byte tabular algorithm",
            "value": 15.4344,
            "range": "± 0.107855",
            "unit": "ns",
            "extra": "50 samples\n2042 iterations"
          },
          {
            "name": "ARMv8 hardware CRC32-C algorithm",
            "value": 2.06214,
            "range": "± 0.015825",
            "unit": "ns",
            "extra": "50 samples\n15259 iterations"
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
          "id": "8e9d0a01ed035fc6debf1723969ac5600d02600e",
          "message": "Fix #160: Guard against invalid accessor indices when generating mesh indices",
          "timestamp": "2026-10-08T21:14:54+02:00",
          "tree_id": "115b0228b2186cd6b1ccaf41ab19203f7a62a59e",
          "url": "https://github.com/spnda/fastgltf/commit/8e9d0a01ed035fc6debf1723969ac5600d02600e"
        },
        "date": 1791487289654,
        "tool": "catch2",
        "benches": [
          {
            "name": "Default 1-byte tabular algorithm",
            "value": 15.7031,
            "range": "± 0.149651",
            "unit": "ns",
            "extra": "50 samples\n2006 iterations"
          },
          {
            "name": "ARMv8 hardware CRC32-C algorithm",
            "value": 2.06171,
            "range": "± 0.0159597",
            "unit": "ns",
            "extra": "50 samples\n15220 iterations"
          },
          {
            "name": "Parse MetalRoughSpheres and decode base64",
            "value": 7.92711,
            "range": "± 112.098",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf",
            "value": 284.685,
            "range": "± 4.33725",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Minify Sponza.gltf",
            "value": 33.8401,
            "range": "± 539.648",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with normal JSON",
            "value": 284.588,
            "range": "± 7.38329",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with minified JSON",
            "value": 284.476,
            "range": "± 6.40748",
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
          "id": "9b115fa8679cd59ecb13c98844832e69ff9d782d",
          "message": "Implement a boxed_optional type as a heap allocated optional",
          "timestamp": "2026-10-09T05:57:29+02:00",
          "tree_id": "b8cfc8c25367b05198ab2c1002424b0335d7a928",
          "url": "https://github.com/spnda/fastgltf/commit/9b115fa8679cd59ecb13c98844832e69ff9d782d"
        },
        "date": 1791518774687,
        "tool": "catch2",
        "benches": [
          {
            "name": "Parse Sponza.gltf",
            "value": 281.522,
            "range": "± 9.59834",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse MetalRoughSpheres and decode base64",
            "value": 7.78447,
            "range": "± 92.6643",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Minify Sponza.gltf",
            "value": 33.9185,
            "range": "± 487.975",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with normal JSON",
            "value": 279.906,
            "range": "± 3.41945",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with minified JSON",
            "value": 280.384,
            "range": "± 4.29575",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Default 1-byte tabular algorithm",
            "value": 15.4912,
            "range": "± 0.221168",
            "unit": "ns",
            "extra": "50 samples\n2043 iterations"
          },
          {
            "name": "ARMv8 hardware CRC32-C algorithm",
            "value": 2.06429,
            "range": "± 0.0229811",
            "unit": "ns",
            "extra": "50 samples\n15248 iterations"
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
          "id": "36e05da6ac6658d58e49fa6e672309db25a19420",
          "message": "Merge pull request #159 from spnda/simdutf_base64\n\nReplace custom base64 decoders/encoders with simdutf",
          "timestamp": "2026-10-10T02:13:12+02:00",
          "tree_id": "5af53a4bd4acaa84d1ce62e72d67df9326078850",
          "url": "https://github.com/spnda/fastgltf/commit/36e05da6ac6658d58e49fa6e672309db25a19420"
        },
        "date": 1791591584949,
        "tool": "catch2",
        "benches": [
          {
            "name": "Parse MetalRoughSpheres and decode base64",
            "value": 7.37993,
            "range": "± 221.696",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf",
            "value": 280.83,
            "range": "± 4.86481",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Default 1-byte tabular algorithm",
            "value": 15.7439,
            "range": "± 0.164732",
            "unit": "ns",
            "extra": "50 samples\n1997 iterations"
          },
          {
            "name": "ARMv8 hardware CRC32-C algorithm",
            "value": 2.19643,
            "range": "± 0.0439891",
            "unit": "ns",
            "extra": "50 samples\n14838 iterations"
          },
          {
            "name": "Minify Sponza.gltf",
            "value": 33.9018,
            "range": "± 514.169",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with normal JSON",
            "value": 281.682,
            "range": "± 3.70408",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with minified JSON",
            "value": 282.467,
            "range": "± 6.59374",
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
          "id": "2a9e939359a997dd795a96839b6643a0319b8d18",
          "message": "Bump fetched simdjson version to 5.0.3",
          "timestamp": "2026-10-10T02:24:13+02:00",
          "tree_id": "e868cd555f3ca4a5d3ecb70d125476448a54e1eb",
          "url": "https://github.com/spnda/fastgltf/commit/2a9e939359a997dd795a96839b6643a0319b8d18"
        },
        "date": 1791592262026,
        "tool": "catch2",
        "benches": [
          {
            "name": "Parse MetalRoughSpheres and decode base64",
            "value": 6.44789,
            "range": "± 191.298",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf",
            "value": 266.984,
            "range": "± 3.54646",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Minify Sponza.gltf",
            "value": 28.7128,
            "range": "± 599.283",
            "unit": "us",
            "extra": "50 samples\n2 iterations"
          },
          {
            "name": "Parse Sponza.gltf with normal JSON",
            "value": 267.808,
            "range": "± 3.80685",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with minified JSON",
            "value": 266.721,
            "range": "± 3.8234",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Default 1-byte tabular algorithm",
            "value": 15.7249,
            "range": "± 0.165154",
            "unit": "ns",
            "extra": "50 samples\n2008 iterations"
          },
          {
            "name": "ARMv8 hardware CRC32-C algorithm",
            "value": 2.06179,
            "range": "± 0.0163139",
            "unit": "ns",
            "extra": "50 samples\n15227 iterations"
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
          "id": "9debfe516901d5e4a57035d6fa5797a0d372a164",
          "message": "Replace pi constant with std::numbers",
          "timestamp": "2026-10-11T04:29:51+02:00",
          "tree_id": "407e5f8f2384271230b7fb816b3b31bd46ce0648",
          "url": "https://github.com/spnda/fastgltf/commit/9debfe516901d5e4a57035d6fa5797a0d372a164"
        },
        "date": 1791686161743,
        "tool": "catch2",
        "benches": [
          {
            "name": "Minify Sponza.gltf",
            "value": 28.4543,
            "range": "± 246.493",
            "unit": "us",
            "extra": "50 samples\n2 iterations"
          },
          {
            "name": "Parse Sponza.gltf with normal JSON",
            "value": 268.412,
            "range": "± 3.60678",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with minified JSON",
            "value": 268.56,
            "range": "± 4.51127",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Default 1-byte tabular algorithm",
            "value": 15.6812,
            "range": "± 0.15234",
            "unit": "ns",
            "extra": "50 samples\n2013 iterations"
          },
          {
            "name": "ARMv8 hardware CRC32-C algorithm",
            "value": 2.07002,
            "range": "± 0.0375007",
            "unit": "ns",
            "extra": "50 samples\n15203 iterations"
          },
          {
            "name": "Parse Sponza.gltf",
            "value": 266.925,
            "range": "± 4.27874",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse MetalRoughSpheres and decode base64",
            "value": 7.19361,
            "range": "± 159.038",
            "unit": "ms",
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
          "id": "6202dd980673d5bd7aa63000c84203223ff067bd",
          "message": "Add missing <numbers> include in fastgltf.ixx",
          "timestamp": "2026-10-11T04:47:52+02:00",
          "tree_id": "fdcf703a6da9e1c84e3d9ef431823392d5a1e6f4",
          "url": "https://github.com/spnda/fastgltf/commit/6202dd980673d5bd7aa63000c84203223ff067bd"
        },
        "date": 1791687252841,
        "tool": "catch2",
        "benches": [
          {
            "name": "Minify Sponza.gltf",
            "value": 28.4781,
            "range": "± 355.449",
            "unit": "us",
            "extra": "50 samples\n2 iterations"
          },
          {
            "name": "Parse Sponza.gltf with normal JSON",
            "value": 268.866,
            "range": "± 6.20395",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse Sponza.gltf with minified JSON",
            "value": 266.784,
            "range": "± 3.39699",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Parse MetalRoughSpheres and decode base64",
            "value": 6.453,
            "range": "± 189.337",
            "unit": "ms",
            "extra": "50 samples\n1 iterations"
          },
          {
            "name": "Default 1-byte tabular algorithm",
            "value": 16.2657,
            "range": "± 3.34816",
            "unit": "ns",
            "extra": "50 samples\n1996 iterations"
          },
          {
            "name": "ARMv8 hardware CRC32-C algorithm",
            "value": 2.06873,
            "range": "± 0.0675182",
            "unit": "ns",
            "extra": "50 samples\n15223 iterations"
          },
          {
            "name": "Parse Sponza.gltf",
            "value": 266.162,
            "range": "± 4.71305",
            "unit": "us",
            "extra": "50 samples\n1 iterations"
          }
        ]
      }
    ]
  }
}