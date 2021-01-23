# NbInjection
This is an example for replace system property "ro.dalvik.vm.native.bridge" to inject zygote process.

I have tested it on my Google Pixel 3 (Android 10, Magisk 20.4), it seems to be working well;
but it maybe not compatible with all devices, if you want to use it, please modify the code according to your devices.

About its working principle, you can refer to this Chinese [article](https://blog.canyie.top/2020/08/18/nbinjection/).
## Build
Run gradle task `:module:assembleMagiskRelease` from Android Studio or command line,
magisk module zip will be saved to module/build/outputs/magisk/.

## Discussion
- [QQ Group: 949888394](https://shang.qq.com/wpa/qunwpa?idkey=25549719b948d2aaeb9e579955e39d71768111844b370fcb824d43b9b20e1c04)

## License
MIT License
