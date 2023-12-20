# AmigaNarrator

Commodore Amiga narrator.device emulator

## Background

The narrator.device is a speech synthesizer for the Commodore Amiga written by
Mark Barton and Joseph Katz. From what I understand, Mark Barton was responsible
for the speech synthesis, while Joseph Katz handled the systems integration. This
was more or less the same speech synthesizer as the original MacinTalk on the
very first Macs. It was the successor to SAM (Software Automatic Mouth) for the
Apple II, Commodore 64, and Atari 8-bit computers.

For details, listen to an interview with Mark Barton on ANTIC The Atari 8-bit
Podcast.

https://ataripodcast.libsyn.com/antic-interview-385-software-automatic-mouth-mark-barton

## Overview

Similar to vamos (Virtual AmigaOS runtime), this is a tool that will run the
code from the narrator.device and translator.library. The actual code is
emulated using the Musashi 680x0 CPU emulator, while just enough of the OS
is simulated by trapping the exec.library calls.

## How to compile and run

$ sh build.sh

This results in two binaries, 'narrator' and 'translator'.

First, use 'translator' to convert English text to phonetic text.

Then, use 'narrator' to convert phonetic text to PCM samples.

## narrator.device and translator.library

These files will loaded from the current directory when the program is run.

## Resources

#### Markus Wandel's Amiga Exec disassembly
https://wandel.ca/homepage/execdis/index.html

#### Amiga Developer Docs
http://amigadev.elowar.com/
 
#### Hunk File Format
http://amiga-dev.wikidot.com/file-format:hunk

#### vamos and hunktool from amitools by Christian Vogelgsang
https://github.com/cnvogelg/amitools

#### Musashi 680x0 CPU emulator by Karl Stenerud
https://github.com/kstenerud/Musashi

## Legal

Copyright (c) 2023 Arthur Choung. All rights reserved.

Email: arthur -at- hotdoglinux.com

Released under the GNU General Public License, version 3.

For details on the license, refer to the LICENSE file.

