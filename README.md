# Xtreme

Renderer, Sound, Input and Music players for the Xtreme engine, plus a test bed.

Fully functional:
* DDrawRenderer, DX11Renderer, DX82dRenderer, DX8MPlayer, DX8OggPlayer, DX8Renderer, DX8Sound, DX9Renderer, DXInput, Rawinput, WinInput, XAudioSound

Fully functional, but for some reason not included here (not building a Dll):
* SDLRenderer, SDLSound, SDLMusic, SDLInput

Not fully implemented yet:
* OpenGLRenderer  (2d functional, 3d still missing most parts)
* VulkanRenderer  (initialising started, but nothing displays yet)

## Installation

Expects drive P: containing code files from https://github.com/GeorgRottensteiner/Common (as P:\Common\...)
Expects several SDKs at these places:
* DirectX 8.1 SDK at D:\SDK\DX81SDK
* DirectX 9 SDK at D:\SDK\DX9SDK
* Windows Driver Development Kit at D:\SDK\WinDDK\7600.16385.1  (for Raw Input)
* Vulkan SDK at D:\SDK\VulkanSDK\1.1.106.0
* additional OpenGL headers at D:\SDK\OpenGL

Compiling creates Dlls and places them in Xtreme2d for 2d and Xtreme3d for 3d

## Usage

Dlls are for use in XFrameApp or XFrameApp2d (found in Common)

## Contributing

1. Fork it!
2. Create your feature branch: `git checkout -b my-new-feature`
3. Commit your changes: `git commit -am 'Add some feature'`
4. Push to the branch: `git push origin my-new-feature`
5. Submit a pull request :D

## History

Chock ful of mistakes

## Credits

Cobbled together over the years by yours truly.

## License

MIT License, do whatever you want, take what you can, leave nothing behind