#include "cminusf_builder.hpp"
#include "BasicBlock.hpp"
#include "Function.hpp"
#include "GlobalVariable.hpp"
#include "IRBuilder.hpp"
#include "Type.hpp"
#include "Value.hpp"
#include "ast.hpp"
#include <asm-generic/errno.h>
#include <cstddef>
#include <string>
#include <vector>
#include "logging.hpp"

#define CONST_FP(num) ConstantFP::get((float)num, module.get())
#define CONST_INT(num) ConstantInt::get(num, module.get())

// types
Type *VOID_T;
Type *INT1_T;
Type *INT32_T;
Type *INT32PTR_T;
Type *FLOAT_T;
Type *FLOATPTR_T;

/*
 * use CMinusfBuilder::Scope to construct scopes
 * scope.enter: enter a new scope
 * scope.exit: exit current scope
 * scope.push: add a new binding to current scope
 * scope.find: find and return the value bound to the name
 */

void print_type_value(Value *val,std::string name, std::string pre, std::string post){
    LOG(DEBUG) << pre << "type and value of " << name << " is " << "<" << val->get_type()->print() << "," << val->print()<< ">" << post;
}

Value* CminusfBuilder::visit(ASTProgram &node) {
    LOG(DEBUG) << "visit ASTProgram";
    VOID_T = module->get_void_type();
    INT1_T = module->get_int1_type();
    INT32_T = module->get_int32_type();
    INT32PTR_T = module->get_int32_ptr_type();
    FLOAT_T = module->get_float_type();
    FLOATPTR_T = module->get_float_ptr_type();

    Value *ret_val = nullptr;
    for (auto &decl : node.declarations) {
        ret_val = decl->accept(*this);
    }
    LOG(DEBUG) << "leave ASTProgram";
    return ret_val;
}

Value* CminusfBuilder::visit(ASTNum &node) {
    // TODO: This function is empty now.
    // Add some code here.
    LOG(DEBUG) << "visit ASTNum";
    if (node.type == TYPE_INT) {
        context.type = INT32_T;
        context.value = CONST_INT(node.i_val);
    }
    if (node.type == TYPE_FLOAT) {
        context.type = FLOAT_T;
        context.value = CONST_FP(node.f_val);
    }
    LOG(DEBUG) << "leave ASTNum";
    return nullptr;
}

Value* CminusfBuilder::visit(ASTVarDeclaration &node) {
    // TODO: This function is empty now.
    // Add some code here.
    LOG(DEBUG) << "visit ASTVarDeclaration for " << node.id;
    Type *basic_type;
    if (node.type == TYPE_INT) {
        basic_type = INT32_T;
    }
    if (node.type == TYPE_FLOAT) {
        basic_type = FLOAT_T;
    }

    if (scope.in_global()) {
        if (node.num == nullptr) {
            auto initializer = ConstantZero::get(basic_type, module.get());
            auto var = GlobalVariable::create(node.id, module.get(), basic_type, false, initializer);
            scope.push(node.id, var);
        }
        else {
            auto array_type = ArrayType::get(basic_type, node.num->i_val);
            auto initializer = ConstantZero::get(array_type, module.get());
            auto var = GlobalVariable::create(node.id, module.get(), array_type, false, initializer);
            scope.push(node.id, var);
        }
    }
    else {
        if (node.num == nullptr) {
            auto var = builder->create_alloca(basic_type);
            scope.push(node.id, var);
        }
        else {
            auto array_type = ArrayType::get(basic_type, node.num->i_val);
            auto var = builder->create_alloca(array_type);
            scope.push(node.id, var);
        }
    }
    LOG(DEBUG) << "leave ASTVarDeclaration for " << node.id;
    return nullptr;
}

