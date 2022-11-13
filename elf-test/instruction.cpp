#include <cassert>
#include <sstream>
#include <bit>
#include <stddef.h>
#include <byteswap.h>
#include <vector>

#include <fmt/core.h>

#include "instruction.hpp"

// Returns std::string containing the four bytes of the number n
std::vector<uint8_t> int32_to_pure_byte_string(int32_t n)
{
    return std::vector<uint8_t> {
        { (uint8_t) ((n & 0xFF000000) >> 24), (uint8_t) ((n & 0x00FF0000) >> 16), (uint8_t) ((n & 0x0000FF00) >> 8), (uint8_t) ((n & 0x000000FF)) }
    };
}

std::vector<uint8_t> Instruction::opcode()
{
    std::vector<uint8_t> res;

    switch(m_op) {
    case Op::syscall: {
        res.push_back(0x0f);
        res.push_back(0x05);
        break;
    }
    case Op::mov: {
        if (m_op1.first == OpType::Register && m_op2.first == OpType::Immediate) {
            assert(m_op2.second >= 0); // for now

            // MOV immediate into register

            const auto bytes = int32_to_pure_byte_string(bswap_32(m_op2.second));

            res.push_back(0xb8 + m_op1.second);
            res.insert(res.end(), bytes.begin(), bytes.end());
        } else if (m_op1.first == OpType::Register && m_op2.first == OpType::String) {
            assert(m_op2.second >= 0);

            // MOV future relocated string into register

            m_rela_entries.push_back({1, m_op2.second});

            const auto bytes = int32_to_pure_byte_string(0); // Placeholder for relocation

            res.push_back(0xb8 + m_op1.second);
            res.insert(res.end(), bytes.begin(), bytes.end());
        } else {
            assert(false && "This mov combination is not implemented yet.");
        }
        break;
    }
    default:
        assert(false && "This instruction is not implemented yet.");
        break;
    }

    m_generated_opcodes = true;

    return res;
}

std::vector<RelaEntry> Instruction::rela_entries(int base)
{
    assert(m_generated_opcodes);

    for (auto &i : m_rela_entries) {
        i.offset += base;
    }

    return m_rela_entries; // TODO: do we need to return copy?
}

std::vector<uint8_t> Instructions::opcodes()
{
    std::vector<uint8_t> res;
    int address = 0;

    for (auto i : m_ins) {
        const auto opcode = i.opcode();
        res.insert(res.end(), opcode.begin(), opcode.end());

        const auto entries = i.rela_entries(address);

        m_rela_entries.insert(m_rela_entries.end(), entries.begin(), entries.end());    // Get offset rela entries from i
                                                                                        // which were calculated in i.opcode()

        address += opcode.size();
    }

    m_generated_opcodes = true;

    return res;
}
