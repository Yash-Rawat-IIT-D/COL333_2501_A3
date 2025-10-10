#include <bits/stdc++.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include "utils.hpp"
using namespace std;

// Y/y -> Rows,  X/x -> Columns 



class Literal {
    private:
        string name; // String Name
        bool is_positive; // true for positive literal, false for negated literal
    public:
        Literal(string v, bool pos) : name(v), is_positive(pos) {}

        Literal getNegation() const {
            return Literal(name, !is_positive);
        }

        string getName() const {
            return name;
        }

        bool isPositive() const {
            return is_positive;
        }

        void setParity(bool pos) {
            is_positive = pos;
        }
    
        string toString() const {
            return (is_positive ? "" : "-") + name;
        }
};

// Helper function to create occupancy literal for cell (i, j) and line k
string __occ_lit_str(int i, int j, int k) {
    ostringstream ss;
    ss << "X_" << i << "_" << j << "_" << k;
    return ss.str();
}

// Helper function to create Sinz auxiliary literal for cell (i, j) and index d
string __sinz_lit_str(int i, int j, int d) {
    ostringstream ss;
    ss << "S_" << i << "_" << j << "_" << d;
    return ss.str();
}

// Helper function to create reachability literal for line k at cell (i, j)

string __reach_lit_str(int i, int j, int k) {
    ostringstream ss;
    ss << "R_" << i << "_" << j << "_" << k;
    return ss.str();
}
// line k at cell (i, j) goes out in direction dir
string __out_lit_str(int i, int j, int k, int dir) {
    ostringstream ss;
    ss << "Out_" << i << "_" << j << "_" << k << "_" << dir;
    return ss.str();
}

string  __in_lit_str(int i, int j, int k, int dir) {
    ostringstream ss;
    ss << "In_" << i << "_" << j << "_" << k << "_" << dir;
    return ss.str();
}

// Clause class to represent a disjunction of literals

class Clause {
    private:
        vector<Literal> literals; // List of literals in the clause
    public:
        Clause() {}

        void addLiteral(const Literal& literal) {
            literals.push_back(literal);
        }

        string toString() const {
            string result;
            for (const auto& literal : literals) {
                result += literal.toString() + " ";
            }
            result += "0"; // End of clause marker
            return result;
        }

        bool isEmpty() const {
            return literals.empty();
        }


};

class SATEncoder {
    private :
        // Access to MetroMap
        MetroMap &metro_map;

        // Variable Tracking
        int var_count;
        vector<string> literal_names;
        map<string, int> literal_var_map; 

        // Problem Parameters
        int N, M, K, J; // Columns, Rows, Lines, Turn Limit

        // CNF Representation
        vector<Clause> occupancy_clauses;
        vector<Clause> directionality_clauses;
        vector<Clause> reachability_clauses;
        vector<Clause> turn_clauses;
        vector<Clause> clauses;
        
        void add_literal(const string &name) {
            // Bypass expensive map checks if known to be new
            literal_var_map[name] = ++var_count;
            literal_names.push_back(name);
            
        }

        void track_literal(const string& name) {
            if (literal_var_map.find(name) == literal_var_map.end()) {
                literal_var_map[name] = ++var_count;
                literal_names.push_back(name);
            }
        }

        bool out_of_bounds(int i, int j, int dir) {
            switch (dir) {
            case RIGHT:
                return j + 1 >= N; // Out of bounds if moving right exceeds column limit
            case UP:
                return i - 1 < 0; // Out of bounds if moving up goes above row 0
            case LEFT:
                return j - 1 < 0; // Out of bounds if moving left goes below column 0
            case DOWN:
                return i + 1 >= M; // Out of bounds if moving down exceeds row limit
            default:
                return true; // Invalid direction
            }
        }

