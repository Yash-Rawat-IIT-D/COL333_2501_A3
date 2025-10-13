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



// Clause class to represent a disjunction of literals

class Clause {
    public:
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

// String Helper Functions

string __dir_lit_str(int i, int j, int dir) {
    ostringstream ss;
    ss << "DIR_" << i << "_" << j << "_" << dir;
    return ss.str();   
}

// Helper function to create Sinz auxiliary literal for direction AMO
string __dir_sinz_lit_str(int i, int j, int idx) {
    ostringstream ss;
    ss << "DS_" << i << "_" << j << "_" << idx;
    return ss.str();
}

// Helper function to create incoming direction literal for cell (i, j) from direction dir
string __in_lit_str(int i, int j, int dir) {
    ostringstream ss;
    ss << "IN_" << i << "_" << j << "_" << dir;
    return ss.str();
}

// Helper function to create outgoing direction literal for cell (i, j) to direction dir
string __out_lit_str(int i, int j, int dir) {
    ostringstream ss;
    ss << "OUT_" << i << "_" << j << "_" << dir;
    return ss.str();
}

string __flow_dir_lit_str(int i, int j, int dir, bool is_in) {
    ostringstream ss;
    ss << (is_in ? "FIS_" : "FOS_") << i << "_" << j << "_" << dir;
    return ss.str();
}

// Helper function to create reachability string literal for cell (i, j) and line k
string __reach_lit_str(int i, int j, int k) {
    ostringstream ss;
    ss << "R_" << i << "_" << j << "_" << k;
    return ss.str();
}

// Helper function to create turn counting string literal for line k at cell (i, j) with t turns used
string __turn_lit_str(int i, int j, int k) {
    ostringstream ss;
    ss << "T_" << i << "_" << j << "_" << k;
    return ss.str();
}

// Helper function to create turn sequence string literal for line k at cell (i, j) with t turns used
string __turn_seq_name(int k, int i, int j) {
    ostringstream ss;
    ss << "TS_" << k << "_" << i << "_" << j;
    return ss.str();
}

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
        
        // CNF Representation - Organized by constraint type
        vector<Clause> direction_clauses;       // Direction constraints  (AMO for each cell)
        vector<Clause> flow_clauses;            // In-degree ≤ 1 constraints (non-intersection)
        vector<Clause> path_coherence_clauses;  // Direction consistency between neighbors
        vector<Clause> reachability_clauses;    // Line-specific reachability propagation
        vector<Clause> turn_limit_clauses;      // Per-line turn counting constraints
        vector<Clause> popular_city_clauses;    // Popular city coverage requirements
        vector<Clause> source_sink_clauses;     // Source/sink specific constraints
        
        vector<Clause> clauses; // Final combined clauses

    
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

        void add_clause_to(vector<Clause>& bucket, std::initializer_list<Literal> lits){
            Clause c; for (auto &L : lits) c.addLiteral(L); bucket.push_back(c);
        }  

        // Core encoding methods begin here -

        // ============================ Direction Constraints ============================

        void sinz_amo_helper(vector<Literal> &lits, int i, int j) {
            int n = lits.size();
            
            if (n <= 1) return; // No need to encode if only one literal
            
            if (n == 2) {
                // Just a pairwise encoding for 2 literals
                Clause clause;
                clause.addLiteral(lits[0].getNegation());
                clause.addLiteral(lits[1].getNegation());
                direction_clauses.push_back(clause);
                return;
            }

            // Create Sinz auxiliary variables
            vector<Literal> sinz_lits;
            for (int d = 1; d <= n - 1; d++) {
                string sinz_name = __dir_sinz_lit_str(i, j, d);
                track_literal(sinz_name);
                sinz_lits.push_back(Literal(sinz_name, true));
            }

            // First clause: ¬x₁ ∨ s₁
            Clause cl_head;
            cl_head.addLiteral(lits[0].getNegation());
            cl_head.addLiteral(sinz_lits[0]);
            direction_clauses.push_back(cl_head);


            // Middle clauses: for each i from 2 to n-1
            for (int d = 1; d <= n - 2; d++) {
                // ¬xᵢ ∨ sᵢ
                Clause cl1;
                cl1.addLiteral(lits[d].getNegation());
                cl1.addLiteral(sinz_lits[d]);
                direction_clauses.push_back(cl1);

                // ¬xᵢ ∨ ¬sᵢ₋₁
                Clause cl2;
                cl2.addLiteral(lits[d].getNegation());
                cl2.addLiteral(sinz_lits[d-1].getNegation());
                direction_clauses.push_back(cl2);

                // ¬sᵢ₋₁ ∨ sᵢ
                Clause cl3;
                cl3.addLiteral(sinz_lits[d-1].getNegation());
                cl3.addLiteral(sinz_lits[d]);
                direction_clauses.push_back(cl3);
            }

            // Last clause: ¬xₙ ∨ ¬sₙ₋₁
            Clause cl_tail;
            cl_tail.addLiteral(lits.back().getNegation());
            cl_tail.addLiteral(sinz_lits.back().getNegation());
            direction_clauses.push_back(cl_tail);
        }

