#include <bits/stdc++.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include "utils.hpp"
using namespace std;

// Y/y -> Rows,  X/x -> Columns 

const int EMPTY_DIR = 0;
const int RIGHT_DIR = 1;
const int UP_DIR = 2;
const int LEFT_DIR = 3;
const int DOWN_DIR = 4;

struct Literal {
    int id;     // variable ID (1-based)
    bool pos;     // true = positive literal, false = negated

    Literal(int v, bool p = true) : id(v), pos(p) {}

    // Return negation
    Literal neg() const { return Literal(id, !pos); }

    // Convert to DIMACS integer: positive = id, negative = -id
    int toInt() const { return pos ? id : -id; }

    // Optional: string representation for debugging
    std::string toString(const std::vector<std::string> &idToName) const {
        std::stringstream ss;
        if (!pos) ss << "-";
        if (!idToName.empty() && id - 1 < (int)idToName.size()) ss << idToName[id - 1];
        else ss << id;
        return ss.str();
    }
};

struct Clause {
    std::vector<int> lits;  // DIMACS literals

    Clause() = default;

    // Construct from Literal objects
    Clause(const std::vector<Literal> &l) {
        for (auto &lit : l) lits.push_back(lit.toInt());
    }

    void addLiteral(const Literal &lit) {
        lits.push_back(lit.toInt());
    }

    bool isEmpty() const { return lits.empty(); }

    // Optional: convert to DIMACS string
    std::string toString() const {
        std::stringstream ss;
        for (auto l : lits) ss << l << " ";
        ss << "0";
        return ss.str();
    }
};


string __occ_lit_str(int i, int j, int k) {
    ostringstream ss;
    ss << "X_" << i << "_" << j << "_" << k;
    return ss.str();
}

string __sinz_occ_lit_str(int i, int j, int d) {
    ostringstream ss;
    ss << "S_" << i << "_" << j << "_" << d;
    return ss.str();
}


string __in_lit_str(int i, int j, int k, int dir) {
    ostringstream ss;
    ss << "IN_" << i << "_" << j << "_" << k << "_" << dir;
    return ss.str();
}

string __out_lit_str(int i, int j, int k, int dir) {
    ostringstream ss;
    ss << "OUT_" << i << "_" << j << "_" << k << "_" << dir;
    return ss.str();
}

string __sinz_out_lit_str(int i, int j, int k, int d) {
    ostringstream ss;
    ss << "SO_" << i << "_" << j << "_" << k << "_" << d;
    return ss.str();
}

string __sinz_in_lit_str(int i, int j, int k, int d) {
    ostringstream ss;
    ss << "SI_" << i << "_" << j << "_" << k << "_" << d;
    return ss.str();
}

string __sinz_dir_lit_str(int i, int j, int k, int d, bool is_in) {
    return is_in ? __sinz_in_lit_str(i, j, k, d) : __sinz_out_lit_str(i, j, k, d);
}

string __turn_lit_str(int i, int j, int k) {
    ostringstream ss;
    ss << "TURN_" << i << "_" << j << "_" << k;
    return ss.str();
}

string __turn_seq_lit_str(int k, int idx, int t) {
    ostringstream ss;
    ss << "TS_" << k << "_" << idx << "_" << t;
    return ss.str();
}

class SATEncoder {
    private :
        // Access to MetroMap
        MetroMap &metro_map;

        // Variable Tracking
        int next_var;
        vector<string> var_to_literal; // 1-based indexing so locate at id-1
        unordered_map<string, int> literal_to_var;

        // Problem Parameters
        int N, M, K, J; // Columns, Rows, Lines, Turn Limit
    
        int getVar(const string &name) {
            auto it = literal_to_var.find(name);
            if (it != literal_to_var.end()) {
                return it->second;
            }
            int id = next_var++;
            literal_to_var[name] = id;
            var_to_literal.push_back(name); // Located at id-1
            return id;
        }

        vector<Clause> clauses;

        void addClause(const Clause &clause) {
            clauses.push_back(clause);
        }

        bool out_of_bounds(int i, int j, int dir) {
            switch (dir) {
            case RIGHT_DIR:
                return j + 1 >= N; // Out of bounds if moving right exceeds column limit
            case UP_DIR:
                return i - 1 < 0; // Out of bounds if moving up goes above row 0
            case LEFT_DIR:
                return j - 1 < 0; // Out of bounds if moving left goes below column 0
            case DOWN_DIR:
                return i + 1 >= M; // Out of bounds if moving down exceeds row limit
            default:
                return true; // Invalid direction
            }
        }

