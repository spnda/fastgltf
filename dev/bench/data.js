window.BENCHMARK_DATA = {
  "lastUpdate": 1790658427665,
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
      }
    ]
  }
}