        void amo_sinz_directions(int i, int j) {
            vector<Literal> dir_lits;
            
            // Collect all 5 direction literals for cell (i,j)
            for (int d = 0; d <= 4; d++) {
                string lit_name = __dir_lit_str(i, j, d);
                // track_literal(lit_name);
                add_literal(lit_name); // First time addition
                dir_lits.push_back(Literal(lit_name, true));
            }
            
            // Apply Sinz AMO encoding on the 5 direction variables
            sinz_amo_helper(dir_lits, i, j);
        }

        void encodeDirectionConstraints() {
            // For each cell: at most one of {EMPTY_DIR, RIGHT_DIR, UP_DIR, LEFT_DIR, DOWN_DIR}
            // This replaces the bloated per-line occupancy constraints
            
            for (int i = 0; i < M; i++) {
                for (int j = 0; j < N; j++) {
                    // Apply AMO constraint on the 5 direction variables
                    amo_sinz_directions(i, j);
                }
            }
        }

        // ========================== Degree Flow Constraints ============================

        void amo_sinz_in_directions(int i, int j) {
            vector<Literal> in_lits;
            
            // Collect all 4 incoming direction literals for cell (i,j)
            for (int d = 1; d <= 4; d++) { // Skip EMPTY (0)
                string lit_name = __in_lit_str(i, j, d);
                add_literal(lit_name);
                in_lits.push_back(Literal(lit_name, true));
            }
            
            // Apply Sinz AMO encoding - reuse the same helper but with different bucket
            sinz_amo_helper_flow(in_lits, i, j, true); // true for IN
        }

        void amo_sinz_out_directions(int i, int j) {
            vector<Literal> out_lits;
            
            // Collect all 4 outgoing direction literals for cell (i,j)
            for (int d = 1; d <= 4; d++) { // Skip EMPTY (0)
                string lit_name = __out_lit_str(i, j, d);
                add_literal(lit_name);
                out_lits.push_back(Literal(lit_name, true));
            }
            
            // Apply Sinz AMO encoding
            sinz_amo_helper_flow(out_lits, i, j, false); // false for OUT
        }

        // Similar to sinz_amo_helper but adds clauses to flow_clauses bucket
        void sinz_amo_helper_flow(vector<Literal> &lits, int i, int j, bool is_in) {
            int n = lits.size();
            
            if (n <= 1) return;
            
            if (n == 2) {
                add_clause_to(flow_clauses, {lits[0].getNegation(), lits[1].getNegation()});
                return;
            }

            // Create Sinz auxiliary variables with different prefix for flow
            vector<Literal> sinz_lits;
            for (int d = 1; d <= n - 1; d++) {
                string sinz_name = __flow_dir_lit_str(i, j, d, is_in);
                track_literal(sinz_name);
                sinz_lits.push_back(Literal(sinz_name, true));
            }

            // First clause: ¬x₁ ∨ s₁
            add_clause_to(flow_clauses, {lits[0].getNegation(), sinz_lits[0]});

            // Middle clauses
            for (int d = 1; d <= n - 2; d++) {
                add_clause_to(flow_clauses, {lits[d].getNegation(), sinz_lits[d]});
                add_clause_to(flow_clauses, {lits[d].getNegation(), sinz_lits[d-1].getNegation()});
                add_clause_to(flow_clauses, {sinz_lits[d-1].getNegation(), sinz_lits[d]});
            }

            // Last clause: ¬xₙ ∨ ¬sₙ₋₁
            add_clause_to(flow_clauses, {lits.back().getNegation(), sinz_lits.back().getNegation()});
        }

