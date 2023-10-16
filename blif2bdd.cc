#include <iostream>
#include <iomanip>
#include <ctime>

#include "blif_par.h"
#include "blif_expr.h"
#include "rexdd.h"
#include "timer.h"

//
// Ordering choices
//

static const unsigned ORDER_FILE_TOP    = 0;
static const unsigned ORDER_FILE_BOT    = 1;

static const unsigned ORDER_WEIGHT_TOP  = 2;
static const unsigned ORDER_WEIGHT_BOT  = 3;

inline bool needs_weights(unsigned order)
{
    return (order > ORDER_FILE_BOT);
}

/*
 *  Used for lexer testing
 */
int lextest(lexer &L)
{
    token t;
    for (;;) {
        L.consume(t);
        std::cerr << "Line " << std::setw(4) << t.getLine();
        t.setDetails();
        std::cerr << " token: " << t.getAttr() << std::setw(4) << " \t\tID: " << t.getId() << "\n";
        if (t) continue;
        break;
    }
    return 0;
}
/*
 * Pull inputs out of symbol list
 */
symbol* remove_inputs(symbol* &st)
{
    symbol* revst = 0;
    symbol* inputs = 0;
    while (st) {
        symbol* ptr = st;
        st = st->next;
        if (INPUT == ptr->type) {
            ptr->next = inputs;
            inputs = ptr;
        } else {
            ptr->next = revst;
            revst = ptr;
        }
    }
    st = revst;
    return inputs;
}

// helper function for determine_weights
unsigned move_weights(symbol* &src, unsigned wt, symbol* &dest)
{
    unsigned minwt = 0xffffffff;
    symbol* rev = 0;
    while (src) {
        symbol* ptr = src;
        src = src->next;
        if (wt == ptr->weight) {
            ptr->next = dest;
            dest = ptr;
        } else {
            if (ptr->weight < minwt) {
                minwt = ptr->weight;
            }
            ptr->next = rev;
            rev = ptr;
        }
    }
    src = rev;
    return minwt;
}

// helper function for determine_weights
bool update_weight(symbol* s)
{
    if (0==s) return true;
    if (0==s->parents) {
        s->weight = 1;
        return true;
    }
    s->weight = 0;
    for (const symlist* p = s->parents; p; p=p->next) {
        if (0==p->item->weight) return false;
        s->weight += p->item->weight;
    }
    return true;
}

/*
 * Determine symbol weights
 */
void determine_weights(symbol* &st)
{
    // Zero out weights
    for (symbol* ptr = st; ptr; ptr = ptr->next) {
        ptr->weight = 0;
    }

    symbol* waiting = st;
    st = nullptr;
    while (waiting) {
        symbol* proc = waiting;
        waiting = nullptr;

        while (proc) {
            symbol* ptr = proc;
            proc = proc->next;

            if (update_weight(ptr)) {
                // This one is done, add to st list
                ptr->next = st;
                st = ptr;
            } else {
                // Not done, add back to waiting list
                ptr->next = waiting;
                waiting = ptr;
            }
        }
    }

    // Now, sort by weights

    symbol* ordered = 0;
    unsigned minwt = 0;
    for (;;) {
        minwt = move_weights(st, minwt, ordered);
        if (0==st) break;
    }

    st = ordered;
}

unsigned determine_levels(symbol* IN, unsigned ordering)
{
    unsigned num_vars = 0;
    for (const symbol* p = IN; p; p=p->next) {
        ++num_vars;
    }
    if (ORDER_FILE_BOT == ordering) return num_vars;
    if (ORDER_FILE_TOP == ordering) {
        for (symbol* p = IN; p; p=p->next) {
            p->level = num_vars+1 - p->level;
        }
        return num_vars;
    }

    // TBD: weights

    return num_vars;
}

