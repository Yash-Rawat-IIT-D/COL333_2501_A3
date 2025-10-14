#include <bits/stdc++.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include "utils.hpp"
using namespace std;

// Grid directions: 0 is reserved for source/sink ports, 1..4 are the four neighbours.
const int DIR_RIGHT = 1;
const int DIR_UP    = 2;
const int DIR_LEFT  = 3;
const int DIR_DOWN  = 4;

struct Literal {
    int id;
    bool pos;

    Literal(int v, bool p = true) : id(v), pos(p) {}

    Literal neg() const { return Literal(id, !pos); }
    int toInt() const { return pos ? id : -id; }
};

struct Clause {
    vector<int> lits;

    Clause() = default;
    Clause(initializer_list<Literal> init) {
        for (const auto &lit : init) lits.push_back(lit.toInt());
    }

    void addLiteral(const Literal &lit) { lits.push_back(lit.toInt()); }
    bool empty() const { return lits.empty(); }
};

static inline string occName(int i, int j, int k) {
    ostringstream ss;
    ss << "X_" << i << "_" << j << "_" << k;
    return ss.str();
}

static inline string inName(int i, int j, int k, int dir) {
    ostringstream ss;
    ss << "IN_" << i << "_" << j << "_" << k << "_" << dir;
    return ss.str();
}

static inline string outName(int i, int j, int k, int dir) {
    ostringstream ss;
    ss << "OUT_" << i << "_" << j << "_" << k << "_" << dir;
    return ss.str();
}

static inline string turnName(int i, int j, int k) {
    ostringstream ss;
    ss << "TURN_" << i << "_" << j << "_" << k;
    return ss.str();
}

static inline string turnSeqName(int k, int idx, int t) {
    ostringstream ss;
    ss << "TS_" << k << "_" << idx << "_" << t;
    return ss.str();
}

class SATEncoder {
  private:
    MetroMap &metro_map;
    int next_var;
    vector<string> var_to_literal;
    unordered_map<string, int> literal_to_var;

    int N, M, K, J;

    vector<vector<int>> line_turn_vars;
    vector<Clause> clauses;

    int getVar(const string &name) {
        auto it = literal_to_var.find(name);
        if (it != literal_to_var.end()) return it->second;
        int id = next_var++;
        literal_to_var[name] = id;
        var_to_literal.push_back(name);
        return id;
    }

    Literal litByName(const string &name) { return Literal(getVar(name)); }

    void addClause(const Clause &clause) { clauses.push_back(clause); }

    void addUnit(const Literal &lit) {
        Clause c;
        c.addLiteral(lit);
        addClause(c);
    }

    void addImplication(const Literal &a, const Literal &b) {
        Clause c;
        c.addLiteral(a.neg());
        c.addLiteral(b);
        addClause(c);
    }

    void addAtLeastOne(const vector<Literal> &lits) {
        if (lits.empty()) return;
        Clause c;
        for (const auto &lit : lits) c.addLiteral(lit);
        addClause(c);
    }

    void addAtMostOne(const vector<Literal> &lits) {
        for (size_t i = 0; i < lits.size(); ++i) {
            for (size_t j = i + 1; j < lits.size(); ++j) {
                Clause c;
                c.addLiteral(lits[i].neg());
                c.addLiteral(lits[j].neg());
                addClause(c);
            }
        }
    }

    void addExactlyOne(const vector<Literal> &lits) {
        if (lits.empty()) return;
        addAtLeastOne(lits);
        addAtMostOne(lits);
    }

    void addGuardedAtLeastOne(const Literal &guard, const vector<Literal> &lits) {
        Clause c;
        c.addLiteral(guard.neg());
        for (const auto &lit : lits) c.addLiteral(lit);
        addClause(c);
    }

    bool inBounds(int i, int j) const {
        return (0 <= i && i < M && 0 <= j && j < N);
    }

    bool step(int i, int j, int dir, int &ni, int &nj) const {
        ni = i;
        nj = j;
        switch (dir) {
            case DIR_RIGHT: nj = j + 1; break;
            case DIR_LEFT:  nj = j - 1; break;
            case DIR_UP:    ni = i - 1; break;
            case DIR_DOWN:  ni = i + 1; break;
            default: return false;
        }
        return inBounds(ni, nj);
    }