        // Helper to add no U-turn constraints for a cell
        void add_no_uturn_constraints(int i, int j) {
            for (int d = 1; d <= 4; d++) {
                // ¬(IN_i_j_d ∧ OUT_i_j_d) ≡ (¬IN_i_j_d ∨ ¬OUT_i_j_d)
                string in_var = __in_lit_str(i, j, d);
                string out_var = __out_lit_str(i, j, d);
                
                add_clause_to(flow_clauses, {
                    Literal(in_var, false),   // ¬IN_i_j_d
                    Literal(out_var, false)   // ¬OUT_i_j_d
                });
            }
        }

        void encodeFlowConstraints() {
            for (int i = 0; i < M; i++) {
                for (int j = 0; j < N; j++) {
                    // 1. In-degree ≤ 1: AMO on incoming directions
                    amo_sinz_in_directions(i, j);
                    
                    // 2. Out-degree ≤ 1: AMO on outgoing directions  
                    amo_sinz_out_directions(i, j);
                    
                    // 3. No U-turns: ¬(IN_i_j_d ∧ OUT_i_j_d) for all d
                    add_no_uturn_constraints(i, j);
                }
            }
        }

        // ============================ Path/Source/Sink Coherence Constraints ============================

        // Add helper function to get neighbor coordinates
        void getNeighbor(int i, int j, int dir, int& ni, int& nj) {
            ni = i; nj = j;
            switch (dir) {
                case RIGHT_DIR: nj = j + 1; break;
                case UP_DIR: ni = i - 1; break;
                case LEFT_DIR: nj = j - 1; break;
                case DOWN_DIR: ni = i + 1; break;
            }
        }

        // Add helper function to get opposite direction
        int getOppositeDir(int dir) {
            switch (dir) {
                case RIGHT_DIR: return LEFT_DIR;
                case UP_DIR: return DOWN_DIR;
                case LEFT_DIR: return RIGHT_DIR;
                case DOWN_DIR: return UP_DIR;
                default: return -1;
            }
        }
        