Value* CminusfBuilder::visit(ASTFunDeclaration &node) {
    LOG(DEBUG) << "visit ASTFunDeclaration for " << node.id;
    FunctionType *fun_type;
    Type *ret_type;
    std::vector<Type *> param_types;
    if (node.type == TYPE_INT)
        ret_type = INT32_T;
    else if (node.type == TYPE_FLOAT)
        ret_type = FLOAT_T;
    else
        ret_type = VOID_T;

    for (auto &param : node.params) {
        // TODO: Please accomplish param_types.
        param->accept(*this);
        param_types.push_back(context.type);
    }

    fun_type = FunctionType::get(ret_type, param_types);
    auto func = Function::create(fun_type, node.id, module.get());
    scope.push(node.id, func);
    context.func = func;
    auto funBB = BasicBlock::create(module.get(), "entry", func);
    context.next_index = 2;
    builder->set_insert_point(funBB);
    scope.enter();
    std::vector<Value *> args;
    for (auto &arg : func->get_args()) {
        args.push_back(&arg);
    }
    for (int i = 0; i < node.params.size(); ++i) {
        // TODO: You need to deal with params and store them in the scope.
        node.params[i]->accept(*this);
        auto address = builder->create_alloca(context.type);
        builder->create_store(args[i], address);
        scope.push(context.id, address);
    }
    node.compound_stmt->accept(*this);
    if (not builder->get_insert_block()->is_terminated())
    {
        if (context.func->get_return_type()->is_void_type())
            builder->create_void_ret();
        else if (context.func->get_return_type()->is_float_type())
            builder->create_ret(CONST_FP(0.));
        else
            builder->create_ret(CONST_INT(0));
    }
    scope.exit();
    LOG(DEBUG) << "leave ASTFunDeclaration for " << node.id;
    return nullptr;
}

Value* CminusfBuilder::visit(ASTParam &node) {
    // TODO: This function is empty now.
    // Add some code here.
    LOG(DEBUG) << "visit ASTParam for " << node.id;
    if (node.isarray == false) {
        if (node.type == TYPE_INT) {
            context.type = INT32_T;
        }
        if (node.type == TYPE_FLOAT) {
            context.type = FLOAT_T;
        }
    }
    else {
        if (node.type == TYPE_INT) {
            context.type = INT32PTR_T;
        }
        if (node.type == TYPE_FLOAT) {
            context.type = FLOATPTR_T;
        }
    }
    context.id = node.id;
    LOG(DEBUG) << "leave ASTParam for " << node.id;
    return nullptr;
}

Value* CminusfBuilder::visit(ASTCompoundStmt &node) {
    // TODO: This function is not complete.
    // You may need to add some code here
    // to deal with complex statements.

    LOG(DEBUG) << "visit ASTCompoundStmt";
    scope.enter();
    for (auto &decl : node.local_declarations) {
        decl->accept(*this);
    }

    for (auto &stmt : node.statement_list) {
        stmt->accept(*this);
        if (builder->get_insert_block()->is_terminated())
            break;
    }
    scope.exit();
    LOG(DEBUG) << "leave ASTCompoundStmt";
    return nullptr;
}

Value* CminusfBuilder::visit(ASTExpressionStmt &node) {
    // TODO: This function is empty now.
    // Add some code here.
    LOG(DEBUG) << "visit ASTExpressionStmt";
    if (node.expression != nullptr) {
        node.expression->accept(*this);
    }
    LOG(DEBUG) << "leave ASTExpressionStmt";
    return nullptr;
}

