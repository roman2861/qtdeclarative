#include "qv4isel_moth_p.h"
#include "qv4vme_moth_p.h"

using namespace QQmlJS;
using namespace QQmlJS::Moth;

InstructionSelection::InstructionSelection(VM::ExecutionEngine *engine, IR::Module * /*module*/,
                                           uchar *code)
: _engine(engine), _code(code), _ccode(code)
{
}

InstructionSelection::~InstructionSelection()
{
}

void InstructionSelection::operator()(IR::Function *function)
{
    qSwap(_function, function);

    _function->code = VME::exec;
    _function->codeData = _ccode;

    int locals = _function->tempCount - _function->locals.size() + _function->maxNumberOfArguments;
    assert(locals >= 0);

    Instruction::Push push;
    push.value = quint32(locals);
    addInstruction(push);

    foreach (_block, _function->basicBlocks) {
        _addrs.insert(_block, _ccode - _code);

        foreach (IR::Stmt *s, _block->statements)
            s->accept(this);
    }

    for (QHash<IR::BasicBlock *, QVector<ptrdiff_t> >::ConstIterator iter = _patches.begin();
         iter != _patches.end(); ++iter) {

        Q_ASSERT(_addrs.contains(iter.key()));
        ptrdiff_t target = _addrs.value(iter.key());

        const QVector<ptrdiff_t> &patchList = iter.value();
        for (int ii = 0; ii < patchList.count(); ++ii) {
            ptrdiff_t patch = patchList.at(ii);

            *((ptrdiff_t *)(_code + patch)) = target - patch;
        }
    }

    qSwap(_function, function);
}

void InstructionSelection::callActivationProperty(IR::Call *c)
{
    IR::Name *n = c->base->asName();
    Q_ASSERT(n);

    if (n->builtin == IR::Name::builtin_invalid) {
        Instruction::LoadName load;
        load.name = _engine->newString(*n->id);
        addInstruction(load);

        Instruction::CallValue call;
        prepareCallArgs(c->args, call.argc, call.args);
        addInstruction(call);
    } else {
        Q_UNIMPLEMENTED();
    }
}

void InstructionSelection::callValue(IR::Call *c)
{
    IR::Temp *t = c->base->asTemp();
    Q_ASSERT(t);

    Instruction::LoadTemp load;
    load.tempIndex = t->index;
    addInstruction(load);

    Instruction::CallValue call;
    prepareCallArgs(c->args, call.argc, call.args);
    addInstruction(call);
}

void InstructionSelection::callProperty(IR::Call *c)
{
    IR::Member *m = c->base->asMember();
    Q_ASSERT(m);

    // load the base
    Instruction::LoadTemp load;
    load.tempIndex = m->base->asTemp()->index;
    addInstruction(load);

    // call the property on the loaded base
    Instruction::CallProperty call;
    call.name = _engine->newString(*m->name);
    prepareCallArgs(c->args, call.argc, call.args);
    addInstruction(call);
}

void InstructionSelection::prepareCallArgs(IR::ExprList *e, quint32 &argc, quint32 &args)
{
    int locals = _function->tempCount - _function->locals.size() + _function->maxNumberOfArguments;

    if (e && e->next == 0 && e->expr->asTemp()->index >= 0 && e->expr->asTemp()->index < locals) {
        // We pass single arguments as references to the stack
        argc = 1;
        args = e->expr->asTemp()->index;
    } else if (e) {
        // We need to move all the temps into the function arg array
        int argLocation = _function->tempCount - _function->locals.size();
        assert(argLocation >= 0);
        args = argLocation;
        while (e) {
            Instruction::MoveTemp move;
            move.fromTempIndex = e->expr->asTemp()->index;
            move.toTempIndex = argLocation;
            addInstruction(move);
            ++argLocation;
            ++argc;
            e = e->next;
        }
    }
}

void InstructionSelection::visitExp(IR::Exp *s)
{
    if (IR::Call *c = s->expr->asCall()) {
        if (c->base->asName()) {
            callActivationProperty(c);
        } else if (c->base->asTemp()) {
            callValue(c);
        } else if (c->base->asMember()) {
            callProperty(c);
        } else {
            Q_UNREACHABLE();
        }

        // TODO: check if we should store the return value ?
    } else {
        Q_UNREACHABLE();
    }
}

