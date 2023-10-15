#ifndef ELF_H_
#define ELF_H_

#include <string_view>

#include "instruction.hpp"

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

    static e64_section_header null_symbol()
    {
        return { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    }
};

struct e64_sym {
    e_word st_name;
    e_uchar st_info;
    e_uchar st_other = 0;
    e_half st_shndx;
    e_addr st_value;
    e_xword st_size;

    static e64_sym null_symbol()
    {
        return { 0, 0, 0, 0, 0, 0 };
    }
};

struct e64_rela {
    e_addr r_offset;
    e_xword r_info;
    e_sxword r_addend;
};

class ElfGenerator {
public:
    ElfGenerator(std::string_view fn, Instructions text);

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

#endif // ELF_H_
