// SPDX-License-Identifier: MIT
//@category HeadHunter
// Oraculo HH03 v2: desassembla os 65.536 opcodes SH-4 com DisassembleCommand
// (follow-flow off, conjunto explicito de todos os slots de 2 bytes).
// Saida por linha: "0xNNNN|hhhh|MNEM|FULL|FLOW".
import ghidra.app.script.GhidraScript;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.program.model.listing.Program;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.address.AddressSetView;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.FlowType;

public class HH03Oracle extends GhidraScript {
    @Override
    public void run() throws Exception {
        Program p = currentProgram;
        Address base = p.getMinAddress();
        AddressSet slots = new AddressSet();
        for (long off = 0; off < 262144; off += 2) {
            slots.add(base.add(off));
        }
        DisassembleCommand cmd = new DisassembleCommand((AddressSetView) slots, null, false);
        final boolean ok = cmd.applyTo(p, monitor);
        println("HH03|cmdOk=" + ok + "|instrs=" + p.getListing().getNumInstructions()
            + "|msgs=" + cmd.getStatusMsg());
        for (long off = 0; off < 262144; off += 2) {
            Address a = base.add(off);
            Instruction ins = p.getListing().getInstructionAt(a);
            final byte[] buf = new byte[2];
            int opcodeTmp = 0xFFFF;
            try {
                p.getMemory().getBytes(a, buf);
                opcodeTmp = ((buf[0] & 0xFF) << 8) | (buf[1] & 0xFF);
            } catch (Exception e) {
                opcodeTmp = 0xFFFF;
            }
            final int opcode = opcodeTmp;
            StringBuilder line = new StringBuilder();
            line.append("0x").append(String.format("%04x", off / 2));
            line.append("|").append(String.format("%04x", opcode));
            line.append('|').append(ins == null ? "ILLEGAL" : ins.getMnemonicString());
            line.append('|').append(ins == null ? "" : ins.toString());
            line.append('|').append(flowToString(ins == null ? null : ins.getFlowType()));
            println(line.toString());
        }
        println("HH03|fim");
    }

    private static String flowToString(FlowType f) {
        if (f == null) return "none";
        String s = f.toString();
        return s == null || s.isEmpty() ? "none" : s;
    }
}
