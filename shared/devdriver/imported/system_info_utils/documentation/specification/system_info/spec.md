## System Information Specification v1.0

### RDF Chunk Format

The system info chunk describes the system configuration for the machine on which the capture of RDF file took place:

| Field Name     | Bits (Width) | Data type | Version added | Description             |
|----------------|--------------|-----------|---------------|-------------------------|
| SystemInfoSize | 0..32        | uin32_t   | 0             | System info JSON size   |
| SystemInfo     | 32..*        | char[]    | 0             | System info JSON string |

### JSON structure

```json
{
  "version": 1,
  "stats": {
    "timestamp": {
      "ticks": "uint64",
      "ticksPerSecond": "uint64"
    }
  },
  "system": {
    "cpus": [
      {
        "architecture": "string",
        "cpuId": "string",
        "deviceId": "string",
        "name": "string",
        "numLogicalCores": "uint32",
        "numPhysicalCores": "uint32",
        "speed": {
          "max": "uint32"
        },
        "vendorId": "string"
      }
    ],
    "devdriver": {
      "version": {
        "major": "uint32"
      },
      "tag": "string"
    },
    "driver": {
      "name": "string",
      "description": "string",
      "packagingVersion": "string",
      "softwareVersion": "string",
      "isClosedSource": "bool"
    },
    "gpus": [
      {
        "asic": {
          "engineClockHz": {
            "max": "uint64",
            "min": "uint64"
          },
          "gpuCounterFreq": "uint64",
          "gpuIndex": "uint32",
          "ids": {
            "device": "uint32",
            "eRev": "uint32",
            "family": "uint32",
            "gfxEngine": "uint32",
            "revision": "uint32"
          }
        },
        "bigSw": {
          "major": "uint32",
          "minor": "uint32",
          "misc": "uint32"
        },
        "memory": {
          "bandwidthBytesPerSec": "uint64",
          "busBitWidth": "uint32",
          "excludedVaRanges": [
            {
              "base": "uint64",
              "size": "uint64"
            }
          ],
          "heaps": {
            "invisible": {
              "physicalAddress": "uint64",
              "size": "uint64"
            },
            "local": {
              "physicalAddress": "uint64",
              "size": "uint64"
            }
          },
          "memClockHz": {
            "max": "uint64",
            "min": "uint64"
          },
          "memOpsPerClock": "uint32",
          "type": "string"
        },
        "name": "string",
        "pci": {
          "bus": "uint32",
          "device": "uint32",
          "function": "uint32"
        }
      }
    ],
    "os": {
      "description": "string",
      "hostname": "string",
      "memory": {
        "physical": "uint64",
        "swap": "uint64"
      },
      "config": {
        "windows": {
          "etwSupport": {
            "hasPermission": "bool",
            "isSupported": "bool",
            "statusCode": "uint32"
          }
        },
        "linux": {
          "drm": {
            "major": "uint32",
            "minor": "uint32"
          },
          "power_dpm_writable": "bool"
        }
      },
      "name": "string"
    }
  }
}
```
