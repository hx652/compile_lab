#include "Mem2Reg.hpp"
#include "IRBuilder.hpp"
#include "Value.hpp"

#include <memory>

#include "logging.hpp"

void Mem2Reg::run() {
    // 创建支配树分析 Pass 的实例
    dominators_ = std::make_unique<Dominators>(m_);
    // 建立支配树
    dominators_->run();
    // 以函数为单元遍历实现 Mem2Reg 算法
    for (auto &f : m_->get_functions()) {
        if (f.is_declaration())
            continue;
        func_ = &f;
        memory_varibles.clear();
        gloabl_name.clear();
        created_block.clear();
        defs.clear();
        stack.clear();
        memory_varible_of.clear();
        renamed.clear();
        if (func_->get_basic_blocks().size() >= 1) {
            LOG(INFO) << "working on function " << func_->get_name();
            // 对应伪代码中 phi 指令插入的阶段
            generate_phi();
            LOG(INFO) << "generate_phi() done";
            // 对应伪代码中重命名阶段
            LOG(INFO) << "size of memory variable: " << memory_varibles.size();
            for (AllocaInst *i : memory_varibles) {
                stack[i] = std::stack<Value *>();
            }
            for (BasicBlock &bb : func_->get_basic_blocks()) {
                renamed.insert({&bb, false});
            }
            rename(func_->get_entry_block());
            LOG(INFO) << "rename() done";
            remove_redundant_store();
            LOG(INFO) << "remove_redundant_store() done";
            LOG(INFO) << "mem2reg on function " << func_->get_name() << " done";
        }
        // 后续 DeadCode 将移除冗余的局部变量的分配空间
    }
}

void Mem2Reg::generate_phi() {
    // TODO
    // 步骤一：找到活跃在多个 block 的全局名字集合，以及它们所属的 bb 块
    // 步骤二：从支配树获取支配边界信息，并在对应位置插入 phi 指令
    for (BasicBlock &bb : func_->get_basic_blocks()) {
        for (auto &instr : bb.get_instructions()) {
            if (instr.is_alloca()) {
                AllocaInst *cur = static_cast<AllocaInst *>(&instr);
                if (!cur->get_alloca_type()->is_array_type()) {
                    created_block[cur] = &bb;
                    stack.insert({cur, std::stack<Value *>()});
                    memory_varibles.insert(cur);
                }
            }
            else if (instr.is_load()) {
                LoadInst *cur = static_cast<LoadInst *>(&instr);
                if (is_valid_ptr(cur->get_operand(0))) {
                    AllocaInst *ptr = static_cast<AllocaInst *>(cur->get_operand(0));
                    if (created_block.at(ptr) != &bb) {
                        gloabl_name.insert(ptr);
                    }
                }
            }
            else if (instr.is_store()) {
                StoreInst *cur = static_cast<StoreInst *>(&instr);
                if (is_valid_ptr(cur->get_operand(1))) {
                    AllocaInst *ptr = static_cast<AllocaInst *>(cur->get_operand(1));
                    defs[ptr].insert(&bb);
                    if (created_block.at(ptr) != &bb) {
                        gloabl_name.insert(ptr);
                    }
                }
            }
        }
    }
    
    std::string set_value = "{ ";
    for (auto *ptr : gloabl_name) {
        if (ptr->get_parent()->get_parent() == func_) {
            set_value += ptr->print() + ", ";
        }
    }
    set_value += "}";
    LOG(INFO) << "global_name " << set_value;
    
    for (AllocaInst *ptr : gloabl_name) {
        LOG(INFO) << "insert phi for " << ptr->print();
        if (ptr->get_parent()->get_parent() == func_) {
            std::set<BasicBlock *> F;
            std::set<BasicBlock *> W;
            F.clear();
            W.clear();
            
            for (BasicBlock *b : defs.at(ptr)) {
                W.insert(b);
            }
            
            while (!W.empty()) {
                BasicBlock *x = *W.begin();
                W.erase(x);
                for (BasicBlock *y : dominators_->get_dominance_frontier(x)) {
                    if (F.find(y) == F.end()) {
                        PhiInst *new_phi = PhiInst::create_phi(ptr->get_type()->get_pointer_element_type(), y);
                        memory_varible_of[new_phi] = ptr;
                        y->add_instr_begin(new_phi);
                        LOG(INFO) << "inserted phi in basicblock " << y->get_name();
                        F.insert(y);
                        if (defs.at(ptr).find(y) == defs.at(ptr).end()) {
                            W.insert(y);
                        }
                    }
                }
            }
        }
    }
}

