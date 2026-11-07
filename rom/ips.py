"""Small IPS writer/reader used by the ROM build and its round-trip tests."""


def create_ips(source: bytes, target: bytes) -> bytes:
    if len(target) < len(source):
        raise ValueError("This IPS writer intentionally supports expansion only")
    changed = [i for i, value in enumerate(target)
               if i >= len(source) or source[i] != value]
    if not changed:
        return b"PATCHEOF"
    ranges = []
    start = previous = changed[0]
    for position in changed[1:]:
        # Coalesce tiny unchanged gaps to keep the patch compact and simple.
        if position - previous > 9:
            ranges.append((start, previous + 1))
            start = position
        previous = position
    ranges.append((start, previous + 1))
    out = bytearray(b"PATCH")
    for start, end in ranges:
        while start < end:
            size = min(end - start, 0xFFFF)
            if start >= 0xFFFFFF:
                raise ValueError("IPS offsets are limited to 24 bits")
            out.extend(start.to_bytes(3, "big"))
            out.extend(size.to_bytes(2, "big"))
            out.extend(target[start:start + size])
            start += size
    out.extend(b"EOF")
    return bytes(out)


def apply_ips(source: bytes, patch: bytes) -> bytes:
    if not patch.startswith(b"PATCH"):
        raise ValueError("Not an IPS patch")
    target = bytearray(source)
    cursor = 5
    while True:
        if patch[cursor:cursor + 3] == b"EOF":
            cursor += 3
            break
        if cursor + 5 > len(patch):
            raise ValueError("Truncated IPS record")
        offset = int.from_bytes(patch[cursor:cursor + 3], "big")
        size = int.from_bytes(patch[cursor + 3:cursor + 5], "big")
        cursor += 5
        if size:
            data = patch[cursor:cursor + size]
            if len(data) != size:
                raise ValueError("Truncated IPS literal")
            cursor += size
        else:
            if cursor + 3 > len(patch):
                raise ValueError("Truncated IPS RLE record")
            size = int.from_bytes(patch[cursor:cursor + 2], "big")
            value = patch[cursor + 2]
            cursor += 3
            data = bytes([value]) * size
        end = offset + size
        if end > len(target):
            target.extend(b"\0" * (end - len(target)))
        target[offset:end] = data
    if cursor != len(patch):
        raise ValueError("Unexpected data after IPS EOF")
    return bytes(target)
