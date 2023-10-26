#ifndef BLIF_LEXER_H
#define BLIF_LEXER_H

#include "blif_tk.h"

/*
 * Brilliantly-designed, perfect in every way,
 * self-contained lexer class for splitting the input stream(s)
 * into tokens for use by the parser(s).
 */
class lexer {
    private:
        class lexeme {
                char* buffer;
                const unsigned bufmax;
                unsigned p;
                bool truncated;
            public:
                lexeme(unsigned bmax);
                ~lexeme();

                inline char start(char c) {
                    truncated = false;
                    p = 1;
                    return buffer[0] = c;
                }

                inline char pop() {
                    if (p) return buffer[--p];
                    return 0;
                }

                inline void append(char c) {
                    if (p < bufmax) {
                        buffer[p++] = c;
                    } else {
                        truncated = true;
                    }
                }

                inline void finish() {
                    buffer[p] = 0;
                }

                inline bool is_truncated() const {
                    return truncated;
                }

                inline const char* get() const {
                    return buffer;
                }

                inline bool matches(const char* text) const {
                    return (0==strcmp(buffer, text));
                }
            private:
                lexeme(const lexeme&) = delete;
                void operator=(const lexeme&) = delete;
        };
    private:
        std::istream& fin;
        token next_tok;
        lexeme text;
        int num_inputs; // number of inputs consumed
        int num_id;     // number of identifiers consumed
        char cover;     // 'i': ident tokens; 'c': cover token; 'o': single out token
    public:
        lexer(std::istream& _fin);

        // Cleanup
        ~lexer();

        // What's the next token
        inline const token& peek() const {
            return next_tok;
        }

        // Consume and discard the next token
        inline void consume() {
            scan_token();
        }

        // Consume the next token and store it in t
        inline void consume(token &t) {
            t = next_tok;
            scan_token();
        }

        inline token::type getID() const {
            return next_tok.getId();
        }

        inline void incNumInputs() {
            num_inputs++;
        }

        inline const int getNumInputs() const {
            return num_inputs;
        }

        inline const int getNum() const {
            return num_id;
        }

        inline const char getCover() const {
            return cover;
        }

    private:
        lexer(const lexer&) = delete;
        void operator=(const lexer&) = delete;

    private:
        void scan_token();

        void ignore_comment();
        void consume_keyword();
        void consume_ident();
        void consume_cover();

        void IllegalChar(char c);

        static inline bool is_ident_char(char c) {
            switch (c) {
                case 'A':
                case 'B':
                case 'C':
                case 'D':
                case 'E':
                case 'F':
                case 'G':
                case 'H':
                case 'I':
                case 'J':
                case 'K':
                case 'L':
                case 'M':
                case 'N':
                case 'O':
                case 'P':
                case 'Q':
                case 'R':
                case 'S':
                case 'T':
                case 'U':
                case 'V':
                case 'W':
                case 'X':
                case 'Y':
                case 'Z':

                case 'a':
                case 'b':
                case 'c':
                case 'd':
                case 'e':
                case 'f':
                case 'g':
                case 'h':
                case 'i':
                case 'j':
                case 'k':
                case 'l':
                case 'm':
                case 'n':
                case 'o':
                case 'p':
                case 'q':
                case 'r':
                case 's':
                case 't':
                case 'u':
                case 'v':
                case 'w':
                case 'x':
                case 'y':
                case 'z':

                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9':

                case ':':
                case '^':
                case '%':
                case '[':
                case ']':
                case '(':
                case ')':
                case '_':
                case '.':
                case '/':
                case '<':
                case '>':
                // case '-':
                            return true;

                default:    return false;
            }
        }
};

#endif