void Mem2Reg::rename(BasicBlock *bb) {
    LOG(INFO) << "rename on basicblock " << bb->get_name();
    // TODO
    // 步骤三：将 phi 指令作为 lval 的最新定值，lval 即是为局部变量 alloca 出的地址空间
    // 步骤四：用 lval 最新的定值替代对应的load指令
    // 步骤五：将 store 指令的 rval，也即被存入内存的值，作为 lval 的最新定值
    // 步骤六：为 lval 对应的 phi 指令参数补充完整
    // 步骤七：对 bb 在支配树上的所有后继节点，递归执行 re_name 操作
    // 步骤八：pop出 lval 的最新定值
    // 步骤九：清除冗余的指令
    
    std::map<AllocaInst *, int> definition_num;
    for (auto *tmp : memory_varibles) {
        definition_num[tmp] = 0;
    }
    LOG(INFO) << "scanning basicblock " << bb->get_name(); 
    for (auto &instr : bb->get_instructions()) {
        if (instr.is_phi()) {
            PhiInst *cur = static_cast<PhiInst *>(&instr);
            AllocaInst *i = memory_varible_of.at(cur);
            LOG(INFO) << "find a phi of " << i->print();
            stack.at(i).push(cur);
            definition_num.at(i)++;
        }
        if (instr.is_store()) {
            StoreInst *cur = static_cast<StoreInst *>(&instr);
            if (is_valid_ptr(cur->get_lval())) {
                AllocaInst *ptr = static_cast<AllocaInst *>(cur->get_lval());
                LOG(INFO) << "find a store of " << ptr->print();
                stack.at(ptr).push(cur->get_rval());
                definition_num.at(ptr)++;
            }
        }
        if (instr.is_load()) {
            LoadInst *cur = static_cast<LoadInst *>(&instr);
            if (is_valid_ptr(cur->get_lval())) {
                AllocaInst *ptr = static_cast<AllocaInst *>(cur->get_lval());
                LOG(INFO) << "find a load of " << ptr->print();
                LOG(INFO) << "size of stack of that memory variable: " << stack.at(ptr).size();
                if (!stack.at(ptr).empty()) {
                    cur->replace_all_use_with(stack.at(ptr).top());
                }
            }
        }
    }
    LOG(INFO) << "scanning basicblock " << bb->get_name() << " done";
    LOG(INFO) << "we have " << stack.size() << " stacks";
    std::string size = " ";
    for (AllocaInst *tmp : memory_varibles) {
        size += std::to_string(stack.at(tmp).size()) + ", ";
    }
    LOG(INFO) << "their size are: " << size;
    for (AllocaInst *tmp : memory_varibles) {
        if (stack.at(tmp).empty()) {
            LOG(INFO) << "stack top and size of " << tmp->print() << " : " << "nullptr" << ", " << stack.at(tmp).size();
        }
        else {
            Instruction *top = static_cast<Instruction *>(stack.at(tmp).top());
            if (top->is_phi() && top->get_num_operand() == 0) {
                LOG(INFO) << "stack top and size of " << tmp->print() << " : " << "empty phi" << ", " << stack.at(tmp).size();
            }
            else {
                LOG(INFO) << "stack top and size of " << tmp->print() << " : " << stack.at(tmp).top()->print() << ", " << stack.at(tmp).size();
            }
        }
    }

    LOG(INFO) << "enter phi function in succ blocks";
    for (BasicBlock *s : bb->get_succ_basic_blocks()) {
        for (auto &instr : s->get_instructions()) {
            if (instr.is_phi()) {
                LOG(INFO) << "find phi in " << s->get_name();
                PhiInst *cur = static_cast<PhiInst *>(&instr);
                AllocaInst *i = memory_varible_of.at(cur);
                LOG(INFO) << "stack size of this phi's memory variable: " << stack.at(i).size();
                if (!stack.at(i).empty()) {
                    std::string value;
                    if (dynamic_cast<PhiInst *>(stack.at(i).top()) != nullptr) {
                        PhiInst *phi = dynamic_cast<PhiInst *>(stack.at(i).top());
                        if (phi->get_num_operand() == 0) {
                            value = "empty phi";
                        }
                    }
                    else {
                        value = stack.at(i).top()->print();
                    }
                    LOG(INFO) << "stack is not empty, we can enter " << "[" << value << ", " << bb->get_name() << "]";
                    cur->add_phi_pair_operand(stack.at(i).top(), bb);
                }
            }
        }
    }
    LOG(INFO) << "enter phi function in succ blocks done";
    
    for (BasicBlock *s : dominators_->get_dom_tree_succ_blocks(bb)) {
        if (!renamed[s]) {
            rename(s);
        }
    }
    
    LOG(INFO) << "restore stacks";
    for (AllocaInst *tmp : memory_varibles) {
        LOG(INFO) << "defintion num of " << tmp->print() << ": " << definition_num.at(tmp);
    }
    for (AllocaInst *i : memory_varibles) {
        for (int n = definition_num.at(i); n>0; n--) {
            stack.at(i).pop();
        }
    }
    LOG(INFO) << "restore stacks done";
    for (AllocaInst *tmp : memory_varibles) {
        if (stack.at(tmp).empty()) {
            LOG(INFO) << "stack top and size of " << tmp->print() << " : " << "nullptr" << ", " << stack.at(tmp).size();
        }
        else {
            LOG(INFO) << "stack top and size of " << tmp->print() << " : " << stack.at(tmp).top()->print() << ", " << stack.at(tmp).size();
        }
    }

    renamed[bb] = true;
    LOG(INFO) << "rename on basicblock " << bb->get_name() << " done";
}

void Mem2Reg::remove_redundant_store(){
    std::set<StoreInst *> redundant_store;
    
    LOG(INFO) << "scanning all instructions";
    for (BasicBlock &bb : func_->get_basic_blocks()) {
        for (Instruction &instr : bb.get_instructions()) {
            if (instr.is_store()) {
                StoreInst *cur = static_cast<StoreInst *>(&instr);
                if (is_valid_ptr(cur->get_lval())) {
                    LOG(INFO) << "in basicblock " << bb.get_name() << " find a redundant store";
                    redundant_store.insert(cur);
                }
            }
        }
    }
    LOG(INFO) << "scanning all instructions done";
    LOG(INFO) << "number of redundant_store: " << redundant_store.size();
    
    for (StoreInst *instr : redundant_store) {
        instr->remove_all_operands();
    }
    for (StoreInst *instr : redundant_store) {
        instr->get_parent()->erase_instr(instr);
    }
}