        bool out_of_bound_rev(int ni, int nj, int dir) {
            // To Check if (i,j) ---dir---> (ni,nj) is valid
            switch (dir) {
            case RIGHT_DIR:
                return nj - 1 < 0; // Out of bounds if moving left goes below column 0
            case UP_DIR:
                return ni + 1 >= M; // Out of bounds if moving down exceeds row limit
            case LEFT_DIR:
                return nj + 1 >= N; // Out of bounds if moving right exceeds column limit
            case DOWN_DIR:
                return ni - 1 < 0; // Out of bounds if moving up goes above row 0
            default:
                return true; // Invalid direction
            }
        }

        // =================== Occupancy Constraints ===================

        void amo_sinz_occ(int i, int j, vector<Literal> &lits) {
            assert(lits.size() == K);
            if (K <= 1) return; // No constraint needed
            if (K <= 2) {
                Clause c;
                for(auto &lit : lits) {
                    c.addLiteral(lit.neg());
                }
                addClause(c);
                return;
            }

            vector<Literal> sinz_lits;
            for (int d = 1; d <= K - 1; d++) {
                string sinz_name = __sinz_occ_lit_str(i, j, d);
                int sinz_id = getVar(sinz_name);
                sinz_lits.push_back(Literal(sinz_id, true));
            }

            Clause cl_head, cl_tail;
            cl_head.addLiteral(lits[0].neg()); cl_head.addLiteral(sinz_lits[0]); addClause(cl_head);
            cl_tail.addLiteral(lits.back().neg()); cl_tail.addLiteral(sinz_lits.back().neg()); addClause(cl_tail);

            for (int d = 1; d <= K - 2; d++) {
                Clause cl1, cl2, cl3;
                cl1.addLiteral(lits[d].neg()); cl1.addLiteral(sinz_lits[d]); addClause(cl1);
                cl2.addLiteral(lits[d].neg()); cl2.addLiteral(sinz_lits[d-1].neg()); addClause(cl2);
                cl3.addLiteral(sinz_lits[d-1].neg()); cl3.addLiteral(sinz_lits[d]); addClause(cl3);
            }


            
        }

        void encodeOccupancy() {
            for(int i = 0; i < M; i++) {
                for(int j = 0; j < N; j++) {
                    vector<Literal> cell_lits;
                    for(int k = 0; k < K; k++) {
                        string lit_name = __occ_lit_str(i, j, k);
                        int lit_id = getVar(lit_name);
                        cell_lits.push_back(Literal(lit_id, true));
                    }
                    amo_sinz_occ(i, j, cell_lits);
                }
            }
        }

        // =================== Path Reachability Constraints ===================

        void amo_sinz_dir(int i, int j, int k, vector<Literal> &lits, bool is_in) {
            int D = lits.size();
            if (D <= 1) return; // No constraint needed
            if (D <= 2) {
                Clause c;
                for(auto &lit : lits) {
                    c.addLiteral(lit.neg());
                }
                addClause(c);
                return;
            }

            vector<Literal> sinz_lits;
            for (int d = 1; d <= D - 1; d++) {
                string sinz_name = __sinz_dir_lit_str(i, j, k, d, is_in);
                int sinz_id = getVar(sinz_name);
                sinz_lits.push_back(Literal(sinz_id, true));
            }

            Clause cl_head, cl_tail;
            cl_head.addLiteral(lits[0].neg()); cl_head.addLiteral(sinz_lits[0]); addClause(cl_head);
            cl_tail.addLiteral(lits.back().neg()); cl_tail.addLiteral(sinz_lits.back().neg()); addClause(cl_tail);

            for (int d = 1; d <= D - 2; d++) {
                Clause cl1, cl2, cl3;
                cl1.addLiteral(lits[d].neg()); cl1.addLiteral(sinz_lits[d]); addClause(cl1);
                cl2.addLiteral(lits[d].neg()); cl2.addLiteral(sinz_lits[d-1].neg()); addClause(cl2);
                cl3.addLiteral(sinz_lits[d-1].neg()); cl3.addLiteral(sinz_lits[d]); addClause(cl3);
            }
        }

