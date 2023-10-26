
#include "blif_lex.h"

#define MAX_LEXEME 1024

// #define DEBUG_LEXER

//
// ======================================================================
//

lexer::lexeme::lexeme(unsigned bmax) : bufmax(bmax)
{
    buffer = new char[bufmax+1];
}

lexer::lexeme::~lexeme()
{
    delete[] buffer;
}

//
// ======================================================================
//

lexer::lexer(std::istream& _fin) : fin(_fin), text(MAX_LEXEME)
{
    next_tok.lineno = 1;
    num_id = 0;
    cover = 0;
    scan_token();
}

lexer::~lexer()
{
}

void lexer::scan_token()
{
    //
    // Read from appropriate input stream, the next token
    // into lookaheads[0].
    //
    next_tok.attribute.clear();
    bool has_backslash = 0;

    for (;;) {
        int c = fin.get();
        if (EOF == c) {
            next_tok.tokenID = token::END;
            return;
        }

        switch (text.start(c)) {
            //
            // Skip whitespace
            //
            case ' ':   continue;
            case '\t':  continue;
            case '\r':  continue;
            case '\\':  
                        has_backslash = 1;
                        continue;
            case '\n':
                        ++ next_tok.lineno;
                        text.finish();
                        next_tok.tokenID = token::NEWLINE;
                        next_tok.attribute = "\\n";
                        if (cover == 'o') return;
                        if (cover == 'i' || cover == 'b') cover = 'c';
                        if (num_id == 1) cover = 'o';
                        if (has_backslash) cover = 'b';
                        return;

            /*
             * Single char symbols
             */
            case '#':   ignore_comment();
                        continue;

            case '.':   return consume_keyword();

            /*
             * Single output cover
             */
            case '1':
            case '0':
            case '-':   
                        if (cover == 'c') {
                            cover = 'o';
                            return consume_cover();
                        } 
                        if (cover == 'o') {
                            text.finish();
                            next_tok.tokenID = token::SO;
                            next_tok.attribute = std::string(text.get());
                            cover = 'c';
                            return;
                        }

            /*
             * IDENT, keyword, or illegal char
             */
            default:
                        if (is_ident_char(c) && (cover == 'i' || cover == 'b')) {
                            return consume_ident();
                        }
                        IllegalChar(c);
                        continue;
        }


    }   // infinite loop
}

void lexer::ignore_comment()
{
    // We've already consumed the #
    for (;;) {
        if ('\n' == fin.peek()) {
            fin.get();
            ++next_tok.lineno;
            return;
        }
        if (EOF == fin.get()) {
            return;
        }
    }
}

void lexer::consume_keyword()
{
    // We already have the first character.
    // Build up remaining chars
    int c;
    for (;;) {
        c = fin.peek();
        if (is_ident_char(c)) {
            text.append(fin.get());
            continue;
        }
        break;
    }
    text.finish();

    // Check for keywords
    // Could do a binary search, but there's not very many.

    if (text.matches(".model")) {
        next_tok.tokenID = token::MODEL;
        cover = 'i';
        return;
    }
    if (text.matches(".inputs")) {
        next_tok.tokenID = token::INPUTS;
        cover = 'i';
        return;
    }
    if (text.matches(".outputs")) {
        next_tok.tokenID = token::OUTPUTS;
        cover = 'i';
        return;
    }
    if (text.matches(".latch")) {
        next_tok.tokenID = token::LATCH;
        cover = 'i';
        return;
    }
    if (text.matches(".names")) {
        next_tok.tokenID = token::NAMES;
        num_id = 0;
        cover = 'i';
        return;
    }
    if (text.matches(".wire_load_slope")) {
        next_tok.tokenID = token::WIRE;
        cover = 'i';
        return;
    }
    if (text.matches(".end")) {
        next_tok.tokenID = token::ENDMODEL;
        return;
    }

    std::cerr << "Unsupported keyword '" << text.get() << "' on line " << next_tok.lineno << "\n";
    exit(1);
}

void lexer::consume_ident()
{
    // We already have the first character.
    // Build up remaining chars
    int c;
    for (;;) {
        c = fin.peek();
        if (is_ident_char(c)) {
            text.append(fin.get());
            continue;
        }
        break;
    }
    text.finish();
    num_id ++;
    next_tok.tokenID = token::IDENT;
    next_tok.attribute = std::string(text.get());
}

void lexer::consume_cover()
{
    // We already have the first character.
    // Build up remaing char(s)
    int c;
    for (;;) {
        c = fin.peek();
        if (c == '1' || c == '0' || c == '-') {
            text.append(fin.get());
            continue;
        }
        break;
    }
    text.finish();
    next_tok.tokenID = token::COVER;
    next_tok.attribute = std::string(text.get());
}

void lexer::IllegalChar(char c)
{
    char txt[2];
    txt[0] = c;
    txt[1] = 0;

    std::cerr << "Ignoring unexpected character '" << c << "' on line " << next_tok.lineno << "\n";
}

