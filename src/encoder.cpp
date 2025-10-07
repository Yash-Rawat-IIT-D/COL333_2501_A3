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
                add_literal(sinz_name);
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
                        add_literal(lit_name);
                        cell_literals.push_back(Literal(lit_name, TRUE));
                    }
                    amo_sinz_occ(i, j, cell_literals);
                }
            }
        }

        void local_constraints(int i, int j, int k, int src_i, int src_j, int sink_i, int sink_j) {
            // Implement Local Directionality Constraints here

            // Step1 : Implement Directionality Guards
            Literal lit_occ = Literal(__occ_lit_str(i, j, k), TRUE);
            for(int dir = 1; dir <= 4; dir++) {
                Literal neg_lit_out = Literal(__out_lit_str(i, j, k, dir), FALSE);
                Literal neg_lit_in = Literal(__in_lit_str(i, j, k, dir), FALSE);
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
                    Literal lit_in = Literal(__in_lit_str(ni, nj, k, getOpoositeDirIndex(dir)), TRUE);  
                    Clause cl_1;
                    cl_1.addLiteral(lit_out.getNegation()); cl_1.addLiteral(lit_in);
                    directionality_clauses.push_back(cl_1);

                    // If the neighbour is non-sink then it must be occupied if it has an incoming direction 

                    if(ni == sink_i && nj == sink_j) continue;

                    Literal lit_occ_neigh = Literal(__occ_lit_str(ni, nj, k), TRUE);
                    Clause cl_2;
                    cl_2.addLiteral(lit_out.getNegation()); cl_2.addLiteral(lit_occ_neigh);
                    directionality_clauses.push_back(cl_2);
                }
            }

            // Step 3 : Implementing Degree Constraints 
            // Source Cell : Exactly one outgoing direction, no incoming directions
            // Sink Cell : Exactly one incoming direction, no outgoing directions
            // Intermediate Cell : At most one incoming and one outgoing direction

        }   

        void encodeReachability() {
            // Encode reachability constraints here
            // Ensure each line can reach from start to end
            
            for (int k = 0; k < K; k++) {

                vector<pair<int,int>> &line_k_start = metro_map.getLineStarts(k);
                vector<pair<int,int>> &line_k_end = metro_map.getLineEnds(k);

                int src_i = line_k_start[0].second, src_j = line_k_start[0].first;
                int sink_i = line_k_end[0].second, sink_j = line_k_end[0].first;

                for(int i = 0; i < M; i++) {
                    for(int j = 0; j < N; j++) {

                        string lit_reach = __reach_lit_str(i, j, k);
                        add_literal(lit_reach);

                        for (int dir = 1; dir <= 4; dir++) {
                            string lit_out = __out_lit_str(i, j, k, dir);
                            add_literal(lit_out);
                            string lit_in = __in_lit_str(i, j, k, dir);
                            add_literal(lit_in);
                        }

                        local_constraints(i, j, k, src_i, src_j, sink_i, sink_j);
                    }
                }
                // reach_line(k, reach_literals);
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
            clauses.insert(clauses.end(), turn_clauses.begin(), turn_clauses.end());
        }



};