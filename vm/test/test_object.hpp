#include "vm.hpp"
#include "objectmemory.hpp"
#include "builtin/object.hpp"
#include "builtin/compactlookuptable.hpp"

#include <cxxtest/TestSuite.h>

using namespace rubinius;

class TestObject : public CxxTest::TestSuite {
  public:

  VM *state;

  void setUp() {
    state = new VM(1024);
  }

  void tearDown() {
    delete state;
  }

  void test_change_class_to() {
    Object* obj = util_new_object();
    Tuple* tup = Tuple::create(state, 1);

    Class* tup_class = tup->klass();

    obj->change_class_to(state, tup_class);

    TS_ASSERT_EQUALS(tup_class, obj->klass());
  }

  void test_kind_of() {
    Object* obj = util_new_object();
    TS_ASSERT(kind_of<Object>(obj));
    TS_ASSERT(kind_of<Object>(Fixnum::from(1)));
    TS_ASSERT(kind_of<Object>(String::create(state, "blah")));
  }

  void test_instance_of() {
    Object* obj = util_new_object();
    TS_ASSERT(instance_of<Object>(obj));
    TS_ASSERT(!instance_of<Object>(Fixnum::from(1)));
    TS_ASSERT(!instance_of<Object>(String::create(state, "blah")));
  }

  void test_instance_variables() {
    Object* obj = util_new_object();
    Symbol* t1 = state->symbol("@test1");
    Symbol* t2 = state->symbol("@test2");

    obj->set_ivar(state, t1, Qtrue);
    obj->set_ivar(state, t2, Qtrue);

    TS_ASSERT_EQUALS(Qtrue, as<CompactLookupTable>(obj->get_ivars(state))->fetch(state, t1));
    TS_ASSERT_EQUALS(Qtrue, as<CompactLookupTable>(obj->get_ivars(state))->fetch(state, t2));
  }

  void test_instance_variables_nil() {
    Object* obj = Qnil;
    Symbol* t1 = state->symbol("@test1");
    Symbol* t2 = state->symbol("@test2");

    obj->set_ivar(state, t1, Qtrue);
    obj->set_ivar(state, t2, Qtrue);

    TS_ASSERT_EQUALS(Qtrue, as<LookupTable>(obj->get_ivars(state))->fetch(state, t1));
    TS_ASSERT_EQUALS(Qtrue, as<LookupTable>(obj->get_ivars(state))->fetch(state, t2));
  }

  void test_instance_variables_none() {
    Object* obj = util_new_object();

    TS_ASSERT_EQUALS(Qnil, obj->get_ivars(state));
  }

  void test_as() {
    Object* obj = util_new_object();
    Fixnum* fix = Fixnum::from(1);

    Object* nil = Qnil;

    // OK
    TS_ASSERT_EQUALS(as<Object>(obj), obj);

    TS_ASSERT_EQUALS(as<Integer>(fix), fix);
    TS_ASSERT_EQUALS(as<Fixnum>(fix), fix);
    TS_ASSERT_EQUALS(as<Object>(fix), fix);

    // Fail
    TS_ASSERT_THROWS(as<String>(nil), TypeError);
    TS_ASSERT_THROWS(as<String>(obj), TypeError);
    TS_ASSERT_THROWS(as<String>(fix), TypeError);
  }

  void test_try_as() {
    Object* obj = util_new_object();
    Fixnum* fix = Fixnum::from(1);

    Object* nil = Qnil;

    // OK
    TS_ASSERT_EQUALS(try_as<Object>(obj), obj);
    TS_ASSERT_EQUALS(try_as<Object>(fix), fix);

    // OK, but returns NULL because there is no conversion
    TS_ASSERT_EQUALS(try_as<String>(nil), static_cast<String*>(NULL));
    TS_ASSERT_EQUALS(try_as<String>(obj), static_cast<String*>(NULL));
    TS_ASSERT_EQUALS(try_as<String>(fix), static_cast<String*>(NULL));
  }