Value* CminusfBuilder::visit(ASTSelectionStmt &node) {
    // TODO: This function is empty now.
    // Add some code here.
    LOG(DEBUG) << "visit ASTSelectionStmt";
    context.l_value = false;
    node.expression->accept(*this);
    auto val = context.value;
    LOG(DEBUG) << "type and value of val is " << "<" << val->get_type()->print() << "," << val->print()<< ">";
    
    // Casting int1 to int32
    if (val->get_type()->is_int1_type()) {
        LOG(DEBUG) << "cast int1 to int32";
        val = builder->create_zext(val, INT32_T);
        LOG(DEBUG) << "after casting, type and value of val is " << "<" << val->get_type()->print() << "," << val->print()<< ">";
    }

    Value *cond;
    if (val->get_type()->is_integer_type()) {
        cond = builder->create_icmp_ne(val, CONST_INT(0));
    }
    else {
        cond = builder->create_fcmp_ne(val, CONST_FP(0.0));
    }
    
    auto IF_TRUE = BasicBlock::create(module.get(), std::to_string(context.next_index), context.func);
    context.next_index++;
    auto IF_FALSE = BasicBlock::create(module.get(), std::to_string(context.next_index), context.func);
    context.next_index++;
    auto COUNTINUE = BasicBlock::create(module.get(), std::to_string(context.next_index), context.func);
    context.next_index++;
    
    if (node.else_statement == nullptr) {
        builder->create_cond_br(cond, IF_TRUE, COUNTINUE);
    }
    else {
        builder->create_cond_br(cond, IF_TRUE, IF_FALSE);
    }
    
    builder->set_insert_point(IF_TRUE);
    node.if_statement->accept(*this);
    
    if (!builder->get_insert_block()->is_terminated()) {
        builder->create_br(COUNTINUE);
    }
    
    if (node.else_statement == nullptr) {
        IF_FALSE->erase_from_parent();
    }
    else {
        builder->set_insert_point(IF_FALSE);
        node.else_statement->accept(*this);
        if (!builder->get_insert_block()->is_terminated()) {
            builder->create_br(COUNTINUE);
        }
    }
    
    builder->set_insert_point(COUNTINUE);
    LOG(DEBUG) << "leave ASTSelectionStmt";

    return nullptr;
}

Value* CminusfBuilder::visit(ASTIterationStmt &node) {
    // TODO: This function is empty now.
    // Add some code here.
    LOG(DEBUG) << "visit ASTIterationStmt";
    auto LOOP = BasicBlock::create(module.get(), std::to_string(context.next_index), context.func);
    context.next_index++;
    if (!builder->get_insert_block()->is_terminated()) {
        builder->create_br(LOOP);
    }
    
    builder->set_insert_point(LOOP);
    
    context.l_value = false;
    node.expression->accept(*this);
    auto val = context.value;
    print_type_value(val, "val", "", "");
    
    // Type casting
    if (val->get_type()->is_int1_type()) {
        val = builder->create_zext(val, INT32_T);
        print_type_value(val, "val", "after casting, ", "");
    }
    if (val->get_type()->is_float_type()) {
        val = builder->create_fptosi(val, INT32_T);
        print_type_value(val, "val", "after casting, ", "");
    }
    
    if (val->get_type()->is_float_type()) {
        val = builder->create_fptosi(val, INT32_T);
        print_type_value(val, "val", "after casting, ", "");
    }

    Value *cond;
    auto IF_TRUE = BasicBlock::create(module.get(), std::to_string(context.next_index), context.func);
    context.next_index++;
    auto COUNTINUE = BasicBlock::create(module.get(), std::to_string(context.next_index), context.func);
    context.next_index++;
    if (val->get_type()->is_integer_type()) {
        cond = builder->create_icmp_ne(val, CONST_INT(0));
    }
    else {
        cond = builder->create_fcmp_ne(val, CONST_FP(0.0));
    }
    builder->create_cond_br(cond, IF_TRUE, COUNTINUE);
    
    builder->set_insert_point(IF_TRUE);
    node.statement->accept(*this);
    if (!builder->get_insert_block()->is_terminated()) {
        builder->create_br(LOOP);
    }

    builder->set_insert_point(COUNTINUE);
    LOG(DEBUG) << "leave ASTIterationStmt";
    return nullptr;
}

Value* CminusfBuilder::visit(ASTReturnStmt &node) {
    LOG(DEBUG) << "visit ASTReturnStmt";
    if (node.expression == nullptr) {
        builder->create_void_ret();
        return nullptr;
    } else {
        // TODO: The given code is incomplete.
        // You need to solve other return cases (e.g. return an integer).
        context.l_value = false;
        node.expression->accept(*this);
        auto ret_val = context.value;
        LOG(DEBUG) << "type and value of ret_val is " << "<" << ret_val->get_type()->print() << "," << ret_val->print()<< ">";
        auto ret_type = builder->get_insert_block()->get_parent()->get_return_type();
        LOG(DEBUG) << "ret_type is " << ret_type->print();
        if (ret_type != ret_val->get_type()) {
            if (ret_type == INT32_T) {
                ret_val = builder->create_fptosi(ret_val, INT32_T);
            }
            else {
                ret_val = builder->create_sitofp(ret_val, INT32_T);
            }
        }

        builder->create_ret(ret_val);
    }
    LOG(DEBUG) << "leave ASTReturnStmt";
    return nullptr;
}