        void classify_source_sink(vector<pair<int,int>> &sources, vector<pair<int,int>> &sinks, vector<pair<int,int>> &normals) {
            vector<vector<int>> cell_type(M, vector<int>(N, 0)); // 0=normal, 1=source, 2=sink, 3=both
            
            for(int k = 0; k < K; k++) {
                auto &starts = metro_map.getLineStarts(k);
                auto &ends = metro_map.getLineEnds(k);
                
                for(auto &start : starts) {
                    int col = start.first;   // column (j)
                    int row = start.second;  // row (i)
                    if (cell_type[row][col] == 0) cell_type[row][col] = 1; // source
                    else if (cell_type[row][col] == 2) cell_type[row][col] = 3; // both source and sink
                }

                for(auto &end : ends) {
                    int col = end.first;     // column (j)
                    int row = end.second;    // row (i)
                    if (cell_type[row][col] == 0) cell_type[row][col] = 2; // sink
                    else if (cell_type[row][col] == 1) cell_type[row][col] = 3; // both source and sink
                }
            }

            // Classify cells based on type
            for(int i = 0; i < M; i++) {
                for(int j = 0; j < N; j++) {
                    if (cell_type[i][j] == 0) {
                        normals.push_back({i, j}); // (row, col) format for consistency
                    } else {
                        // Any cell that is source, sink, or both goes into sources/sinks
                        // Since source and sink points have to be unique, we can assert 

                        assert(cell_type[i][j] != 3);

                        if (cell_type[i][j] == 1 || cell_type[i][j] == 3) {
                            sources.push_back({i, j});
                        }
                        if (cell_type[i][j] == 2 || cell_type[i][j] == 3) {
                            sinks.push_back({i, j});
                        }
                    }
                }
            }
        }

        
        void encodePathCoherence(const vector<pair<int,int>> &normals) {
            // Only process normal (non-source/sink) cells
            for (auto &normal : normals) {
                int i = normal.first;  // row
                int j = normal.second; // col
                
                // Direction-Flow Connection: DIR_i_j_d => OUT_i_j_d for d ∈ {1,2,3,4}
                for (int d = 1; d <= 4; d++) { // RIGHT, UP, LEFT, DOWN
                    string dir_var = __dir_lit_str(i, j, d);
                    string out_var = __out_lit_str(i, j, d);
                    
                    // DIR_i_j_d => OUT_i_j_d  ≡  ¬DIR_i_j_d ∨ OUT_i_j_d
                    add_clause_to(path_coherence_clauses, {
                        Literal(dir_var, false),
                        Literal(out_var, true)
                    });
                }

                // Direction coherence for non-endpoint cells
                for (int d = 1; d <= 4; d++) { // RIGHT, UP, LEFT, DOWN
                    string dir_var = __dir_lit_str(i, j, d);
                    string out_var = __out_lit_str(i, j, d);
                    
                    // DIR_i_j_d ⇒ OUT_i_j_d
                    add_clause_to(path_coherence_clauses, {
                        Literal(dir_var, false), 
                        Literal(out_var, true)
                    });
                    
                    // DIR_i_j_d ⇒ (IN from some other direction)
                    Clause in_clause;
                    in_clause.addLiteral(Literal(dir_var, false)); // ¬DIR_i_j_d
                    
                    for (int other_d = 1; other_d <= 4; other_d++) {
                        if (other_d != d) { // Different direction
                            string in_var = __in_lit_str(i, j, other_d);
                            in_clause.addLiteral(Literal(in_var, true));
                        }
                    }
                    path_coherence_clauses.push_back(in_clause);
                }
                
                // DIR_EMPTY ⇒ (no IN and no OUT)
                string empty_var = __dir_lit_str(i, j, EMPTY_DIR);
                for (int d = 1; d <= 4; d++) {
                    // ¬EMPTY ∨ ¬IN_d  and  ¬EMPTY ∨ ¬OUT_d
                    add_clause_to(path_coherence_clauses, {
                        Literal(empty_var, false),
                        Literal(__in_lit_str(i, j, d), false)
                    });
                    add_clause_to(path_coherence_clauses, {
                        Literal(empty_var, false), 
                        Literal(__out_lit_str(i, j, d), false)
                    });
                }
                
                // Neighbor coherence: OUT_i_j_d ⇒ IN_neighbor_opposite_d
                for (int d = 1; d <= 4; d++) {
                    if (out_of_bounds(i, j, d)) continue;
                    
                    int ni, nj;
                    getNeighbor(i, j, d, ni, nj);
                    int opposite_d = getOppositeDir(d);
                    
                    string out_var = __out_lit_str(i, j, d);
                    string neighbor_in_var = __in_lit_str(ni, nj, opposite_d);
                    
                    // OUT_i_j_d ⇒ IN_ni_nj_opposite_d
                    add_clause_to(path_coherence_clauses, {
                        Literal(out_var, false),
                        Literal(neighbor_in_var, true)
                    });
                }
            }
        }

