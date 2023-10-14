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

// 定义一个从常数值获取/创建 ConstantFP 类实例化的宏，方便多次调用
#define CONST_FP(num) \
    ConstantFP::get(num, module)

int main() {
    // Create module
    auto module = new Module();

    auto builder = new IRBuilder(nullptr, module);

    Type *int32_type = module->get_int32_type();
    Type *float_type = module->get_float_type();

    // Create function main
    std::vector<Type *> main_args;
    auto mainFunTy = FunctionType::get(int32_type, main_args);
    auto mainFun = Function::create(mainFunTy, "main", module);

    // Create basic block ENTRY for main
    auto main_ENTRY = BasicBlock::create(module, "ENTRY", mainFun);
    // Create basic block IFTRUE for main
    auto main_IFTRUE = BasicBlock::create(module, "IFTRUE", mainFun);
    // Create basic block IFFALSE for main
    auto main_IFFALSE = BasicBlock::create(module, "IFFALSE", mainFun);
    
    builder->set_insert_point(main_ENTRY);
    auto i0 = builder->create_alloca(float_type);
    builder->create_store(CONST_FP(5.555), i0);
    auto i1 = builder->create_sitofp(CONST_INT(1), float_type);
    auto i2 = builder->create_load(i0);
    auto i3 = builder->create_fcmp_gt(i2, i1);
    builder->create_cond_br(i3, main_IFTRUE, main_IFFALSE);

    builder->set_insert_point(main_IFTRUE);
    builder->create_ret(CONST_INT(233));
    
    builder->set_insert_point(main_IFFALSE);
    builder->create_ret(CONST_INT(0));
    
    // Print result
    std::cout << module->print();
    delete module;
    return 0;
}
