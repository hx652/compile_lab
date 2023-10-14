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
    Type *array_type = module->get_array_type(int32_type, 10);
    
    // Create function main
    std::vector<Type *> main_args;
    auto mainFunTy = FunctionType::get(int32_type, main_args);
    auto mainFun = Function::create(mainFunTy, "main", module);

    // Create basic block ENTRY for main
    auto main_ENTRY = BasicBlock::create(module, "ENTRY", mainFun);

    builder->set_insert_point(main_ENTRY);
    auto i0 = builder->create_alloca(array_type);
    auto i1 = builder->create_gep(i0, {CONST_INT(0), CONST_INT(0)});
    builder->create_store(CONST_INT(10), i1);
    auto i2 = builder->create_load(i1);
    auto i3 = builder->create_imul(i2, CONST_INT(2));
    auto i4 = builder->create_gep(i0, {CONST_INT(0), CONST_INT(1)});
    builder->create_store(i3, i4);
    auto i5 = builder->create_load(i4);
    builder->create_ret(i5);

    // Print result
    std::cout << module->print();
    delete module;
    return 0;
}