        void encodeSourceSinkConstraints(const vector<pair<int,int>> &sources, const vector<pair<int,int>> &sinks) {
            // Handle sources
            for (auto &source : sources) {
                int src_i = source.first;   // row
                int src_j = source.second;  // col
                
                // Direction-Flow Connection: DIR_i_j_d => OUT_i_j_d for d ∈ {1,2,3,4}
                for (int d = 1; d <= 4; d++) { // RIGHT, UP, LEFT, DOWN
                    string dir_var = __dir_lit_str(src_i, src_j, d);
                    string out_var = __out_lit_str(src_i, src_j, d);

                    // DIR_i_j_d => OUT_i_j_d  ≡  ¬DIR_i_j_d ∨ OUT_i_j_d
                    add_clause_to(path_coherence_clauses, {
                        Literal(dir_var, false),
                        Literal(out_var, true)
                    });
                }

                // Source must not be empty
                add_clause_to(source_sink_clauses, {
                    Literal(__dir_lit_str(src_i, src_j, EMPTY_DIR), false)
                });
                
                // Source must have exactly one outgoing direction (ALO)
                Clause out_alo;
                for (int d = 1; d <= 4; d++) {
                    out_alo.addLiteral(Literal(__out_lit_str(src_i, src_j, d), true));
                }
                source_sink_clauses.push_back(out_alo);
                
                // Source must have no incoming directions
                for (int d = 1; d <= 4; d++) {
                    add_clause_to(source_sink_clauses, {
                        Literal(__in_lit_str(src_i, src_j, d), false)
                    });
                }
                
                // Source must have a direction (ALO on directions)
                Clause dir_alo;
                for (int d = 1; d <= 4; d++) {
                    dir_alo.addLiteral(Literal(__dir_lit_str(src_i, src_j, d), true));
                }
                source_sink_clauses.push_back(dir_alo);
            }
        
            // Handle sinks  
            for (auto &sink : sinks) {
                int sink_i = sink.first;    // row
                int sink_j = sink.second;   // col
                
                // Sink must not be empty
                add_clause_to(source_sink_clauses, {
                    Literal(__dir_lit_str(sink_i, sink_j, EMPTY_DIR), false)
                });
                
                // Sink must have exactly one incoming direction (ALO)
                Clause in_alo;
                for (int d = 1; d <= 4; d++) {
                    in_alo.addLiteral(Literal(__in_lit_str(sink_i, sink_j, d), true));
                }
                source_sink_clauses.push_back(in_alo);
                
                // Sink must have no outgoing directions
                for (int d = 1; d <= 4; d++) {
                    add_clause_to(source_sink_clauses, {
                        Literal(__out_lit_str(sink_i, sink_j, d), false)
                    });
                }
                
                // Sink must have a direction (ALO on directions)  
                Clause dir_alo;
                for (int d = 1; d <= 4; d++) {
                    dir_alo.addLiteral(Literal(__dir_lit_str(sink_i, sink_j, d), true));
                }
                source_sink_clauses.push_back(dir_alo);
            }
        }

        // ============================ Reachability and Turn Limit Constraints ============================
        
        void encodeReachability() {
            // Create reachability variables and encode constraints
            
            // 1. Source Seeding: R(src_i, src_j, k) = TRUE
            for (int k = 0; k < K; k++) {
                auto& starts = metro_map.getLineStarts(k);
                
                for (auto& start : starts) {
                    int src_j = start.first;   // column
                    int src_i = start.second;  // row
                    
                    string reach_var = __reach_lit_str(src_i, src_j, k);
                    add_literal(reach_var);
                    
                    // Unit clause: R(src_i, src_j, k) = TRUE
                    add_clause_to(reachability_clauses, {
                        Literal(reach_var, true)
                    });
                }
            }
            
            // 2. Sink Assertion: R(sink_i, sink_j, k) = TRUE
            for (int k = 0; k < K; k++) {
                auto& ends = metro_map.getLineEnds(k);
                
                for (auto& end : ends) {
                    int sink_j = end.first;    // column
                    int sink_i = end.second;   // row
                    
                    string reach_var = __reach_lit_str(sink_i, sink_j, k);
                    add_literal(reach_var);
                    
                    // Unit clause: R(sink_i, sink_j, k) = TRUE
                    add_clause_to(reachability_clauses, {
                        Literal(reach_var, true)
                    });
                }
            }
            
            // 3. Propagation Rule: R(i,j,k) ∧ OUT(i,j,d) → R(ni,nj,k)
            // CNF: ¬R(i,j,k) ∨ ¬OUT(i,j,d) ∨ R(ni,nj,k)
            for (int i = 0; i < M; i++) {
                for (int j = 0; j < N; j++) {
                    for (int k = 0; k < K; k++) {
                        string reach_current = __reach_lit_str(i, j, k);
                        add_literal(reach_current);
                        
                        // For each direction this cell can output to
                        for (int d = 1; d <= 4; d++) { // RIGHT, UP, LEFT, DOWN
                            if (out_of_bounds(i, j, d)) continue;
                            
                            // Get neighbor coordinates
                            int ni, nj;
                            getNeighbor(i, j, d, ni, nj);
                            
                            string out_var = __out_lit_str(i, j, d);
                            string reach_neighbor = __reach_lit_str(ni, nj, k);
                            add_literal(reach_neighbor);
                            
                            // Propagation constraint: R(i,j,k) ∧ OUT(i,j,d) → R(ni,nj,k)
                            // CNF: ¬R(i,j,k) ∨ ¬OUT(i,j,d) ∨ R(ni,nj,k)
                            add_clause_to(reachability_clauses, {
                                Literal(reach_current, false),    // ¬R(i,j,k)
                                Literal(out_var, false),          // ¬OUT(i,j,d)
                                Literal(reach_neighbor, true)     // R(ni,nj,k)
                            });
                        }
                    }
                }
            }
        }

