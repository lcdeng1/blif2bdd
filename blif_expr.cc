#include "blif_expr.h"

/*
 * symble table related functions
 */
symbol::symbol(const token& t, symbol* x)
{
    name = t.getAttr();
    lineno = t.getLine();
    next = x;
    type = UNSET;
    build = nullptr;
    computed = false;
    parents = nullptr;
}

void symbol::init_input(unsigned lvl)
{
    type = INPUT;
    level = lvl;
}

void symbol::init_output()
{
    type = OUTPUT;
}

void symbol::init_temp()
{
    type = TEMP;
}

void symbol::init_latch(bool io)
{
    if (io) {
        type = LATCH_IN;
    } else {
        type = LATCH_OUT;
    }
}

symbol* symbol::get_nsymbol(unsigned num)
{
    symbol* curr = this;
    for (unsigned i=0; i<num; i++) {
        curr = curr->next;
    }
    return curr;
}

void symbol::set_rhs(unsigned lineno, expr* rhs)
{
    if (nullptr == build) {
        build = rhs;
        rhs->add_parent(this);
        return;
    }
    std::cerr << "Error on line " << lineno << "\n    ";
    std::cerr << "Another assignment for " << name << "\n";
    throw 2;
}

void symbol::duplicate_error() const
{
    std::cerr << name << " defined already on line " << lineno;
    if (INPUT == type)  std::cerr << " as an input\n";
    if (OUTPUT == type) std::cerr << " as an output\n";
    if (TEMP == type)   std::cerr << " as a temporary\n";
    throw 2;
}

void symbol::add_parent(symbol* p)
{
    if (0==p) return;
    symlist* front = new symlist;
    front->next = parents;
    front->item = p;
    parents = front;
}

symbol* move_to_front(symbol* st, std::string name)
{
    if (0==st) return 0;

    symbol* prev = 0;
    symbol* curr = st;
    while (curr) {
        if (curr->name == name) break;
        prev = curr;
        curr = curr->next;
    }
    if (0==curr) return 0; // not found
    if (prev) {
        // There's stuff in front of curr;
        // move curr to front
        prev->next = curr->next;
        curr->next = st;
    }
    return curr;
}

symbol* make_symbol_entry(symbol* st, bool lhs, const token& name)
{
    symbol* find = move_to_front(st, name.getAttr());

    if (nullptr == find) {
        find = new symbol(name, st);
        find->init_temp();
    }
    if (lhs && INPUT == find->type) {
        std::cerr << "Error on line " << name.getLine() << ":\n    ";
        std::cerr << "Input on LHS of assignment\n";
        throw 2;
    }
    return find;
}

void show_symbols(char stype, const symbol* st)
{
    bool printed = false;
    for (const symbol* curr = st; curr; curr = curr->next) {
        if (curr->type != stype) continue;
        if (printed) std::cerr << ", ";
        std::cerr << curr->name;
        if (stype == INPUT) std::cerr << ": " << curr->level;
        printed = true;
    }
}

/*
 *  expression related functions
 */
expr::expr()
{
    top_level = 0;
    knows_top_level = false;
    has_complement = false;
    knows_complement = false;
}

expr::~expr()
{
}

void expr::rearrange()
{
}

rexdd_edge_t expr::complement_if_needed(rexdd_forest_t *F, rexdd_edge_t ans) const
{
    //
    if (!is_complemented()) return ans;
    return rexdd_NOT_edge(F,&ans,F->S.num_levels);
}

/*
 * term methods
 */

term::term(symbol *v)
{
    var = v;
    ASSERT(var);
    is_const = false;
}

bool term::ready() const
{
    return var->computed;
}

rexdd_edge_t term::construct(rexdd_forest_t *F) const
{
    if (! this->var->computed) {
        // std::cerr << "Uncomputed symbol in term::construct\n";
        // throw 5;
        // std::cerr << "uncomputed symbol term" << var->name << "\n";
        if (is_const) {
            // this is for the constant input, change it to be a constant edge
            var->dd = build_constant(F, var->level, (is_complemented()?1:0));
            var->computed = true;
        } else {
            var->build_bdd(F);
        }
    }

    return complement_if_needed(F, var->dd);
}

void term::show(std::ostream &s) const
{
    if (this->is_const) {
        if (is_complemented()) {
            s << "1";
        } else {
            s << "0";
        }
        return;
    }
    s << ( var ? var->name : "null");
    if (is_complemented()) s << "'";
}

