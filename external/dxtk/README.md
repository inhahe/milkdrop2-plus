# DirectXTK sources

The part of [DirectX Tool Kit for DirectX 11](https://github.com/microsoft/DirectXTK) that the component uses, unmodified, from release [`sep2024`](https://github.com/microsoft/DirectXTK/tree/sep2024) (the code of NuGet package `directxtk_desktop_2019` 2024.9.5.1, which the project used before). MIT licensed; see [LICENSE](LICENSE).

- `Inc/`: `CommonStates.h`, `DDSTextureLoader.h`, `WICTextureLoader.h`, `DirectXHelpers.h`, the public headers the component includes.
- `Src/`: `CommonStates.cpp`, `DDSTextureLoader.cpp`, `WICTextureLoader.cpp`, and the internal headers they include.

The three `.cpp` files are compiled into `vis_milk2` without its precompiled header. They are built from source rather than linked from the NuGet package because the package's libraries use the DLL C/C++ runtime, and the component links the runtime statically (see `Directory.Build.targets`).

To update, copy the same files from a newer release tag.
