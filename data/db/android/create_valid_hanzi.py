def get_validate_hanzi():
    with open("valid_utf16.txt", encoding="utf16") as file:
        return set(file.read())


if __name__ == "__main__":
    hanzi = get_validate_hanzi()
    print("valid_hanzi = set([")
    for c in sorted(hanzi):
        print(f'    u"{c}",')
    print("])")
