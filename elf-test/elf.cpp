#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <fmt/core.h>
#include <fmt/ostream.h>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <stddef.h>
#include <string_view>
#include <vector>

#include "elf_consts.hpp"
#include "instruction.hpp"

using namespace std::string_literals;

typedef uint64_t e_addr;
typedef uint64_t e_off;
typedef uint16_t e_half;
typedef int32_t e_sword;
typedef uint32_t e_word;
typedef uint64_t e_xword;
typedef int64_t e_sxword;
typedef uint8_t e_uchar;

struct e64_header {
    e_uchar e_ident[EI_NIDENT]; /* ELF identification */
    e_half e_type;              /* Object file type */
    e_half e_machine;           /* Machine type */
    e_word e_version;           /* Object file version */
    e_addr e_entry;             /* Entry point address */
    e_off e_phoff;              /* Program header offset */
    e_off e_shoff;              /* Section header offset */
    e_word e_flags;             /* Processor-specific flags */
    e_half e_ehsize;            /* ELF header size */
    e_half e_phentsize;         /* Size of program header entry */
    e_half e_phnum;             /* Number of program header entries */
    e_half e_shentsize;         /* Size of section header entry */
    e_half e_shnum;             /* Number of section header entries */
    e_half e_shstrndx;          /* Section name string table index */
};

struct e64_section_header {
    e_word sh_name;
    e_word sh_type;
    e_xword sh_flags;
    e_addr sh_addr;
    e_off sh_offset;
    e_xword sh_size;
    e_word sh_link;
    e_word sh_info;
    e_xword sh_addralign;
    e_xword sh_entsize;
};

struct e64_sym {
    e_word st_name;
    e_uchar st_info;
    e_uchar st_other = 0;
    e_half st_shndx;
    e_addr st_value;
    e_xword st_size;
};

struct e64_rela {
    e_addr r_offset;
    e_xword r_info;
    e_sxword r_addend;
};

class ElfGenerator {
public:
    ElfGenerator(std::string_view fn, Instructions text, const std::vector<ElfString> rodata)
        : m_fn(fn)
        , m_text(std::move(text))
        , m_rodata(std::move(rodata))
    {
        m_out.open(fn.data());
        if (!m_out.is_open()) {
            fmt::print(std::cerr, "{}: Aborting: {}\n", fn, strerror(errno));
            std::exit(1);
        }
    }

    ~ElfGenerator()
    {
        m_out.close();
    }

    void generate();

private:
    const std::string_view m_fn;

    Instructions m_text;
    const std::vector<ElfString> m_rodata;

    std::ofstream m_out;

    void print_struct(void* struc, size_t sz);

    template<typename T>
    void print_table(std::vector<T> v)
    {
        for (auto& e : v) {
            print_struct(&e, sizeof(e));
        }
    }

    static e_word strtab_offset(std::vector<char> tab, std::string_view name);
    void add_padding(int n, int a);
};

e_word ElfGenerator::strtab_offset(std::vector<char> tab, std::string_view name)
{
    // fmt::print("tab: {}\n", tab);
    auto it = std::search(tab.begin(), tab.end(), name.data(), name.data() + name.length());
    return it - tab.begin();
}

void ElfGenerator::print_struct(void* struc, size_t sz)
{
    std::string_view struct_view { (char*)struc, sz };
    fmt::print(m_out, "{}", struct_view);
}

int round_up_to_multiple(int n, int p)
{
    if (p == 0)
        return n;

    int r = n % p;
    if (r == 0)
        return n;

    return n + p - r;
}

void ElfGenerator::add_padding(int n, int a)
{
    int diff = round_up_to_multiple(n, a) - n;
    for (int i = 0; i < diff; i++)
        m_out << (unsigned char)0x0;
}

