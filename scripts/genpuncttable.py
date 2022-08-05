#!/usr/bin/python3
from punct import *


def tocstr(s: str):
    s = s.replace("\\", "\\\\")
    s = s.replace('"', '\\"')
    return f'"{s}"'


def gen_table():
    array = []
    i = 0
    print("static const gchar * const")
    print("puncts[] = {")
    for k, vs in punct_map:
        k = tocstr(k)
        vs = [tocstr(s) for s in vs]
        array.append((i, k))
        print(f"    {k}, {', '.join(vs)}, NULL,")
        i += len(vs) + 2
    print("};")
    print()
    print("static const gchar * const * const")
    print("punct_table[] = {")
    for i, k in array:
        print(f"    &puncts[{i}],    // {k}")
    print("};")


if __name__ == "__main__":
    gen_table()
