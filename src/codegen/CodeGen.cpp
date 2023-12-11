#include "CodeGen.hpp"

#include "CodeGenUtil.hpp"

#include "logging.hpp"

void CodeGen::allocate() {
    // 备份 $ra $fp
    unsigned offset = PROLOGUE_OFFSET_BASE;

    // 为每个参数分配栈空间
    for (auto &arg : context.func->get_args()) {
        auto size = arg.get_type()->get_size();
        offset = ALIGN(offset + size, size);
        context.offset_map[&arg] = -static_cast<int>(offset);
        LOG(DEBUG) << "offset of " << arg.print() << " is " << context.offset_map[&arg];
    }

    // 为指令结果分配栈空间
    for (auto &bb : context.func->get_basic_blocks()) {
        for (auto &instr : bb.get_instructions()) {
            // 每个非 void 的定值都分配栈空间
            if (not instr.is_void()) {
                auto size = instr.get_type()->get_size();
                offset = ALIGN(offset + size, size);
                context.offset_map[&instr] = -static_cast<int>(offset);
                LOG(DEBUG) << "offset of " << instr.print() << " is " << context.offset_map[&instr];
            }
            // alloca 的副作用：分配额外空间
            if (instr.is_alloca()) {
                auto *alloca_inst = static_cast<AllocaInst *>(&instr);
                auto alloc_size = alloca_inst->get_alloca_type()->get_size();
                offset += alloc_size;
                LOG(DEBUG) << "address of alloced space is " << offset;
            }
        }
    }

    // 分配栈空间，需要是 16 的整数倍
    context.frame_size = ALIGN(offset, PROLOGUE_ALIGN);
}

void CodeGen::load_to_greg(Value *val, const Reg &reg) {
    assert(val->get_type()->is_integer_type() ||
           val->get_type()->is_pointer_type());

    if (auto *constant = dynamic_cast<ConstantInt *>(val)) {
        int32_t val = constant->get_value();
        if (IS_IMM_12(val)) {
            append_inst(ADDI WORD, {reg.print(), "$zero", std::to_string(val)});
        } else {
            load_large_int32(val, reg);
        }
    } else if (auto *global = dynamic_cast<GlobalVariable *>(val)) {
        append_inst(LOAD_ADDR, {reg.print(), global->get_name()});
    } else {
        load_from_stack_to_greg(val, reg);
    }
}

void CodeGen::load_large_int32(int32_t val, const Reg &reg) {
    int32_t high_20 = val >> 12; // si20
    uint32_t low_12 = val & LOW_12_MASK;
    append_inst(LU12I_W, {reg.print(), std::to_string(high_20)});
    append_inst(ORI, {reg.print(), reg.print(), std::to_string(low_12)});
}

void CodeGen::load_large_int64(int64_t val, const Reg &reg) {
    auto low_32 = static_cast<int32_t>(val & LOW_32_MASK);
    load_large_int32(low_32, reg);

    auto high_32 = static_cast<int32_t>(val >> 32);
    int32_t high_32_low_20 = (high_32 << 12) >> 12; // si20
    int32_t high_32_high_12 = high_32 >> 20;        // si12
    append_inst(LU32I_D, {reg.print(), std::to_string(high_32_low_20)});
    append_inst(LU52I_D,
                {reg.print(), reg.print(), std::to_string(high_32_high_12)});
}

void CodeGen::load_from_stack_to_greg(Value *val, const Reg &reg) {
    auto offset = context.offset_map.at(val);
    auto offset_str = std::to_string(offset);
    auto *type = val->get_type();
    if (IS_IMM_12(offset)) {
        if (type->is_int1_type()) {
            append_inst(LOAD BYTE, {reg.print(), "$fp", offset_str});
        } else if (type->is_int32_type()) {
            append_inst(LOAD WORD, {reg.print(), "$fp", offset_str});
        } else { // Pointer
            append_inst(LOAD DOUBLE, {reg.print(), "$fp", offset_str});
        }
    } else {
        load_large_int64(offset, reg);
        append_inst(ADD DOUBLE, {reg.print(), "$fp", reg.print()});
        if (type->is_int1_type()) {
            append_inst(LOAD BYTE, {reg.print(), reg.print(), "0"});
        } else if (type->is_int32_type()) {
            append_inst(LOAD WORD, {reg.print(), reg.print(), "0"});
        } else { // Pointer
            append_inst(LOAD DOUBLE, {reg.print(), reg.print(), "0"});
        }
    }
}