    public :
        SATEncoder(MetroMap &m) : metro_map(m) {
            var_count = 0;  // No variables initially
            N = metro_map.getColNum();
            M = metro_map.getRowNum();
            K = metro_map.getLineNum();
            J = metro_map.getTurnLimit();
        }

        
        void amo_sinz_occ(int i, int j, vector<Literal> &lits) {
            
            assert(lits.size() == K);
            
            if(K <= 1) return; // No need to encode if only one line
            if(K <= 2) {
                // Just a Pairwise encoding
                Clause clause;
                for(auto &lit : lits) {
                    Literal neg_lit = lit.getNegation();
                    clause.addLiteral(neg_lit);
                }
                occupancy_clauses.push_back(clause);
                return;
            }

            vector<Literal> sinz_lits;

            // Setting up Sinz Literals
            for (int d = 1; d <= K - 1; d++) {
                string sinz_name = __sinz_lit_str(i, j, d);
                track_literal(sinz_name);
                sinz_lits.push_back(Literal(sinz_name, TRUE));
            }

            // First Clause
            Clause cl_hd, cl_tl;
            cl_hd.addLiteral(lits[0].getNegation()); cl_hd.addLiteral(sinz_lits[0]);
            occupancy_clauses.push_back(cl_hd);
            cl_tl.addLiteral(lits.back().getNegation()); cl_tl.addLiteral(sinz_lits.back().getNegation());
            occupancy_clauses.push_back(cl_tl);

            // Middle Clauses
            for (int d = 1; d <= K - 2; d++) {
                Clause cl1, cl2, cl3;
                cl1.addLiteral(lits[d].getNegation()); cl1.addLiteral(sinz_lits[d]);
                occupancy_clauses.push_back(cl1);
                cl2.addLiteral(lits[d].getNegation()); cl2.addLiteral(sinz_lits[d-1].getNegation());
                occupancy_clauses.push_back(cl2);
                cl3.addLiteral(sinz_lits[d-1].getNegation()); cl3.addLiteral(sinz_lits[d]);
                occupancy_clauses.push_back(cl3);  
            }
        }

        void encodeOccupancy() {
            // Encode occupancy constraints here
            // For each cell, ensure that at most one line occupies it
            
            for (int i = 0; i < M; i++) {
                for (int j = 0; j < N; j++) {
                    vector<Literal> cell_literals;
                    for (int k = 0; k < K; k++) {
                        string lit_name = __occ_lit_str(i, j, k);
                        track_literal(lit_name);
                        cell_literals.push_back(Literal(lit_name, TRUE));
                    }
                    amo_sinz_occ(i, j, cell_literals);
                }
            }
        }