    void encodeCellAMO() {
        // One metro per cell using pairwise AMO (K ≤ 10 in inputs so this stays cheap).
        for (int i = 0; i < M; ++i) {
            for (int j = 0; j < N; ++j) {
                vector<Literal> occ;
                occ.reserve(K);
                for (int k = 0; k < K; ++k) {
                    occ.push_back(litByName(occName(i, j, k)));
                }
                addAtMostOne(occ);
            }
        }
    }

    void encodeLineFlow(int k) {
        auto &starts = metro_map.getLineStarts(k);
        auto &ends   = metro_map.getLineEnds(k);
        int src_i = starts[0].second;
        int src_j = starts[0].first;
        int sink_i = ends[0].second;
        int sink_j = ends[0].first;

        for (int i = 0; i < M; ++i) {
            for (int j = 0; j < N; ++j) {
                bool is_source = (i == src_i && j == src_j);
                bool is_sink   = (i == sink_i && j == sink_j);

                Literal occ = litByName(occName(i, j, k));
                Literal turn = litByName(turnName(i, j, k));
                addImplication(turn, occ);  // A turn can only appear on an occupied cell.
                line_turn_vars[k].push_back(turn.id);

                vector<pair<int, Literal>> incomingPorts;
                vector<pair<int, Literal>> outgoingPorts;

                // Zero ports only live on source/sink cells.
                if (is_source) {
                    Literal in_zero = litByName(inName(i, j, k, 0));
                    addUnit(in_zero);                 // Start is anchored to the virtual source.
                    addImplication(in_zero, occ);     // Occupancy follows from using the source.
                    incomingPorts.push_back({0, in_zero});
                }
                if (is_sink) {
                    Literal out_zero = litByName(outName(i, j, k, 0));
                    addUnit(out_zero);                 // Sink is anchored to the virtual sink.
                    addImplication(out_zero, occ);     // Occupancy follows from using the sink.
                    outgoingPorts.push_back({0, out_zero});
                }

                // Real grid directions.
                for (int dir = DIR_RIGHT; dir <= DIR_DOWN; ++dir) {
                    int ni, nj;
                    if (!step(i, j, dir, ni, nj)) continue;

                    Literal in_lit  = litByName(inName(i, j, k, dir));
                    Literal out_lit = litByName(outName(i, j, k, dir));
                    incomingPorts.push_back({dir, in_lit});
                    outgoingPorts.push_back({dir, out_lit});

                    // A single side cannot be used simultaneously for entry and exit.
                    Clause block_same;
                    block_same.addLiteral(in_lit.neg());
                    block_same.addLiteral(out_lit.neg());
                    addClause(block_same);

                    // Port usage implies occupancy of the cell itself.
                    addImplication(in_lit, occ);   // (¬IN → X_ijk)
                    addImplication(out_lit, occ);  // (¬OUT → X_ijk)

                    // Synchronise with the neighbour.
                    int opp = getOppositeDirIndex(dir);
                    Literal neigh_in  = litByName(inName(ni, nj, k, opp));
                    Literal neigh_out = litByName(outName(ni, nj, k, opp));
                    Literal neigh_occ = litByName(occName(ni, nj, k));

                    // Leaving towards (ni,nj) forces that cell to register the entry and be occupied.
                    addImplication(out_lit, neigh_in);   // (¬OUT_ijk_d → IN_ni_nj_k_opp)
                    addImplication(out_lit, neigh_occ);  // (¬OUT_ijk_d → X_ni_nj_k)

                    // If we claim to arrive from (ni,nj) then the neighbour must really send us here.
                    addImplication(in_lit, neigh_out);   // (¬IN_ijk_d → OUT_ni_nj_k_opp)
                    addImplication(in_lit, neigh_occ);   // (¬IN_ijk_d → X_ni_nj_k)
                }

                // Prepare lists without the virtual ports for the usual degree constraints.
                vector<Literal> incomingReal;
                vector<Literal> outgoingReal;
                for (auto &[dir, lit] : incomingPorts) {
                    if (dir != 0) incomingReal.push_back(lit);
                }
                for (auto &[dir, lit] : outgoingPorts) {
                    if (dir != 0) outgoingReal.push_back(lit);
                }

                if (is_source) {
                    addUnit(occ);  // Source cell must belong to the line.

                    // No other entry is allowed into the source.
                    for (const auto &lit : incomingReal) {
                        Clause forbid_entry;
                        forbid_entry.addLiteral(lit.neg());
                        addClause(forbid_entry);        // IN ports (dir≠0) are forced false.
                    }

                    if (outgoingReal.empty()) {
                        Clause impossible;
                        impossible.addLiteral(occ.neg());   // Isolated source => infeasible instance.
                        addClause(impossible);
                    } else {
                        addExactlyOne(outgoingReal);    // Exactly one real edge leaves the source.
                    }
                    Clause no_turn;
                    no_turn.addLiteral(turn.neg());
                    addClause(no_turn);                 // Source is never counted as a turn.
                    continue;
                }

                if (is_sink) {
                    addUnit(occ);  // Sink cell must belong to the line.

                    // No real edge may leave the sink.
                    for (const auto &lit : outgoingReal) {
                        Clause forbid_exit;
                        forbid_exit.addLiteral(lit.neg());
                        addClause(forbid_exit);         // OUT ports (dir≠0) are forced false.
                    }

                    if (incomingReal.empty()) {
                        Clause impossible;
                        impossible.addLiteral(occ.neg());   // Isolated sink => infeasible instance.
                        addClause(impossible);
                    } else {
                        addExactlyOne(incomingReal);    // Exactly one real edge enters the sink.
                    }
                    Clause no_turn;
                    no_turn.addLiteral(turn.neg());
                    addClause(no_turn);                 // Sink is never counted as a turn.
                    continue;
                }

                // Interior cells: occupancy drives the presence of ports.
                if (incomingReal.empty() || outgoingReal.empty()) {
                    // Dead-ends cannot appear on a valid interior cell.
                    Clause forbid;
                    forbid.addLiteral(occ.neg());
                    addClause(forbid);
                    Clause no_turn;
                    no_turn.addLiteral(turn.neg());
                    addClause(no_turn);
                    continue;
                }

                addGuardedAtLeastOne(occ, incomingReal);  // If occupied, pick at least one entry.
                addGuardedAtLeastOne(occ, outgoingReal);  // If occupied, pick at least one exit.
                addAtMostOne(incomingReal);               // At most one real entry port.
                addAtMostOne(outgoingReal);               // At most one real exit port.

                // Encode the exact turning behaviour.
                for (auto &[dir_in, in_lit] : incomingPorts) {
                    if (dir_in == 0) continue;
                    for (auto &[dir_out, out_lit] : outgoingPorts) {
                        if (dir_out == 0) continue;
                        Clause c;
                        c.addLiteral(in_lit.neg());
                        c.addLiteral(out_lit.neg());
                        if (dir_in == getOppositeDirIndex(dir_out)) {
                            c.addLiteral(turn.neg());   // Straight segment forces TURN = false.
                        } else {
                            c.addLiteral(turn);         // A bend forces TURN = true.
                        }
                        addClause(c);
                    }
                }
            }
        }
    }

