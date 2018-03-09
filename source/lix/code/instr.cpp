#include "instr.hpp"

#include <iomanip>

namespace {
namespace is = lix::code::is_types;
struct code_ostream_visitor {
    std::ostream& o;

    void operator()(is::ret r) { o << std::setw(13) << "ret  " << r.slot; }
    void operator()(is::call c) { o << std::setw(13) << "call  " << c.fn << ", " << c.arg; }
    void operator()(is::tail t) { o << std::setw(13) << "tail  " << t.fn << ", " << t.arg; }
    void operator()(is::call_mfa c) {
        o << std::setw(13) << "call_mfa  " << c.module.string() << "." << c.fn.string() << "(";
        auto arg_iter = c.args.begin();
        auto arg_end  = c.args.end();
        while (arg_iter != arg_end) {
            o << *arg_iter++;
            if (arg_iter != arg_end) {
                o << ", ";
            }
        }
        o << ')';
    }
    void operator()(is::tail_mfa c) {
        o << std::setw(13) << "tail_mfa  " << c.module.string() << "." << c.fn.string() << "(";
        auto arg_iter = c.args.begin();
        auto arg_end  = c.args.end();
        while (arg_iter != arg_end) {
            o << *arg_iter++;
            if (arg_iter != arg_end) {
                o << ", ";
            }
        }
        o << ')';
    }
    void operator()(is::add a) { o << std::setw(13) << "add  " << a.a << ", " << a.b; }
    void operator()(is::sub s) { o << std::setw(13) << "sub  " << s.a << ", " << s.b; }
    void operator()(is::mul m) { o << std::setw(13) << "mul  " << m.a << ", " << m.b; }
    void operator()(is::div d) { o << std::setw(13) << "div  " << d.a << ", " << d.b; }
    void operator()(is::mk_cons c) { o << std::setw(13) << "mk_cons  " << c.lhs << ", " << c.rhs; }
    void operator()(is::push_front c) {
        o << std::setw(13) << "push_front  " << c.elem << ", " << c.list;
    }
    void operator()(is::eq e) { o << std::setw(13) << "eq  " << e.a << ", " << e.b; }
    void operator()(is::concat c) { o << std::setw(13) << "concat  " << c.a << ", " << c.b; }
    void operator()(is::neq e) { o << std::setw(13) << "neq  " << e.a << ", " << e.b; }
    void operator()(is::negate n) { o << std::setw(13) << "negate  " << n.arg; }
    void operator()(is::const_int i) { o << std::setw(13) << "const_int  " << i.value; }
    void operator()(is::const_real r) { o << std::setw(13) << "const_real  " << r.value; }
    void operator()(is::const_symbol sym) { o << std::setw(13) << "const_sym  " << sym.sym; }
    void operator()(const is::const_str& str) {
        o << std::setw(13) << "const_str  " << '"' << str.string << '"';
    }
    void operator()(is::hard_match m) {
        o << std::setw(13) << "hard_match  " << m.lhs << ", " << m.rhs;
    }
    void operator()(is::try_match m) {
        o << std::setw(13) << "try_match  " << m.lhs << ", " << m.rhs;
    }
    void operator()(is::try_match_conj m) {
        o << std::setw(13) << "try_match_conj  " << m.lhs << ", " << m.rhs;
    }
    void operator()(is::const_binding_slot s) { o << std::setw(13) << "bind_slot  " << s.slot; }

    void operator()(is::mk_tuple_0) { o << std::setw(13) << "mk_tuple_0   "; }
    void operator()(is::mk_tuple_1 t) { o << std::setw(13) << "mk_tuple_1  " << t.a; }
    void operator()(is::mk_tuple_2 t) {
        o << std::setw(13) << "mk_tuple_2  " << t.a << ", " << t.b;
    }
    void operator()(is::mk_tuple_3 t) {
        o << std::setw(13) << "mk_tuple_3  " << t.a << ", " << t.b << ", " << t.c;
    }
    void operator()(is::mk_tuple_4 t) {
        o << std::setw(13) << "mk_tuple_4  " << t.a << ", " << t.b << ", " << t.c << ", " << t.d;
    }
    void operator()(is::mk_tuple_5 t) {
        o << std::setw(13) << "mk_tuple_5  " << t.a << ", " << t.b << ", " << t.c << ", " << t.d
          << ", " << t.e;
    }
    void operator()(is::mk_tuple_6 t) {
        o << std::setw(13) << "mk_tuple_6  " << t.a << ", " << t.b << ", " << t.c << ", " << t.d
          << ", " << t.e << ", " << t.f;
    }
    void operator()(is::mk_tuple_7 t) {
        o << std::setw(13) << "mk_tuple_7  " << t.a << ", " << t.b << ", " << t.c << ", " << t.d
          << ", " << t.e << ", " << t.f << ", " << t.g;
    }
    void operator()(const is::mk_list& l) {
        o << std::setw(13) << "mk_list  ";
        for (auto slot : l.slots) {
            o << slot << ", ";
        }
    }
    void operator()(const is::mk_map& l) {
        o << std::setw(13) << "mk_map  ";
        for (auto slot : l.slots) {
            o << slot << ", ";
        }
    }
    void operator()(const is::mk_tuple_n& n) {
        o << std::setw(13) << "mk_tuple_n  ";
        for (auto slot : n.slots) {
            o << slot << ", ";
        }
    }
    void operator()(const is::mk_closure& clos) {
        o << std::setw(13) << "mk_closure  " << clos.code_begin << " -> " << clos.code_end;
        for (auto& slot : clos.captures) {
            o << ", " << slot;
        }
    }

    void operator()(is::jump j) { o << std::setw(13) << "jump  " << j.target; }
    void operator()(is::test_true t) { o << std::setw(13) << "test_true  " << t.slot; }
    void operator()(is::is_list i) { o << std::setw(13) << "is_list  " << i.arg; }
    void operator()(is::is_symbol i) { o << std::setw(13) << "is_symbol  " << i.arg; }
    void operator()(is::is_string i) { o << std::setw(13) << "is_string  " << i.arg; }
    void operator()(is::to_string i) { o << std::setw(13) << "to_string  " << i.arg; }
    void operator()(is::inspect i) { o << std::setw(13) << "inspect  " << i.arg; }
    void operator()(is::raise r) { o << std::setw(13) << "raise  " << r.arg; }
    void operator()(is::false_jump j) { o << std::setw(13) << "false_jump  " << j.target; }
    void operator()(is::rewind r) { o << std::setw(13) << "rewind  " << r.slot; }
    void operator()(const is::frame_id& id) { o << std::setw(13) << "frame_id  " << id.id; }
    void operator()(is::apply a) {
        o << std::setw(13) << "apply  " << a.mod << ", " << a.fn << ", " << a.arglist;
    }
    void operator()(is::no_clause n) {
        // No matching clause
        o << std::setw(13) << "no_clause  " << n.unmatched;
    }

    void operator()(is::dot d) { o << std::setw(13) << "dot  " << d.object << ", " << d.attr_name; }
};
}  // namespace

std::ostream& lix::code::operator<<(std::ostream& o, const lix::code::instr& inst) {
    inst.visit(code_ostream_visitor{o});
    return o;
}