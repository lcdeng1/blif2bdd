#ifndef BLIF_EXPR_H
#define BLIF_EXPR_H

#include "blif_tk.h"
#include "rexdd.h"
#include "defines.h"

#include <string.h>

struct symbol;

/*
 * Find a symbol with the given name
 * and move it to the front of the list if found.
 * If not found, we return null and the list is unchanged.
 */
symbol* move_to_front(symbol* st, std::string name);

/*
 * Build a temp symbol with the given name,
 * or use existing symbol.
 *
 *      @param  st      List of symbols
 *      @param  lhs     If true, this is on the lhs of =
 *                      (error for inputs)
 *      @param  name    Token containing the identifier
 *
 *      @return Updated list, with new symbol at the front
 */
symbol* make_symbol_entry(symbol* st, bool lhs, const token& name);

/*
 * Primarily for debugging.
 * Traverse symbol list and print names for matching type.
 */
void show_symbols(char stype, const symbol* st);

//
// Expression class hierarchy; built by parser
//
class expr {
        unsigned top_level;
        bool knows_top_level;
        bool has_complement;        // if is_constant, true: constant 1; false: constant 0
        bool knows_complement;      // used for check single output 1s or 0s
    public:
        expr();
        virtual ~expr();

        inline void complement() { has_complement = true; }
        inline bool is_complemented() const { return has_complement; }
        inline void got_so() { knows_complement = true; }
        inline bool knows_complemented() const { return knows_complement; }

        /// Ready to construct (dependencies built already)
        virtual bool ready() const = 0;

        /// Build BDD for this expr
        virtual rexdd_edge_t construct(rexdd_forest_t *F) const = 0;

        /// Display, for debugging
        virtual void show(std::ostream &s) const = 0;

        inline unsigned topLevel() {
            if (!knows_top_level) {
                top_level = find_top();
                knows_top_level = true;
            }
            return top_level;
        }

        virtual void add_parent(symbol* p) = 0;

        /// Reorder expression based on top levels
        virtual void rearrange();

        rexdd_edge_t complement_if_needed(rexdd_forest_t *F, rexdd_edge_t ans) const;

    private:
        virtual unsigned find_top() = 0;
};

class term : public expr {
        symbol* var;
        bool is_const;
    public:
        term(symbol *v);

        inline void constant() { is_const = true; };
        inline bool is_constant() { return is_const; };

        virtual bool ready() const;
        virtual rexdd_edge_t construct(rexdd_forest_t *F) const;
        virtual void show(std::ostream &s) const;
        virtual void add_parent(symbol* p);

    protected:
        virtual unsigned find_top();
};


class assoc : public expr {
    protected:
        struct node {
            expr* term;
            node* next;
            node(expr* t, node* n) {
                term = t;
                next = n;
            }
        };
    private:
        node* list;
    public:
        assoc();
        virtual ~assoc();

        void push(expr* t);

        virtual bool ready() const;
        virtual void add_parent(symbol* p);

        virtual void rearrange();

    protected:
        virtual unsigned find_top();
        void showlist(std::ostream &s, char op) const;

        const node* List() const { return list; }

    private:
        static node* splitsort(node* in);
        static node* append(node* front, node* back);
};

class sum : public assoc {
    public:
        sum();
        virtual rexdd_edge_t construct(rexdd_forest_t *F) const;
        virtual void show(std::ostream &s) const;
};

class product : public assoc {
    public:
        product();
        virtual rexdd_edge_t construct(rexdd_forest_t *F) const;
        virtual void show(std::ostream &s) const;
};

//
//
// Symbol type constants
//
const char INPUT  = 'i';
const char OUTPUT = 'o';
const char LATCH_IN = 'l';
const char LATCH_OUT = 'L';
const char TEMP   = 't';
const char UNSET  = ' ';

struct symlist {
    symbol* item;
    symlist* next;
};

//
// What we need to keep track of for each identifier
//
struct symbol {
        // Symbol name.
        std::string name;
        // Where defined (for errors)
        unsigned lineno;
        // Type of the symbol (input, output, temporary)
        char type;
        // BDD level, for input variables only
        unsigned level;
        // Expression to build; for temp/output variables
        expr* build;
        // Have we computed the BDD root edge yet?
        bool computed;
        // Do we need to keep the BDD root edge?
//        bool keep_dd;     // TBD: add this!
        // The BDD root edge
        // bdd_edge dd;            // placeholder so far
        rexdd_edge_t dd;            // using RexDD lib

        // Next symbol in the list
        symbol* next;

        // List of symbols we appear in RHS of
        symlist* parents;
        // Heuristic: how much does this affect outputs
        unsigned weight;

    public: // I know, so is the above

        symbol(const token& t, symbol* x);

        void init_input(unsigned lvl);
        void init_output();
        // latches TBD
        void init_temp();
        void init_latch(bool io);   // io = 1: in
        // return the nth symbol from the front
        symbol* get_nsymbol(unsigned num);
        void set_rhs(unsigned lineno, expr* rhs);
        void duplicate_error() const;

        void add_parent(symbol* p);

        void build_bdd(rexdd_forest_t *F) {
            ASSERT(build);
            // std::cerr << "building BDD for " << name << " " << type << "\n";
            // build->show(std::cerr);
            // std::cerr << "\n";
            dd = build->construct(F);
            // std::cerr << "Done building BDD for " << name << "\n";
            computed = true;
            // FILE* fout;
            // std::string filename = name + F->S.type_name;
            // filename += ".gv";
            // fout = fopen(filename.c_str(), "w+");
            // build_gv(fout, F, dd);
            // fclose(fout);
            // std::cerr << "card of " << name << ": " << card_edge(F, &dd, F->S.num_levels) << "\n";

        }
};

#endif