    void enforceTurnBudget(int k) {
        if (J < 0) return;
        auto &turn_ids = line_turn_vars[k];
        if (turn_ids.empty()) return;

        vector<Literal> turn_lits;
        turn_lits.reserve(turn_ids.size());
        for (int id : turn_ids) turn_lits.emplace_back(id);

        if (J == 0) {
            for (auto &lit : turn_lits) {
                Clause c{lit.neg()};
                addClause(c);                  // No turns allowed.
            }
            return;
        }
        if ((int)turn_lits.size() <= J) return;  // Fewer candidates than the budget.

        int n = turn_lits.size();
        vector<vector<int>> seq_id(n + 1, vector<int>(J + 1, 0));
        for (int i = 1; i <= n; ++i) {
            for (int t = 1; t <= J; ++t) {
                seq_id[i][t] = getVar(turnSeqName(k, i, t));
            }
        }

        {
            Clause c;
            c.addLiteral(turn_lits[0].neg());
            c.addLiteral(Literal(seq_id[1][1]));
            addClause(c);                      // turn_1 → s_1,1
        }
        for (int t = 2; t <= J; ++t) {
            Clause c{Literal(seq_id[1][t]).neg()};
            addClause(c);                      // s_1,t = false for t > 1
        }

        for (int i = 2; i <= n; ++i) {
            Clause c1;
            c1.addLiteral(turn_lits[i - 1].neg());
            c1.addLiteral(Literal(seq_id[i][1]));
            addClause(c1);                     // turn_i → s_i,1

            Clause c2;
            c2.addLiteral(Literal(seq_id[i - 1][1]).neg());
            c2.addLiteral(Literal(seq_id[i][1]));
            addClause(c2);                     // s_{i-1,1} → s_{i,1}

            for (int t = 2; t <= J; ++t) {
                Clause carry;
                carry.addLiteral(Literal(seq_id[i - 1][t]).neg());
                carry.addLiteral(Literal(seq_id[i][t]));
                addClause(carry);              // s_{i-1,t} → s_{i,t}

                Clause rise;
                rise.addLiteral(turn_lits[i - 1].neg());
                rise.addLiteral(Literal(seq_id[i - 1][t - 1]).neg());
                rise.addLiteral(Literal(seq_id[i][t]));
                addClause(rise);               // (turn_i ∧ s_{i-1,t-1}) → s_{i,t}
            }

            Clause cap;
            cap.addLiteral(turn_lits[i - 1].neg());
            cap.addLiteral(Literal(seq_id[i - 1][J]).neg());
            addClause(cap);                    // turn_i → ¬s_{i-1,J}
        }
    }

