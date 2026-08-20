from dataclasses import dataclass
from pathlib import Path

from elftools.elf.elffile import ELFFile


RAM_START = 0x20000000
RAM_END = 0x20005000


@dataclass
class ElfSymbol:
    name: str
    address: int
    size: int


def load_ram_symbols(path: Path) -> list[ElfSymbol]:
    symbols = []

    with path.open("rb") as stream:
        elf = ELFFile(stream)
        symbol_table = elf.get_section_by_name(".symtab")

        if symbol_table is None:
            raise RuntimeError("ELF에 .symtab이 없습니다.")

        for symbol in symbol_table.iter_symbols():
            if symbol["st_info"]["type"] != "STT_OBJECT":
                continue

            address = int(symbol["st_value"])
            size = int(symbol["st_size"])

            if not symbol.name or size == 0:
                continue

            if not (
                RAM_START <= address
                and address + size <= RAM_END
            ):
                continue

            symbols.append(
                ElfSymbol(
                    name=symbol.name,
                    address=address,
                    size=size,
                )
            )

    return sorted(symbols, key=lambda item: item.address)




if __name__ == "__main__":
    project_root = Path(__file__).resolve().parents[2]

    elf_path = (
        project_root
        / "Debug"
        / "stm32f103c8t6.elf"
    )

    print(f"ELF: {elf_path}")

    symbols = load_ram_symbols(elf_path)

    print(f"RAM symbols: {len(symbols)}")

    for symbol in symbols:
        print(
            f"{symbol.name:<36} "
            f"0x{symbol.address:08X} "
            f"{symbol.size:>5} bytes"
        )