void CodeGen::store_from_greg(Value *val, const Reg &reg) {
    auto offset = context.offset_map.at(val);
    auto offset_str = std::to_string(offset);
    auto *type = val->get_type();
    if (IS_IMM_12(offset)) {
        if (type->is_int1_type()) {
            append_inst(STORE BYTE, {reg.print(), "$fp", offset_str});
        } else if (type->is_int32_type()) {
            append_inst(STORE WORD, {reg.print(), "$fp", offset_str});
        } else { // Pointer
            append_inst(STORE DOUBLE, {reg.print(), "$fp", offset_str});
        }
    } else {
        auto addr = Reg::t(8);
        load_large_int64(offset, addr);
        append_inst(ADD DOUBLE, {addr.print(), "$fp", addr.print()});
        if (type->is_int1_type()) {
            append_inst(STORE BYTE, {reg.print(), addr.print(), "0"});
        } else if (type->is_int32_type()) {
            append_inst(STORE WORD, {reg.print(), addr.print(), "0"});
        } else { // Pointer
            append_inst(STORE DOUBLE, {reg.print(), addr.print(), "0"});
        }
    }
}

void CodeGen::load_to_freg(Value *val, const FReg &freg) {
    assert(val->get_type()->is_float_type());
    if (auto *constant = dynamic_cast<ConstantFP *>(val)) {
        float val = constant->get_value();
        load_float_imm(val, freg);
    } else {
        auto offset = context.offset_map.at(val);
        auto offset_str = std::to_string(offset);
        if (IS_IMM_12(offset)) {
            append_inst(FLOAD SINGLE, {freg.print(), "$fp", offset_str});
        } else {
            auto addr = Reg::t(8);
            load_large_int64(offset, addr);
            append_inst(ADD DOUBLE, {addr.print(), "$fp", addr.print()});
            append_inst(FLOAD SINGLE, {freg.print(), addr.print(), "0"});
        }
    }
}

void CodeGen::load_float_imm(float val, const FReg &r) {
    int32_t bytes = *reinterpret_cast<int32_t *>(&val);
    load_large_int32(bytes, Reg::t(8));
    append_inst(GR2FR WORD, {r.print(), Reg::t(8).print()});
}

void CodeGen::store_from_freg(Value *val, const FReg &r) {
    auto offset = context.offset_map.at(val);
    if (IS_IMM_12(offset)) {
        auto offset_str = std::to_string(offset);
        append_inst(FSTORE SINGLE, {r.print(), "$fp", offset_str});
    } else {
        auto addr = Reg::t(8);
        load_large_int64(offset, addr);
        append_inst(ADD DOUBLE, {addr.print(), "$fp", addr.print()});
        append_inst(FSTORE SINGLE, {r.print(), addr.print(), "0"});
    }
}

void CodeGen::gen_prologue() {
    // 寄存器备份及栈帧设置
    if (IS_IMM_12(-static_cast<int>(context.frame_size))) {
        append_inst("st.d $ra, $sp, -8");
        append_inst("st.d $fp, $sp, -16");
        append_inst("addi.d $fp, $sp, 0");
        append_inst("addi.d $sp, $sp, " +
                    std::to_string(-static_cast<int>(context.frame_size)));
    } else {
        load_large_int64(context.frame_size, Reg::t(0));
        append_inst("st.d $ra, $sp, -8");
        append_inst("st.d $fp, $sp, -16");
        append_inst("sub.d $sp, $sp, $t0");
        append_inst("add.d $fp, $sp, $t0");
    }

    // 将函数参数转移到栈帧上
    int garg_cnt = 0;
    int farg_cnt = 0;
    for (auto &arg : context.func->get_args()) {
        if (arg.get_type()->is_float_type()) {
            store_from_freg(&arg, FReg::fa(farg_cnt++));
        } else { // int or pointer
            store_from_greg(&arg, Reg::a(garg_cnt++));
        }
    }
}