void ElfGenerator::generate()
{
    const int default_align = 16;

    // Use vector of chars to avoid hassle with strings containing '\0'
    std::vector<char> section_names;
    // ""s literals can contain '\0'
    const std::string names_literal = "\0.text\0"
                                      ".rodata\0"
                                      ".shstrtab\0"
                                      ".symtab\0"
                                      ".strtab\0"
                                      ".rela.text\0"s;
    section_names.insert(section_names.end(), names_literal.begin(), names_literal.end());

    e64_header header {};

    const int shnum = 7; // TODO: unhardcode (at the earliest when .bss comes into play)

    header.e_ident[EI_MAG0] = '\x7f';
    header.e_ident[EI_MAG1] = 'E';
    header.e_ident[EI_MAG2] = 'L';
    header.e_ident[EI_MAG3] = 'F';
    header.e_ident[EI_CLASS] = ELFCLASS64;
    header.e_ident[EI_DATA] = ELFDATA2LSB;
    header.e_ident[EI_VERSION] = EV_CURRENT;
    header.e_ident[EI_OSABI] = ELFOSABI_SYSV;
    header.e_ident[EI_ABIVERSION] = SYSV_ABI_VERSION;

    header.e_type = ET_REL;
    header.e_machine = EM_X86_64;
    header.e_version = EV_CURRENT;
    header.e_entry = 0x0; // ld's job
    header.e_phoff = 0x0;
    header.e_shoff = sizeof(header);
    header.e_flags = 0x0; // Do we need flags?
    header.e_ehsize = sizeof(header);
    // Object files do not have program headers
    header.e_phentsize = 0x0;
    header.e_phnum = 0x0;
    header.e_shentsize = sizeof(e64_section_header);
    header.e_shnum = shnum;
    header.e_shstrndx = 3; // TODO: unhardcode

    print_struct(&header, sizeof(header));

    const auto instructions = m_text.opcodes();
    const auto rela_entries = m_text.rela_entries();

    std::string rodata_string;
    std::map<int, int> rodata_offsets; // map strids to offsets

    for (const auto elfstring : m_rodata) {
        rodata_offsets[elfstring.id] = rodata_string.size();
        rodata_string.append(elfstring.sv);
    }

    std::vector<e64_rela> rela;
    for (const auto entry : rela_entries) {
        // TODO: unhardcode symtab value for .rodata (3)
        rela.push_back({ (e_addr)entry.offset, ELF64_R_INFO(3, R_X86_64_32), rodata_offsets[entry.strid] });
    }

    std::vector<char> str_tab {};
    str_tab.push_back('\0');

    const std::string file = "elf.cpp\0"s;
    str_tab.insert(str_tab.end(), file.begin(), file.end());

    for (size_t i = 0; i < m_rodata.size(); i++) {
        const std::string str = fmt::format("str{}", i);
        str_tab.insert(str_tab.end(), str.begin(), str.end());
        str_tab.push_back('\0');
    }
    const std::string start = "_start\0"s;
    str_tab.insert(str_tab.end(), start.begin(), start.end());

    // TODO: unhardcode here

    std::vector<e64_sym> sym_tab {};
    sym_tab.push_back({});
    sym_tab.push_back({ strtab_offset(str_tab, "elf.cpp"), (STB_LOCAL << 4) + STT_FILE, 0, SHN_ABS, 0, 0 });
    // TODO: unhardcode section indeces
    // .text and .rodata
    sym_tab.push_back({ 0, (STB_LOCAL << 4) + STT_SECTION, 0, 1, 0, 0 });
    sym_tab.push_back({ 0, (STB_LOCAL << 4) + STT_SECTION, 0, 2, 0, 0 });
    for (size_t i = 0; i < m_rodata.size(); i++) {
        sym_tab.push_back({ strtab_offset(str_tab, fmt::format("str{}\0", i)), (STB_LOCAL << 4) + STT_NOTYPE, 0, 2, (e_addr)rodata_offsets[m_rodata[i].id], 0 });
    }

    e_word n_local_symbols = sym_tab.size();

    sym_tab.push_back({ strtab_offset(str_tab, "_start"), (STB_GLOBAL << 4) + STT_NOTYPE, 0, 1, 0, 0 });

    e_addr section_offset = round_up_to_multiple(sizeof(header) + sizeof(e64_section_header) * shnum, default_align);

    std::vector<e64_section_header> sections {};

    sections.push_back({});

    sections.push_back({ strtab_offset(section_names, ".text"), SHT_PROGBITS, SHF_ALLOC | SHF_EXECINSTR, 0, section_offset, instructions.size(), 0, 0, 16, 0 });
    section_offset += round_up_to_multiple(instructions.size(), default_align);

    sections.push_back({ strtab_offset(section_names, ".rodata"), SHT_PROGBITS, SHF_ALLOC, 0, section_offset, rodata_string.size(), 0, 0, 4, 0 });
    section_offset += round_up_to_multiple(rodata_string.size(), default_align);

    sections.push_back({ strtab_offset(section_names, ".shstrtab"), SHT_STRTAB, 0, 0, section_offset, section_names.size(), 0, 0, 1, 0 });
    section_offset += round_up_to_multiple(section_names.size(), default_align);

    // TODO: unhardcode sh_link = 5 (strtab)
    sections.push_back({ strtab_offset(section_names, ".symtab"), SHT_SYMTAB, 0, 0, section_offset, sym_tab.size() * sizeof(e64_sym), 5, n_local_symbols, 8, 0x18 });
    section_offset += round_up_to_multiple(sym_tab.size() * sizeof(e64_sym), default_align);

    sections.push_back({ strtab_offset(section_names, ".strtab"), SHT_STRTAB, 0, 0, section_offset, str_tab.size(), 0, 0, 1, 0 });
    section_offset += round_up_to_multiple(str_tab.size(), default_align);

    // TODO: unhardcode sh_info? maybe not because section number of .text is always 1?
    sections.push_back({ strtab_offset(section_names, ".rela.text"), SHT_RELA, 0, 0, section_offset, rela_entries.size() * sizeof(e64_rela), 4, 1, 8, 0x18 });

    print_table(sections);

    // This very c++-esque construction allows us to copy
    // vectors into output streams
    std::ostream_iterator<unsigned char> out_it(m_out);

    std::copy(instructions.begin(), instructions.end(), out_it);
    add_padding(instructions.size(), default_align);

    fmt::print(m_out, "{}", rodata_string);
    add_padding(rodata_string.size(), default_align);

    std::copy(section_names.begin(), section_names.end(), out_it);
    add_padding(section_names.size(), default_align);

    print_table(sym_tab);
    add_padding(sym_tab.size() * sizeof(e64_sym), default_align);

    std::copy(str_tab.begin(), str_tab.end(), out_it);
    add_padding(str_tab.size(), default_align);

    print_table(rela);
    add_padding(rela.size(), default_align);
}

