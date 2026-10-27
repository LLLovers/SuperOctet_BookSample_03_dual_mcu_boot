#!/usr/bin/env python3
"""Generate the only Artemis object that embeds the paired K210 images."""

import argparse
import json
import pathlib
import zlib


def quote(path: pathlib.Path) -> str:
    return str(path.resolve()).replace("\\", "\\\\").replace('"', '\\"')


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--loader", required=True, type=pathlib.Path)
    parser.add_argument("--helios", required=True, type=pathlib.Path)
    parser.add_argument("--assembly", required=True, type=pathlib.Path)
    parser.add_argument("--manifest", required=True, type=pathlib.Path)
    parser.add_argument("--build-id", required=True)
    args = parser.parse_args()

    images = {"loader": args.loader.read_bytes(), "helios": args.helios.read_bytes()}
    values = {name: {"bytes": len(data), "crc32": zlib.crc32(data) & 0xFFFFFFFF}
              for name, data in images.items()}
    args.assembly.parent.mkdir(parents=True, exist_ok=True)
    asm = [".syntax unified", ".section .rodata.so_k210_images,\"a\",%progbits", ".balign 4"]
    for name, path in (("loader", args.loader), ("helios", args.helios)):
        asm += [f".global so_{name}_image", f"so_{name}_image:", f'.incbin "{quote(path)}"',
                f".global so_{name}_image_end", f"so_{name}_image_end:", ".balign 4",
                f".global so_{name}_size", f"so_{name}_size: .long {values[name]['bytes']}",
                f".global so_{name}_crc32", f"so_{name}_crc32: .long 0x{values[name]['crc32']:08x}"]
    asm.append('.section .note.GNU-stack,"",%progbits')
    args.assembly.write_text("\n".join(asm) + "\n", encoding="utf-8")
    args.manifest.write_text(json.dumps({"schema": 1, "buildId": args.build_id, **values},
                                        indent=2, sort_keys=True) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
