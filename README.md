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

## Example

Cover of Robots (The Humans Are Dead) - Flight of the Conchords

Performed by Amiga narrator.device (and YM2413)

Listen on YouTube:

https://www.youtube.com/watch?v=kT68O6jtOVY

Or audio only:

https://fmamp.com/AmigaNarrator/robots.m4a


## How to compile and run

```
$ sh build.sh
```

This results in two binaries, 'narrator' and 'translator'.

First, use 'translator' to convert English text to phonetic text.

Then, use 'narrator' to convert phonetic text to PCM samples.

The PCM samples are S8 (mono signed 8-bit) at 22200 Hz.

Don't forget to end your sentences with a period.

Run 'translator' and 'narrator' with no arguments for a list of options, such
as pitch, rate, and so forth.

The 'say.sh' script is an example for Linux, and will run 'translator',
'narrator', and then play the PCM samples with ALSA using 'aplay'.

```
$ sh say.sh "Hello world."
```

To run separately:

```
$ ./translator "Hello world." >hello_world.txt
```

The file 'hello_world.txt' should contain:

```
/HEH4LOW WER4LD.
```

```
$ cat hello_world.txt | ./narrator - >hello_world.s8
```

It will run faster if stderr is redirected to /dev/null:

```
$ cat hello_world.txt | ./narrator - 2>/dev/null >hello_world.s8
```

On Linux, 'aplay' can be used to play the file:

```
$ aplay -f S8 -r 22200 hello_world.s8
```

For OS X, a program such as Audacity or SoX will need to be used to convert
the raw samples to a playable format like WAV. This has been tested on OS X
10.12 Sierra and seems to work fine.

## narrator.device

This file will be loaded from the current directory when 'narrator' is run. An
alternative file can be specified using the '-d' flag on 'narrator', but the
flag does not work on 'say.sh'.

It has been tested with the narrator.device from:

- Workbench 1.0
- Workbench 1.1
- Workbench 1.2
- Workbench 1.3 (same as 1.2)
- Workbench 2.04
- Workbench 2.05 (same as 2.04)

This file is not supplied.

## translator.library

This file will be loaded from the current directory when 'translator' is run. An
alternative file can be specified using the '-l' flag on 'translator', but the
flag does not work on 'say.sh'.

It has been tested with the translator.library from:

- Workbench 1.0
- Workbench 1.1
- Workbench 1.2
- Workbench 1.3 (same as 1.2)
- Workbench 1.3.3
- Workbench 2.04
- Workbench 2.05 (same as 2.04)

This file is not supplied.

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

Email: arthur -at- fmamp.com

Released under the GNU General Public License, version 3.

For details on the license, refer to the LICENSE file.

