#!/usr/bin/env python3
"""Inspect a PVRP1 PvZ TV replay without starting the game.

The event names are read from the checked-out NetPlay.h, so the tool stays in
sync with changes to the event enum. It prints a compact timeline by default;
pass --all-events when raw protocol traffic is needed.
"""

from __future__ import annotations

import argparse
import collections
import pathlib
import re
import struct
import sys
from dataclasses import dataclass
from typing import Iterable


MAGIC = b"PVRP1\0"
MAX_META_SIZE = 64 * 1024
MAX_PACKET_SIZE = 1024 * 1024
DIRECTIONS = {0: "outbound", 1: "inbound-client", 2: "inbound-server"}
INTERESTING_EVENT_PARTS = (
    "TOUCH_",
    "GAMEPAD_",
    "PLANT_ADD",
    "ZOMBIE_ADD",
    "TAKE_SUNMONEY",
    "TAKE_DEATHMONEY",
    "SEEDPACKET_WASPLANTED",
    "GRIDITEM_ADD",
    "LOCAL_BOARD_ACTION",
    "CONCEDE",
    "_WIN",
    "START_LEVEL",
)


@dataclass(frozen=True)
class Packet:
    index: int
    direction: int
    tick: int
    data: bytes


@dataclass(frozen=True)
class Event:
    packet_index: int
    direction: int
    tick: int
    offset: int
    event_type: int
    size: int
    data: bytes


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("replay", type=pathlib.Path, help="Path to a .rpl file")
    parser.add_argument(
        "--netplay-header",
        type=pathlib.Path,
        default=pathlib.Path(__file__).resolve().parents[1] / "app/src/main/cpp/PvZ/include/PvZ/NetPlay.h",
        help="NetPlay.h used to resolve EventType names",
    )
    parser.add_argument(
        "--seed-header",
        type=pathlib.Path,
        default=pathlib.Path(__file__).resolve().parents[1] / "app/src/main/cpp/PvZ/include/PvZ/Lawn/Common/ConstEnums.h",
        help="ConstEnums.h used to resolve deck seed names",
    )
    parser.add_argument("--all-events", action="store_true", help="Print every event instead of only gameplay-relevant events")
    parser.add_argument("--limit", type=int, default=0, help="Limit printed timeline events; zero means unlimited")
    return parser.parse_args()


def strip_cpp_comments(source: str) -> str:
    source = re.sub(r"/\*.*?\*/", "", source, flags=re.DOTALL)
    return re.sub(r"//[^\n]*", "", source)