        void sinz_gen_amo_helper(vector<Literal> &lits, int line_k, int max_count) {
            int n = lits.size();
            
            if (n <= 1 || max_count >= n) return; // No constraint needed
            
            if (max_count == 0) {
                // Special case: forbid all literals
                for (auto& lit : lits) {
                    add_clause_to(turn_limit_clauses, {lit.getNegation()});
                }
                return;
            }
            
            // Create auxiliary variables s_{i,j} for i=0..n-1, j=0..max_count-1
            // Note: Converting from 1-based (paper) to 0-based (code)
            vector<vector<Literal>> s(n, vector<Literal>(max_count));
            
            for (int i = 0; i < n; i++) {
                for (int j = 0; j < max_count; j++) {
                    string aux_name = __turn_seq_name(line_k, i, j);
                    track_literal(aux_name);
                    s[i][j] = Literal(aux_name, true);
                }
            }
            
            // Clause 1: (¬x₀ ∨ s_{0,0})
            add_clause_to(turn_limit_clauses, {
                lits[0].getNegation(),
                s[0][0]
            });
            
            // Clause 2: (¬s_{0,j}) for 0 < j < max_count
            for (int j = 1; j < max_count; j++) {
                add_clause_to(turn_limit_clauses, {
                    s[0][j].getNegation()
                });
            }
            
            // Main clauses for 0 < i < n
            for (int i = 1; i < n - 1; i++) {
                // Clause 3: (¬x_i ∨ s_{i,0})
                add_clause_to(turn_limit_clauses, {
                    lits[i].getNegation(),
                    s[i][0]
                });
                
                // Clause 4: (¬s_{i-1,0} ∨ s_{i,0})
                add_clause_to(turn_limit_clauses, {
                    s[i-1][0].getNegation(),
                    s[i][0]
                });
                
                // Clauses 5 & 6: for 0 < j < max_count
                for (int j = 1; j < max_count; j++) {
                    // Clause 5: (¬x_i ∨ ¬s_{i-1,j-1} ∨ s_{i,j})
                    add_clause_to(turn_limit_clauses, {
                        lits[i].getNegation(),
                        s[i-1][j-1].getNegation(),
                        s[i][j]
                    });
                    
                    // Clause 6: (¬s_{i-1,j} ∨ s_{i,j})
                    add_clause_to(turn_limit_clauses, {
                        s[i-1][j].getNegation(),
                        s[i][j]
                    });
                }
                
                // Clause 7: (¬x_i ∨ ¬s_{i-1,max_count-1})
                add_clause_to(turn_limit_clauses, {
                    lits[i].getNegation(),
                    s[i-1][max_count-1].getNegation()
                });
            }
            
            // Final clause 8: (¬x_{n-1} ∨ ¬s_{n-2,max_count-1})
            if (n > 1) {
                add_clause_to(turn_limit_clauses, {
                    lits[n-1].getNegation(),
                    s[n-2][max_count-1].getNegation()
                });
            }
        }
    
