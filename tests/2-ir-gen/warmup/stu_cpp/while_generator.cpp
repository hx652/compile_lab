#include "BasicBlock.hpp"
#include "Constant.hpp"
#include "Function.hpp"
#include "IRBuilder.hpp"
#include "Module.hpp"
#include "Type.hpp"

#include <iostream>
#include <memory>

// 定义一个从常数值获取/创建 ConstantInt 类实例化的宏，方便多次调用
#define CONST_INT(num) \
    ConstantInt::get(num, module)

int main() {
    // Create module
    auto module = new Module();

    auto builder = new IRBuilder(nullptr, module);

    Type *int32_type = module->get_int32_type();

    // Create function main
    std::vector<Type *> main_args;
    auto mainFunTy = FunctionType::get(int32_type, main_args);
    auto mainFun = Function::create(mainFunTy, "main", module);

    // Create basic block ENTRY for main
    auto main_ENTRY = BasicBlock::create(module, "ENTRY", mainFun);
    // Create basic block LOOP for main
    auto main_LOOP = BasicBlock::create(module, "LOOP", mainFun);
    // Create basic block INNER for main
    auto main_INNER = BasicBlock::create(module, "INNER", mainFun);
    // Create basic block OUT for main
    auto main_OUT = BasicBlock::create(module, "OUT", mainFun);
    
    builder->set_insert_point(main_ENTRY);
    auto i0 = builder->create_alloca(int32_type);
    auto i1 = builder->create_alloca(int32_type);
    builder->create_store(CONST_INT(10), i0);
    builder->create_store(CONST_INT(0), i1);
    builder->create_br(main_LOOP);

    builder->set_insert_point(main_LOOP);
    auto i2 = builder->create_load(i1);
    auto i3 = builder->create_icmp_lt(i2, CONST_INT(10));
    builder->create_cond_br(i3, main_INNER, main_OUT);
    
    builder->set_insert_point(main_INNER);
    auto i4 = builder->create_load(i1);
    auto i5 = builder->create_iadd(i4, CONST_INT(1));
    builder->create_store(i5, i1);
    auto i6 = builder->create_load(i0);
    auto i7 = builder->create_load(i1);
    auto i8 = builder->create_iadd(i6, i7);
    builder->create_store(i8, i0);
    builder->create_br(main_LOOP);

    builder->set_insert_point(main_OUT);
    auto i9 = builder->create_load(i0);
    builder->create_ret(i9);
    
    // Print result
    std::cout << module->print();
    delete module;
    return 0;
}