Value* CminusfBuilder::visit(ASTVar &node) {
    // TODO: This function is empty now.
    // Add some code here.
    LOG(DEBUG) << "visit ASTVar for " << node.id;
    auto var = scope.find(node.id);
    LOG(DEBUG) << "type and value of var "<< "<" << var->get_type()->print() << "," << var->print() << ">";

    if (node.expression == nullptr) {
        if (context.l_value) {
            context.value = var;
        }
        else {
            if (var->get_type()->get_pointer_element_type()->is_array_type()) {
                context.value = builder->create_gep(var, {CONST_INT(0), CONST_INT(0)});
            }
            else {
                context.value = builder->create_load(var);
            }
        }
    }
    else {
        bool pre_l_val = context.l_value;
        context.l_value = false;
        node.expression->accept(*this);
        auto offset = context.value;
        context.l_value = pre_l_val;

        LOG(DEBUG) << "type and value of offset "<< "<" << offset->get_type()->print() << "," << offset->print() << ">";
        // Convert the result to int32 type
        if (offset->get_type()->is_float_type()) {
            LOG(DEBUG) << "convert offset from float to int";
            offset = builder->create_fptosi(offset, INT32_T);
        }
        
        // Check if the index is negtive
        auto trueBB = BasicBlock::create(module.get(), std::to_string(context.next_index), context.func);
        context.next_index++;
        auto falseBB = BasicBlock::create(module.get(), std::to_string(context.next_index), context.func);
        context.next_index++;

        auto cond = builder->create_icmp_ge(offset, CONST_INT(0));

        builder->create_cond_br(cond, trueBB, falseBB);
        
        builder->set_insert_point(falseBB);
        auto fun = static_cast<Function *>(scope.find("neg_idx_except"));
        builder->create_call(fun, {});
        builder->create_br(trueBB);

        builder->set_insert_point(trueBB);
        Value *address;
        print_type_value(var, "var", "", "");
        if (var->get_type()->get_pointer_element_type()->is_integer_type() || var->get_type()->get_pointer_element_type()->is_float_type()) {
            LOG(DEBUG) << "case 1";
            address = builder->create_gep(var, {offset});
        }
        else if (var->get_type()->get_pointer_element_type()->is_pointer_type()) {
            LOG(DEBUG) << "case 2";
            var = builder->create_load(var);
            address = builder->create_gep(var, {offset});
        }
        else {
            LOG(DEBUG) << "case 3";
            address = builder->create_gep(var, {CONST_INT(0), offset});
        }
        
        if (context.l_value) {
            context.value = address;
        }
        else {
            context.value = builder->create_load(address);
        }
    }
    LOG(DEBUG) << "leave ASTvar for " << node.id;
    return nullptr;
}

Value* CminusfBuilder::visit(ASTAssignExpression &node) {
    // TODO: This function is empty now.
    // Add some code here.
    LOG(DEBUG) << "visit ASTAssignExpression";
    context.l_value = false;
    node.expression->accept(*this);
    auto rhs = context.value;
    LOG(DEBUG) << "type and value of rhs is " << "<" << rhs->get_type()->print() << "," << rhs->print() << ">";

    context.l_value = true;
    node.var->accept(*this);
    auto address = context.value;
    LOG(DEBUG) << "address points to type " << address->get_type()->get_pointer_element_type()->print();

    // Type casting
    if (address->get_type()->get_pointer_element_type() != rhs->get_type()) {
        if (address->get_type()->get_pointer_element_type() == INT32_T) {
            if (rhs->get_type() == FLOAT_T) {
                rhs = builder->create_fptosi(rhs, INT32_T);
            }
            else if (rhs->get_type() == INT1_T) {
                rhs = builder->create_zext(rhs, INT32_T);
            }
        }
        else if (address->get_type()->get_pointer_element_type() == FLOAT_T) {
            rhs = builder->create_sitofp(rhs, FLOAT_T);
        }
        else if (address->get_type()->get_pointer_element_type() == INT1_T) {
            rhs = builder->create_fptosi(rhs, INT1_T);
        }
    }

    builder->create_store(rhs, address);
    context.value = rhs;
    LOG(DEBUG) << "leave ASTAssignExpression";
    return nullptr;
}