        void local_constraints(int i, int j, int k, int src_i, int src_j, int sink_i, int sink_j) {
            bool is_source = (i == src_i && j == src_j);
            bool is_sink   = (i == sink_i && j == sink_j);

            string Xij = __occ_lit_str(i, j, k);
            int Xij_id = getVar(Xij);

            vector<pair<int, Literal>> IN_lits, OUT_lits;
            IN_lits.reserve(4);
            OUT_lits.reserve(4);

            auto neighbour_coords = [&](int dir) {
                int ni = i, nj = j;
                if (dir == RIGHT_DIR) nj += 1;
                else if (dir == LEFT_DIR) nj -= 1;
                else if (dir == UP_DIR) ni -= 1;
                else if (dir == DOWN_DIR) ni += 1;
                return pair<int,int>{ni, nj};
            };

            for (int d = 1; d <= 4; d++) {
                if (out_of_bounds(i, j, d)) continue;

                string in_name  = __in_lit_str(i, j, k, d);
                string out_name = __out_lit_str(i, j, k, d);
                int in_id  = getVar(in_name);
                int out_id = getVar(out_name);

                IN_lits.emplace_back(d, Literal(in_id, true));
                OUT_lits.emplace_back(d, Literal(out_id, true));

                auto [ni, nj] = neighbour_coords(d);
                bool neigh_is_sink = (ni == sink_i && nj == sink_j);

                // Local occupancy guards (skip for sink so it may stay unoccupied).
                if (!is_sink) {
                    { Clause c; c.addLiteral(Literal(in_id, false));  c.addLiteral(Literal(Xij_id, true)); addClause(c); }
                    { Clause c; c.addLiteral(Literal(out_id, false)); c.addLiteral(Literal(Xij_id, true)); addClause(c); }
                }

                string neigh_occ = __occ_lit_str(ni, nj, k);
                int neigh_occ_id = getVar(neigh_occ);

                int opp = getOppositeDirIndex(d);
                string neigh_in  = __in_lit_str(ni, nj, k, opp);
                string neigh_out = __out_lit_str(ni, nj, k, opp);
                int neigh_in_id  = getVar(neigh_in);
                int neigh_out_id = getVar(neigh_out);

                // Out coherence.
                { Clause c; c.addLiteral(Literal(out_id, false)); c.addLiteral(Literal(neigh_in_id, true)); addClause(c); }
                if (!neigh_is_sink) {
                    { Clause c; c.addLiteral(Literal(out_id, false)); c.addLiteral(Literal(neigh_occ_id, true)); addClause(c); }
                }
                // In coherence.
                { Clause c; c.addLiteral(Literal(in_id, false)); c.addLiteral(Literal(neigh_out_id, true)); addClause(c); }
                if (!neigh_is_sink) {
                    { Clause c; c.addLiteral(Literal(in_id, false)); c.addLiteral(Literal(neigh_occ_id, true)); addClause(c); }
                }
            }

            string turn_name = __turn_lit_str(i, j, k);
            int turn_id = getVar(turn_name);
            Literal turn_lit(turn_id, true);

            if (is_source || is_sink) {
                { Clause c; c.addLiteral(turn_lit.neg()); addClause(c); }
            }

            if (is_source && is_sink) {
                { Clause c; c.addLiteral(Literal(Xij_id, false)); addClause(c); }
                for (auto &[dir, lit] : IN_lits)  { Clause c; c.addLiteral(lit.neg()); addClause(c); }
                for (auto &[dir, lit] : OUT_lits) { Clause c; c.addLiteral(lit.neg()); addClause(c); }
                return;
            }

            if (is_source) {
                { Clause c; c.addLiteral(Literal(Xij_id, true)); addClause(c); }
                for (auto &[dir, lit] : IN_lits) { Clause c; c.addLiteral(lit.neg()); addClause(c); }

                Clause any_out;
                for (auto &[dir, lit] : OUT_lits) any_out.addLiteral(lit);
                if (!any_out.isEmpty()) addClause(any_out);

                vector<Literal> outs_only;
                outs_only.reserve(OUT_lits.size());
                for (auto &[dir, lit] : OUT_lits) outs_only.push_back(lit);
                amo_sinz_dir(i, j, k, outs_only, false);
                return;
            }

            if (is_sink) {
                { Clause c; c.addLiteral(Literal(Xij_id, false)); addClause(c); }
                for (auto &[dir, lit] : OUT_lits) { Clause c; c.addLiteral(lit.neg()); addClause(c); }

                Clause any_in;
                for (auto &[dir, lit] : IN_lits) any_in.addLiteral(lit);
                if (!any_in.isEmpty()) addClause(any_in);

                vector<Literal> ins_only;
                ins_only.reserve(IN_lits.size());
                for (auto &[dir, lit] : IN_lits) ins_only.push_back(lit);
                amo_sinz_dir(i, j, k, ins_only, true);
                return;
            }

            if (!(is_source || is_sink)) {
                // Turn bookkeeping for interior cells.
                { Clause c; c.addLiteral(turn_lit.neg()); c.addLiteral(Literal(Xij_id, true)); addClause(c); }

                Clause t_in_clause;
                t_in_clause.addLiteral(turn_lit.neg());
                if (IN_lits.empty()) {
                    addClause(t_in_clause);
                } else {
                    for (auto &[dir, lit] : IN_lits) t_in_clause.addLiteral(lit);
                    addClause(t_in_clause);
                }

                Clause t_out_clause;
                t_out_clause.addLiteral(turn_lit.neg());
                if (OUT_lits.empty()) {
                    addClause(t_out_clause);
                } else {
                    for (auto &[dir, lit] : OUT_lits) t_out_clause.addLiteral(lit);
                    addClause(t_out_clause);
                }

                for (auto &[dir_in, lit_in] : IN_lits) {
                    for (auto &[dir_out, lit_out] : OUT_lits) {
                        Clause c;
                        c.addLiteral(lit_in.neg());
                        c.addLiteral(lit_out.neg());
                        if (dir_in == getOppositeDirIndex(dir_out)) {
                            c.addLiteral(turn_lit.neg());
                        } else {
                            c.addLiteral(turn_lit);
                        }
                        addClause(c);
                    }
                }
            }

            if (!IN_lits.empty()) {
                Clause c;
                c.addLiteral(Literal(Xij_id, false));
                for (auto &[dir, lit] : IN_lits) c.addLiteral(lit);
                addClause(c);
            }
            if (!OUT_lits.empty()) {
                Clause c;
                c.addLiteral(Literal(Xij_id, false));
                for (auto &[dir, lit] : OUT_lits) c.addLiteral(lit);
                addClause(c);
            }

            vector<Literal> ins_only, outs_only;
            ins_only.reserve(IN_lits.size());
            outs_only.reserve(OUT_lits.size());
            for (auto &[dir, lit] : IN_lits) ins_only.push_back(lit);
            for (auto &[dir, lit] : OUT_lits) outs_only.push_back(lit);

            amo_sinz_dir(i, j, k, ins_only, true);
            amo_sinz_dir(i, j, k, outs_only, false);
        }