void CodeGen::gen_epilogue() {
    // TODO 根据你的理解设定函数的 epilogue
    append_inst(context.func->get_name() + "_exit", ASMInstruction::Label);
    if (IS_IMM_12(static_cast<int>(context.frame_size))) {
        append_inst("addi.d $sp, $sp, " + std::to_string(static_cast<int>(context.frame_size)));
        append_inst("ld.d $ra, $sp, -8");
        append_inst("ld.d $fp, $sp, -16");
        append_inst("jr $ra");
    }
    else {
        load_large_int64(context.frame_size, Reg::t(0));
        append_inst("add.d $sp, $sp, $t0");
        append_inst("ld.d $ra, $sp, -8");
        append_inst("ld.d $fp, $sp, -16");
        append_inst("jr $ra");
    }
}

void CodeGen::gen_ret() {
    // TODO 函数返回，思考如何处理返回值、寄存器备份，如何返回调用者地址
    auto *returnInst = static_cast<ReturnInst *>(context.inst);
    if (returnInst->is_void_ret()) {
        load_to_greg(ConstantInt::get(0, m), Reg::a(0));
    }
    else if (returnInst->get_operand(0)->get_type()->is_integer_type()) {
        load_to_greg(returnInst->get_operand(0), Reg::a(0));
    }
    else if (returnInst->get_operand(0)->get_type()->is_float_type()) {
        load_to_freg(returnInst->get_operand(0), FReg::fa(0));
    }
    append_inst("b " + context.func->get_name() + "_exit");
}

std::vector<PhiInst *> CodeGen::check_phi(BasicBlock *bb, BasicBlock *succ_bb, std::vector<int> &index){
    std::vector<PhiInst *> phi;
    phi.clear();
    index.clear();
    for (Instruction &instr : succ_bb->get_instructions()) {
        if (instr.is_phi()) {
            PhiInst *cur = static_cast<PhiInst *>(&instr);
            int n = cur->get_operands().size();
            for (int i = 1; i < n; i += 2) {
                if (cur->get_operands().at(i) == bb) {
                    phi.push_back(cur);
                    index.push_back(i - 1);
                }
            }
        }
    }
    return phi;
}

void CodeGen::insert_copy_for_phi(std::vector<PhiInst *> phi, std::vector<int> index){
    LOG(INFO) << "insert copy for phi, number of phis: " << phi.size();
    for (int i = 0; i < phi.size(); ++i) {
        PhiInst *cur = phi.at(i);
        Value *src_op = cur->get_operand(index.at(i));
        if (cur->get_type()->is_float_type()) {
            load_to_freg(src_op, FReg::ft(0));
            store_from_freg(cur, FReg::ft(0));
        }
        else {
            load_to_greg(src_op, Reg::t(0));
            store_from_greg(cur, Reg::t(0));
        }
    }
}

void CodeGen::gen_br() {
    auto *branchInst = static_cast<BranchInst *>(context.inst);
    BasicBlock *cur_bb = branchInst->get_parent();
    if (branchInst->is_cond_br()) {
        // TODO 补全条件跳转的情况
        auto *cond = static_cast<Value *>(branchInst->get_operand(0));
        auto *trueBB = static_cast<BasicBlock *>(branchInst->get_operand(1));
        auto *falseBB = static_cast<BasicBlock *>(branchInst->get_operand(2));
        load_to_greg(cond, Reg::t(0));
        append_inst("beqz $t0, "+context.func->get_name()+"_cond_br_"+std::to_string(context.cond_br_number)+"_false");
        std::vector<int> index;
        std::vector<PhiInst *> phi = check_phi(cur_bb, trueBB, index);
        insert_copy_for_phi(phi, index);
        append_inst("b " + label_name(trueBB));
        append_inst(context.func->get_name()+"_cond_br_"+std::to_string(context.cond_br_number)+"_false", ASMInstruction::Label);
        std::vector<PhiInst *> phi1 = check_phi(cur_bb, falseBB, index);
        insert_copy_for_phi(phi1, index);
        append_inst("b " + label_name(falseBB));
        context.cond_br_number++;
    } else {
        auto *branchbb = static_cast<BasicBlock *>(branchInst->get_operand(0));
        LOG(INFO) << "translate " << branchInst->print() <<" in basicblock " << branchInst->get_parent()->get_name();
        LOG(INFO) << "check if basicblock " << branchbb->get_name() << " has phi";
        std::vector<int> index;
        std::vector<PhiInst *> phi = check_phi(cur_bb, branchbb, index);
        LOG(INFO) << "find phi number: " << phi.size();
        insert_copy_for_phi(phi, index);
        append_inst("b " + label_name(branchbb));
    }
}