void InstructionSelection::visitEnter(IR::Enter *)
{
    qWarning("%s", __PRETTY_FUNCTION__);
    Q_UNREACHABLE();
}

void InstructionSelection::visitLeave(IR::Leave *)
{
    qWarning("%s", __PRETTY_FUNCTION__);
    Q_UNREACHABLE();
}

void InstructionSelection::visitMove(IR::Move *s)
{
    if (s->op == IR::OpInvalid) 
        simpleMove(s);
    else 
        qWarning("UNKNOWN MOVE");
}

typedef VM::Value (*ALUFunction)(const VM::Value, const VM::Value, VM::Context*);
static inline ALUFunction aluOpFunction(IR::AluOp op)
{
    switch (op) {
    case IR::OpInvalid:
        return 0;
    case IR::OpIfTrue:
        return 0;
    case IR::OpNot:
        return 0;
    case IR::OpUMinus:
        return 0;
    case IR::OpUPlus:
        return 0;
    case IR::OpCompl:
        return 0;
    case IR::OpBitAnd:
        return VM::__qmljs_bit_and;
    case IR::OpBitOr:
        return VM::__qmljs_bit_or;
    case IR::OpBitXor:
        return VM::__qmljs_bit_xor;
    case IR::OpAdd:
        return VM::__qmljs_add;
    case IR::OpSub:
        return VM::__qmljs_sub;
    case IR::OpMul:
        return VM::__qmljs_mul;
    case IR::OpDiv:
        return VM::__qmljs_div;
    case IR::OpMod:
        return VM::__qmljs_mod;
    case IR::OpLShift:
        return VM::__qmljs_shl;
    case IR::OpRShift:
        return VM::__qmljs_shr;
    case IR::OpURShift:
        return VM::__qmljs_ushr;
    case IR::OpGt:
        return VM::__qmljs_gt;
    case IR::OpLt:
        return VM::__qmljs_lt;
    case IR::OpGe:
        return VM::__qmljs_ge;
    case IR::OpLe:
        return VM::__qmljs_le;
    case IR::OpEqual:
        return VM::__qmljs_eq;
    case IR::OpNotEqual:
        return VM::__qmljs_ne;
    case IR::OpStrictEqual:
        return VM::__qmljs_se;
    case IR::OpStrictNotEqual:
        return VM::__qmljs_sne;
    case IR::OpInstanceof:
        return VM::__qmljs_instanceof;
    case IR::OpIn:
        return VM::__qmljs_in;
    case IR::OpAnd:
        return 0;
    case IR::OpOr:
        return 0;
    default:
        assert(!"Unknown AluOp");
        return 0;
    }
};

