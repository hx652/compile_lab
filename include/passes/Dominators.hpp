#pragma once

#include "BasicBlock.hpp"
#include "PassManager.hpp"

#include <map>
#include <set>

class Dominators : public Pass {
  public:
    using BBSet = std::set<BasicBlock *>;

    explicit Dominators(Module *m) : Pass(m) {}
    ~Dominators() = default;
    void run() override;

    BasicBlock *get_idom(BasicBlock *bb) { return idom_.at(bb); }
    const BBSet &get_dominance_frontier(BasicBlock *bb) {
        return dom_frontier_.at(bb);
    }
    const BBSet &get_dom_tree_succ_blocks(BasicBlock *bb) {
        return dom_tree_succ_blocks_.at(bb);
    }

  private:
    void create_idom(Function *f);
    void create_dominance_frontier(Function *f);
    void create_dom_tree_succ(Function *f);

    // TODO 补充需要的函数
    BasicBlock *intersect(BasicBlock *b1, BasicBlock *b2);
    void postorder_traverse(Function *f);
    void postorder_index(BasicBlock *start);

    // variables below will be maintained module wide, not function wide
    std::map<BasicBlock *, BasicBlock *> idom_{};  // 直接支配
    std::map<BasicBlock *, BBSet> dom_frontier_{}; // 支配边界集合
    std::map<BasicBlock *, BBSet> dom_tree_succ_blocks_{}; // 支配树中的后继节点

    // variables below will be maintained function wide, not module wide
    // index each BasicBlock based on postorder, starting from 0
    std::map<int, BasicBlock *> index_to_BB{};
    std::map<BasicBlock *, int> BB_to_index{};
    // the number of nodes that we finished visited
    int next_index;
    // whether a basicblock waits for finish of recursive calls
    std::map<BasicBlock *, bool> is_waiting{};
};