void CodeGen::gen_binary() {
    // 分别将左右操作数加载到 $t0 $t1
    load_to_greg(context.inst->get_operand(0), Reg::t(0));
    load_to_greg(context.inst->get_operand(1), Reg::t(1));
    // 根据指令类型生成汇编
    switch (context.inst->get_instr_type()) {
    case Instruction::add:
        output.emplace_back("add.w $t2, $t0, $t1");
        break;
    case Instruction::sub:
        output.emplace_back("sub.w $t2, $t0, $t1");
        break;
    case Instruction::mul:
        output.emplace_back("mul.w $t2, $t0, $t1");
        break;
    case Instruction::sdiv:
        output.emplace_back("div.w $t2, $t0, $t1");
        break;
    default:
        assert(false);
    }
    // 将结果填入栈帧中
    store_from_greg(context.inst, Reg::t(2));
}

void CodeGen::gen_float_binary() {
    // TODO 浮点类型的二元指令
    load_to_freg(context.inst->get_operand(0), FReg::ft(0));
    load_to_freg(context.inst->get_operand(1), FReg::ft(1));
    switch (context.inst->get_instr_type()) {
        case Instruction::fadd:
            output.emplace_back("fadd.s $ft2, $ft0, $ft1");
            break;
        case Instruction::fsub:
            output.emplace_back("fsub.s $ft2, $ft0, $ft1");
            break;
        case Instruction::fmul:
            output.emplace_back("fmul.s $ft2, $ft0, $ft1");
            break;
        case Instruction::fdiv:
            output.emplace_back("fdiv.s $ft2, $ft0, $ft1");
            break;
        default:
            assert(false);
    }
    store_from_freg(context.inst, FReg::ft(2));
}

void CodeGen::gen_alloca() {
    /* 我们已经为 alloca 的内容分配空间，在此我们还需保存 alloca
     * 指令自身产生的定值，即指向 alloca 空间起始地址的指针
     */
    // TODO 将 alloca 出空间的起始地址保存在栈帧上
    auto *allocaInst = static_cast<AllocaInst *>(context.inst);
    // compute address of alloced space
    auto start = context.offset_map[context.inst];
    LOG(DEBUG) << "offset of ptr is " << start;
    start = start - allocaInst->get_alloca_type()->get_size();
    LOG(DEBUG) << "start of alloced space is " << start;
    load_large_int64(start, Reg::t(0));
    append_inst("add.d $t0, $t0, $fp");
    // store address
    store_from_greg(allocaInst, Reg::t(0));
}

void CodeGen::gen_load() {
    auto *ptr = context.inst->get_operand(0);
    auto *type = context.inst->get_type();
    load_to_greg(ptr, Reg::t(0));

    if (type->is_float_type()) {
        append_inst("fld.s $ft0, $t0, 0");
        store_from_freg(context.inst, FReg::ft(0));
    }
    else if (type->is_pointer_type()) {
        append_inst("ld.d $t0, $t0, 0");
        store_from_greg(context.inst, Reg::t(0));
    }
    else {
        // TODO load 整数类型的数据
        append_inst("ld.w $t0, $t0, 0");
        store_from_greg(context.inst, Reg::t(0));
    }
}

