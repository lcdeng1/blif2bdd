#include "blif_par.h"
#include "blif_expr.h"

#define MAX_SYMBOLS 1024


void expected(token::type want, const token& got)
{
    std::cerr << "Error line " << got.getLine() << ":\n    ";
    std::cerr << "expected " << want << ", got " << got.getId() << "\n";
    throw 2;
}

void expect2(token::type w1, token::type w2, const token& got)
{
    std::cerr << "Error line " << got.getLine() << ":\n    ";
    std::cerr << "expected " << w1 << " or " << w2 << ", got " << got.getId() << "\n";
    throw 2;
}

void match(lexer &L, token::type t)
{
    token x;
    L.consume(x);
#ifdef DEBUG_PARSER
    x.set_details();
    std::cerr << "consumed " << x << "\n";
#endif
    if (! x.matches(t)) {
        expected(t, x);
    }
}

void process_inputs(lexer &L, symbol* &ST)
{
    unsigned num_inputs = 0;
    token t;
    for (;;) {
        L.consume(t);

        if (t.matches(token::NEWLINE)) {
            if (L.getCover() == 'b') continue;
            break;
        }

        if (! t.matches(token::IDENT)) {
            expected(token::IDENT, t);
        }
        // std::cerr << "Get input: " << t.getAttr() << "\n";

        // Make sure it's not a duplicate symbol
        symbol* find = move_to_front(ST, t.getAttr());
        if (find) {
            std::cerr << "Error line " << t.getLine() << ":\n    ";
            find->duplicate_error();
        }

        // another input variable
        ST = new symbol(t, ST);
        ST->init_input(++num_inputs);
    }
}

void process_outputs(lexer &L, symbol* &ST)
{
    token t;
    for (;;) {
        L.consume(t);

        if (t.matches(token::NEWLINE)) {
            if (L.getCover() == 'b') continue;
            break;
        }

        if (! t.matches(token::IDENT)) {
            expected(token::IDENT, t);
        }

        // Make sure it's not a duplicate symbol
        symbol* find = move_to_front(ST, t.getAttr());
        if (find) {
            if (find->type == INPUT) {
                // which means this input is output
                expr* E;
                product* P = new product;
                sum* S = new sum;
                E = new term(find);
                P->push(E);
                S->push(P);
                ST = new symbol(t, find);
                ST->init_output();
                ST->name += "_OUT";
                ST->build = S;
                continue;
            }
            std::cerr << "Error line " << t.getLine() << ":\n    ";
            find->duplicate_error();
            throw 2;
        }

        // another output variable
        ST = new symbol(t, ST);
        ST->init_output();
    }
}

void process_latches(lexer &L, symbol* &ST)
{
    //
    std::cerr << "latches not implemented\n";
}

expr* parse_product(lexer &L, symbol* &ST, const unsigned num) {
    // this is used for parsing a single line of covers
    token t;
    L.consume(t);
    expr* E;
    product* P = new product;
    for (unsigned i=0; i<num-1; i++) {
        if (t.getAttr()[i] == '1') {
            E = new term(ST->get_nsymbol(num-1-i));
            P->push(E);
            E = 0;
        } else if (t.getAttr()[i] == '0') {
            E = new term(ST->get_nsymbol(num-1-i));
            E->complement();
            P->push(E);
            E = 0;
        } else {
            continue;
        }
    }
    return P;
}