        void encodeTurnLimits() {
            // Special case: J == 0, forbid all turns directly
            if (J == 0) {
                for (int k = 0; k < K; k++) {
                    for (int i = 0; i < M; i++) {
                        for (int j = 0; j < N; j++) {
                            add_clause_to(turn_limit_clauses, {
                                Literal(__turn_lit_str(i, j, k), false)
                            });
                        }
                    }
                }
                return;
            }

            // General case: J > 0
            for (int k = 0; k < K; k++) {
                vector<Literal> turn_vars_k;

                for (int i = 0; i < M; i++) {
                    for (int j = 0; j < N; j++) {
                        string turn_var = __turn_lit_str(i, j, k);
                        string reach_var = __reach_lit_str(i, j, k);
                        add_literal(turn_var);

                        // T(i,j,k) ⇒ R(i,j,k)
                        add_clause_to(turn_limit_clauses, {
                            Literal(turn_var, false),
                            Literal(reach_var, true)
                        });

                        // T(i,j,k) ⇒ (∃d: IN(i,j,d))
                        Clause in_alo;
                        in_alo.addLiteral(Literal(turn_var, false));
                        for (int d = 1; d <= 4; d++)
                            in_alo.addLiteral(Literal(__in_lit_str(i, j, d), true));
                        turn_limit_clauses.push_back(in_alo);

                        // T(i,j,k) ⇒ (∃d: OUT(i,j,d))
                        Clause out_alo;
                        out_alo.addLiteral(Literal(turn_var, false));
                        for (int d = 1; d <= 4; d++)
                            out_alo.addLiteral(Literal(__out_lit_str(i, j, d), true));
                        turn_limit_clauses.push_back(out_alo);

                        // Turn detection: non-straight pairs
                        for (int d1 = 1; d1 <= 4; d1++) {
                            for (int d2 = 1; d2 <= 4; d2++) {
                                if (d1 == getOppositeDir(d2)) {
                                    // Straight: forbid turn
                                    add_clause_to(turn_limit_clauses, {
                                        Literal(reach_var, false),
                                        Literal(__in_lit_str(i, j, d1), false),
                                        Literal(__out_lit_str(i, j, d2), false),
                                        Literal(turn_var, false)
                                    });
                                } else {

                                    if(d1 == d2) continue; // Skip same direction pairs

                                    // Non-straight: force turn
                                    add_clause_to(turn_limit_clauses, {
                                        Literal(reach_var, false),
                                        Literal(__in_lit_str(i, j, d1), false),
                                        Literal(__out_lit_str(i, j, d2), false),
                                        Literal(turn_var, true)
                                    });
                                }
                            }
                        }

                        turn_vars_k.push_back(Literal(turn_var, true));
                    }
                }

                // Apply generalized Sinz sequential counter: at most J turns for line k
                if (!turn_vars_k.empty()) {
                    sinz_gen_amo_helper(turn_vars_k, k, J);
                }
            }
        }
        // ============================ Popular City Constraints ============================