        void encodeDirectionConstraints() {
            for(int k = 0; k < K; k++) {
                auto &start = metro_map.getLineStarts(k);
                auto &end = metro_map.getLineEnds(k);
                int src_i = start[0].second, src_j = start[0].first;
                int sink_i = end[0].second, sink_j = end[0].first;

                for(int i = 0; i < M; i++) {
                    for(int j = 0; j < N; j++) {
                        local_constraints(i, j, k, src_i, src_j, sink_i, sink_j);
                    }
                }
            }
        }

        // ======================== Turn Constraints =======================

        void sinz_turn_counter(int k, vector<Literal> &turn_lits) {
            int n = turn_lits.size();
            if (n == 0) return;
            if (J < 0) return;
            if (J == 0) {
                for (auto &lit : turn_lits) {
                    Clause c;
                    c.addLiteral(lit.neg());
                    addClause(c);
                }
                return;
            }
            if (n <= J) return;

            vector<vector<int>> seq_id(n + 1, vector<int>(J + 1, 0));
            for (int i = 1; i <= n; ++i) {
                for (int t = 1; t <= J; ++t) {
                    string aux_name = __turn_seq_lit_str(k, i, t);
                    int aux_id = getVar(aux_name);
                    seq_id[i][t] = aux_id;
                }
            }

            {
                Clause c;
                c.addLiteral(turn_lits[0].neg());
                c.addLiteral(Literal(seq_id[1][1], true));
                addClause(c);
            }
            for (int t = 2; t <= J; ++t) {
                Clause c;
                c.addLiteral(Literal(seq_id[1][t], false));
                addClause(c);
            }

            for (int i = 2; i <= n; ++i) {
                {
                    Clause c;
                    c.addLiteral(turn_lits[i-1].neg());
                    c.addLiteral(Literal(seq_id[i][1], true));
                    addClause(c);
                }
                {
                    Clause c;
                    c.addLiteral(Literal(seq_id[i-1][1], false));
                    c.addLiteral(Literal(seq_id[i][1], true));
                    addClause(c);
                }
                for (int t = 2; t <= J; ++t) {
                    {
                        Clause c;
                        c.addLiteral(Literal(seq_id[i-1][t], false));
                        c.addLiteral(Literal(seq_id[i][t], true));
                        addClause(c);
                    }
                    {
                        Clause c;
                        c.addLiteral(turn_lits[i-1].neg());
                        c.addLiteral(Literal(seq_id[i-1][t-1], false));
                        c.addLiteral(Literal(seq_id[i][t], true));
                        addClause(c);
                    }
                }
                {
                    Clause c;
                    c.addLiteral(turn_lits[i-1].neg());
                    c.addLiteral(Literal(seq_id[i-1][J], false));
                    addClause(c);
                }
            }
        }