// A move that doesn't involve an inplace operation
void InstructionSelection::simpleMove(IR::Move *s)
{
    if (IR::Name *n = s->target->asName()) {
        Q_UNUSED(n);
        // set activation property
        if (IR::Temp *t = s->source->asTemp()) {
            // TODO: fold the next 2 instructions.
            Instruction::LoadTemp load;
            load.tempIndex = t->index;
            addInstruction(load);

            Instruction::StoreName store;
            store.name = _engine->newString(*n->id);
            addInstruction(store);
        } else {
            Q_UNREACHABLE();
        }
    } else if (IR::Temp *t = s->target->asTemp()) {
        // Check what kind of load it is, and generate the instruction for that.
        // The store to the temp (the target) is done afterwards.
        if (IR::Name *n = s->source->asName()) {
            Q_UNUSED(n);
            if (*n->id == QStringLiteral("this")) { // ### `this' should be a builtin.
                addInstruction(Instruction::LoadThis());
            } else {
                Instruction::LoadName load;
                load.name = _engine->newString(*n->id);
                addInstruction(load);
            }
        } else if (IR::Const *c = s->source->asConst()) {
            switch (c->type) {
            case IR::UndefinedType:
                addInstruction(Instruction::LoadUndefined());
                break;
            case IR::NullType:
                addInstruction(Instruction::LoadNull());
                break;
            case IR::BoolType:
                if (c->value) addInstruction(Instruction::LoadTrue());
                else addInstruction(Instruction::LoadFalse());
                break;
            case IR::NumberType: {
                Instruction::LoadNumber load;
                load.value = c->value;
                addInstruction(load);
                } break;
            default:
                Q_UNREACHABLE();
                break;
            }
        } else if (IR::Temp *t2 = s->source->asTemp()) {
            Instruction::LoadTemp load;
            load.tempIndex = t2->index;
            addInstruction(load);
        } else if (IR::String *str = s->source->asString()) {
            Instruction::LoadString load;
            load.value = _engine->newString(*str->value);
            addInstruction(load);
        } else if (IR::Closure *clos = s->source->asClosure()) {
            Instruction::LoadClosure load;
            load.value = clos->value;
            addInstruction(load);
        } else if (IR::New *ctor = s->source->asNew()) {
            Q_UNUSED(ctor);
            qWarning("  NEW");
        } else if (IR::Member *m = s->source->asMember()) {
            Q_UNUSED(m);
            qWarning("  MEMBER");
        } else if (IR::Subscript *ss = s->source->asSubscript()) {
            Q_UNUSED(ss);
            qWarning("  SUBSCRIPT");
        } else if (IR::Unop *u = s->source->asUnop()) {
            Q_UNUSED(u);
            qWarning("  UNOP");
        } else if (IR::Binop *b = s->source->asBinop()) {
            Instruction::Binop binop;
            binop.alu = aluOpFunction(b->op);
            binop.lhsTempIndex = b->left->index;
            binop.rhsTempIndex = b->right->index;
            addInstruction(binop);
        } else if (IR::Call *c = s->source->asCall()) {
            if (c->base->asName()) {
                callActivationProperty(c);
            } else if (c->base->asMember()) {
                callProperty(c);
            } else if (c->base->asTemp()) {
                callValue(c);
            } else {
                Q_UNREACHABLE();
            }
        }

        Instruction::StoreTemp st;
        st.tempIndex = t->index;
        addInstruction(st);
    } else if (IR::Member *m = s->target->asMember()) {
        Q_UNUSED(m);
        qWarning("MEMBER");
    } else if (IR::Subscript *ss = s->target->asSubscript()) {
        Q_UNUSED(ss);
        qWarning("SUBSCRIPT");
    } else {
        Q_UNREACHABLE();
    }
}

void InstructionSelection::visitJump(IR::Jump *s)
{
    Instruction::Jump jump;
    jump.offset = 0;
    ptrdiff_t loc = addInstruction(jump) + (((const char *)&jump.offset) - ((const char *)&jump));

    _patches[s->target].append(loc);
}

void InstructionSelection::visitCJump(IR::CJump *s)
{
    if (IR::Temp *t = s->cond->asTemp()) {
        Instruction::LoadTemp load;
        load.tempIndex = t->index;
        addInstruction(load);
    } else if (IR::Binop *b = s->cond->asBinop()) {
        Instruction::Binop binop;
        binop.alu = aluOpFunction(b->op);
        binop.lhsTempIndex = b->left->index;
        binop.rhsTempIndex = b->right->index;
        addInstruction(binop);
    } else {
        Q_UNREACHABLE();
    }

    Instruction::CJump jump;
    jump.offset = 0;
    ptrdiff_t tl = addInstruction(jump) + (((const char *)&jump.offset) - ((const char *)&jump));
    _patches[s->iftrue].append(tl);

    if (_block->index + 1 != s->iffalse->index) {
        Instruction::Jump jump;
        jump.offset = 0;
        ptrdiff_t fl = addInstruction(jump) + (((const char *)&jump.offset) - ((const char *)&jump));
        _patches[s->iffalse].append(fl);
    }
}

void InstructionSelection::visitRet(IR::Ret *s)
{
    Instruction::Ret ret;
    ret.tempIndex = s->expr->index;
    addInstruction(ret);
}

ptrdiff_t InstructionSelection::addInstructionHelper(Instr::Type type, Instr &instr)
{
#ifdef MOTH_THREADED_INTERPRETER
    instr.common.code = VME::instructionJumpTable()[static_cast<int>(type)];
#else
    instr.common.instructionType = type;
#endif

    ptrdiff_t ptrOffset = _ccode - _code;
    int size = Instr::size(type);

    ::memcpy(_ccode, reinterpret_cast<const char *>(&instr), size);
    _ccode += size;

    return ptrOffset;
}

