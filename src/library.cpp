/*****************************************************************************
Copyright (C) 2016  Security Research Labs
Copyright (C) 2018  Davin Choo, Kian Ming A. Chai, DSO National Laboratories
Copyright (C) 2018  Mate Soos

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

#include "library.h"

#include "elimlin.hpp"
#include "extendedlinearization.hpp"
#include "simplifybysat.hpp"

Library::~Library()
{
    delete polybori_ring;
}

void Library::check_library_in_use()
{
    if (read_in_data) {
        cout << "ERROR: data already read in."
             << " You can only read in *one* ANF or CNF per library creation"
             << endl;
        exit(-1);
    }
    read_in_data = true;

    assert(extra_clauses.empty());
    //     assert(anf == nullptr);
    //     assert(cnf = nullptr);
    assert(polybori_ring == nullptr);
}

ANF* Library::read_anf(const char* fname)
{
    check_library_in_use();

    // Find out maxVar in input ANF file
    size_t maxVar = ANF::readFileForMaxVar(fname);

    // Construct ANF
    // ring size = maxVar + 1, because ANF variables start from x0
    polybori_ring = new BoolePolyRing(maxVar + 1);
    ANF* anf = new ANF(polybori_ring, config);
    anf->readFile(fname);
    return anf;
}

ANF* Library::read_cnf(const char* fname)
{
    check_library_in_use();

    DIMACSCache dimacs_cache(fname);
    const vector<Clause>& orig_clauses(dimacs_cache.getClauses());
    size_t maxVar = dimacs_cache.getMaxVar();

    // Chunk up by L positive literals. L = config.cutNum
    vector<Clause> chunked_clauses;
    for (auto clause : orig_clauses) {
        // small already
        if (clause.size() <= config.cutNum) {
            chunked_clauses.push_back(clause);
            continue;
        }

        // Count number of positive literals
        size_t positive_count = 0;
        for (const Lit& l : clause.getLits()) {
            positive_count += !l.sign();
        }
        if (positive_count <= config.cutNum) {
            chunked_clauses.push_back(clause);
            continue;
        }

        // need to chop it up
        if (config.verbosity >= 5)
            cout << clause << " --> ";

        vector<Lit> collect;
        size_t count = 0;
        for (auto l : clause.getLits()) {
            collect.push_back(l);
            count += !l.sign();
            if (count > config.cutNum) {
                // Create new aux
                Lit aux = Lit(maxVar, false);
                maxVar++;

                // replace the last one when a negative auxilary
                Lit prev = collect.back();
                collect.back() = ~aux;

                extra_clauses.push_back(collect);
                chunked_clauses.push_back(collect);
                if (config.verbosity >= 5)
                    cout << collect << " and ";

                collect.clear();
                collect.push_back(aux);
                collect.push_back(prev);
                count = 2; // because both aux and prev are positives!
            }
        } //idx
        if (!collect.empty()) {
            extra_clauses.push_back(collect);
            chunked_clauses.push_back(collect);
            if (config.verbosity >= 5)
                cout << collect;
        }
        if (config.verbosity >= 5)
            cout << endl;
    } //for

    // Construct ANF
    if (config.verbosity >= 4) {
        cout << "c Constructing CNF with " << maxVar << " variables." << endl;
    }
    // ring size = maxVar, because CNF variables start from 1
    polybori_ring = new BoolePolyRing(maxVar);
    ANF* anf = new ANF(polybori_ring, config);
    for (auto clause : chunked_clauses) {
        BoolePolynomial poly(1, *polybori_ring);
        for (const Lit& l : clause.getLits()) {
            BoolePolynomial alsoAdd(*polybori_ring);
            if (!l.sign())
                alsoAdd = poly;
            poly *= BooleVariable(l.var(), *polybori_ring);
            poly += alsoAdd;
        }
        anf->addBoolePolynomial(poly);
        if (config.verbosity >= 5) {
            cout << clause << " -> " << poly << endl;
        }
    }

    return anf;
}

void Library::write_anf(const char* fname, const ANF* anf)
{
    std::ofstream ofs;
    ofs.open(fname);
    if (!ofs) {
        std::cerr << "c Error opening file \"" << fname << "\" for writing\n";
        exit(-1);
    } else {
        ofs << "c Executed arguments: " << config.executedArgs << endl;
        ofs << *anf << endl;
    }
    ofs.close();
}

void Library::write_cnf(const char* input_cnf_fname,
                        const char* output_cnf_fname, const ANF* anf,
                        const vector<BoolePolynomial>& learnt)
{
    CNF* cnf = NULL;
    if (input_cnf_fname == NULL) {
        cnf = cnf_from_anf_and_cnf(input_cnf_fname, anf);
    } else {
        cnf = anf_to_cnf(anf);
    }

    std::ofstream ofs;
    ofs.open(output_cnf_fname);
    if (!ofs) {
        std::cerr << "c Error opening file \"" << output_cnf_fname
                  << "\" for writing\n";
        exit(-1);
    } else {
        if (config.writecomments) {
            ofs << "c Executed arguments: " << config.executedArgs << endl;
            for (size_t i = 0; i < anf->getRing().nVariables(); i++) {
                Lit l = anf->getReplaced(i);
                BooleVariable v(l.var(), anf->getRing());
                if (l.sign()) {
                    ofs << "c MAP " << i + 1 << " = -" << cnf->getVarForMonom(v)
                        << endl;
                } else {
                    ofs << "c MAP " << i + 1 << " = " << cnf->getVarForMonom(v)
                        << endl;
                }
            }
            for (size_t i = 0; i < cnf->getNumVars(); ++i) {
                const BooleMonomial mono = cnf->getMonomForVar(i);
                if (mono.deg() > 0)
                    assert(i == cnf->getVarForMonom(mono));
                if (mono.deg() > 1)
                    ofs << "c MAP " << i + 1 << " = " << mono << endl;
            }
        }

        cnf->print_without_header(ofs);

        ofs << "c Learnt " << learnt.size() << " fact(s)\n";
        if (config.writecomments) {
            for (const BoolePolynomial& poly : learnt) {
                ofs << "c " << poly << endl;
            }
        }
    }
    ofs.close();
}

CNF* Library::anf_to_cnf(const ANF* anf)
{
    double convStartTime = cpuTime();
    CNF* cnf = new CNF(*anf, config);
    if (config.verbosity >= 2) {
        cout << "c [CNF conversion] in " << (cpuTime() - convStartTime)
             << " seconds.\n";
        cnf->printStats();
    }
    return cnf;
}

CNF* Library::cnf_from_anf_and_cnf(const char* cnf_fname, const ANF* anf)
{
    double convStartTime = cpuTime();
    CNF* cnf = new CNF(cnf_fname, *anf, extra_clauses, config);
    if (config.verbosity >= 2) {
        cout << "c [CNF enhancing] in " << (cpuTime() - convStartTime)
             << " seconds.\n";
        cnf->printStats();
    }
    return cnf;
}

Solution Library::simplify(ANF* anf, const char* orig_cnf_file,
                           vector<BoolePolynomial>& loop_learnt)
{
    cout << "c [boshp] Running iterative simplification..." << endl;
    bool timeout = (cpuTime() > config.maxTime);
    if (timeout) {
        if (config.verbosity) {
            cout << "c Timeout before learning" << endl;
        }
        return Solution();
    }

    double loopStartTime = cpuTime();
    // Perform initial propagation to avoid needing >= 2 iterations
    anf->propagate();
    timeout = (cpuTime() > config.maxTime);

    Solution solution;
    bool changes[] = {true, true, true}; // any changes for the strategies
    size_t waits[] = {0, 0, 0};
    size_t countdowns[] = {0, 0, 0};
    uint32_t numIters = 0;
    unsigned char subIters = 0;
    CNF* cnf = NULL;
    SimplifyBySat* sbs = NULL;

    while (!timeout && anf->getOK() && solution.ret == l_Undef &&
           (std::accumulate(changes, changes + 3, false,
                            std::logical_or<bool>()) ||
            numIters < config.minIter)) {
        cout << "c [iter-simp] ------ Iteration " << std::fixed << std::dec
             << (int)numIters << endl;

        static const char* strategy_str[] = {"XL", "ElimLin", "SAT"};
        const double startTime = cpuTime();
        int num_learnt = 0;

        if (countdowns[subIters] > 0) {
            cout << "c [" << strategy_str[subIters] << "] waiting for "
                 << countdowns[subIters] << " iteration(s)." << endl;
        } else {
            const size_t prevsz = loop_learnt.size();
            switch (subIters) {
                case 0:
                    if (config.doXL) {
                        if (!extendedLinearization(config, anf->getEqs(),
                                                   loop_learnt)) {
                            anf->setNOTOK();
                        } else {
                            for (size_t i = prevsz; i < loop_learnt.size(); ++i)
                                num_learnt +=
                                    anf->addBoolePolynomial(loop_learnt[i]);
                        }
                    }
                    break;
                case 1:
                    if (config.doEL) {
                        if (!elimLin(config, anf->getEqs(), loop_learnt)) {
                            anf->setNOTOK();
                        } else {
                            for (size_t i = prevsz; i < loop_learnt.size(); ++i)
                                num_learnt +=
                                    anf->addBoolePolynomial(loop_learnt[i]);
                        }
                    }
                    break;
                case 2:
                    if (config.doSAT) {
                        size_t no_cls = 0;
                        if (orig_cnf_file) {
                            if (cnf == NULL) {
                                assert(sbs == NULL);
                                cnf = new CNF(orig_cnf_file, *anf,
                                              extra_clauses, config);
                                sbs = new SimplifyBySat(*cnf, config);
                            } else {
                                no_cls = cnf->update();
                            }
                        } else {
                            delete cnf;
                            delete sbs;
                            cnf = new CNF(*anf, config);
                            sbs = new SimplifyBySat(*cnf, config);
                        }

                        num_learnt =
                            sbs->simplify(config.numConfl_lim,
                                          config.numConfl_inc, config.maxTime,
                                          no_cls, loop_learnt, *anf, solution);
                    }
                    break;
            }

            if (config.verbosity >= 2) {
                cout << "c [" << strategy_str[subIters] << "] learnt "
                     << num_learnt << " new facts in "
                     << (cpuTime() - startTime) << " seconds." << endl;
            }
        }

        // Determine if there are any changes to the system
        if (num_learnt <= 0) {
            changes[subIters] = false;
        } else {
            changes[subIters] = true;
            bool ok = anf->propagate();
            if (!ok) {
                if (config.verbosity >= 1)
                    cout << "c [ANF Propagation] is false\n";
            }
        }

        // Scheduling strategies
        if (changes[subIters]) {
            waits[subIters] = 0;
        } else {
            if (countdowns[subIters] > 0)
                --countdowns[subIters];
            else {
                static size_t series[] = {0, 1,  1,  2,  3,  5,
                                          8, 13, 21, 34, 55, 89};
                countdowns[subIters] = series[std::min(
                    waits[subIters], sizeof(series) / sizeof(series[0]) - 1)];
                ++waits[subIters];
            }
        }

        if (subIters < 2)
            ++subIters;
        else {
            ++numIters;
            subIters = 0;
            deduplicate(loop_learnt); // because this is a good time to do this
        }
        timeout = (cpuTime() > config.maxTime);
    }

    if (config.verbosity) {
        cout << "c [";
        if (timeout) {
            cout << "Timeout";
        }

        cout << " after " << numIters << '.' << (int)subIters
             << " iteration(s) in " << (cpuTime() - loopStartTime)
             << " seconds.]\n";
    }

    delete sbs;
    delete cnf;
    anf->contextualize(loop_learnt);
    return solution;
}

void Library::deduplicate(vector<BoolePolynomial>& learnt)
{
    vector<BoolePolynomial> dedup;
    ANF::eqs_hash_t hash;
    for (const BoolePolynomial& p : learnt) {
        if (hash.insert(p.hash()).second)
            dedup.push_back(p);
    }
    if (config.verbosity >= 3) {
        cout << "c [Dedup] " << learnt.size() << "->" << dedup.size() << endl;
    }
    learnt.swap(dedup);
}

void Library::set_config(const ConfigData& cfg)
{
    config = cfg;
}
