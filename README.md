jit.gl.hap
==========

jitter external to provide native HAP playback

jit.gl.hap provides native support for the HAP quicktime codec. Images are decoded and rendered directly to an OpenGL texture. Non-hap quicktime files are also supported. For general information about Hap, see [the Hap project](http://github.com/vidvox/hap).


Building
========

To build the external, the [Max 6.1 SDK](http://cycling74.com/products/sdk/) must be downloaded and placed at the same level as the jit.gl.hap project folder.

To export and playback HAP encoded movies, the [Hap QuickTime codec](https://github.com/vidvox/hap-qt-codec) must be installed.

Limitations
===========

Currently only supported on 32 bit Macs, OS 10.6 and later.
Requires [Max 6.1](http://cycling74.com/products/max/) and later 

Modifying the dim attribute (native movie dimensions) is currently not supported.


Open-Source
===========

This code is open-source, licensed under a [New BSD License](http://github.com/vidvox/hap-quicktime-playback-demo/blob/master/LICENSE), meaning you can use it in your commercial or non-commercial applications free of charge.
