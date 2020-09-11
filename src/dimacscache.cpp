/*****************************************************************************
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

#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <complex>

#include "dimacscache.hpp"

using std::cout;
using std::endl;

using namespace BLib;

void DIMACSCache::addClause(const Lit* lits, const uint32_t size)
{
    clauses.push_back(vector<Lit>());
    const size_t at = clauses.size()-1;
    clauses[at].lits.resize(size);
    std::copy(lits, lits+size, clauses[at].lits.data());

    for(uint32_t i = 0; i < size; i ++) {
        maxVar = std::max<uint32_t>(maxVar, lits[i].var()+1);
    }
}

void DIMACSCache::addClause(const Clause& cls)
{
    clauses.push_back(cls);
    for(auto lit : cls.getLits()) {
        maxVar = std::max<uint32_t>(maxVar, lit.var()+1);
    }
}

DIMACSCache::DIMACSCache(const char* _fname)
{
    if (_fname == fname)
        return;
    fname = _fname;

    maxVar = 0;
    clauses.clear();

    std::ifstream ifs;
    std::string temp;
    std::string x;
    ifs.open(fname);
    if (!ifs) {
        cout << "ERROR: Problem opening file '" << fname << "' for reading\n";
        exit(-1);
    }

    vector<Lit> lits;
    while (std::getline(ifs, temp)) {
        if (temp.length() == 0 || temp[0] == 'p' || temp[0] == 'c') {
            continue;
        } else if (temp[0] == 'x') {
            cout << "ERROR: xor clause found in CNF, we cannot deal with that"
                 << endl;
            exit(-1);
        } else {
            std::istringstream iss(temp);
            lits.clear();
            while (iss.good() && !iss.eof()) {
                iss >> x;
                int v = stoi(x);
                if (v == 0) {
                    clauses.push_back(Clause(lits));
                    break;
                } else {
                    lits.push_back(Lit(std::abs(v) - 1, v < 0));
                }
                maxVar = std::max<uint32_t>(maxVar, std::abs(stoi(x)));
            }
        }
    }
}