Value* CminusfBuilder::visit(ASTSimpleExpression &node) {
    // TODO: This function is empty now.
    // Add some code here.
    LOG(DEBUG) << "visit ASTSimpleExpression";
    if (node.additive_expression_r == nullptr) {
        node.additive_expression_l->accept(*this);
    }
    else {
        context.l_value = false;
        node.additive_expression_l->accept(*this);
        auto lhs = context.value;
        context.l_value = false;
        node.additive_expression_r->accept(*this);
        auto rhs = context.value;
        
        LOG(DEBUG) << "finish evaluation of lhs and rhs";
        LOG(DEBUG) << "type and value of lhs " << "<" << lhs->get_type()->print() << "," << lhs->print() << ">";
        LOG(DEBUG) << "type and value of rhs " << "<" << rhs->get_type()->print() << "," << rhs->print() << ">";
        
        bool is_int;
        if (lhs->get_type()->is_integer_type() && rhs->get_type()->is_integer_type()) {
            if (lhs->get_type()->is_int1_type()) {
                lhs = builder->create_zext(lhs, INT32_T);
            }
            if (rhs->get_type()->is_int1_type()) {
                rhs = builder->create_zext(rhs, INT32_T);
            }
            is_int = true;
        }
        else if (lhs->get_type()->is_integer_type() && rhs->get_type()->is_float_type()) {
            is_int = false;
            lhs = builder->create_sitofp(lhs, FLOAT_T);
        }
        else if (lhs->get_type()->is_float_type() && rhs->get_type()->is_integer_type()) {
            is_int = false;
            rhs = builder->create_sitofp(rhs, FLOAT_T);
        }
        else {
            is_int = false;
        }
        LOG(DEBUG) << "is_int " << is_int;
        
        Value *result;
        if (is_int) {
            switch (node.op) {
                case OP_LE:
                    result = builder->create_icmp_le(lhs, rhs);
                    break;
                case OP_LT:
                    result = builder->create_icmp_lt(lhs, rhs);
                    break;
                case OP_GT:
                    result = builder->create_icmp_gt(lhs, rhs);
                    break;
                case OP_GE:
                    result = builder->create_icmp_ge(lhs, rhs);
                    break;
                case OP_EQ:
                    result = builder->create_icmp_eq(lhs, rhs);
                    break;
                case OP_NEQ:
                    result = builder->create_icmp_ne(lhs, rhs);
                    break;
            }
        }
        else {
            switch (node.op) {
                case OP_LE:
                    result = builder->create_fcmp_le(lhs, rhs);
                    break;
                case OP_LT:
                    result = builder->create_fcmp_lt(lhs, rhs);
                    break;
                case OP_GT:
                    result = builder->create_fcmp_gt(lhs, rhs);
                    break;
                case OP_GE:
                    result = builder->create_fcmp_ge(lhs, rhs);
                    break;
                case OP_EQ:
                    result = builder->create_fcmp_eq(lhs, rhs);
                    break;
                case OP_NEQ:
                    result = builder->create_fcmp_ne(lhs, rhs);
                    break;
            }
            
        }
        
        context.value = result;
        context.type = INT1_T;
    }
    
    LOG(DEBUG) << "leave ASTSimpleExpression";
    return nullptr;
}

