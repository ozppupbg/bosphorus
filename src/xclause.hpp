/*****************************************************************************
Copyright (C) 2016  Security Research Labs
Copyright (C) 2018  Mate Soos, Davin Choo, Kian Ming A. Chai, DSO National Laboratories

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
***********************************************/

#ifndef _CLAUSES_H_
#define _CLAUSES_H_

#include <limits>
#include <vector>
#include "assert.h"
#include "bosphorus/solvertypesmini.hpp"


namespace BLib {

using std::vector;

class XClause: public GenericClause<uint32_t>
{
   public:
    XClause(const vector<uint32_t>& _vars, bool _rhs) : vars(_vars), rhs(_rhs)
    {
    }

    const vector<uint32_t>& getVars() const
    {
        return vars;
    }

    bool getRhs() const
    {
        return rhs;
    }

    size_t size() const
    {
        return vars.size();
    }

    bool empty() const
    {
        return vars.empty();
    }

    std::vector<uint32_t> getClause() const
    {
        assert(!vars.empty));
        return vars;
    }

    friend std::ostream& operator<<(std::ostream& os, const XClause& xcl);

    vector<uint32_t> vars;
    bool rhs;
};

inline std::ostream& operator<<(std::ostream& os, const XClause& xcl)
{
    //Empty is special
    if (xcl.size() == 0) {
        if (xcl.rhs == 1) {
            os << "0" << std::endl;
        }
        return os;
    }

    os << "x";
    if (!xcl.rhs) {
        os << "-";
    }
    for (const auto it: xcl.vars) {
        os << (it+1) << " ";
    }
    os << "0" << std::endl;

    return os;
}

}

#endif //_CLAUSES_H_