        void encodePopularCities() {
            // Get popular cities from metro map
            auto popular_cities = metro_map.getPopularCities();
            
            // Each popular city must be covered by at least one line
            // This means it cannot be empty (must have some direction)
            for (auto& city_v : popular_cities) {

                for(auto &city : city_v) {
                    int col = city.first;   // column (j) 
                    int row = city.second;  // row (i)
                    
                    // Popular city cannot be empty - must have at least one direction
                    // This is equivalent to: ALO(DIR_i_j_RIGHT, DIR_i_j_UP, DIR_i_j_LEFT, DIR_i_j_DOWN)
                    Clause coverage_clause;
                    for (int d = 1; d <= 4; d++) { // RIGHT, UP, LEFT, DOWN
                        string dir_var = __dir_lit_str(row, col, d);
                        coverage_clause.addLiteral(Literal(dir_var, true));
                    }
                    popular_city_clauses.push_back(coverage_clause);
                    
                    // Alternatively, we can express this as: ¬DIR_i_j_EMPTY
                    // add_clause_to(popular_city_clauses, {
                    //     Literal(__dir_lit_str(row, col, EMPTY_DIR), false)
                    // });
                }
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
        
        // Top level encoding method
        void encode() {

            // Classify cells first
            vector<pair<int,int>> sources, sinks, normals;
            classify_source_sink(sources, sinks, normals);

            encodeDirectionConstraints();
            encodeFlowConstraints(); 
            encodePathCoherence(normals);  // Pass normal cells only
            encodeSourceSinkConstraints(sources, sinks);  // Pass classified source/sink cells
            encodeReachability();
            encodeTurnLimits();
            encodePopularCities();

            // Combine all clause buckets
            clauses.insert(clauses.end(), direction_clauses.begin(), direction_clauses.end());
            clauses.insert(clauses.end(), flow_clauses.begin(), flow_clauses.end());
            clauses.insert(clauses.end(), path_coherence_clauses.begin(), path_coherence_clauses.end());
            clauses.insert(clauses.end(), source_sink_clauses.begin(), source_sink_clauses.end());
            clauses.insert(clauses.end(), reachability_clauses.begin(), reachability_clauses.end());
            clauses.insert(clauses.end(), turn_limit_clauses.begin(), turn_limit_clauses.end());
            clauses.insert(clauses.end(), popular_city_clauses.begin(), popular_city_clauses.end());
        }

        // Output CNF in DIMACS format

         // Output methods (add these back)
        void outputDIMACS(const string &filename) {
            ofstream out(filename, ios::out | ios::trunc);
            if (!out.is_open()) {
                cerr << "Error: Cannot open CNF output file: " << filename << endl;
                return;
            }

            // Write DIMACS header
            out << "p cnf " << var_count << " " << clauses.size() << '\n';

            // Emit clauses
            for (const auto &cl : clauses) {
                // Skip empty clauses if any (though none expected)
                if (cl.isEmpty()) {
                    out << 0 << '\n';
                    continue;
                }
                for (const auto &lit : cl.literals) {
                    auto it = literal_var_map.find(lit.getName());
                    if (it == literal_var_map.end()) {
                        // Fallback: assign an id if somehow missing
                        // (should not happen with proper tracking during encode)
                        track_literal(lit.getName());
                        it = literal_var_map.find(lit.getName());
                    }
                    int v = it->second;
                    out << (lit.isPositive() ? v : -v) << ' ';
                }
                out << 0 << '\n';
            }

            out.close();
        }

        void outputVariableMapping(const string &filename) {
            ofstream out(filename, ios::out | ios::trunc);
            if (!out.is_open()) {
                cerr << "Error: Cannot open mapping output file: " << filename << endl;
                return;
            }

            // Emit mappings in variable-id order for fast decoder lookup
            // literal_names is stored in creation order where index = id-1
            for (int id = 1; id <= var_count; ++id) {
                const string &name = literal_names[id - 1];
                out << id << ' ' << name << '\n';
            }

            out.close();
        }

        // Get clause count for debugging
        int getClauseCount() const {
            return clauses.size();
        }

        int getVariableCount() const {
            return var_count;
        }
};

// Main function
int main(int argc, char* argv[]) {
    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " <input_file> <output_cnf_file>" << endl;
        return 1;
    }

    string input_file = argv[1];
    string cnf_file = argv[2];

    // Parse input file
    ifstream input_stream(input_file);
    if (!input_stream.is_open()) {
        cerr << "Error: Cannot open input file: " << input_file << endl;
        return 1;
    }

    MetroMap metro_map = parseInputFile(input_stream);
    input_stream.close();

    cout << "Parsed input file successfully." << endl;
    metro_map.printInfo();

    // Create encoder and encode
    SATEncoder encoder(metro_map);
    cout << "Encoding problem to CNF..." << endl;
    encoder.encode();

    cout << "Generated " << encoder.getClauseCount() << " clauses with " 
         << encoder.getVariableCount() << " variables." << endl;

    // Output CNF
    encoder.outputDIMACS(cnf_file);
    
    // Output variable mapping for decoder
    string mapping_file = cnf_file + ".map";
    encoder.outputVariableMapping(mapping_file);
    cout << "Variable mapping written to " << mapping_file << endl;

    return 0;
}