  void test_dup() {
    Tuple* tup = Tuple::create(state, 1);
    tup->put(state, 0, Qtrue);

    tup->set_ivar(state, state->symbol("@name"), state->symbol("foo"));

    Tuple* tup2 = as<Tuple>(tup->dup(state));

    TS_ASSERT_EQUALS(tup2->at(state, 0), Qtrue);
    TS_ASSERT_DIFFERS(tup->id(state), tup2->id(state));

    TS_ASSERT(tup->ivars_ != tup2->ivars_);

    TS_ASSERT_EQUALS(tup2->get_ivar(state, state->symbol("@name")),
        state->symbol("foo"));

    tup->ivars_ = as<CompactLookupTable>(tup->ivars_)->to_lookuptable(state);
    Tuple* tup3 = as<Tuple>(tup->dup(state));

    TS_ASSERT(tup->ivars_ != tup2->ivars_);
    TS_ASSERT_EQUALS(tup3->get_ivar(state, state->symbol("@name")),
        state->symbol("foo"));
  }

  void test_dup_ignores_metaclass() {
    Tuple* tup = Tuple::create(state, 1);
    tup->put(state, 0, Qtrue);

    // Force it to exist.
    tup->metaclass(state);

    Tuple* tup2 = as<Tuple>(tup->dup(state));

    TS_ASSERT(!try_as<MetaClass>(tup2->klass_));

    TS_ASSERT_DIFFERS(tup->metaclass(state), tup2->metaclass(state));
  }

  void test_clone() {
    Tuple* tup = Tuple::create(state, 1);
    tup->put(state, 0, Qtrue);

    Tuple* tup2 = (Tuple*)tup->clone(state);

    TS_ASSERT_EQUALS(tup2->at(state, 0), Qtrue);

    TS_ASSERT_DIFFERS(tup2->id(state), tup->id(state));

    TS_ASSERT_DIFFERS(tup2->metaclass(state), tup->metaclass(state));
    TS_ASSERT_DIFFERS(tup2->metaclass(state)->method_table(), tup->metaclass(state)->method_table());
    TS_ASSERT_DIFFERS(tup2->metaclass(state)->constants(), tup->metaclass(state)->constants());
  }

  void test_dup_bytes() {
    ByteArray* obj = state->om->new_object_bytes<ByteArray>(G(object), 1);
    obj->StoresBytes = 1;

    obj->bytes[0] = 8;

    ByteArray* obj2 = (ByteArray*)obj->dup(state);

    TS_ASSERT(obj2->stores_bytes_p());
    TS_ASSERT_EQUALS(obj2->bytes[0], 8);
  }

  void test_kind_of_p() {
    String* str = String::create(state, "blah");

    TS_ASSERT(str->kind_of_p(state, G(string)));
    TS_ASSERT(!str->kind_of_p(state, G(tuple)));
  }

  void test_kind_of_prim() {
    String* str = String::create(state, "thingy");

    TS_ASSERT_EQUALS(Qtrue, str->kind_of_prim(state, G(string)));
    TS_ASSERT_EQUALS(Qfalse, str->kind_of_prim(state, G(tuple)));
  }

  void test_hash() {
    TS_ASSERT_EQUALS(Qnil->hash(state), Qnil->hash(state));
    TS_ASSERT(Qnil->hash(state) > 0);

    TS_ASSERT_EQUALS(Qtrue->hash(state), Qtrue->hash(state));
    TS_ASSERT(Qtrue->hash(state) > 0);

    TS_ASSERT_EQUALS(Qfalse->hash(state), Qfalse->hash(state));
    TS_ASSERT(Qfalse->hash(state) > 0);

    TS_ASSERT_DIFFERS(Fixnum::from(1)->hash(state), Fixnum::from(2)->hash(state));
    TS_ASSERT_DIFFERS(Fixnum::from(-1)->hash(state), Fixnum::from(1)->hash(state));
    TS_ASSERT_DIFFERS(Fixnum::from(-2)->hash(state), Fixnum::from(-1)->hash(state));

    TS_ASSERT_EQUALS(Bignum::from(state, (native_int)13)->hash(state), Bignum::from(state, (native_int)13)->hash(state));
    TS_ASSERT(Bignum::from(state, (native_int)13)->hash(state) > 0);

    TS_ASSERT_EQUALS(Float::create(state, 15.0)->hash(state), Float::create(state, 15.0)->hash(state));
    TS_ASSERT(Float::create(state, 15.0)->hash(state) > 0);

    Object* obj = util_new_object();
    TS_ASSERT_EQUALS(obj->hash(state), obj->hash(state));
    TS_ASSERT(obj->hash(state) > 0);

    TS_ASSERT_EQUALS(String::create(state, "blah")->hash(state), String::create(state, "blah")->hash(state));
    TS_ASSERT(String::create(state, "blah")->hash(state) > 0);
  }

