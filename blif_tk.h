#ifndef BLIF_TK_H
#define BLIF_TK_H

#include <iostream>

class token {
    public:
        enum type {
            END         = 0,

            NEWLINE     = 100,

            IDENT       = 200,
            COVER       = 300,
            SO          = 301,

            // Keywords
            MODEL       = 400,
            INPUTS      = 401,
            OUTPUTS     = 402,
            CLOCK       = 403,
            LATCH       = 404,
            NAMES       = 405,
            ENDMODEL    = 406,

            WIRE        = 500

        };
    private:
        unsigned lineno;
        type tokenID;
        std::string attribute;

        static bool show_details;
    public:
        token();    

        inline operator bool() const { return tokenID != END; }

        inline bool matches(type t) const { return t == tokenID; }
        inline type getId() const { return tokenID; }
        inline unsigned getLine() const { return lineno; }
        inline const std::string& getAttr() const { return attribute; }

        static inline void setDetails() { show_details = true; }

        std::ostream& show(std::ostream &s) const;
        static std::ostream& showType(std::ostream &s, type t);

        friend class lexer;
};

#endif