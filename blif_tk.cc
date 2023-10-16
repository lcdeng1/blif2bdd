
#include "blif_tk.h"

bool token::show_details;

token::token()
{
    lineno = 0;
    tokenID = END;
    show_details = false;
}

std::ostream& token::show(std::ostream &s) const
{
    if (IDENT == tokenID) {
        if (show_details) s << "identifier ";
        show_details = false;
        return s << attribute;
    }
    show_details = false;
    return showType(s, tokenID);
}

std::ostream& token::showType(std::ostream &s, type t)
{
    switch (t) {
        case END:       return s << "End of file";

        case MODEL:     return s << ".model";
        case INPUTS:    return s << ".inputs";
        case OUTPUTS:   return s << ".outputs";
        case LATCH:     return s << ".latch";
        case ENDMODEL:  return s << ".end";

        case IDENT:     return s << "identifier";

        default:        return s << "???";
    }
}