def enum_body(source: str, enum_name: str) -> str:
    match = re.search(rf"\benum(?:\s+class)?\s+{re.escape(enum_name)}\b[^{{]*{{", source)
    if match is None:
        raise ValueError(f"cannot find enum {enum_name}")
    start = match.end()
    depth = 1
    for index in range(start, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[start:index]
    raise ValueError(f"unterminated enum {enum_name}")


def parse_integer(expression: str, symbols: dict[str, int]) -> int:
    expression = expression.strip().replace("UINT16_MAX", "65535")
    if not re.fullmatch(r"[A-Za-z0-9_+\-*/()xXa-fA-F\s]+", expression):
        raise ValueError(f"unsupported enum expression: {expression}")
    return int(eval(expression, {"__builtins__": {}}, symbols))


def parse_enum_values(path: pathlib.Path, enum_name: str) -> dict[int, str]:
    source = strip_cpp_comments(path.read_text(encoding="utf-8"))
    values: dict[int, str] = {}
    symbols: dict[str, int] = {}
    value = 0
    for raw_item in enum_body(source, enum_name).split(","):
        item = raw_item.strip()
        if not item:
            continue
        match = re.match(r"([A-Za-z_][A-Za-z0-9_]*)(?:\s*=\s*(.+))?$", item, flags=re.DOTALL)
        if match is None:
            continue
        name, expression = match.groups()
        if expression is not None:
            value = parse_integer(expression, symbols)
        symbols[name] = value
        values.setdefault(value, name)
        value += 1
    return values


def netplay_version(path: pathlib.Path) -> int | None:
    source = path.read_text(encoding="utf-8")
    match = re.search(r"NETPLAY_VERSION\s*=\s*(\d+)", source)
    return int(match.group(1)) if match else None


def read_replay(path: pathlib.Path) -> tuple[dict[str, str], list[Packet]]:
    raw = path.read_bytes()
    if len(raw) < len(MAGIC) + 8 or raw[: len(MAGIC)] != MAGIC:
        raise ValueError("not a PVRP1 replay")

    offset = len(MAGIC)
    meta_size = struct.unpack_from("<I", raw, offset)[0]
    offset += 4
    if meta_size == 0 or meta_size > MAX_META_SIZE or offset + meta_size + 4 > len(raw):
        raise ValueError(f"invalid metadata size: {meta_size}")
    meta_text = raw[offset : offset + meta_size].decode("utf-8", errors="replace")
    offset += meta_size
    metadata = dict(line.split("=", 1) for line in meta_text.splitlines() if "=" in line)
    packet_count = struct.unpack_from("<I", raw, offset)[0]
    offset += 4

    packets: list[Packet] = []
    for index in range(packet_count):
        if offset + 9 > len(raw):
            raise ValueError(f"truncated packet header at index {index}")
        direction, tick, length = struct.unpack_from("<BII", raw, offset)
        offset += 9
        if length > MAX_PACKET_SIZE or offset + length > len(raw):
            raise ValueError(f"invalid packet length at index {index}: {length}")
        packets.append(Packet(index, direction, tick, raw[offset : offset + length]))
        offset += length
    if offset != len(raw):
        raise ValueError(f"trailing data: {len(raw) - offset} bytes")
    return metadata, packets


def iter_events(packets: Iterable[Packet]) -> Iterable[Event]:
    for packet in packets:
        offset = 0
        while offset + 2 <= len(packet.data):
            event_type, size = struct.unpack_from("<BB", packet.data, offset)
            if size < 2 or offset + size > len(packet.data):
                raise ValueError(
                    f"invalid event in packet {packet.index} at byte {offset}: type={event_type}, size={size}, packet-size={len(packet.data)}"
                )
            yield Event(packet.index, packet.direction, packet.tick, offset, event_type, size, packet.data[offset : offset + size])
            offset += size
        if offset != len(packet.data):
            raise ValueError(f"trailing byte in packet {packet.index}")


def signed_byte(value: int) -> int:
    return value - 256 if value >= 128 else value


def decode_event(event: Event, name: str, action_names: dict[int, str], seed_names: dict[int, str]) -> str:
    data = event.data
    if name == "EVENT_LOCAL_BOARD_ACTION" and len(data) == 24:
        _, _, side, kind, slot, _, expected, object_id, col, row, sequence, not_before, expires = struct.unpack("<BBBBBBHIbbHII", data)
        side_name = ("plants", "zombies")[side] if side < 2 else f"invalid-{side}"
        return (
            f"side={side_name} action={action_names.get(kind, str(kind))} slot={slot} "
            f"seed={seed_names.get(expected, expected)} object={object_id} target=({col},{row}) "
            f"sequence={sequence} window=[{not_before},{expires}]"
        )
    if name in {"EVENT_CLIENT_BOARD_TOUCH_DOWN", "EVENT_CLIENT_BOARD_TOUCH_DRAG", "EVENT_CLIENT_BOARD_TOUCH_UP"} and len(data) == 6:
        x, y = struct.unpack_from("<hh", data, 2)
        return f"pixel=({x},{y})"
    if name in {"EVENT_SERVER_BOARD_TOUCH_DOWN", "EVENT_BOARD_TOUCH_DOWN_REPLY"} and len(data) == 8:
        slot, state, x, y = struct.unpack_from("<BBhh", data, 2)
        return f"slot={slot} state={state} pixel=({x},{y})"
    if name in {"EVENT_SERVER_BOARD_TOUCH_DRAG"} and len(data) == 6:
        x, y = struct.unpack_from("<HH", data, 2)
        return f"pixel=({x},{y})"
    if name in {"EVENT_SERVER_BOARD_TOUCH_UP", "EVENT_BOARD_TOUCH_UP_REPLY", "EVENT_SERVER_BOARD_SEEDPACKET_WASPLANTED"} and len(data) == 4:
        first, second = struct.unpack_from("<BB", data, 2)
        if name == "EVENT_SERVER_BOARD_SEEDPACKET_WASPLANTED":
            return f"slot={first} bank={'plants' if second else 'zombies'}"
        return f"value=({first},{second})"
    if name in {"EVENT_SERVER_BOARD_TAKE_SUNMONEY", "EVENT_SERVER_BOARD_TAKE_DEATHMONEY"} and len(data) == 4:
        return f"remaining={struct.unpack_from('<h', data, 2)[0]}"
    if name == "EVENT_SERVER_BOARD_PLANT_ADD" and len(data) == 16:
        col, row, launch, seed_pair, id_pair = struct.unpack_from("<HHHII", data, 2)
        seed = seed_pair & 0xFFFF
        imitater = seed_pair >> 16
        plant_id = id_pair & 0xFFFF
        return f"plant={seed_names.get(seed, seed)} grid=({col},{row}) launch={launch} id={plant_id} imitater={seed_names.get(imitater, imitater)}"
    if name == "EVENT_SERVER_BOARD_ZOMBIE_ADD" and len(data) == 20:
        zombie_type, row, from_wave, rustle, bloated = data[2:7]
        zombie_id = struct.unpack_from("<H", data, 8)[0]
        velocity, position_x = struct.unpack_from("<ff", data, 12)
        return f"zombie-type={zombie_type} row={row} wave={signed_byte(from_wave)} id={zombie_id} x={position_x:.1f} velocity={velocity:.2f} rustle={rustle} bloated={bloated}"
    if len(data) == 3:
        return f"value={data[2]}"
    if len(data) == 4:
        return f"u16={struct.unpack_from('<H', data, 2)[0]}"
    return "payload=" + data[2:].hex(" ")


def main() -> int:
    args = parse_args()
    try:
        metadata, packets = read_replay(args.replay)
        event_names = parse_enum_values(args.netplay_header, "EventType")
        seed_names = parse_enum_values(args.seed_header, "SeedType")
    except (OSError, ValueError, UnicodeError, struct.error) as error:
        print(f"error: {error}", file=sys.stderr)
        return 2

    action_names = {0: "play-seed", 1: "shovel", 2: "fire-cob", 3: "collect-resource", 4: "concede"}
    recorded_version = metadata.get("netplay_version", "unknown")
    local_version = netplay_version(args.netplay_header)
    print(f"Replay: {args.replay}")
    print(f"Packets: {len(packets)}  Duration: {metadata.get('duration_ticks', 'unknown')} ticks")
    print(f"Host: {metadata.get('host', '')} ({metadata.get('host_camp', '')})")
    print(f"Guest: {metadata.get('guest', '')} ({metadata.get('guest_camp', '')})")
    print(f"Winner: {metadata.get('winner', '')}  Map: {metadata.get('map', '')}")
    print(f"Plant deck: {decode_deck(metadata.get('plant_deck', ''), seed_names)}")
    print(f"Zombie deck: {decode_deck(metadata.get('zombie_deck', ''), seed_names)}")
    print(f"Netplay version: replay={recorded_version}, header={local_version if local_version is not None else 'unknown'}")
    if local_version is not None and recorded_version.isdigit() and int(recorded_version) != local_version:
        print("Warning: version mismatch can make event names after a protocol change inaccurate.")

    try:
        events = list(iter_events(packets))
    except ValueError as error:
        print(f"error: {error}", file=sys.stderr)
        return 2
    counts = collections.Counter(event_names.get(event.event_type, f"EVENT_{event.event_type}") for event in events)
    print(f"Events: {len(events)}")
    print("Top event counts:")
    for name, count in counts.most_common(16):
        print(f"  {count:6d}  {name}")

    print("Timeline:")
    printed = 0
    for event in events:
        name = event_names.get(event.event_type, f"EVENT_{event.event_type}")
        if not args.all_events and not any(part in name for part in INTERESTING_EVENT_PARTS):
            continue
        detail = decode_event(event, name, action_names, seed_names)
        print(f"  t={event.tick:6d} {DIRECTIONS.get(event.direction, str(event.direction)):14s} {name:48s} {detail}")
        printed += 1
        if args.limit and printed >= args.limit:
            print(f"  ... limited to {args.limit} events")
            break
    return 0


def decode_deck(raw_deck: str, seed_names: dict[int, str]) -> str:
    result: list[str] = []
    for token in raw_deck.split(","):
        try:
            seed = int(token)
        except ValueError:
            result.append(token)
            continue
        result.append(seed_names.get(seed, str(seed)))
    return ", ".join(result)


if __name__ == "__main__":
    raise SystemExit(main())