        void local_constraints(int i, int j, int k, int src_i, int src_j, int sink_i, int sink_j) {
            // Step 0: Forbid OOB directions
            for (int dir = 1; dir <= 4; ++dir) {
                if (out_of_bounds(i, j, dir)) {
                    track_literal(__out_lit_str(i, j, k, dir));
                    track_literal(__in_lit_str(i, j, k, dir));
                    Clause c1; c1.addLiteral(Literal(__out_lit_str(i, j, k, dir), false)); directionality_clauses.push_back(c1);
                    Clause c2; c2.addLiteral(Literal(__in_lit_str(i, j, k, dir),  false)); directionality_clauses.push_back(c2);
                }
            }

            // Step1 : Implement Directionality Guards
            Literal lit_occ(__occ_lit_str(i, j, k), TRUE);
            for (int dir = 1; dir <= 4; dir++) {
                Literal neg_lit_out(__out_lit_str(i, j, k, dir), FALSE);
                Literal neg_lit_in (__in_lit_str(i, j, k, dir), FALSE);
                Clause cl_out_guard, cl_in_guard;
                cl_out_guard.addLiteral(lit_occ); cl_out_guard.addLiteral(neg_lit_out);
                directionality_clauses.push_back(cl_out_guard);

                int ni = i, nj = j;
                if (dir == RIGHT) nj += 1;
                else if (dir == UP) ni -= 1;
                else if (dir == LEFT) nj -= 1;
                else if (dir == DOWN) ni += 1;

                // If this is sink cell, we cannot enforce the incoming direction guard
                if(ni == sink_i && nj == sink_j) continue;

                cl_in_guard.addLiteral(lit_occ); cl_in_guard.addLiteral(neg_lit_in);
                directionality_clauses.push_back(cl_in_guard);
            }

            // Step 2 : Implement Neighbour Coherence
            for(int dir = 1; dir <= 4; dir++) {
                if(!out_of_bounds(i, j, dir)) {
                    // Must Match incoming dir of neighbour with outgoing dir of current cell
                    Literal lit_out = Literal(__out_lit_str(i, j, k, dir), TRUE);
                    int ni = i, nj = j;
                    if (dir == RIGHT) nj += 1;
                    else if (dir == UP) ni -= 1;
                    else if (dir == LEFT) nj -= 1;
                    else if (dir == DOWN) ni += 1;
                    Literal lit_in(__in_lit_str(ni, nj, k, getOpoositeDirIndex(dir)), TRUE);

                    Clause cl1; cl1.addLiteral(lit_out.getNegation()); cl1.addLiteral(lit_in);
                    directionality_clauses.push_back(cl1);

                    if (ni == sink_i && nj == sink_j) continue;

                    Literal lit_occ_neigh(__occ_lit_str(ni, nj, k), TRUE);
                    Clause cl2; cl2.addLiteral(lit_out.getNegation()); cl2.addLiteral(lit_occ_neigh);
                    directionality_clauses.push_back(cl2);
                }
            }

            // Step 3 : Degree Constraints

            // Build lists of In*/Out* names
            vector<string> INs, OUTs;
            for (int d = 1; d <= 4; ++d) {
                INs.push_back(__in_lit_str(i, j, k, d));
                OUTs.push_back(__out_lit_str(i, j, k, d));
                track_literal(INs.back());
                track_literal(OUTs.back());
            }
            string Xij = __occ_lit_str(i, j, k);
            track_literal(Xij);

            bool is_source = (i == src_i && j == src_j);
            bool is_sink   = (i == sink_i && j == sink_j);

            if (is_source) {
                // Force occupied
                { Clause c; c.addLiteral(Literal(Xij, true)); directionality_clauses.push_back(c); }

                // ALO on Out*: (OutR ∨ OutU ∨ OutL ∨ OutD)
                { Clause c; for (auto &s : OUTs) c.addLiteral(Literal(s, true)); directionality_clauses.push_back(c); }

                // Pairwise AMO on Out* (6 binary clauses)
                for (int a = 0; a < 4; ++a) for (int b = a+1; b < 4; ++b) {
                    Clause c; c.addLiteral(Literal(OUTs[a], false)); c.addLiteral(Literal(OUTs[b], false));
                    directionality_clauses.push_back(c);
                }

                // Forbid all In*
                for (auto &s : INs) { Clause c; c.addLiteral(Literal(s, false)); directionality_clauses.push_back(c); }
            }
            else if (is_sink) {
                // Force occupied
                { Clause c; c.addLiteral(Literal(Xij, true)); directionality_clauses.push_back(c); }

                // ALO on In*
                { Clause c; for (auto &s : INs) c.addLiteral(Literal(s, true)); directionality_clauses.push_back(c); }

                // Pairwise AMO on In*
                for (int a = 0; a < 4; ++a) for (int b = a+1; b < 4; ++b) {
                    Clause c; c.addLiteral(Literal(INs[a], false)); c.addLiteral(Literal(INs[b], false));
                    directionality_clauses.push_back(c);
                }

                // Forbid all Out*
                for (auto &s : OUTs) { Clause c; c.addLiteral(Literal(s, false)); directionality_clauses.push_back(c); }
            }
            else {
                // If occupied ⇒ at least one In*
                { Clause c; c.addLiteral(Literal(Xij, false)); for (auto &s : INs) c.addLiteral(Literal(s, true)); directionality_clauses.push_back(c); }
                // If occupied ⇒ at least one Out*
                { Clause c; c.addLiteral(Literal(Xij, false)); for (auto &s : OUTs) c.addLiteral(Literal(s, true)); directionality_clauses.push_back(c); }

                // Pairwise AMO on In*
                for (int a = 0; a < 4; ++a) for (int b = a+1; b < 4; ++b) {
                    Clause c; c.addLiteral(Literal(INs[a], false)); c.addLiteral(Literal(INs[b], false));
                    directionality_clauses.push_back(c);
                }
                // Pairwise AMO on Out*
                for (int a = 0; a < 4; ++a) for (int b = a+1; b < 4; ++b) {
                    Clause c; c.addLiteral(Literal(OUTs[a], false)); c.addLiteral(Literal(OUTs[b], false));
                    directionality_clauses.push_back(c);
                }
            }
        }