  void test_metaclass() {
    TS_ASSERT(kind_of<MetaClass>(G(object)->metaclass(state)));
    TS_ASSERT_EQUALS(Qnil->metaclass(state), G(nil_class));
    TS_ASSERT_EQUALS(Qtrue->metaclass(state), G(true_class));
    TS_ASSERT_EQUALS(Qfalse->metaclass(state), G(false_class));

    Tuple *tup = Tuple::create(state, 1);
    TS_ASSERT(!kind_of<MetaClass>(tup->klass()));

    TS_ASSERT(kind_of<MetaClass>(tup->metaclass(state)));
    TS_ASSERT(kind_of<MetaClass>(tup->klass()));
  }

  void test_equal() {
    String* s1 = String::create(state, "whatever");
    String* s2 = String::create(state, "whatever");

    TS_ASSERT_EQUALS(as<Object>(s1)->equal(state, as<Object>(s2)), Qfalse);
    TS_ASSERT_EQUALS(as<Object>(Fixnum::from(0))->equal(state, as<Object>(Fixnum::from(0))), Qtrue);
  }

  void test_set_ivar() {
    size_t size = COMPACTLOOKUPTABLE_SIZE / 2 + 2;
    Object* obj = util_new_object();
    Symbol* sym;

    for(size_t i = 0; i < size; i++) {
      std::stringstream name;
      name << "@test" << i;
      sym = state->symbol(name.str().c_str());
      obj->set_ivar(state, sym, Fixnum::from(i));
    }

    sym = state->symbol("@test5");
    TS_ASSERT_EQUALS(obj->get_ivar(state, sym), Fixnum::from(5));
  }

  void test_set_ivar_on_immediate() {
    size_t size = COMPACTLOOKUPTABLE_SIZE / 2 + 2;
    Object* obj = Fixnum::from(-10);
    Symbol* sym;

    for(size_t i = 0; i < size; i++) {
      std::stringstream name;
      name << "@test" << i;
      sym = state->symbol(name.str().c_str());
      obj->set_ivar(state, sym, Fixnum::from(i));
    }

    sym = state->symbol("@test5");
    TS_ASSERT_EQUALS(obj->get_ivar(state, sym), Fixnum::from(5));
  }

  void test_get_ivar() {
    Symbol* sym = state->symbol("@test");
    Object* val = Fixnum::from(33);
    Object* obj = util_new_object();

    TS_ASSERT_EQUALS(Qnil, obj->get_ivar(state, state->symbol("@non_existent")));
    TS_ASSERT_EQUALS(Qnil, obj->get_ivar(state, sym));

    obj->set_ivar(state, sym, val);

    TS_ASSERT_EQUALS(val, obj->get_ivar(state, sym));
  }

  void test_get_ivar_on_immediate() {
    Symbol* sym = state->symbol("@test");
    Object* val = Fixnum::from(33);
    Object* obj = Fixnum::from(-10);

    TS_ASSERT_EQUALS(Qnil, obj->get_ivar(state, state->symbol("@non_existent")));
    TS_ASSERT_EQUALS(Qnil, obj->get_ivar(state, sym));

    obj->set_ivar(state, sym, val);

    TS_ASSERT_EQUALS(val, obj->get_ivar(state, sym));
  }

  void test_get_ivars() {
    Object* obj = util_new_object();

    Symbol* sym1 = state->symbol("@test1");
    Symbol* sym2 = state->symbol("@test2");
    Fixnum* one = Fixnum::from(1);
    Fixnum* two = Fixnum::from(2);
    obj->set_ivar(state, sym1, one);
    obj->set_ivar(state, sym2, two);

    CompactLookupTable* ivars = (CompactLookupTable*)obj->get_ivars(state);

    TS_ASSERT_EQUALS(ivars->fetch(state, sym1), one);
    TS_ASSERT_EQUALS(ivars->fetch(state, sym2), two);
  }