void CodeGen::gen_store() {
    // TODO 翻译 store 指令
    auto *value = context.inst->get_operand(0);
    auto *ptr = context.inst->get_operand(1);
    load_to_greg(ptr, Reg::t(0));
    if (value->get_type()->is_int1_type()) {
        load_to_greg(value, Reg::t(1));
        append_inst("st.b $t1, $t0, 0");
    }
    else if (value->get_type()->is_int32_type()) {
        load_to_greg(value, Reg::t(1));
        append_inst("st.w $t1, $t0, 0");
    }
    else if (value->get_type()->is_float_type()) {
        load_to_freg(value, FReg::ft(1));
        append_inst("fst.s $ft1, $t0, 0");
    }
    else if (value->get_type()->is_pointer_type()) {
        load_to_greg(value, Reg::t(1));
        append_inst("st.d $t1, $t0, 0");
    }
}

void CodeGen::gen_icmp() {
    // TODO 处理各种整数比较的情况
    load_to_greg(context.inst->get_operand(0), Reg::t(0));
    load_to_greg(context.inst->get_operand(1), Reg::t(1));
    switch (context.inst->get_instr_type()) {
        case Instruction::eq:
            append_inst("slt $t2, $t1, $t0");
            append_inst("slt $t3, $t0, $t1");
            append_inst("nor $t2, $t2, $t3");
            append_inst("bstrpick.d $t2, $t2, 0, 0");
            break;
        case Instruction::ne:
            append_inst("slt $t2, $t1, $t0");
            append_inst("slt $t3, $t0, $t1");
            append_inst("or $t2, $t2, $t3");
            break;
        case Instruction::gt:
            append_inst("slt $t2, $t1, $t0");
            break;
        case Instruction::ge:
            append_inst("slt $t2, $t1, $t0");
            append_inst("slt $t3, $t0, $t1");
            append_inst("nor $t2, $t2, $t3");
            append_inst("bstrpick.d $t2, $t2, 0, 0");
            append_inst("slt $t3, $t1, $t0");
            append_inst("or $t2, $t2, $t3");
            break;
        case Instruction::lt:
            append_inst("slt $t2, $t0, $t1");
            break;
        case Instruction::le:
            append_inst("slt $t2, $t1, $t0");
            append_inst("slt $t3, $t0, $t1");
            append_inst("nor $t2, $t2, $t3");
            append_inst("bstrpick.d $t2, $t2, 0, 0");
            append_inst("slt $t3, $t0, $t1");
            append_inst("or $t2, $t2, $t3");
            break;
        default:
            assert(false);
    }
    store_from_greg(context.inst, Reg::t(2));
}

void CodeGen::gen_fcmp() {
    // TODO 处理各种浮点数比较的情况
    load_to_freg(context.inst->get_operand(0), FReg::ft(0));
    load_to_freg(context.inst->get_operand(1), FReg::ft(1));
    switch (context.inst->get_instr_type()) {
        case Instruction::fge:
            append_inst("fcmp.sle.s $fcc0 $ft1, $ft0");
            break;
        case Instruction::fgt:
            append_inst("fcmp.slt.s $fcc0 $ft1, $ft0");
            break;
        case Instruction::fle:
            append_inst("fcmp.sle.s $fcc0, $ft0, $ft1");
            break;
        case Instruction::flt:
            append_inst("fcmp.slt.s $fcc0, $ft0, $ft1");
            break;
        case Instruction::feq:
            append_inst("fcmp.seq.s $fcc0, $ft0, $ft1");
            break;
        case Instruction::fne:
            append_inst("fcmp.sne.s $fcc0, $ft0, $ft1");
            break;
        default:
            assert(false);
    }
    append_inst("beqz $fcc0, .write0_" + std::to_string(context.fcmp_number));
    append_inst("b .write1_" + std::to_string(context.fcmp_number));

    append_inst(".write0_" + std::to_string(context.fcmp_number), ASMInstruction::Label);
    store_from_greg(context.inst, Reg::zero());
    append_inst("b .succ_" + std::to_string(context.fcmp_number));

    append_inst(".write1_" + std::to_string(context.fcmp_number), ASMInstruction::Label);
    load_to_greg(ConstantInt::get(1, m), Reg::t(0));
    store_from_greg(context.inst, Reg::t(0));
    append_inst("b .succ_" + std::to_string(context.fcmp_number));

    append_inst(".succ_" + std::to_string(context.fcmp_number), ASMInstruction::Label);
    context.fcmp_number++;
}