int main()
{
    std::vector<ElfString> strings { { 0, "Hello, World!\n" } };

    Instructions a {};

    a.add({ Instruction::Op::mov, std::make_pair(Instruction::OpType::Register, (int)Register::rax), std::make_pair(Instruction::OpType::Immediate, 1) });
    a.add({ Instruction::Op::mov, std::make_pair(Instruction::OpType::Register, (int)Register::rdi), std::make_pair(Instruction::OpType::Immediate, 1) });
    a.add({ Instruction::Op::mov, std::make_pair(Instruction::OpType::Register, (int)Register::rsi), std::make_pair(Instruction::OpType::String, strings[0].id) });
    a.add({ Instruction::Op::mov, std::make_pair(Instruction::OpType::Register, (int)Register::rdx), std::make_pair(Instruction::OpType::Immediate, strings[0].sv.length()) });
    a.add({ Instruction::Op::syscall });
    a.add({ Instruction::Op::mov, std::make_pair(Instruction::OpType::Register, (int)Register::rax), std::make_pair(Instruction::OpType::Immediate, 0x3c) });
    a.add({ Instruction::Op::mov, std::make_pair(Instruction::OpType::Register, (int)Register::rdi), std::make_pair(Instruction::OpType::Immediate, 0) });
    a.add({ Instruction::Op::syscall });

    ElfGenerator gen { "out.o", a, strings };
    gen.generate();
}
