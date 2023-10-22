#include <cassert>
#include <cstdint>
#include <sstream>
#include <bit>
#include <stddef.h>
#include <byteswap.h>
#include <vector>

#include <fmt/core.h>

#include "instruction.hpp"
#include "macros.hpp"

namespace elf {

const std::map<Instruction::Op, std::vector<uint8_t>> Instruction::op_opcode_map = {
    {Op::mov, { 0xb8 }},
    {Op::syscall, { 0x0f, 0x05 }},
    {Op::call, { 0xe8 }},
    {Op::jmp, { 0xe9 }},
    {Op::je, { 0x0f, 0x84 }},
    {Op::jne, { 0x0f, 0x85 }},
    {Op::jl, { 0x0f, 0x8c }},
    {Op::jle, { 0x0f, 0x8e }},
    {Op::jg, { 0x0f, 0x8f }},
    {Op::jge, { 0x0f, 0x8d }},
    {Op::sub, { 0x81 }},
    {Op::cmp, { 0x81 }},
    {Op::add, { 0x81 }},
};

// modrm second field 0x81 instruction
const std::map<Instruction::Op, uint8_t> Instruction::op_modrm_modifier_map = {
    {Op::add, { 0 }},
    {Op::sub, { 5 }},
    {Op::cmp, { 7 }},
};

// register|modr/m and modr/m|register opcodes
const std::map<Instruction::Op, std::pair<uint8_t, uint8_t>> Instruction::op_rrm_rmr_map = {
    {Op::mov, { 0x8b, 0x89 }},
    {Op::xor_, { 0x31, 0x33 }},
};

// const int address_size_override_prefix = 0x67;

ModRM::ModRM(Register address_reg, AddressingMode sz, uint8_t reg_op, int p_imm) 
    : address(address_reg)
    , mode(sz)
    , reg_op_field(reg_op)
    , imm(p_imm)
{
    // FIXME: rbp without addend doesn't exist, instead there is [rbp + disp32].
    // implement a natural interface for this
    if (address == Register::rbp && mode == AddressingMode::disp0) {
        mode = AddressingMode::disp8;
        imm = 0;
    }
}

uint8_t ModRM::value() const
{
    return (uint8_t)mode << 6 | reg_op_field << 3 | (uint8_t)address;
}

bool Instruction::OpContent::equal(OpType t, const OpContent& o1, const OpContent& o2)
{
    switch(t) {
        case OpType::Register:
        case OpType::Immediate:
            return o1.number == o2.number;
        case OpType::SymbolName:
            return o1.symbol_name == o2.symbol_name;
        case OpType::LabelInfo:
            fmt::print("TODO: Compare label_info");
            std::exit(1);
        case OpType::Memory:
            return o1.memory == o2.memory;
        default:
            UNREACHABLE();
            break;
    }
}

std::vector<uint8_t> Instruction::opcode()
{
    m_generated_opcodes = true;

    std::vector<uint8_t> res;

    // 64 Bit Operand Size Prefix
    // might need some adjusting with later operations
    if (m_64bit)
        res.push_back(0x48);

    // Insert bytes of i into res in reverse order
    auto parse_imm32 = [&res](int i) {
         const std::vector<uint8_t> bytes = { (uint8_t) ((i & 0x000000FF)),
                                              (uint8_t) ((i & 0x0000FF00) >>  8),
                                              (uint8_t) ((i & 0x00FF0000) >> 16),
                                              (uint8_t) ((i & 0xFF000000) >> 24), };

        res.insert(res.end(), bytes.begin(), bytes.end());
    };


    // TODO:
    // accessing 64-bit double constants is done via ModR/M byte
    // referring to an SIB byte. Implement this.

    // Special case where Register is encoded in opcode
    if (m_op1.type == OpType::Register && (m_op2.type == OpType::Immediate || m_op2.type == OpType::String)) {
        if (m_op == Op::mov) {
            // assert(m_op2.cont.number >= 0);

            // MOV future relocated string into register
            res.push_back((int) MovOpCodes::RegImm + m_op1.cont.number);

            if (m_op2.type == OpType::String) {
                m_rela_entries.push_back(RelaEntry::string(1, m_op2.cont.number));
                parse_imm32(0);
            } else {
                parse_imm32(m_op2.cont.number);
            }

            return res;
        } 
    }

    if ((m_op == Op::push || m_op == Op::pop) && (m_op1.type == OpType::Register)) {
        res.push_back(((int) (m_op == Op::push ? PushPopCodes::PushReg : PushPopCodes::PopReg) + m_op1.cont.number));
        return res;
    }


    auto is_modrm = [](OpType o) {
        return o == OpType::Register || o == OpType::Memory;
    };

    auto make_modrm = [](const Operand &o) {
        ModRM modrm((Register)0, (ModRM::AddressingMode)0, 0, 0);
        if (o.type == OpType::Register) {
            modrm.mode = ModRM::AddressingMode::reg;
            modrm.address = (Register) o.cont.number;
        } else if (o.type == OpType::Memory) {
            if (o.cont.memory.addend >= -(255/2) && o.cont.memory.addend <= (255/2))
                modrm.mode = ModRM::AddressingMode::disp8;
            else
                modrm.mode = ModRM::AddressingMode::disp32;
            modrm.address = o.cont.memory.reg;
            modrm.imm = o.cont.memory.addend;
        }

        return modrm;
    };

    auto parse_modrm = [&res, parse_imm32](ModRM m) {
        res.push_back(m.value());

        if (m.mode == ModRM::AddressingMode::disp8 || m.mode == ModRM::AddressingMode::disp32) {
            if (m.mode == ModRM::AddressingMode::disp8) {
                res.push_back((uint8_t)m.imm);
            } else if (m.mode == ModRM::AddressingMode::disp32) {
                parse_imm32(m.imm);
            }
        }
    };

    // Memory access has precedence over register when it comes to constructing ModR/M byte.
    // As you cannot move from memory to memory, the assumption that if op2 is memory, it
    // will be the ModR/M byte can safely be made.

    ModRM modrm;
    if (is_modrm(m_op1.type) && m_op2.type != OpType::Memory) {
        modrm = make_modrm(m_op1);
    } else if (is_modrm(m_op2.type)) {
        modrm = make_modrm(m_op2);
    }

    if (m_op1.type == OpType::None && m_op2.type == OpType::None) {
        res.insert(res.end(), op_opcode_map.at(m_op).begin(), op_opcode_map.at(m_op).end());
    } else if (is_modrm(m_op1.type) && m_op2.type == OpType::Immediate) {
        if (m_op == Op::sub || m_op == Op::cmp) {
            res.insert(res.end(), op_opcode_map.at(m_op).begin(), op_opcode_map.at(m_op).end());

            modrm.reg_op_field = op_modrm_modifier_map.at(m_op); // Turns opcode 0x81 into specific instruction

            parse_modrm(modrm);
            parse_imm32(m_op2.cont.number);
        } else if (m_op == Op::mov) {
            // res.push_back(address_size_override_prefix);
            res.push_back((int) MovOpCodes::RMImm);

            parse_modrm(modrm);
            parse_imm32(m_op2.cont.number);
        }
    } else if (is_modrm(m_op1.type) && m_op2.type == OpType::Register) {
        res.push_back((int) op_rrm_rmr_map.at(m_op).second);

        modrm.reg_op_field = m_op2.cont.number;
        parse_modrm(modrm);
    } else if (m_op1.type == OpType::Register && is_modrm(m_op2.type)) {
        res.push_back((int) op_rrm_rmr_map.at(m_op).first);

        modrm.reg_op_field = m_op1.cont.number;
        parse_modrm(modrm);
    } else if (m_op1.type == OpType::SymbolName) {
        /* NOTE: Here, we differ from NASM in having the linker calculate the
        *        relative JMP operand with .rela.text, like it does with a CALL
        *        to an extern symbol. Why do work we don't have to?
        *        Also avoid having to do two passes over the instructions to pre-calculate
        *        the positions of the labels. (Here's to doing two passes later anyway
        *        for some reason.) */
        m_rela_entries.push_back(RelaEntry::call(op_opcode_map.at(m_op).size(), m_op1.cont.symbol_name));

        res.insert(res.end(), op_opcode_map.at(m_op).begin(), op_opcode_map.at(m_op).end());
        parse_imm32(0);
    } else {
        assert(false && "Unrecognized combination");
    }

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

void Instructions::make_top_64bit()
{
    assert(!m_ins.empty());
    m_ins.back().set64bit(true);
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

} // namespace elf