  void test_id() {
    Tuple* t1 = Tuple::create(state, 2);
    Tuple* t2 = Tuple::create(state, 2);

    Integer* id1 = t1->id(state);
    Integer* id2 = t2->id(state);

    TS_ASSERT(id1->to_native() > 0);
    TS_ASSERT(id2->to_native() > 0);

    TS_ASSERT_DIFFERS(id1, id2)

    TS_ASSERT_EQUALS(id1, t1->id(state));

    Integer* id3 = Fixnum::from(33)->id(state);
    TS_ASSERT_DIFFERS(id3, id1);

    Integer* id4 = Fixnum::from(33)->id(state);
    TS_ASSERT_EQUALS(id3, id4);
    TS_ASSERT(id4->to_native() % 2 == 1);

    Integer* id5 = Fixnum::from(34)->id(state);
    TS_ASSERT_DIFFERS(id4, id5);
    TS_ASSERT(id5->to_native() % 2 == 1);
  }

  void test_infect() {
    Object* obj1 = util_new_object();
    Object* obj2 = util_new_object();

    TS_ASSERT_EQUALS(obj1->tainted_p(), Qfalse);
    TS_ASSERT_EQUALS(obj2->tainted_p(), Qfalse);

    obj1->infect(obj2);

    TS_ASSERT_EQUALS(obj2->tainted_p(), Qfalse);

    obj1->taint();
    obj1->infect(obj2);

    TS_ASSERT_EQUALS(obj2->tainted_p(), Qtrue);
  }

  void test_infect_non_reference() {
    Object* obj1 = util_new_object();
    Object* obj2 = Integer::from(state, 5);

    obj1->infect(obj2);

    TS_ASSERT_EQUALS(obj2->tainted_p(), Qfalse);

    obj1->taint();
    obj1->infect(obj2);

    TS_ASSERT_EQUALS(obj2->tainted_p(), Qfalse);
  }

  void test_tainted_p() {
    Object* obj = util_new_object();

    TS_ASSERT_EQUALS(obj->tainted_p(), Qfalse);
    obj->taint();
    TS_ASSERT_EQUALS(obj->tainted_p(), Qtrue);
  }

  void test_tainted_p_non_reference() {
    Object* obj = Integer::from(state, 5);

    TS_ASSERT_EQUALS(obj->tainted_p(), Qfalse);
    obj->taint();
    TS_ASSERT_EQUALS(obj->tainted_p(), Qfalse);
  }

  void test_taint() {
    Object* obj = util_new_object();

    TS_ASSERT(!obj->IsTainted);
    obj->taint();
    TS_ASSERT(obj->IsTainted);
  }

  void test_untaint() {
    Object* obj = util_new_object();

    obj->IsTainted = TRUE;
    TS_ASSERT(obj->IsTainted);
    obj->untaint();
    TS_ASSERT(!obj->IsTainted);
  }

  void test_frozen_p() {
    Object* obj = util_new_object();

    TS_ASSERT_EQUALS(obj->frozen_p(), Qfalse);
    obj->IsFrozen = TRUE;
    TS_ASSERT_EQUALS(obj->frozen_p(), Qtrue);
  }

  void test_freeze() {
    Object* obj = util_new_object();

    TS_ASSERT(!obj->IsFrozen);
    obj->freeze();
    TS_ASSERT(obj->IsFrozen);
  }

  void test_nil_class() {
    TS_ASSERT_EQUALS(Qnil->class_object(state), G(nil_class));
  }

  void test_true_class() {
    TS_ASSERT_EQUALS(Qtrue->class_object(state), G(true_class));
  }

  void test_false_class() {
    TS_ASSERT_EQUALS(Qfalse->class_object(state), G(false_class));
  }

  void test_fixnum_class() {
    for(size_t i = 0; i < SPECIAL_CLASS_MASK; i++) {
      TS_ASSERT_EQUALS(Fixnum::from(i)->class_object(state), G(fixnum_class));
    }
  }

  void test_object_class() {
    Array* ary = Array::create(state, 1);

    TS_ASSERT_EQUALS(G(array), ary->class_object(state));
  }

