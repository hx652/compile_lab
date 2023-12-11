#pragma once

#include "Dominators.hpp"
#include "Instruction.hpp"
#include "Value.hpp"

#include <map>
#include <memory>
#include <stack>

class Mem2Reg : public Pass {
  private:
    Function *func_;
    std::unique_ptr<Dominators> dominators_;

    // TODO 添加需要的变量
    // all varialbes below will be maintained funciton wide, not module wide
    // store all memory variables
    std::set<AllocaInst *> memory_varibles;
    // store memory variables active in multiple basicblocks
    std::set<AllocaInst *> gloabl_name;
    // store the block in which a memory variable is created
    std::map<AllocaInst *, BasicBlock *> created_block{};
    // store all blocks in which a memory variable is defined
    std::map<AllocaInst *, std::set<BasicBlock *>> defs{};
    // create a stack for each memory variable, to store its definitons(i.e. store and phi to it)
    std::map<AllocaInst *, std::stack<Value *>> stack{};
    // store the relation between phi function and memory variable
    std::map<PhiInst *, AllocaInst *> memory_varible_of{};
    // whether a basicblock has been renamed
    std::map<BasicBlock *, bool> renamed{};

  public:
    Mem2Reg(Module *m) : Pass(m) {}
    ~Mem2Reg() = default;

    void run() override;

    void generate_phi();
    void rename(BasicBlock *bb);
    void remove_redundant_store();

    static inline bool is_global_variable(Value *l_val) {
        return dynamic_cast<GlobalVariable *>(l_val) != nullptr;
    }
    static inline bool is_gep_instr(Value *l_val) {
        return dynamic_cast<GetElementPtrInst *>(l_val) != nullptr;
    }

    static inline bool is_valid_ptr(Value *l_val) {
        return not is_global_variable(l_val) and not is_gep_instr(l_val);
    }
};