        void encodeReachability() {
            // Encode reachability constraints here
            // Ensure each line can reach from start to end
            
            for (int k = 0; k < K; k++) {

                vector<pair<int,int>> &line_k_start = metro_map.getLineStarts(k);
                vector<pair<int,int>> &line_k_end   = metro_map.getLineEnds(k);

                int src_i  = line_k_start[0].second, src_j  = line_k_start[0].first;
                int sink_i = line_k_end[0].second,   sink_j = line_k_end[0].first;

                // Seed and sink reachable
                track_literal(__reach_lit_str(src_i,  src_j,  k));
                track_literal(__reach_lit_str(sink_i, sink_j, k));
                { Clause c; c.addLiteral(Literal(__reach_lit_str(src_i,src_j,k), true)); reachability_clauses.push_back(c); }
                { Clause c; c.addLiteral(Literal(__reach_lit_str(sink_i,sink_j,k), true)); reachability_clauses.push_back(c); }

                for (int i = 0; i < M; i++) {
                    for (int j = 0; j < N; j++) {
                        // Declare vars
                        track_literal(__reach_lit_str(i, j, k));
                        for (int dir = 1; dir <= 4; dir++) {
                            track_literal(__out_lit_str(i, j, k, dir));
                            track_literal(__in_lit_str(i, j, k, dir));
                        }

                        // Local: guards + neighbour coherence + degrees
                        local_constraints(i, j, k, src_i, src_j, sink_i, sink_j);

                        // If (i,j) is reachable and you take the Out edge in dir, then the neighbor (ni,nj) is reachable
                        // Propagation: (¬R(i,j) ∨ ¬Out(i,j,dir) ∨ R(ni,nj))
                        for (int dir = 1; dir <= 4; ++dir) {
                            if (out_of_bounds(i, j, dir)) continue;
                            int ni = i, nj = j;
                            if (dir == RIGHT) nj++; else if (dir == UP) ni--; else if (dir == LEFT) nj--; else ni++;
                            Clause c;
                            c.addLiteral(Literal(__reach_lit_str(i, j, k), false));
                            c.addLiteral(Literal(__out_lit_str(i, j, k, dir), false));
                            c.addLiteral(Literal(__reach_lit_str(ni, nj, k), true));
                            reachability_clauses.push_back(c);
                        }

                        // If occupied and not sink ⇒ reachable: (¬X ∨ R)
                        if (!(i == sink_i && j == sink_j)) {
                            Clause c;
                            c.addLiteral(Literal(__occ_lit_str(i, j, k), false));
                            c.addLiteral(Literal(__reach_lit_str(i, j, k), true));
                            reachability_clauses.push_back(c);
                        }
                    }
                }
            }
        }


        void encodeTurnLimit() {
            // Encode turn limit constraints here
            // Ensure that no line exceeds the turn limit
            // Placeholder implementation
            Clause clause;
            clause.addLiteral(Literal("z1", true));
            clause.addLiteral(Literal("z2", false));
            turn_clauses.push_back(clause);
        }


        // Top level function to encode the problem into CNF
        void encode() {
            encodeOccupancy();
            encodeReachability();
            encodeTurnLimit();

            // Combine all clauses
            clauses.insert(clauses.end(), occupancy_clauses.begin(), occupancy_clauses.end());
            clauses.insert(clauses.end(), reachability_clauses.begin(), reachability_clauses.end());
            clauses.insert(clauses.end(), directionality_clauses.begin(), directionality_clauses.end());
            clauses.insert(clauses.end(), turn_clauses.begin(), turn_clauses.end());
        }



};