  void test_object_class_with_superclass_chain() {
    Module* mod = Module::create(state);
    Class* cls = Class::create(state, G(object));
    Object* obj = state->new_object<Object>(cls);

    /* This should be functionally correct but not actually the
     * way a superclass chain is implemented. However, it doesn't
     * require that we create a root for IncludedModule.
     */
    Module* m = cls->superclass();
    cls->superclass(state, mod);
    mod->superclass(state, m);

    TS_ASSERT_EQUALS(cls, obj->class_object(state));

    obj->klass(state, (Class*)Qnil);

    TS_ASSERT_THROWS_ASSERT(obj->class_object(state), const RubyException &e,
                            TS_ASSERT(Exception::assertion_error_p(state, e.exception)));
  }

  void test_symbol_class() {
    TS_ASSERT_EQUALS(state->symbol("blah")->class_object(state), G(symbol));
  }

  CompiledMethod* create_cm() {
    CompiledMethod* cm = CompiledMethod::create(state);
    cm->iseq(state, InstructionSequence::create(state, 1));
    cm->iseq()->opcodes()->put(state, 0, Fixnum::from(InstructionSequence::insn_ret));
    cm->stack_size(state, Fixnum::from(10));
    cm->total_args(state, Fixnum::from(0));
    cm->required_args(state, cm->total_args());

    // cm->formalize(state);

    return cm;
  }

  void test_send_prim() {
    CompiledMethod* cm = create_cm();
    cm->required_args(state, Fixnum::from(2));
    cm->total_args(state, cm->required_args());
    cm->local_count(state, cm->required_args());
    cm->stack_size(state, cm->required_args());
    cm->splat(state, Qnil);

    G(true_class)->method_table()->store(state, state->symbol("blah"), cm);

    Task* task = Task::create(state, 3);

    task->push(state->symbol("blah"));
    task->push(Fixnum::from(3));
    task->push(Fixnum::from(4));

    MethodContext* input_context = task->active();

    Message msg(state);
    msg.block = Qnil;
    msg.recv = Qtrue;
    msg.lookup_from = G(true_class);
    msg.name = state->symbol("__send__");
    msg.send_site = SendSite::create(state, state->symbol("__send__"));
    msg.use_from_task(task, 3);

    Qtrue->send_prim(state, NULL, task, msg);

    TS_ASSERT(task->active() != input_context);
    TS_ASSERT_EQUALS(task->active()->args, 2U);
    TS_ASSERT_EQUALS(task->stack_at(0), Fixnum::from(3));
    TS_ASSERT_EQUALS(task->stack_at(1), Fixnum::from(4));
    TS_ASSERT_EQUALS(task->active()->cm(), cm);
    TS_ASSERT_EQUALS(task->active()->name(), state->symbol("blah"));
  }

  void test_send_prim_with_string() {
    CompiledMethod* cm = create_cm();
    cm->required_args(state, Fixnum::from(2));
    cm->total_args(state, cm->required_args());
    cm->local_count(state, cm->required_args());
    cm->stack_size(state, cm->required_args());
    cm->splat(state, Qnil);

    G(true_class)->method_table()->store(state, state->symbol("blah"), cm);

    Task* task = Task::create(state, 3);

    task->push(String::create(state, "blah", 4));
    task->push(Fixnum::from(3));
    task->push(Fixnum::from(4));

    MethodContext* input_context = task->active();

    Message msg(state);
    msg.block = Qnil;
    msg.recv = Qtrue;
    msg.lookup_from = G(true_class);
    msg.name = state->symbol("__send__");
    msg.send_site = SendSite::create(state, state->symbol("__send__"));
    msg.use_from_task(task, 3);

    Qtrue->send_prim(state, NULL, task, msg);

    TS_ASSERT(task->active() != input_context);
    TS_ASSERT_EQUALS(task->active()->args, 2U);
    TS_ASSERT_EQUALS(task->stack_at(0), Fixnum::from(3));
    TS_ASSERT_EQUALS(task->stack_at(1), Fixnum::from(4));
    TS_ASSERT_EQUALS(task->active()->cm(), cm);
    TS_ASSERT_EQUALS(task->active()->name(), state->symbol("blah"));
  }