    void encodeTurnBudget() {
        for (int k = 0; k < K; ++k) {
            enforceTurnBudget(k);
        }
    }

    void encodePopularCities() {
        int P = metro_map.getPopularCitiesCount();
        if (P <= 0) return;

        auto &groups = metro_map.getPopularCities();
        for (int p = 0; p < P; ++p) {
            Clause coverage;
            for (auto &cell : groups[p]) {
                int col = cell.first;
                int row = cell.second;
                if (!inBounds(row, col)) continue;
                for (int k = 0; k < K; ++k) {
                    coverage.addLiteral(litByName(occName(row, col, k)));
                }
            }
            if (!coverage.empty()) addClause(coverage);
        }
    }

  public:
    SATEncoder(MetroMap &m) : metro_map(m) {
        next_var = 1;
        N = metro_map.getColNum();
        M = metro_map.getRowNum();
        K = metro_map.getLineNum();
        J = metro_map.getTurnLimit();
        line_turn_vars.resize(K);
    }

    int variableCount() const { return next_var - 1; }
    size_t clauseCount() const { return clauses.size(); }

    const vector<Clause>& getClauses() const { return clauses; }
    const vector<string>& getVariableNames() const { return var_to_literal; }

    void encode() {
        encodeCellAMO();
        for (int k = 0; k < K; ++k) {
            encodeLineFlow(k);
        }
        encodeTurnBudget();
        encodePopularCities();
    }

    bool outputDIMACS(const string &filename) const {
        ofstream out(filename.c_str());
        if (!out.is_open()) {
            cerr << "Error: Cannot open DIMACS file for writing: " << filename << endl;
            return false;
        }
        out << "c Re-implemented SAT encoding" << '\n';
        out << "p cnf " << variableCount() << " " << clauseCount() << '\n';
        for (const auto &clause : clauses) {
            for (int lit : clause.lits) out << lit << ' ';
            out << "0\n";
        }
        return true;
    }

    bool outputVariableMapping(const string &filename) const {
        ofstream out(filename.c_str());
        if (!out.is_open()) {
            cerr << "Error: Cannot open variable map file for writing: " << filename << endl;
            return false;
        }
        for (size_t idx = 0; idx < var_to_literal.size(); ++idx) {
            out << (idx + 1) << ' ' << var_to_literal[idx] << '\n';
        }
        return true;
    }
};

int main(int argc, char* argv[]) {
    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " <input.metromap> <output.cnf>" << endl;
        return 1;
    }

    const string input_path = argv[1];
    const string cnf_path   = argv[2];

    ifstream input_stream(input_path.c_str());
    if (!input_stream.is_open()) {
        cerr << "Error: Unable to open input file " << input_path << endl;
        return 1;
    }

    MetroMap metro_map = parseInputFile(input_stream);
    input_stream.close();

    SATEncoder encoder(metro_map);
    encoder.encode();

    if (!encoder.outputDIMACS(cnf_path)) return 1;

    const string map_path = cnf_path + ".map";
    if (!encoder.outputVariableMapping(map_path)) return 1;

    return 0;
}
