#pragma once

#include <cassert>
#include <utility>
#include <string>
#include <vector>
#include <string_view>

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

struct RelaEntry {
    int offset;
    int strid;
};

class Instruction {
public:
    enum class Op {
        mov,
        syscall,
    };

    enum class OpType {
        Register,
        Immediate,
        String,
    };

    Instruction(Op op) : m_op(op)
    {}

    Instruction(Op op, std::pair<OpType, int> op1) : m_op(op), m_op1(op1)
    {}

    Instruction(Op op, std::pair<OpType, int> op1, std::pair<OpType, int> op2) : m_op(op), m_op1(op1), m_op2(op2)
    {}

    std::vector<uint8_t> opcode();
    std::vector<RelaEntry> rela_entries(int base);

private:
    Op m_op;

    std::pair<OpType, int> m_op1;
    std::pair<OpType, int> m_op2;

    std::vector<RelaEntry> m_rela_entries;

    bool m_generated_opcodes = false;
};

class Instructions {
public:
    Instructions()
    {}

    void add(Instruction i) { m_ins.push_back(std::move(i)); }
    std::vector<uint8_t> opcodes();

    const std::vector<RelaEntry> &rela_entries()
    {
        assert(m_generated_opcodes);
        return m_rela_entries;
    }

private:
    std::vector<Instruction> m_ins;
    std::vector<RelaEntry> m_rela_entries;

    bool m_generated_opcodes = false;
};

struct ElfString {
    int id;
    std::string_view sv;
};