  void test_send_prim_private() {
    CompiledMethod* cm = create_cm();
    cm->required_args(state, Fixnum::from(2));
    cm->total_args(state, cm->required_args());
    cm->local_count(state, cm->required_args());
    cm->stack_size(state, cm->required_args());
    cm->splat(state, Qnil);

    MethodVisibility* vis = MethodVisibility::create(state);
    vis->method(state, cm);
    vis->visibility(state, G(sym_private));

    G(true_class)->method_table()->store(state, state->symbol("blah"), vis);

    Task* task = Task::create(state, 3);

    task->push(state->symbol("blah"));
    task->push(Fixnum::from(3));
    task->push(Fixnum::from(4));

    MethodContext* input_context = task->active();

    Message msg(state);
    msg.recv = Qtrue;
    msg.lookup_from = G(true_class);
    msg.name = state->symbol("__send__");
    msg.send_site = SendSite::create(state, state->symbol("__send__"));
    msg.use_from_task(task, 3);

    Qtrue->send_prim(state, NULL, task, msg);

    TS_ASSERT(task->active() != input_context);
    TS_ASSERT_EQUALS(task->active()->args, 2U);
    TS_ASSERT_EQUALS(task->stack_at(0), Fixnum::from(3));
    TS_ASSERT_EQUALS(task->stack_at(1), Fixnum::from(4));
    TS_ASSERT_EQUALS(task->active()->cm(), cm);
    TS_ASSERT_EQUALS(task->active()->name(), state->symbol("blah"));
  }

  void test_send() {
    CompiledMethod* cm = create_cm();
    cm->required_args(state, Fixnum::from(2));
    cm->total_args(state, cm->required_args());
    cm->local_count(state, cm->required_args());
    cm->stack_size(state, cm->required_args());
    cm->splat(state, Qnil);

    G(true_class)->method_table()->store(state, state->symbol("blah"), cm);

    Task* task = Task::create(state, 2);

    state->globals.current_task.set(task);

    MethodContext* input_context = task->active();

    Qtrue->send(state, state->symbol("blah"), 2, Fixnum::from(3),
          Fixnum::from(4));

    TS_ASSERT(task->active() != input_context);
    TS_ASSERT_EQUALS(task->active()->args, 2U);
    TS_ASSERT_EQUALS(task->stack_at(0), Fixnum::from(3));
    TS_ASSERT_EQUALS(task->stack_at(1), Fixnum::from(4));
    TS_ASSERT_EQUALS(task->active()->cm(), cm);
    TS_ASSERT_EQUALS(task->active()->name(), state->symbol("blah"));

  }

  void test_nil_p() {
    TS_ASSERT(Qnil->nil_p());
    TS_ASSERT(!Qundef->nil_p());
    TS_ASSERT(!Qtrue->nil_p());
    TS_ASSERT(!Qfalse->nil_p());
  }

  void test_undef_p() {
    TS_ASSERT(!Qnil->undef_p());
    TS_ASSERT(Qundef->undef_p());
    TS_ASSERT(!Qtrue->undef_p());
    TS_ASSERT(!Qfalse->undef_p());
  }

  void test_true_p() {
    TS_ASSERT(!Qnil->true_p());
    TS_ASSERT(!Qundef->true_p());
    TS_ASSERT(Qtrue->true_p());
    TS_ASSERT(!Qfalse->true_p());
  }

  void test_false_p() {
    TS_ASSERT(!Qnil->false_p());
    TS_ASSERT(!Qundef->false_p());
    TS_ASSERT(!Qtrue->false_p());
    TS_ASSERT(Qfalse->false_p());
  }

  void test_get_type() {
    TS_ASSERT_EQUALS(Qnil->get_type(), NilType);
    TS_ASSERT_EQUALS(Qtrue->get_type(), TrueType);
    TS_ASSERT_EQUALS(Qfalse->get_type(), FalseType);
    TS_ASSERT_EQUALS(state->symbol("blah")->get_type(), SymbolType);
    Object* obj = util_new_object();
    Bignum* big = Bignum::from(state, (native_int)13);
    TS_ASSERT_EQUALS(obj->get_type(), ObjectType);
    TS_ASSERT_EQUALS(big->get_type(), BignumType);
  }

  Object* util_new_object() {
    return state->new_object<Object>(G(object));
  }
};