void CodeGen::gen_zext() {
    // TODO 将窄位宽的整数数据进行零扩展
    load_to_greg(context.inst->get_operand(0), Reg::t(0));
    store_from_greg(context.inst, Reg::t(0));
}

void CodeGen::gen_call() {
    // TODO 函数调用，注意我们只需要通过寄存器传递参数，即不需考虑栈上传参的情况
    LOG(DEBUG) << "enter function CodeGen::gen_call()";
    auto *callInst = static_cast<CallInst *>(context.inst);
    std::vector<Value *>operands = callInst->get_operands();
    LOG(DEBUG) << "size of operands is " << operands.size();
    LOG(DEBUG) << "operands of instruction " << callInst->print() << " are: ";
    for (auto *element : operands) {
        LOG(DEBUG) << element->print();
    }
    Function *func;
    int i = 0;
    int j = 0;
    bool is_first = true;
    for (auto *operand : operands) {
        if (is_first) {
            func = static_cast<Function *>(operand);
            is_first = false;
        }
        else {
            if (operand->get_type()->is_integer_type() || operand->get_type()->is_pointer_type()) {
                load_to_greg(operand, Reg::a(i));
                i++;
            }
            else if (operand->get_type()->is_float_type()) {
                load_to_freg(operand, FReg::fa(j));
                j++;
            }
        }
    }
    LOG(DEBUG) << "function name is " << func->get_name();
    append_inst("bl " + func->get_name());
    if (func->get_return_type()->is_float_type()) {
        store_from_freg(context.inst, FReg::fa(0));
    }
    else if (func->get_return_type()->is_integer_type()) {
        store_from_greg(context.inst, Reg::a(0));
    }
    LOG(DEBUG) << "leave function CodeGen::gen_call()";
}

void CodeGen::gen_gep() {
    // TODO 计算内存地址
    auto *ptr = static_cast<Value *>(context.inst->get_operand(0));
    load_to_greg(ptr, Reg::t(0));
    if (ptr->get_type()->get_pointer_element_type()->is_array_type()) {
        auto *offset = context.inst->get_operand(2);
        load_to_greg(offset, Reg::t(1));
    }
    else {
        auto *offset = context.inst->get_operand(1);
        load_to_greg(offset, Reg::t(1));
    }
    auto *gepInst = static_cast<GetElementPtrInst *>(context.inst);
    auto *element_size = ConstantInt::get(static_cast<int>(gepInst->get_element_type()->get_size()), m);
    load_to_greg(element_size, Reg::t(2));
    append_inst("mul.d $t1, $t1, $t2");
    append_inst("add.d $t0, $t0, $t1");
    store_from_greg(context.inst, Reg::t(0));
}

void CodeGen::gen_sitofp() {
    // TODO 整数转向浮点数
    load_to_greg(context.inst->get_operand(0), Reg::t(0));
    append_inst("movgr2fr.w $ft0, $t0");
    append_inst("ffint.s.w $ft0, $ft0");
    store_from_freg(context.inst, FReg::ft(0));
}

void CodeGen::gen_fptosi() {
    // TODO 浮点数转向整数，注意向下取整(round to zero)
    load_to_freg(context.inst->get_operand(0), FReg::ft(0));
    append_inst("ftintrz.w.s $ft0, $ft0");
    append_inst("movfr2gr.s $t0, $ft0");
    store_from_greg(context.inst, Reg::t(0));
}

