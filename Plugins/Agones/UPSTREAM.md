# Agones Unreal Plugin

- Upstream: https://github.com/agones-dev/agones
- Release: `v1.59.0`
- Commit: `9005c6c511699eaa5799b4295cae0f91c686b1a0`
- Source directory: `sdks/unreal/Agones`
- Retrieved: 2026-07-22
- License: Apache-2.0; see `LICENSE`

This directory vendors the official Agones Unreal Game Server Client Plugin so
the project can compile it for both the editor and Linux dedicated-server
targets without relying on machine-local engine plugins.

## Local compatibility patch

`Source/Agones/Classes/Classes.h` contains a guarded UE 5.8 compatibility
adjustment for the JSON object's shared-string key storage introduced by UE
5.8. Older UE5 versions retain the upstream `FString` path.
