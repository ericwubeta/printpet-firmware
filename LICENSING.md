# Firmware licensing

The PrintPet application source, project build and test code, and changes licensable by this project in the current source distribution are subject to the [Mozilla Public License 2.0](LICENSE). This notice applies to the project's source in the root build files, `main/`, `tests/` and `tools/`, subject to the original notices and the exceptions below.

- FoloToy's BSP under `components/bsp/`, other inherited MIT code, and prior contributions made available under MIT retain their copyright notices and [MIT terms](LICENSES/MIT.txt). Third-party files with their own notices follow those notices.
- ESP-IDF and its precompiled Wi-Fi libraries retain their upstream terms, including the [Wi-Fi libraries' Apache-2.0 license](https://github.com/espressif/esp32-wifi-lib/blob/master/LICENSE). LVGL and other dependencies retain their respective licenses.
- Embedded font and logo data in `main/font_zh14.c`, `main/font_zh20.c` and `main/logo_bambu.c`, vendor certificate data in `main/bambu_ca.h`, and third-party media follow their respective rights and permissions. Spirit character, voice, and other separately licensed assets require their own authorization.

When distributing a build from this source, tell recipients where to obtain the corresponding covered source and provide the applicable license texts and notices. Link the exact source revision or include its source with the package.

This licensing revision starts with the first public source commit containing this notice. Prior tags and release packages are governed by the terms under which they were distributed. Apply this policy to eligible source in future Spirit releases when those versions are published.