uint64_t mark_and_count(rexdd_forest_t *F, rexdd_edge_t *e)
{
    unmark_forest(F);
    mark_nodes(F, e->target);
    uint_fast64_t q, n;
    uint64_t num_nodes = 0;
    for (q=0; q<F->M->pages_size; q++) {
        const rexdd_nodepage_t *page = F->M->pages+q;
        for (n=0; n<page->first_unalloc; n++) {
            if (rexdd_is_packed_marked(page->chunk+n)) {
                num_nodes++;
            }
        } // for n
    } // for p
    return num_nodes;
}

/*
 *  link list of inputs to array on index of level
 */
void store_inputs_by_level(symbol* &IN, symbol** out, unsigned num_vars)
{
  for (unsigned i=0; i<=num_vars; i++) out[i] = 0;

  while (IN) {
      symbol* p = IN;
      IN = IN->next;

      ASSERT(p->level > 0);
      ASSERT(p->level <= num_vars);

      p->next = nullptr;
      out[p->level] = p;
  }

  std::cerr << "Variable ordering:\n    (TOP), ";
  unsigned col=7;
  for (unsigned i=num_vars; i; i--) {
      unsigned ilen = out[i]->name.length();
      if (col + ilen > 60) {
          std::cerr << "\n    ";
          col = 0;
      }
      std::cerr << out[i]->name << ", ";
      col += ilen + 2;
  }
  std::cerr << "(BOTTOM)\n";
}

/*
 *  Some information showing
 */
void show_symbols(const symbol* IN, const symbol* ST, unsigned ordering)
{
    std::cerr << "Inputs: ";
    show_symbols(INPUT, IN);

    std::cerr << "\nOutputs: ";
    show_symbols(OUTPUT, ST);

    std::cerr << "\nGates: ";
    show_symbols(TEMP, ST);
    std::cerr << "\n";

    std::cerr << "Assignments:\n";
    for (const symbol* ptr =ST; ptr; ptr = ptr->next) {
        std::cerr << "    " << ptr->name << " = ";
        if (ptr->build) {
            ptr->build->show(std::cerr);
        } else {
            std::cerr << "null";
        }
        std::cerr << "\n";
    }

    if (!needs_weights(ordering)) return;

    std::cerr << "Weights (Inputs):\n";
    for (const symbol* ptr = IN; ptr; ptr = ptr->next) {
        std::cerr << "    " << ptr->name << " : " << ptr->weight << "\n";
    }

    std::cerr << "Weights:\n";
    for (const symbol* ptr = ST; ptr; ptr = ptr->next) {
        std::cerr << "    " << ptr->name << " : " << ptr->weight << "\n";
    }
}

void show_edge(rexdd_edge_t ans)
{
    char rule[16];
    for (int i=0; i<16; i++) {
      rule[i] = 0;
    }
    if (ans.label.rule == rexdd_rule_X) {
      strcpy(rule, "X");
    } else if (ans.label.rule == rexdd_rule_EL0) {
      strcpy(rule, "EL0");
    } else if (ans.label.rule == rexdd_rule_EL1) {
      strcpy(rule, "EL1");
    } else if (ans.label.rule == rexdd_rule_EH0) {
      strcpy(rule, "EH0");
    } else if (ans.label.rule == rexdd_rule_EH1) {
      strcpy(rule, "EH1");
    } else if (ans.label.rule == rexdd_rule_AL0) {
      strcpy(rule, "AL0");
    } else if (ans.label.rule == rexdd_rule_AL1) {
      strcpy(rule, "AL1");
    } else if (ans.label.rule == rexdd_rule_AH0) {
      strcpy(rule, "AH0");
    } else if (ans.label.rule == rexdd_rule_AH1) {
      strcpy(rule, "AH1");
    } 
    printf("\trule is %s\n", rule);
    printf("\tcomp is %d\n", ans.label.complemented);
    printf("\tswap is %d\n", ans.label.swapped);
    printf("\ttarget is %s%llu\t", rexdd_is_terminal(ans.target)?"T":"", rexdd_is_terminal(ans.target)? rexdd_terminal_value(ans.target): ans.target);
}