        void encodeTurnConstraints() {
            if (J < 0) return;
            for (int k = 0; k < K; ++k) {
                vector<Literal> turn_lits;
                turn_lits.reserve(M * N);
                for (int i = 0; i < M; ++i) {
                    for (int j = 0; j < N; ++j) {
                        string turn_name = __turn_lit_str(i, j, k);
                        int turn_id = getVar(turn_name);
                        turn_lits.emplace_back(Literal(turn_id, true));
                    }
                }
                sinz_turn_counter(k, turn_lits);
            }
        }

        // ======================== Popular Constraints =======================

        void encodePopularConstraints() {
            int P = metro_map.getPopularCitiesCount();
            if (P <= 0) return;

            auto &popular_groups = metro_map.getPopularCities();
            for (int p = 0; p < P; ++p) {
                Clause coverage_clause;
                for (auto &cell : popular_groups[p]) {
                    int col = cell.first;
                    int row = cell.second;
                    if (row < 0 || row >= M || col < 0 || col >= N) continue;
                    for (int k = 0; k < K; ++k) {
                        string occ_name = __occ_lit_str(row, col, k);
                        int occ_id = getVar(occ_name);
                        coverage_clause.addLiteral(Literal(occ_id, true));
                    }
                }
                if (!coverage_clause.isEmpty()) {
                    addClause(coverage_clause);
                }
            }
        }

    public :
        SATEncoder(MetroMap &m) : metro_map(m) {
            next_var = 1;   // Start variable IDs from 1
            N = metro_map.getColNum();
            M = metro_map.getRowNum();
            K = metro_map.getLineNum();
            J = metro_map.getTurnLimit();
        }

        int variableCount() const {
            return next_var - 1;
        }

        const vector<Clause>& getClauses() const {
            return clauses;
        }

        const vector<string>& getVariableNames() const {
            return var_to_literal;
        }

        size_t clauseCount() const {
            return clauses.size();
        }

        bool outputDIMACS(const string &filename) const {
            ofstream out(filename.c_str());
            if (!out.is_open()) {
                cerr << "Error: Cannot open DIMACS file for writing: " << filename << endl;
                return false;
            }

            out << "c Generated by SATEncoder" << '\n';
            out << "p cnf " << variableCount() << " " << clauses.size() << '\n';
            for (const auto &clause : clauses) {
                for (int lit : clause.lits) {
                    out << lit << ' ';
                }
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

        void encode() {
            // Encode the problem constraints into CNF clauses
            encodeOccupancy();
            encodeDirectionConstraints();
            encodeTurnConstraints();
            encodePopularConstraints();
        }

        
        
};

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <input.metromap> <output.cnf>" << std::endl;
        return 1;
    }

    const std::string input_path = argv[1];
    const std::string cnf_path   = argv[2];

    std::ifstream input_stream(input_path.c_str());
    if (!input_stream.is_open()) {
        std::cerr << "Error: Unable to open input file " << input_path << std::endl;
        return 1;
    }

    MetroMap metro_map = parseInputFile(input_stream);
    input_stream.close();

    SATEncoder encoder(metro_map);
    encoder.encode();

    if (!encoder.outputDIMACS(cnf_path)) {
        return 1;
    }

    const std::string map_path = cnf_path + ".map";
    if (!encoder.outputVariableMapping(map_path)) {
        return 1;
    }

    return 0;
}