void CodeGen::run() {
    // 确保每个函数中基本块的名字都被设置好
    // 想一想：为什么？
    m->set_print_name();

    /* 使用 GNU 伪指令为全局变量分配空间
     * 你可以使用 `la.local` 指令将标签 (全局变量) 的地址载入寄存器中, 比如
     * 要将 `a` 的地址载入 $t0, 只需要 `la.local $t0, a`
     */
    if (!m->get_global_variable().empty()) {
        append_inst("Global variables", ASMInstruction::Comment);
        /* 虽然下面两条伪指令可以简化为一条 `.bss` 伪指令, 但是我们还是选择使用
         * `.section` 将全局变量放到可执行文件的 BSS 段, 原因如下:
         * - 尽可能对齐交叉编译器 loongarch64-unknown-linux-gnu-gcc 的行为
         * - 支持更旧版本的 GNU 汇编器, 因为 `.bss` 伪指令是应该相对较新的指令,
         *   GNU 汇编器在 2023 年 2 月的 2.37 版本才将其引入
         */
        append_inst(".text", ASMInstruction::Atrribute);
        append_inst(".section", {".bss", "\"aw\"", "@nobits"},
                    ASMInstruction::Atrribute);
        for (auto &global : m->get_global_variable()) {
            auto size =
                global.get_type()->get_pointer_element_type()->get_size();
            append_inst(".globl", {global.get_name()},
                        ASMInstruction::Atrribute);
            append_inst(".type", {global.get_name(), "@object"},
                        ASMInstruction::Atrribute);
            append_inst(".size", {global.get_name(), std::to_string(size)},
                        ASMInstruction::Atrribute);
            append_inst(global.get_name(), ASMInstruction::Label);
            append_inst(".space", {std::to_string(size)},
                        ASMInstruction::Atrribute);
        }
    }

    // 函数代码段
    output.emplace_back(".text", ASMInstruction::Atrribute);
    for (auto &func : m->get_functions()) {
        if (not func.is_declaration()) {
            LOG(DEBUG) << "working on function " <<func.get_name();
            // 更新 context
            context.clear();
            context.func = &func;

            // 函数信息
            append_inst(".globl", {func.get_name()}, ASMInstruction::Atrribute);
            append_inst(".type", {func.get_name(), "@function"},
                        ASMInstruction::Atrribute);
            append_inst(func.get_name(), ASMInstruction::Label);

            // 分配函数栈帧
            allocate();
            // 生成 prologue
            gen_prologue();

            for (auto &bb : func.get_basic_blocks()) {
                append_inst(label_name(&bb), ASMInstruction::Label);
                for (auto &instr : bb.get_instructions()) {
                    // For debug
                    LOG(DEBUG) << "working on instruction " << instr.print();
                    append_inst(instr.print(), ASMInstruction::Comment);
                    context.inst = &instr; // 更新 context
                    switch (instr.get_instr_type()) {
                    case Instruction::ret:
                        gen_ret();
                        break;
                    case Instruction::br:
                        gen_br();
                        break;
                    case Instruction::add:
                    case Instruction::sub:
                    case Instruction::mul:
                    case Instruction::sdiv:
                        gen_binary();
                        break;
                    case Instruction::fadd:
                    case Instruction::fsub:
                    case Instruction::fmul:
                    case Instruction::fdiv:
                        gen_float_binary();
                        break;
                    case Instruction::alloca:
                        gen_alloca();
                        break;
                    case Instruction::load:
                        gen_load();
                        break;
                    case Instruction::store:
                        gen_store();
                        break;
                    case Instruction::ge:
                    case Instruction::gt:
                    case Instruction::le:
                    case Instruction::lt:
                    case Instruction::eq:
                    case Instruction::ne:
                        gen_icmp();
                        break;
                    case Instruction::fge:
                    case Instruction::fgt:
                    case Instruction::fle:
                    case Instruction::flt:
                    case Instruction::feq:
                    case Instruction::fne:
                        gen_fcmp();
                        break;
                    case Instruction::phi:
                        break;
                    case Instruction::call:
                        gen_call();
                        break;
                    case Instruction::getelementptr:
                        gen_gep();
                        break;
                    case Instruction::zext:
                        gen_zext();
                        break;
                    case Instruction::fptosi:
                        gen_fptosi();
                        break;
                    case Instruction::sitofp:
                        gen_sitofp();
                        break;
                    }
                }
            }
            // 生成 epilogue
            gen_epilogue();
        }
    }
}

std::string CodeGen::print() const {
    std::string result;
    for (const auto &inst : output) {
        result += inst.format();
    }
    return result;
}
