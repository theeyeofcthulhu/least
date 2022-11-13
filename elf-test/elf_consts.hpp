#pragma once

#define SYSV_ABI_VERSION 0 // FIXME: important?

#define EV_CURRENT 1

enum IdentFields {
    EI_MAG0,
    EI_MAG1,
    EI_MAG2,
    EI_MAG3,
    EI_CLASS,
    EI_DATA,
    EI_VERSION,
    EI_OSABI,
    EI_ABIVERSION,
    EI_PAD,
    EI_NIDENT = 16,
};

enum ElfClass {
    ELFCLASS32 = 1,
    ELFCLASS64 = 2,
};

enum DataEncoding {
    ELFDATA2LSB = 1,
    ELFDATA2MSB = 2,
};

enum ABI {
    ELFOSABI_SYSV = 0,
    ELFOSABI_HPUX = 1,
    ELFOSABI_STANDALONE = 255,
};

enum ObjectFileType {
    ET_NONE = 0,
    ET_REL = 1,
    ET_EXEC = 2,
    ET_DYN = 3,
    ET_CORE = 4,
    ET_LOOS = 0xFE00,
    ET_HIOS = 0xFEFF,
    ET_LOPROC = 0xFF00,
    ET_HIPROC = 0xFFFF,
};

/* Legal values for ST_BIND subfield of st_info (symbol binding) */
#define STB_LOCAL	0	/* Local symbol */
#define STB_GLOBAL	1	/* Global symbol */
#define STB_WEAK	2	/* Weak symbol */
#define STB_NUM		3	/* Number of defined types */
#define STB_LOOS	10	/* Start of OS-specific */
#define STB_HIOS	12	/* End of OS-specific */
#define STB_LOPROC	13	/* Start of processor-specific */
#define STB_HIPROC	15	/* End of processor-specific */

/* Symbol types */
#define STT_NOTYPE	0	/* Symbol type is unspecified */
#define STT_OBJECT	1	/* Symbol is a data object */
#define STT_FUNC	2	/* Symbol is a code object */
#define STT_SECTION	3	/* Symbol associated with a section */
#define STT_FILE	4	/* Symbol's name is file name */
#define STT_COMMON	5	/* Symbol is a common data object */
#define STT_TLS		6	/* Symbol is thread-local data object */
#define STT_NUM		7	/* Number of defined types */

/* Symbol visibilities */
#define STV_DEFAULT	0	/* Default symbol visibility rules */
#define STV_INTERNAL	1	/* Processor specific hidden class */
#define STV_HIDDEN	2	/* Sym unavailable in other modules */
#define STV_PROTECTED	3	/* Not preemptible, not exported */

/* Special section numbers */
#define SHN_UNDEF       0x0000
#define SHN_LORESERVE   0xff00
#define SHN_LOPROC      0xff00
#define SHN_HIPROC      0xff1f
#define SHN_ABS         0xfff1
#define SHN_COMMON      0xfff2
#define SHN_XINDEX	0xffff
#define SHN_HIRESERVE   0xffff

// TODO: fill out (for example with nasm's values)
enum MachineTypes {
    EM_X86_64 = 62,
};