void term::add_parent(symbol* p)
{
    if (var) var->add_parent(p);
}

unsigned term::find_top()
{
    if (var->build) return var->build->topLevel();
    else return var->level;
}

/*
 * assoc methods
 */

assoc::assoc()
{
    list = nullptr;
}

assoc::~assoc()
{
    while (list) {
        node* n = list->next;
        delete list;
        list = n;
    }
}

void assoc::push(expr* t)
{
    list = new node(t, list);
}

bool assoc::ready() const
{
    for (node* ptr = list; ptr; ptr = ptr->next) {
        if (! ptr->term->ready()) return false;
    }
    return true;
}

void assoc::add_parent(symbol* p)
{
    for (node* ptr = list; ptr; ptr = ptr->next) {
        ptr->term->add_parent(p);
    }
}

void assoc::rearrange()
{
    find_top();

#ifdef DEBUG_REARRANGE
    std::cerr << "Rearranging; old:\n";
    for (node* ptr = list; ptr; ptr = ptr->next) {
        std::cerr << "    " << ptr->term->topLevel() << " : ";
        ptr->term->show(std::cerr);
        std::cerr << "\n";
    }
#endif

    list = splitsort(list);

#ifdef DEBUG_REARRANGE
    std::cerr << "Rearranging; new:\n";
    for (node* ptr = list; ptr; ptr = ptr->next) {
        std::cerr << "    " << ptr->term->topLevel() << " : ";
        ptr->term->show(std::cerr);
        std::cerr << "\n";
    }
#endif
}

unsigned assoc::find_top()
{
    unsigned tl = 0;
    for (const node* ptr = list; ptr; ptr = ptr->next) {
        unsigned lvl = ptr->term->topLevel();
        if (lvl > tl) tl = lvl;
    }
    return tl;
}

void assoc::showlist(std::ostream &s, char op) const
{
    s << "(";
    bool optor = false;
    for (const node* ptr = list; ptr; ptr = ptr->next) {
        if (optor) s << op;
        ptr->term->show(s);
        optor = true;
    }
    s << ")";
    if (is_complemented()) s << "'";
}

assoc::node* assoc::splitsort(node* in)
{
    if (nullptr == in) return in;
    if (nullptr == in->next) return in;

    unsigned key = in->term->topLevel();
    node* small = 0;
    node* mid = 0;
    node* large = 0;
    while (in) {
        node* n = in;
        in = in->next;

        if (n->term->topLevel() == key) {
            n->next = mid;
            mid = n;
            continue;
        }
        if (n->term->topLevel() > key) {
            n->next = large;
            large = n;
            continue;
        }
        n->next = small;
        small = n;
        continue;
    }

    mid = append(mid, splitsort(large));
    return append(splitsort(small), mid);
}

assoc::node* assoc::append(node* front, node* back)
{
    if (nullptr == front) return back;
    if (nullptr == back) return front;

    // find tail of front
    node* tail = front;
    while (tail->next) {
        tail = tail->next;
    }
    tail->next = back;
    return front;
}

/*
 * sum methods
 */

sum::sum()
{
}

rexdd_edge_t sum::construct(rexdd_forest_t *F) const
{
    bool empty = true;
    rexdd_edge_t ans, t;
    for (const node* p=List(); p; p=p->next) {
        if (empty) {
            ans = p->term->construct(F);
            empty = false;
            continue;
        }
        t = p->term->construct(F);
        ans = rexdd_OR_edges(F, &t, &ans, F->S.num_levels);
        // decrement something like reference count and remove if zero? TBD
    }
    return complement_if_needed(F, ans);
}

void sum::show(std::ostream &s) const
{
    showlist(s, '+');
}

/*
 * product methods
 */

product::product()
{
}

rexdd_edge_t product::construct(rexdd_forest_t *F) const
{
    // this->show(std::cerr);
    bool empty = true;
    rexdd_edge_t ans, t;
    for (const node* p=List(); p; p=p->next) {
        // the first BDD edge
        if (empty) {
            ans = p->term->construct(F);
            empty = false;
            continue;
        }
        t = p->term->construct(F);
        ans = rexdd_AND_edges(F, &t, &ans, F->S.num_levels);
        // decrement something like reference count and remove if zero? TBD
    }
    return complement_if_needed(F, ans);
}

void product::show(std::ostream &s) const
{
    showlist(s, '*');
}