void typelist()
{
    std::cerr << "\t\tRexBDD:   0\n";
    std::cerr << "\t\tQBDD:     1\n";
    std::cerr << "\t\tCQBDD:    2\n";
    std::cerr << "\t\tSQBDD:    3\n";
    std::cerr << "\t\tCSQBDD:   4\n";
    std::cerr << "\t\tFBDD:     5\n";
    std::cerr << "\t\tCFBDD:    6\n";
    std::cerr << "\t\tSFBDD:    7\n";
    std::cerr << "\t\tCSFBDD:   8\n";
    std::cerr << "\t\tZBDD:     9   (TBD)\n";
    std::cerr << "\t\tESRBDD:   10  (TBD)\n";
    std::cerr << "\t\tCESRBDD:  11\n";
}

/*
 *  The usage information
 */
int usage(const char* exe)
{
    const char* base = exe;
    for (; *exe; ++exe) {
        if ('/' == *exe) base = exe+1;
    }
    std::cerr << "Usage: " << base << " (switches)\n\n";
    std::cerr << "Reads a circuit from a BLIF file on standard input, and builds\n";
    std::cerr << "a BDD, which is written to standard output.\n\n";
    std::cerr << "Switches:\n";
    std::cerr << "    -h: This help\n";
    std::cerr << "\n";
    std::cerr << "    -t: The BDD type for building (default: RexBDD)\n";
    typelist();
    std::cerr << "\n";
    std::cerr << "    -d: Display the BDD forest when done\n";
    std::cerr << "\n";
    std::cerr << "    -oF: Order based on file, first .input at TOP\n";
    std::cerr << "    -of: Order based on file, first .input at BOTTOM\n";
    std::cerr << "    -oW: Order by input weights, largest at TOP\n";
    std::cerr << "    -ow: Order by input weights, largest at BOTTOM\n";
    std::cerr << "\n";
    std::cerr << "    -L: test BLIF lexer\n";
    std::cerr << "    -P: test BLIF parser\n";
    std::cerr << "\n";
    std::cerr << "Written by Lichuan Deng, 09/27/2023\n\n";
    std::cerr << "** Based on SLIF praser Written by Andrew Miner, 11/19/2022 **\n\n";
    return 1;
}

/*
 *  The main function
 */