enum reloc64_type {
	R_X86_64_NONE		=  0,	/* No reloc */
	R_X86_64_64		=  1,	/* Direct 64 bit  */
	R_X86_64_PC32		=  2,	/* PC relative 32 bit signed */
	R_X86_64_GOT32		=  3,	/* 32 bit GOT entry */
	R_X86_64_PLT32		=  4,	/* 32 bit PLT address */
	R_X86_64_COPY		=  5,	/* Copy symbol at runtime */
	R_X86_64_GLOB_DAT	=  6,	/* Create GOT entry */
	R_X86_64_JUMP_SLOT	=  7,	/* Create PLT entry */
	R_X86_64_RELATIVE	=  8,	/* Adjust by program base */
	R_X86_64_GOTPCREL	=  9,	/* 32 bit signed PC relative offset to GOT */
	R_X86_64_32		= 10,	/* Direct 32 bit zero extended */
	R_X86_64_32S		= 11,	/* Direct 32 bit sign extended */
	R_X86_64_16		= 12,	/* Direct 16 bit zero extended */
	R_X86_64_PC16		= 13,	/* 16 bit sign extended pc relative */
	R_X86_64_8		= 14,	/* Direct 8 bit sign extended  */
	R_X86_64_PC8		= 15,	/* 8 bit sign extended pc relative */
	R_X86_64_DTPMOD64	= 16,	/* ID of module containing symbol */
	R_X86_64_DTPOFF64	= 17,	/* Offset in module's TLS block */
	R_X86_64_TPOFF64	= 18,	/* Offset in initial TLS block */
	R_X86_64_TLSGD		= 19,	/* 32 bit signed PC relative offset to two GOT entries for GD symbol */
	R_X86_64_TLSLD		= 20,	/* 32 bit signed PC relative offset to two GOT entries for LD symbol */
	R_X86_64_DTPOFF32	= 21,	/* Offset in TLS block */
	R_X86_64_GOTTPOFF	= 22,	/* 32 bit signed PC relative offset to GOT entry for IE symbol */
	R_X86_64_TPOFF32	= 23,	/* Offset in initial TLS block */
	R_X86_64_PC64		= 24,	/* word64 S + A - P */
	R_X86_64_GOTOFF64	= 25,	/* word64 S + A - GOT */
	R_X86_64_GOTPC32	= 26,	/* word32 GOT + A - P */
	R_X86_64_GOT64		= 27,	/* word64 G + A */
	R_X86_64_GOTPCREL64	= 28,	/* word64 G + GOT - P + A */
	R_X86_64_GOTPC64	= 29,	/* word64 GOT - P + A */
	R_X86_64_GOTPLT64	= 30,	/* word64 G + A */
	R_X86_64_PLTOFF64	= 31,	/* word64 L - GOT + A */
	R_X86_64_SIZE32		= 32,	/* word32 Z + A */
	R_X86_64_SIZE64		= 33,	/* word64 Z + A */
	R_X86_64_GOTPC32_TLSDESC= 34,	/* word32 */
	R_X86_64_TLSDESC_CALL	= 35,	/* none */
	R_X86_64_TLSDESC	= 36	/* word64?2 */
};

#define ELF64_R_SYM(x)		((x) >> 32)
#define ELF64_R_TYPE(x)		((x) & 0xffffffff)
#define ELF64_R_INFO(s,t)	(((e_xword)(s) << 32) + ELF64_R_TYPE(t))
/* Section header types */
#define SHT_NULL        0
#define SHT_PROGBITS    1
#define SHT_SYMTAB      2
#define SHT_STRTAB      3
#define SHT_RELA        4
#define SHT_HASH        5
#define SHT_DYNAMIC     6
#define SHT_NOTE        7
#define SHT_NOBITS      8
#define SHT_REL         9
#define SHT_SHLIB       10
#define SHT_DYNSYM      11
#define SHT_INIT_ARRAY	14
#define SHT_FINI_ARRAY	15
#define SHT_PREINIT_ARRAY 16
#define SHT_GROUP	17
#define SHT_SYMTAB_SHNDX 18
#define SHT_LOPROC      0x70000000
#define SHT_HIPROC      0x7fffffff
#define SHT_LOUSER      0x80000000
#define SHT_HIUSER      0xffffffff

/* Section header flags */
#define SHF_WRITE		(1 << 0)	/* Writable */
#define SHF_ALLOC		(1 << 1)	/* Occupies memory during execution */
#define SHF_EXECINSTR		(1 << 2)	/* Executable */
#define SHF_MERGE		(1 << 4)	/* Might be merged */
#define SHF_STRINGS		(1 << 5)	/* Contains nul-terminated strings */
#define SHF_INFO_LINK		(1 << 6)	/* `sh_info' contains SHT index */
#define SHF_LINK_ORDER		(1 << 7)	/* Preserve order after combining */
#define SHF_OS_NONCONFORMING	(1 << 8)	/* Non-standard OS specific handling required */
#define SHF_GROUP		(1 << 9)	/* Section is member of a group */
#define SHF_TLS			(1 << 10)	/* Section hold thread-local data */