expr* process_covers(lexer &L, symbol* &ST, const unsigned num){
    // for A B ... C D, we know the idents in ST (linked list) is 
    //  in order of D C ... B A
    
    // if the number is 1, next token should be 0/1 or .names/.end
    if (num == 1) {
        if (L.peek().matches(token::SO)) {
            // 0 or 1?
            term* E = 0;
            E = new term(ST);
            E->constant();
            if (L.peek().getAttr() == "1") {
                E->complement();
            }
            return E;
        } else if (L.peek().matches(token::NAMES) || L.peek().matches(token::ENDMODEL)) {
            // .names/.end
            term* E = 0;
            E = new term(ST);
            E->constant();
            return E;
        } else {
            std::cerr << "got number of ident: " << L.getNum() << "\n";
            expect2(token::SO, token::NAMES, L.peek());
        }
    }

    // bool has_cover = 0;
    token t;
    expr* E = 0;
    sum* S = new sum;
    // now ready to parse the cover and set the expression
    for (;;) {
        t = L.peek();
        if (t.matches(token::NAMES) || t.matches(token::ENDMODEL)) {
            break;
        }
        // it should be cover to build product
        if (! t.matches(token::COVER)) {
            expected(token::COVER, t);
        }
        if (t.getAttr()[num-2] == 0 || t.getAttr()[num-1] != 0) {
            std::cerr << "number of digits should equal to the number of input gates\n";
            exit(1);
        }

        E = parse_product(L, ST, num);
        L.consume(t);
        if (! t.matches(token::SO)) {
            if (t.matches(token::NEWLINE)) {
                L.consume(t);
                if (!t.matches(token::SO)) expected(token::SO, t);
            } else {
                expected(token::SO, t);
            }
        }
        if (t.getAttr()[0] == 0 || t.getAttr()[1] != 0) {
            std::cerr << "number of digits should equal to 1 for single output\n";
            exit(1);
        }
        if (!S->knows_complemented()) {
            // 
            if (t.getAttr()[0] == '0') {
                E->complement();
                S->got_so();
            } else if (t.getAttr()[0] == '1') {
                S->got_so();
            } else {
                std::cerr << "unknown single output flag\n";
                exit(1);
            }
        } else {
            if ((t.getAttr()[0] == '0') ^ (S->is_complemented())) {
                std::cerr << "single output flag error\n";
                exit(1);
            }
        }
        if (t.getAttr()[0] == '0') E->complement();
        S->push(E);
        E = 0;
        L.consume(t);
        if (! t.matches(token::NEWLINE)) {
            expected(token::NEWLINE, t);
        }
    }
    return S;
}

void process_names(lexer &L, symbol* &ST) {
    // parse the idents in order and move them to front
    unsigned num_names = 0;
    token t;
    for (;;) {
        L.consume(t);

        if (t.matches(token::NEWLINE)) {
            if (L.getCover() == 'b') continue;
            symbol* lhs = ST;
            lhs->set_rhs(t.getLine(), process_covers(L, ST, num_names));
            // process_covers(L, ST, num_names);
            break;
        }

        if (! t.matches(token::IDENT)) {
            expected(token::IDENT, t);
        }

        // Here must be ident, move to front if found
        symbol* find = move_to_front(ST, t.getAttr());
        if (find) {
            ST = find;
            num_names ++;
            continue;
        }

        // another temp gate
        ST = new symbol(t, ST);
        ST->init_temp();
        num_names ++;
    }
}

/*
 * Main parsing process
 */
symbol* parse(lexer &L)
{
    // Really simple symbol table (linked list), should be fine
    // unless we get models with more than hundreds of variables
    symbol* ST = 0;

    // TBD: output roots
    // TBD: symbol table for inputs
    // TBD: symbol table for outputs

    token t;

    //
    // Loop over possible statements until end
    //
    for (;;) {
        L.consume(t);
#ifdef DEBUG_PARSER
        t.set_details();
        std::cerr << "consumed " << t << "\n";
#endif
        // Process special model name stmt
        if (t.matches(token::MODEL)) {
            L.consume(t);
            if (! t.matches(token::IDENT))  expected(token::IDENT, t);
            std::cerr << "Processing model " << t.getAttr() << "\n";
            continue;
        }
        // End of this model
        if (t.matches(token::ENDMODEL) || t.matches(token::END)) break;

        // Process inputs variables
        if (t.matches(token::INPUTS)) {
            // if (saw_inputs) {
            //     std::cerr << "Error on line " << t.getLine() << ":\n    ";
            //     std::cerr << "Another .inputs statement?\n";
            //     throw 2;
            // }
            process_inputs(L, ST);
            // saw_inputs = true;
            continue;
        }

        // Process outputs variables
        if (t.matches(token::OUTPUTS)) {
            // if (saw_outputs) {
            //     std::cerr << "Error on line " << t.getLine() << ":\n    ";
            //     std::cerr << "Another .outputs statement?\n";
            //     throw 2;
            // }
            process_outputs(L, ST);
            // saw_outputs = true;
            continue;
        }

        // Process Latches
        if (t.matches(token::LATCH)) {
            process_latches(L, ST);
            continue;
        }

        if (t.matches(token::WIRE)) {
            std::cerr << ".wire_load_slope not implemented yet, sorry\n";
            continue;
        }

        // Process expressions 
        if (t.matches(token::NAMES)) {
            process_names(L, ST);
            continue;
        }
    }

    return ST;
}
