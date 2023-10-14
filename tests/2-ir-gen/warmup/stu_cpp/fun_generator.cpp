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

    // Create function callee
    std::vector<Type *> callee_args(1, int32_type);
    auto calleeFunTy = FunctionType::get(int32_type, callee_args);
    auto calleeFun = Function::create(calleeFunTy, "callee", module);
    
    // Create basic block ENTRY for callee
    auto callee_ENTRY = BasicBlock::create(module, "ENTRY", calleeFun);

    builder->set_insert_point(callee_ENTRY);
    // Get arguemnet value of callee
    std::vector <Value *> args;
    for(auto &arg: calleeFun->get_args()){
        args.push_back(&arg);
    }
    auto i0 = builder->create_imul(CONST_INT(2), args[0]);
    builder->create_ret(i0);

    // Create function main
    std::vector<Type *> main_args;
    auto mainFunTy = FunctionType::get(int32_type, main_args);
    auto mainFun = Function::create(mainFunTy, "main", module);

    // Create basic block ENTRY for main
    auto main_ENTRY = BasicBlock::create(module, "ENTRY", mainFun);

    builder->set_insert_point(main_ENTRY);
    auto i1 = builder->create_call(calleeFun, {CONST_INT(110)});
    builder->create_ret(i1);
    
    // Print result
    std::cout << module->print();
    delete module;
    return 0;
}
