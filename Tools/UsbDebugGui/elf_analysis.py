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
    type_name: str = "raw"
    value_kind: str = "raw"
    element_kind: str = "raw"
    element_size: int = 0
    element_count: int = 0


def _decode_name(attribute, default: str = "") -> str:
    if attribute is None:
        return default
    value = attribute.value
    return value.decode("utf-8", errors="replace") if isinstance(value, bytes) else str(value)


def _resolve_type(die) -> dict:
    if die is None:
        return {"type_name": "raw", "value_kind": "raw"}

    qualifiers = {
        "DW_TAG_const_type",
        "DW_TAG_volatile_type",
        "DW_TAG_restrict_type",
        "DW_TAG_atomic_type",
    }
    if die.tag in qualifiers:
        nested = die.get_DIE_from_attribute("DW_AT_type") \
            if "DW_AT_type" in die.attributes else None
        return _resolve_type(nested)

    if die.tag == "DW_TAG_typedef":
        nested = die.get_DIE_from_attribute("DW_AT_type") \
            if "DW_AT_type" in die.attributes else None
        info = _resolve_type(nested)
        info["type_name"] = _decode_name(
            die.attributes.get("DW_AT_name"), info.get("type_name", "raw")
        )
        return info

    if die.tag == "DW_TAG_base_type":
        encoding = int(die.attributes.get("DW_AT_encoding").value)
        kinds = {
            0x02: "bool",
            0x04: "float",
            0x05: "signed",
            0x06: "signed",
            0x07: "unsigned",
            0x08: "unsigned",
        }
        return {
            "type_name": _decode_name(die.attributes.get("DW_AT_name"), "scalar"),
            "value_kind": kinds.get(encoding, "raw"),
            "element_size": int(die.attributes.get("DW_AT_byte_size").value),
        }

    if die.tag == "DW_TAG_pointer_type":
        nested = die.get_DIE_from_attribute("DW_AT_type") \
            if "DW_AT_type" in die.attributes else None
        target = _resolve_type(nested)
        return {
            "type_name": f"{target.get('type_name', 'void')} *",
            "value_kind": "pointer",
            "element_size": int(die.attributes["DW_AT_byte_size"].value)
            if "DW_AT_byte_size" in die.attributes else 4,
        }

    if die.tag == "DW_TAG_array_type":
        nested = die.get_DIE_from_attribute("DW_AT_type") \
            if "DW_AT_type" in die.attributes else None
        element = _resolve_type(nested)
        count = 1
        for child in die.iter_children():
            if child.tag != "DW_TAG_subrange_type":
                continue
            if "DW_AT_count" in child.attributes:
                count *= int(child.attributes["DW_AT_count"].value)
            elif "DW_AT_upper_bound" in child.attributes:
                lower = int(child.attributes.get("DW_AT_lower_bound", 0).value) \
                    if "DW_AT_lower_bound" in child.attributes else 0
                count *= int(child.attributes["DW_AT_upper_bound"].value) - lower + 1
        element_size = int(element.get("element_size", 0))
        return {
            "type_name": f"{element.get('type_name', 'raw')}[{count}]",
            "value_kind": "array",
            "element_kind": element.get("value_kind", "raw"),
            "element_size": element_size,
            "element_count": count,
        }

    if die.tag in {"DW_TAG_structure_type", "DW_TAG_union_type", "DW_TAG_enumeration_type"}:
        prefix = {
            "DW_TAG_structure_type": "struct",
            "DW_TAG_union_type": "union",
            "DW_TAG_enumeration_type": "enum",
        }[die.tag]
        name = _decode_name(die.attributes.get("DW_AT_name"), "anonymous")
        return {"type_name": f"{prefix} {name}", "value_kind": "raw"}

    return {"type_name": "raw", "value_kind": "raw"}


def _load_dwarf_types(elf: ELFFile) -> dict[str, dict]:
    types = {}
    if not elf.has_dwarf_info():
        return types

    dwarf = elf.get_dwarf_info()
    for compilation_unit in dwarf.iter_CUs():
        for die in compilation_unit.iter_DIEs():
            if die.tag != "DW_TAG_variable":
                continue
            source = die
            if "DW_AT_specification" in die.attributes:
                source = die.get_DIE_from_attribute("DW_AT_specification")
            name = _decode_name(source.attributes.get("DW_AT_name"))
            if not name or "DW_AT_type" not in source.attributes:
                continue
            types[name] = _resolve_type(source.get_DIE_from_attribute("DW_AT_type"))
    return types


def load_ram_symbols(path: Path) -> list[ElfSymbol]:
    symbols = []

    with path.open("rb") as stream:
        elf = ELFFile(stream)
        dwarf_types = _load_dwarf_types(elf)
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

            type_info = dwarf_types.get(symbol.name, {})
            symbols.append(ElfSymbol(
                name=symbol.name,
                address=address,
                size=size,
                type_name=type_info.get("type_name", f"raw[{size}]"),
                value_kind=type_info.get("value_kind", "raw"),
                element_kind=type_info.get("element_kind", "raw"),
                element_size=type_info.get("element_size", 0),
                element_count=type_info.get("element_count", 0),
            ))

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
