# Screenshot NoShard

`screenshot_noshard.suprx` is a taiHEN user plugin for PS Vita retail firmware
3.60 through 3.73 and the corresponding testkit/devkit releases available in
the firmware archive. It writes screenshots directly below:

```text
ux0:picture/SCREENSHOT/
```

It also completes screenshot export without creating or queueing Sony's
`ScePhotoPromote` job. Screenshot exports therefore do not initialize MetaGen,
open or transact on `AVContent.db`, or create Photos database entries.

## Hook boundary

The plugin runs in `SceAvMediaService`, after Sony has generated the destination
name and physically written the screenshot.

It applies two changes:

1. A two-byte Thumb branch skips the code that appends the random two-character
   shard directory to the screenshot path. Sony's normal destination writer
   continues with the flat path.
2. A screenshot-filtered hook intercepts the post-write promotion scheduler.
   For a single completed `photo0:/SCREENSHOT/` item, it returns the completed
   path through Sony's existing synchronous completion adapter and returns
   success before the `ScePhotoPromote` job is allocated.

Other media and non-screenshot exports continue through the original scheduler.
The plugin does not replace capture, image encoding, destination naming, file
creation, file writing, or the export response protocol.

## Firmware compatibility

The plugin selects a profile using the loaded `SceAvMediaService` module NID:

| System | Firmware | Module NID |
| --- | --- | --- |
| Retail | 3.60 | `0x18B3FEEF` |
| Retail | 3.65 | `0x2708938B` |
| Retail | 3.67 | `0xEFCA5EE7` |
| Retail | 3.68 | `0xDE405E39` |
| Retail | 3.69 | `0x6CF9DFFE` |
| Retail | 3.70 | `0xDFB649A5` |
| Retail | 3.71 | `0x3B8766B7` |
| Retail | 3.72 | `0x42335CC5` |
| Retail | 3.73 | `0x1E0D7085` |
| Testkit | 3.60 (`3.600.011`) | `0x3FE87731` |
| Testkit | 3.65 (`3.650.011`) | `0x4DDE4533` |
| Testkit | 3.67 (`3.670.011`) | `0x787F0022` |
| Testkit | 3.68 (`3.680.011`) | `0x62BE8716` |
| Testkit | 3.72 (`3.720.011`) | `0xC385918D` |
| Devkit | 3.60 (`3.600.011`) | `0x1656745F` |
| Devkit | 3.65 (`3.650.011`) | `0x135F2E28` |
| Devkit | 3.67 (`3.670.011`) | `0xD55DFE9C` |
| Devkit | 3.68 (`3.680.011`) | `0x035EAA17` |
| Devkit | 3.71 (`3.710.011`) | `0xF0897155` |
| Devkit | 3.73 (`3.730.011`) | `0x7AC573E8` |

Retail modules use path/scheduler/completion offsets
`0x4084`/`0x54BE`/`0x53E8`. Testkit and devkit modules use
`0x40B8`/`0x5550`/`0x547A`.

For the selected profile, the plugin validates the live instruction contexts
for the path branch, branch destination, promotion scheduler, and completion
adapter before installing anything. Only relocated `MOVW`/`MOVT` immediate
fields are masked. An unknown module NID or any byte mismatch fails closed.

The supplied decrypted archive does not contain retail 3.74 or later
testkit/devkit modules, so builds not listed above are not accepted by
association with a neighboring firmware.

## taiHEN configuration

Load the plugin in `SceAvMediaService`:

```text
*NPXS10036
ur0:tai/screenshot_noshard.suprx
```

## Building

```sh
docker run --rm --platform linux/amd64 \
  -v "$PWD:/work" -w /work \
  vitasdk/vitasdk:latest \
  cmake -S . -B build -G "Unix Makefiles" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5
```

```sh
docker run --rm --platform linux/amd64 \
  -v "$PWD:/work" -w /work \
  vitasdk/vitasdk:latest \
  cmake --build build --parallel
```

The resulting plugin is `build/screenshot_noshard.suprx`.

Runtime logging is disabled by default. Set `ENABLE_LOG_MESSAGE` to `1` in
`main.c` before building to write logs to
`ux0:data/screenshot_noshard.log`.