int main(int argc, char** argv) {
    char bdd_type = 0;      // default RexBDD: 0
    bool display = false;
    bool show_card = false;
    bool testlex = false;
    bool testparse = false;
    unsigned ordering = ORDER_WEIGHT_BOT;
    for (int i=1; i<argc; i++) {
        if (0==strcmp("-h", argv[i])) {
            return usage(argv[0]);
        }
        if (0==strcmp("-d", argv[i])) {
            display = true;
            continue;
        }
        if (0==strcmp("-c", argv[i])) {
            show_card = true;
            continue;
        }
        if (0==strcmp("-t", argv[i])) {
            i++;
            bdd_type = std::stoi(argv[i]);
            continue;
        }
        if (0==strcmp("-L", argv[i])) {
            testlex = true;
            continue;
        }
        if (0==strcmp("-P", argv[i])) {
            testparse = true;
            continue;
        }
        if (0==strcmp("-oF", argv[i])) {
            ordering = ORDER_FILE_TOP;
            continue;
        }
        if (0==strcmp("-of", argv[i])) {
            ordering = ORDER_FILE_BOT;
            continue;
        }
        if (0==strcmp("-oW", argv[i])) {
            ordering = ORDER_WEIGHT_TOP;
            continue;
        }
        if (0==strcmp("-ow", argv[i])) {
            ordering = ORDER_WEIGHT_BOT;
            continue;
        }
        return usage(argv[0]);
    }
    //
    // Lexer here
    //
    lexer L(std::cin);
    if (testlex) return lextest(L);

    //
    // Parse input
    //
    symbol* slist = 0;
    try {
        slist = parse(L);
    }
    catch (int c) {
        return c;
    }

    //
    // Remove inputs from the symbol table, slist
    //
    symbol* inlist = remove_inputs(slist);

    if (needs_weights(ordering)) {
        determine_weights(slist);
        determine_weights(inlist);
    }
    if (testparse) {
        show_symbols(inlist, slist, ordering);
        return 0;
    }
    unsigned num_vars = determine_levels(inlist, ordering);

    //
    // now ready to initialize BDD forest
    //
    rexdd_forest_t F;
    rexdd_forest_settings_t s;
    rexdd_default_forest_settings(num_vars, &s);
    rexdd_type_setting(&s, bdd_type);
    rexdd_init_forest(&F, &s);
    std::cerr << "Forest level is : " << F.S.num_levels << "\n";
    std::cerr << "Forest type is: " << F.S.type_name << "\n";

    //
    //  Rearrange expressions for variable order
    //
    for (symbol* p = slist; p; p=p->next) {
        if (p->build) {
            p->build->rearrange();
        }
    }

    //
    //  Build BDD for .inputs
    //
    symbol** inputs = new symbol* [num_vars+1];
    store_inputs_by_level(inlist, inputs, num_vars);    // level as index
    for (unsigned i=1; i<=num_vars; i++) {
        inputs[i]->dd = build_variable(&F, i);
        inputs[i]->computed = true;
    }

    //
    //  Build BDD from outputs
    //
    timer* rtime = new timer;
    for (symbol* p=slist; p; p=p->next) {
        //
        if (p->type == OUTPUT) {
            // std::cerr << "building " << p->name << "...\n";
            p->build_bdd(&F);
            if (display) {
                std::cerr << "\t" << p->name << ":\n";
                show_edge(p->dd);
                printf("level is %d\n", (rexdd_is_terminal(p->dd.target))?0:rexdd_unpack_level(rexdd_get_packed_for_handle(F.M, p->dd.target)));
                if (show_card) {
                    long long card = card_edge(&F, &p->dd, F.S.num_levels);
                    std::cerr << "card is: " << card << "\n";
                }
                uint64_t num_nodes;
                num_nodes = mark_and_count(&F, &p->dd);
                std::cerr << "number of nodes: " << num_nodes << "\n";
            }
            
            // if (p->name == "615(1750)") {
            //     FILE* fout;
            //     std::string filename = "out_edge";
            //     filename += F.S.type_name;
            //     filename += ".gv";
            //     fout = fopen(filename.c_str(), "w+");
            //     build_gv(fout, &F, p->dd);
            //     fclose(fout);
            // }
            std::cerr << "Garbage collecting...\n";
            unmark_forest(&F);
            // mark inputs
            for (unsigned i=1; i<=num_vars; i++) {
                mark_nodes(&F, inputs[i]->dd.target);
            }
            // mark computed gates or outputs
            for (symbol* q=slist; q; q=q->next) {
                if (q->computed) {
                    mark_nodes(&F, q->dd.target);
                }
            }
            //
            rexdd_sweep_UT(F.UT);
            rexdd_sweep_CT(F.CT, F.M);
            rexdd_sweep_nodeman(F.M);
            std::cerr << "Done GC!\n";
        }
    }
    rtime->note_time();

    //
    //  counting the number of nodes
    //
    unmark_forest(&F);
    for (symbol* p=slist; p; p=p->next) {
        if (p->type == OUTPUT) {
            mark_nodes(&F, p->dd.target);
        }
    }
    uint_fast64_t q, n;
    uint64_t num_nodes = 0;
    for (q=0; q<F.M->pages_size; q++) {
        const rexdd_nodepage_t *page = F.M->pages+q;
        for (n=0; n<page->first_unalloc; n++) {
            if (rexdd_is_packed_marked(page->chunk+n)) {
                num_nodes++;
            }
        } // for n
    } // for p
    
    std::cerr << "Total number of nodes in " << F.S.type_name <<": " << num_nodes << "\n";
    std::cerr << "Time taken of " << F.S.type_name << ": \t\t" << rtime->get_last_seconds() << " seconds\n";


    rexdd_free_forest(&F);
    return 0;
}