Value* CminusfBuilder::visit(ASTAdditiveExpression &node) {
    // TODO: This function is empty now.
    // Add some code here.
    LOG(DEBUG) << "visit ASTAdditiveExpression";
    if (node.additive_expression == nullptr) {
        node.term->accept(*this);
    }
    else {
        node.additive_expression->accept(*this);
        auto lhs = context.value;
        node.term->accept(*this);
        auto rhs = context.value;
        
        bool is_int;
        if (lhs->get_type()->is_integer_type() && rhs->get_type()->is_integer_type()) {
            if (lhs->get_type()->is_int1_type()) {
                lhs = builder->create_zext(lhs, INT32_T);
            }
            if (rhs->get_type()->is_int1_type()) {
                rhs = builder->create_zext(rhs, INT32_T);
            }
            is_int = true;
        }
        else if (lhs->get_type()->is_integer_type() && rhs->get_type()->is_float_type()) {
            is_int = false;
            lhs = builder->create_sitofp(lhs, FLOAT_T);
        }
        else if (lhs->get_type()->is_float_type() && rhs->get_type()->is_integer_type()) {
            is_int = false;
            rhs = builder->create_sitofp(rhs, FLOAT_T);
        }
        else {
            is_int = false;
        }
        LOG(DEBUG) << "is_int " << is_int;

        Value *result;
        switch (node.op) {
            case OP_PLUS:
                if (is_int) {
                    result = builder->create_iadd(lhs, rhs);
                }
                else {
                    result = builder->create_fadd(lhs, rhs);
                }
                break;
            case OP_MINUS:
                if (is_int) {
                    result = builder->create_isub(lhs, rhs);
                }
                else {
                    result = builder->create_fsub(lhs, rhs);
                }
                break;
        }

        context.value = result;
        if (is_int) {
            context.type = INT32_T;
        }
        else {
            context.type = FLOAT_T;
        }
    }
    LOG(DEBUG) << "leave ASTAdditiveExpression";
    return nullptr;
}

Value* CminusfBuilder::visit(ASTTerm &node) {
    // TODO: This function is empty now.
    // Add some code here.
    LOG(DEBUG) << "visit ASTTerm";
    if (node.term == nullptr) {
        node.factor->accept(*this);
    }
    else {
        context.l_value = false;
        node.term->accept(*this);
        auto lhs = context.value;
        context.l_value = false;
        node.factor->accept(*this);
        auto rhs = context.value;

        bool is_int;
        if (lhs->get_type() == INT32_T && rhs->get_type() == INT32_T) {
            is_int = true;
        }
        else if (lhs->get_type() == INT32_T && rhs->get_type() == FLOAT_T) {
            is_int = false;
            lhs = builder->create_sitofp(lhs, FLOAT_T);
        }
        else if (lhs->get_type() == FLOAT_T && rhs->get_type() == INT32_T) {
            is_int = false;
            rhs = builder->create_sitofp(rhs, FLOAT_T);
        }
        else {
            is_int = false;
        }

        Value *result;
        switch (node.op) {
            case OP_MUL:
                if (is_int) {
                    result = builder->create_imul(lhs, rhs);
                }
                else {
                    result = builder->create_fmul(lhs, rhs);
                }
                break;
            case OP_DIV:
                if (is_int) {
                    result = builder->create_isdiv(lhs, rhs);
                }
                else {
                    result = builder->create_fdiv(lhs, rhs);
                }
                break;
        }

        context.value = result;
        if (is_int) {
            context.type = INT32_T;
        }
        else {
            context.type = FLOAT_T;
        }
    }
    LOG(DEBUG) << "leave ASTTerm";
    return nullptr;
}

Value* CminusfBuilder::visit(ASTCall &node) {
    // TODO: This function is empty now.
    // Add some code here.
    LOG(DEBUG) << "visit ASTCall for func " << node.id;
    auto fun = static_cast<Function *>(scope.find(node.id));
    auto param_type = fun->get_function_type()->param_begin();
    // Get values of arguements
    std::vector<Value *> vals;
    for (auto& arg : node.args) {
        context.l_value = false;
        arg->accept(*this);
        auto val = context.value;
        
        print_type_value(val, "val", "", "");
        // Type casting
        if (*param_type != val->get_type() && !val->get_type()->is_pointer_type()) {
            if (*param_type == INT32_T) {
                if (val->get_type()->is_int1_type()) {
                    val = builder->create_zext(val, INT32_T);
                }
                else {
                    val = builder->create_fptosi(val, INT32_T);
                }
            }
            if (*param_type == FLOAT_T) {
                val = builder->create_sitofp(val, FLOAT_T);
            }
        }
        
        vals.push_back(val);
        param_type++;
    }
    
    auto ret_val = builder->create_call(fun, vals);
    context.value = ret_val;
    LOG(DEBUG) << "leave ASTCall for func " << node.id;
    return nullptr;
}
