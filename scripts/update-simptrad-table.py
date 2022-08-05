#!/usr/bin/python3
import sys

sys.path.append(".")

from ZhConversion import *
from valid_hanzi import *


def convert(s, d, n):
    out = ""
    end = len(s)
    begin = 0
    while begin < end:
        for i in range(min(n, end - begin), 0, -1):
            t = s[begin : begin + i]
            t = d.get(t, t if i == 1 else None)
            if t:
                break
        out = out + t
        begin += i
    return out


def filter_more(records, n):
    han = [(k, v) for (k, v) in records if len(k) <= 0]
    hand = dict(han)
    hanm = [(k, v) for (k, v) in records if convert(k, hand, n) != v]
    return hanm + han


def filter_func(args):
    k, v = args
    # length is not equal or length > 6
    if len(k) != len(v) or len(k) > 6:
        return False
    # k includes invalid hanzi
    if not all(c in valid_hanzi for c in k):
        return False
    # v includes invalid hanzi
    if not all(c in valid_hanzi for c in v):
        return False

    # # check chars in k and v
    # for c1, c2 in zip(k, v):
    #     if c1 == c2:
    #         continue
    #     if c2 not in S_2_T.get(c1, []):
    #         return False
    return True


def get_records():
    records = [kv for kv in zh2Hant.items() if filter_func(kv)]

    maxlen = max([len(k) for (k, v) in records])
    for i in range(1, maxlen - 1):
        records = filter_more(records, i)
    records.sort()
    return maxlen, records


def main():
    print("static const gchar *simp_to_trad[][2] = {")
    maxlen, records = get_records()
    for s, ts in records:
        print(f'    {{ "{s}", "{ts}" }},')
    print("};")
    print(f"#define SIMP_TO_TRAD_MAX_LEN ({maxlen})")


if __name__ == "__main__":
    main()
