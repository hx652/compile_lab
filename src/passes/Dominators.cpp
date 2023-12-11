#include "Dominators.hpp"
#include "logging.hpp"
#include "IRprinter.hpp"

void Dominators::run() {
    for (auto &f1 : m_->get_functions()) {
        auto f = &f1;
        if (f->get_basic_blocks().size() == 0)
            continue;
        for (auto &bb1 : f->get_basic_blocks()) {
            auto bb = &bb1;
            idom_.insert({bb, {}});
            dom_frontier_.insert({bb, {}});
            dom_tree_succ_blocks_.insert({bb, {}});
        }

        LOG(INFO) << "run dominators in function " << f->get_name();
        postorder_traverse(f);
        create_idom(f);
        create_dominance_frontier(f);
        create_dom_tree_succ(f);
    }
}

void Dominators::create_idom(Function *f) {
    // TODO 分析得到 f 中各个基本块的 idom
    BasicBlock *start = f->get_entry_block();
    idom_[start] = start;

    bool changed = true;
    while (changed) {
        changed = false;
        for (int i = next_index - 2; i >= 0; --i) {
            BasicBlock *b = index_to_BB.at(i);
            BasicBlock *first_processed;
            for (BasicBlock *pre : b->get_pre_basic_blocks()) {
                if (idom_.at(pre) != nullptr) {
                    first_processed = pre;
                    break;
                }
            }
            BasicBlock *new_idom = first_processed;
            for (BasicBlock *p : b->get_pre_basic_blocks()) {
                if (p != first_processed && idom_.at(p) != nullptr) {
                    new_idom = intersect(p, new_idom);
                }
            }

            if (idom_.at(b) != new_idom) {
                idom_[b] = new_idom;
                changed = true;
            }
        }
    }

    for (auto &tmp : idom_) {
        if (tmp.first->get_parent() == f) {
            if (tmp.second == nullptr) {
                LOG(INFO) << "<bb, idom>: <" << tmp.first->get_name() << ", " << "nullptr" << ">";
            }
            else {
                LOG(INFO) << "<bb, idom>: <" << tmp.first->get_name() << ", " << tmp.second->get_name() << ">";
            }
        }
    }
}

void Dominators::create_dominance_frontier(Function *f) {
    // TODO 分析得到 f 中各个基本块的支配边界集合
    for (BasicBlock &b : f->get_basic_blocks()) {
        if (b.get_pre_basic_blocks().size() >= 2) {
            for (BasicBlock *p : b.get_pre_basic_blocks()) {
                BasicBlock *runner = p;
                while (runner != idom_.at(&b)) {
                   dom_frontier_.at(runner).insert(&b);
                   runner = idom_.at(runner);
                }
            }
        }
    }
    
    for (auto &tmp : dom_frontier_) {
        if (tmp.first->get_parent() == f) {
            std::string set_value;
            set_value += "{ ";
            for (BasicBlock *b : tmp.second) {
                set_value += b->get_name() + ", ";
            }
            set_value += "}";
            
            LOG(INFO) << "<bb, dom_frontier>: <" << tmp.first->get_name() << ", " << set_value << ">";
        }
    }
}

void Dominators::create_dom_tree_succ(Function *f) {
    // TODO 分析得到 f 中各个基本块的支配树后继
    for (auto &tmp : index_to_BB) {
        BasicBlock *b = tmp.second;
        BasicBlock *idom = idom_.at(b);
        for (BasicBlock *element : dom_tree_succ_blocks_.at(b)) {
            dom_tree_succ_blocks_.at(idom).insert(element);
        }
        if (b != f->get_entry_block()) {
            dom_tree_succ_blocks_.at(idom).insert(b);
        }
    }
    
    for (auto &tmp : dom_tree_succ_blocks_) {
        if (tmp.first->get_parent() == f) {
            std::string set_value;
            set_value += "{ ";
            for (BasicBlock *b : tmp.second) {
                set_value += b->get_name() + ", ";
            }
            set_value += "}";
            
            LOG(INFO) << "<bb, dom_tree_succ_blocks>: <" << tmp.first->get_name() << ", " << set_value << ">";
        }
    }
}

BasicBlock *Dominators::intersect(BasicBlock *b1, BasicBlock *b2) {
    BasicBlock *finger1 = b1; BasicBlock *finger2 = b2;

    while (BB_to_index.at(finger1) != BB_to_index.at(finger2)) {
        while (BB_to_index.at(finger1) < BB_to_index.at(finger2)) {
            finger1 = idom_.at(finger1);
        }
        while (BB_to_index.at(finger2) < BB_to_index.at(finger1)) {
            finger2 = idom_.at(finger2);
        }
    }
    return finger1;
}

void Dominators::postorder_traverse(Function *f){
    index_to_BB.clear();
    BB_to_index.clear();
    is_waiting.clear();
    next_index = 0;
    
    for (BasicBlock &bb : f->get_basic_blocks()) {
        BB_to_index.insert({&bb, -1});
        is_waiting.insert({&bb, false});
    }
    LOG(INFO) << "initialized BB_to_index";
    
    postorder_index(f->get_entry_block());
    LOG(INFO) << "postorder_index done, next_index: " << next_index;

    for (auto &tmp : index_to_BB) {
        LOG(INFO) << "<postorder, bb>: <" << tmp.first << ", " << tmp.second->get_name() << ">";
    }
}

void Dominators::postorder_index(BasicBlock *start) {
    LOG(INFO) << "postorder_index in basicblock " << start->get_name();
    is_waiting[start] = true;
    for (BasicBlock *succ : start->get_succ_basic_blocks()) {
        if (BB_to_index.at(succ) == -1 && !is_waiting.at(succ)) {
            postorder_index(succ);
        }
    }

    if (BB_to_index.at(start) == -1) {
        LOG(INFO) << "enter <" << start->get_name() << ", " << next_index << "> to BB_to_index";
        BB_to_index[start] = next_index;
        index_to_BB[next_index] = start;
        next_index++;
    }   
    LOG(INFO) << "postorder_index in basicblock " << start->get_name() << " done";
}