#pragma once

#include <cassert>
#include <utility>
#include <string>
#include <vector>
#include <string_view>
#include <optional>
#include <map>
#include <cstdint>

#include "elf_consts.hpp"

namespace elf {

enum class Register {
    rax = 0b0000,
    rcx = 0b0001,
    rdx = 0b0010,
    rbx = 0b0011,
    rsp = 0b0100,
    rbp = 0b0101,
    rsi = 0b0110,
    rdi = 0b0111,
    r8  = 0b1000,
    r9  = 0b1001,
    r10 = 0b1010,
    r11 = 0b1011,
    r12 = 0b1100,
    r13 = 0b1101,
    r14 = 0b1110,
    r15 = 0b1111,
};

struct ElfString {
    int id;
    std::string_view sv;
};

struct RelaEntry {
    int offset;
    int strid;
    std::string_view function_name;
    bool is_call;

    // Rela entry referring to a string .rodata
    static RelaEntry string(int offset, int strid)
    {
        return RelaEntry { offset, strid, "", false };
    }

    // Rela entry referring to a relative symbol
    static RelaEntry call(int offset, std::string_view function_name)
    {
        return RelaEntry { offset, 0, function_name, true };
    }

    explicit RelaEntry(int p_offset, int p_strid, std::string_view p_function_name, bool m_is_call)
        : offset(p_offset)
        , strid(p_strid)
        , function_name(p_function_name)
        , is_call(m_is_call)
    {}

    RelaEntry() : offset(0), strid(0), function_name(""), is_call(0)
    {}
};

struct LabelInfo {
    std::string_view name;
    unsigned char visiblity;
    int position;
    bool is_sh_undef;

    // Local or global label in this file
    static LabelInfo infile(std::string_view p_name, int p_visibility)
    {
        return LabelInfo { p_name, p_visibility, 0, false };
    }

    // Label implemented in another file
    static LabelInfo externsym(std::string_view p_name)
    {
        return LabelInfo { p_name, STB_GLOBAL, 0, true };
    }

    LabelInfo() : name(""), visiblity(0), position(-1)
    {}
    explicit LabelInfo(std::string_view p_name, int p_visibility, int p_position, bool p_is_sh_undef)
        : name(p_name)
        , visiblity(p_visibility)
        , position(p_position)
        , is_sh_undef(p_is_sh_undef)
    {}
};

// TODO: r8-r15 with REX
// TODO: SIB byte
// TODO: oddities of ModRM byte
struct ModRM {
    enum class AddressingMode {
        disp0 = 0b00,
        disp8 = 0b01,
        disp32 = 0b10,
        reg = 0b11,
    };

    Register address;
    AddressingMode mode;
    uint8_t reg_op_field;
    int imm;

    ModRM(Register address_reg, AddressingMode sz) : ModRM(address_reg, sz, 0, 0)
    {
    }

    ModRM(Register address_reg, AddressingMode sz, int p_imm) : ModRM(address_reg, sz, 0, p_imm)
    {
    }

    ModRM(Register address_reg, AddressingMode sz, int reg_op, int p_imm);

    ModRM()
    {
    }

    uint8_t value() const;
};

struct MemoryAccess {
    Register reg;
    int addend;
};

class Instruction {
public:
    enum class Op {
        mov,
        syscall,
        label,
        call,
        jmp,
        je,
        jne,
        jl,
        jle,
        jg,
        jge,
        add,
        sub,
        div,
        mul,
        cmp,
        xor_,
        jb,
        jae,
        jbe,
        ja,
        push,
        pop,
    };

    enum class OpType {
        None,
        Register,
        Immediate,
        String,
        DoubleConst,
        SymbolName,
        LabelInfo,
        Memory,
    };

    enum class MovOpCodes {
        RegImm = 0xb8,
        RMImm = 0xc7,
    };

    enum class PushPopCodes {
        PushReg = 0x50,
        PopReg = 0x58,
    };

    union OpContent {
        int number;
        std::string_view symbol_name;
        LabelInfo label;
        MemoryAccess memory;

        OpContent() : number(0)
        {}
        OpContent(int p_number) : number(p_number)
        {}
        OpContent(Register p_reg) : number((int) p_reg)
        {}
        OpContent(std::string_view p_function_name) : symbol_name(p_function_name)
        {}
        OpContent(LabelInfo p_label) : label(p_label)
        {}
        OpContent(MemoryAccess p_memory) : memory(p_memory)
        {}

        friend bool operator==(const OpContent& o1, const OpContent& o2)
        {
            fmt::print("TODO: {}\n", __PRETTY_FUNCTION__);
            return false;
        }
    };

    struct Operand {
        OpType type;
        OpContent cont;

        Operand(OpType p_type, OpContent p_cont)
            : type(p_type)
            , cont(p_cont)
        {}

        static Operand Register(Register reg) { return Operand(Instruction::OpType::Register, OpContent((int) reg)); }
    };

    static const std::map<Op, std::vector<uint8_t>> op_opcode_map;
    static const std::map<Instruction::Op, uint8_t> op_modrm_modifier_map;
    static const std::map<Instruction::Op, std::pair<uint8_t, uint8_t>> op_rrm_rmr_map ;

    Instruction(Op op) : m_op(op), m_op1(OpType::None, {}), m_op2(OpType::None, {})
    {}

    Instruction(Op op, Operand op1) : m_op(op), m_op1(op1), m_op2(OpType::None, {})
    {}

    Instruction(Op op, Operand op1, Operand op2) : m_op(op), m_op1(op1), m_op2(op2)
    {}

    std::vector<uint8_t> opcode();
    std::vector<RelaEntry> rela_entries(int base);
    std::optional<LabelInfo> label(int base);

private:
    Op m_op;

    Operand m_op1;
    Operand m_op2;

    std::vector<RelaEntry> m_rela_entries;

    bool m_generated_opcodes = false;
};

// Holds a set of assembly instructions and labels. Very closely resembles a
// nasm file.
//
// opcodes() returns the machine code for the instruction set. As a byproduct
// this generates the necessary entries in the ELF section .rela.text and labels
// for the ELF symtab. These vectors can then also be accessed through functions.
class Instructions {
public:
    Instructions()
    {}

    void add(Instruction i) { m_ins.push_back(std::move(i)); }
    void add_string(int id, std::string_view sv) { m_strings.push_back({ id, sv }); }
    void add_label(LabelInfo info) { m_labels.push_back(info); }

    std::vector<uint8_t> opcodes();

    const std::vector<RelaEntry> &rela_entries()
    {
        assert(m_generated_opcodes);
        return m_rela_entries;
    }

    const std::vector<LabelInfo> &labels()
    {
        assert(m_generated_opcodes);
        return m_labels;
    }

    const std::vector<ElfString> &strings()
    {
        return m_strings;
    }

private:
    std::vector<Instruction> m_ins;
    std::vector<RelaEntry> m_rela_entries;
    std::vector<LabelInfo> m_labels;
    std::vector<ElfString> m_strings;

    bool m_generated_opcodes = false;
};

} // namespace elf
