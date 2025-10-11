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


// Per-cell per-line rank (level) literal: O_(i,j,k,t) means
// "cell (i,j) of line k has rank ≥ t"
string __ord_lit_str(int i, int j, int k, int t) {
    ostringstream ss;
    ss << "O_" << i << "_" << j << "_" << k << "_" << t;
    return ss.str();
}

string __rank_bit_lit_str(int i, int j, int k, int b) {
    ostringstream ss; ss << "RB_" << i << "_" << j << "_" << k << "_" << b; return ss.str();
}

string __eqpref_lit_str(int i, int j, int k, int dir, int p) {
    ostringstream ss; ss << "EQP_" << i << "_" << j << "_" << k << "_" << dir << "_" << p; return ss.str();
}

string __ltw_lit_str(int i, int j, int k, int dir, int p) {
    ostringstream ss; ss << "LTW_" << i << "_" << j << "_" << k << "_" << dir << "_" << p; return ss.str();
}

// string helper methods for turn limit encoding can be added here

string __turn_lit_str(int i, int j, int k) {
    ostringstream ss;
    ss << "T_" << i << "_" << j << "_" << k;
    return ss.str();
}

// Sequential counter s(i,t) for turns of line k (1-based i,t)
string __turn_seq_name(int k, int i, int t){
    ostringstream ss; ss << "TS_" << k << "_" << i << "_" << t; return ss.str();
}
// Optional per-line prefix if you later add more families
string __turn_prefix(int k){
    ostringstream ss; ss << "TL_k" << k << "_"; return ss.str();
}


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

        bool out_of_bound_rev(int ni, int nj, int dir) {
            // To Check if (i,j) ---dir---> (ni,nj) is valid
            switch (dir) {
            case RIGHT:
                return nj - 1 < 0; // Out of bounds if moving left goes below column 0
            case UP:
                return ni + 1 >= M; // Out of bounds if moving down exceeds row limit
            case LEFT:
                return nj + 1 >= N; // Out of bounds if moving right exceeds column limit
            case DOWN:
                return ni - 1 < 0; // Out of bounds if moving up goes above row 0
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

        void add_clause_to(vector<Clause>& bucket, std::initializer_list<Literal> lits){
            Clause c; for (auto &L : lits) c.addLiteral(L); bucket.push_back(c);
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

            // Step1 : Implement Directionality Guards
            Literal lit_occ(__occ_lit_str(i, j, k), TRUE);
            for (int dir = 1; dir <= 4; dir++) {
                if (out_of_bounds(i,j,dir)) continue;
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
                // if(ni == sink_i && nj == sink_j) continue;

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
                    Literal lit_in(__in_lit_str(ni, nj, k, getOppositeDirIndex(dir)), TRUE);

                    Clause cl1; cl1.addLiteral(lit_out.getNegation()); cl1.addLiteral(lit_in);
                    directionality_clauses.push_back(cl1);

                    // if (ni == sink_i && nj == sink_j) continue;

                    Literal lit_occ_neigh(__occ_lit_str(ni, nj, k), TRUE);
                    Clause cl2; cl2.addLiteral(lit_out.getNegation()); cl2.addLiteral(lit_occ_neigh);
                    directionality_clauses.push_back(cl2);

                    // In(i,j,dir) ⇒ Out(neigh, opp)    : (¬In(i,j,dir) ∨ Out(ni,nj,opp))
                    {
                        Clause c;
                        c.addLiteral(Literal(__in_lit_str(i, j, k, dir), false));
                        c.addLiteral(Literal(__out_lit_str(ni, nj, k, getOppositeDirIndex(dir)), true));
                        directionality_clauses.push_back(c);
                    }

                    // In(i,j,dir) ⇒ X(neigh)           : (¬In(i,j,dir) ∨ X(ni,nj))
                    {
                        Clause c;
                        c.addLiteral(Literal(__in_lit_str(i, j, k, dir), false));
                        c.addLiteral(Literal(__occ_lit_str(ni, nj, k), true));
                        directionality_clauses.push_back(c);
                    }
                }
            }

            // Step 3 : Degree Constraints

            // Build lists of In*/Out* names
            vector<string> INs, OUTs;
            for (int d = 1; d <= 4; ++d) {
                if (out_of_bounds(i,j,d)) continue;
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

                // Pairwise AMO on Out*
                for (int a = 0; a < (int)OUTs.size(); ++a){
                    for (int b = a+1; b < (int)OUTs.size(); ++b) {
                        Clause c; c.addLiteral(Literal(OUTs[a], false));
                                c.addLiteral(Literal(OUTs[b], false));
                        directionality_clauses.push_back(c);
                    }
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
                for (int a = 0; a < (int)INs.size(); ++a){
                    for (int b = a+1; b < (int)INs.size(); ++b) {
                        Clause c; c.addLiteral(Literal(INs[a], false));
                                c.addLiteral(Literal(INs[b], false));
                        directionality_clauses.push_back(c);
                    }
                }
                // Forbid all Out*
                for (auto &s : OUTs) { Clause c; c.addLiteral(Literal(s, false)); directionality_clauses.push_back(c); }
            }
            else {
                // If occupied ⇒ at least one In*
                { Clause c; c.addLiteral(Literal(Xij, false)); for (auto &s : INs) c.addLiteral(Literal(s, true)); directionality_clauses.push_back(c); }
                // If occupied ⇒ at least one Out*
                { Clause c; c.addLiteral(Literal(Xij, false)); for (auto &s : OUTs) c.addLiteral(Literal(s, true)); directionality_clauses.push_back(c); }

                for (int a = 0; a < (int)OUTs.size(); ++a){
                    for (int b = a+1; b < (int)OUTs.size(); ++b) {
                        Clause c; 
                        c.addLiteral(Literal(OUTs[a], false));
                        c.addLiteral(Literal(OUTs[b], false));
                        directionality_clauses.push_back(c);
                    }
                }
                    
                for (int a = 0; a < (int)INs.size(); ++a){
                    for (int b = a+1; b < (int)INs.size(); ++b) {
                        Clause c; 
                        c.addLiteral(Literal(INs[a], false));
                        c.addLiteral(Literal(INs[b], false));
                        directionality_clauses.push_back(c);
                    }
                }

            }
        }


        // Builds ONLY direction/degree/neighbor-coherence constraints.
        // No R(*), no witnesses. Occupancy AMO is handled in encodeOccupancy().
        // Acyclicity will be added separately in encodeAcyclicity().
        // Builds direction/degree/neighbor-coherence (via local_constraints)
        // + Horn reachability:
        //
        //  Seed: R(src) = true
        //  Seed: R(sink) = true
        //  Forward Horn: (¬R(u) ∨ ¬Out(u→v) ∨ R(v))
        //  Tie for non-sink cells: (¬X(u) ∨ R(u))
        //
        void encodeReachability() {
            for (int k = 0; k < K; ++k) {
                // (x,y) stored as (col,row); we use (i=row, j=col)
                auto &S = metro_map.getLineStarts(k);
                auto &E = metro_map.getLineEnds(k);
                int src_i  = S[0].second, src_j  = S[0].first;
                int sink_i = E[0].second, sink_j = E[0].first;

                // Seed R at source and sink
                {
                    string Rsrc  = __reach_lit_str(src_i,  src_j,  k);
                    string Rsink = __reach_lit_str(sink_i, sink_j, k);
                    track_literal(Rsrc);
                    track_literal(Rsink);
                    { Clause c; c.addLiteral(Literal(Rsrc,  true)); reachability_clauses.push_back(c); }
                    { Clause c; c.addLiteral(Literal(Rsink, true)); reachability_clauses.push_back(c); }
                }

                // Per cell: structural constraints + Horn rules
                for (int i = 0; i < M; ++i) {
                    for (int j = 0; j < N; ++j) {
                        // Make sure R(i,j,k) exists
                        track_literal(__reach_lit_str(i, j, k));

                        // All the local guards / neighbor coherence / degree constraints
                        // (includes Out(u→v) ⇒ In(v←u) and Out(u→v) ⇒ X(v))
                        local_constraints(i, j, k, src_i, src_j, sink_i, sink_j);

                        // Tie occupancy to reachability for NON-SINK cells:
                        // (¬X(i,j,k) ∨ R(i,j,k))
                        if (!(i == sink_i && j == sink_j)) {
                            Clause c;
                            c.addLiteral(Literal(__occ_lit_str(i, j, k), false));
                            c.addLiteral(Literal(__reach_lit_str(i, j, k), true));
                            reachability_clauses.push_back(c);
                        }

                        // Forward Horn propagation:
                        // (¬R(i,j,k) ∨ ¬Out(i,j,k,dir) ∨ R(ni,nj,k))
                        for (int dir = 1; dir <= 4; ++dir) {
                            if (out_of_bounds(i, j, dir)) continue;
                            int ni = i, nj = j;
                            if      (dir == RIGHT) nj += 1;
                            else if (dir == UP)    ni -= 1;
                            else if (dir == LEFT)  nj -= 1;
                            else                   ni += 1; // DOWN

                            // ensure vars exist
                            track_literal(__out_lit_str(i, j, k, dir));
                            track_literal(__reach_lit_str(ni, nj, k));

                            Clause c;
                            c.addLiteral(Literal(__reach_lit_str(i, j, k), false));    // ¬R(u)
                            c.addLiteral(Literal(__out_lit_str(i, j, k, dir), false)); // ¬Out(u→v)
                            c.addLiteral(Literal(__reach_lit_str(ni, nj, k), true));   // R(v)
                            reachability_clauses.push_back(c);
                        }
                    }
                }
            }
        }

        int numRankBits() const {
            long long T = 1LL * M * N;   // upper bound on path length
            int B = 0; while ((1LL << B) <= T) ++B;
            return max(1, B);
        }

        // For every used edge Out(u->v), enforce rank(u) < rank(v) in binary.
        // This forbids all directed cycles.
        void encodeAcyclicityBinary() {
            const int B = numRankBits();

            auto RB  = [&](int i,int j,int k,int b){ return __rank_bit_lit_str(i,j,k,b); };
            auto EQP = [&](int i,int j,int k,int d,int p){ return __eqpref_lit_str(i,j,k,d,p); };
            auto LTW = [&](int i,int j,int k,int d,int p){ return __ltw_lit_str(i,j,k,d,p); };

            for (int k = 0; k < K; ++k) {
                // Declare rank bits for all cells of line k (guarded later by edges)
                for (int i = 0; i < M; ++i) for (int j = 0; j < N; ++j)
                    for (int b = 0; b < B; ++b) track_literal(RB(i,j,k,b));

                // For each directed in-bounds edge u=(i,j) -> v
                for (int i = 0; i < M; ++i) for (int j = 0; j < N; ++j) {
                    for (int dir = 1; dir <= 4; ++dir) {
                        if (out_of_bounds(i,j,dir)) continue;

                        int ni=i, nj=j;
                        if (dir==RIGHT) nj++; else if (dir==UP) ni--; else if (dir==LEFT) nj--; else ni++;
                        string OUT = __out_lit_str(i,j,k,dir);
                        track_literal(OUT);

                        // EqPref seed and chain: EqPref(B-1)=true, then push down
                        for (int p = 0; p < B; ++p) track_literal(EQP(i,j,k,dir,p));
                        { Clause c; c.addLiteral(Literal(EQP(i,j,k,dir,B-1), true));
                        reachability_clauses.push_back(c); }

                        for (int p = B-1; p >= 1; --p) {
                            string eqp   = EQP(i,j,k,dir,p);
                            string eqpm1 = EQP(i,j,k,dir,p-1);
                            string au    = RB(i,j,k,p);
                            string bv    = RB(ni,nj,k,p);

                            // eqpm1 => eqp
                            { Clause c; c.addLiteral(Literal(eqpm1, false));
                                        c.addLiteral(Literal(eqp,   true)); reachability_clauses.push_back(c); }
                            // eqpm1 => (au <-> bv)
                            { Clause c; c.addLiteral(Literal(eqpm1, false));
                                        c.addLiteral(Literal(au,    true));
                                        c.addLiteral(Literal(bv,    false)); reachability_clauses.push_back(c); }
                            { Clause c; c.addLiteral(Literal(eqpm1, false));
                                        c.addLiteral(Literal(au,    false));
                                        c.addLiteral(Literal(bv,    true));  reachability_clauses.push_back(c); }
                            // (eqp & au==bv) => eqpm1
                            { Clause c; c.addLiteral(Literal(eqp,  false));
                                        c.addLiteral(Literal(au,    false));
                                        c.addLiteral(Literal(bv,    false));
                                        c.addLiteral(Literal(eqpm1, true));  reachability_clauses.push_back(c); }
                            { Clause c; c.addLiteral(Literal(eqp,  false));
                                        c.addLiteral(Literal(au,    true));
                                        c.addLiteral(Literal(bv,    true));
                                        c.addLiteral(Literal(eqpm1, true));  reachability_clauses.push_back(c); }
                        }

                        // LTW(p) <-> EqPref(p) & ~a_p & b_p  (witness that first differing bit makes u<v)
                        for (int p = B-1; p >= 0; --p) {
                            string eqp = EQP(i,j,k,dir,p);
                            string ap  = RB(i,j,k,p);
                            string bp  = RB(ni,nj,k,p);
                            string wp  = LTW(i,j,k,dir,p);
                            track_literal(wp);
                            { Clause c; c.addLiteral(Literal(wp, false)); c.addLiteral(Literal(eqp, true)); reachability_clauses.push_back(c); }
                            { Clause c; c.addLiteral(Literal(wp, false)); c.addLiteral(Literal(ap,  false));reachability_clauses.push_back(c); }
                            { Clause c; c.addLiteral(Literal(wp, false)); c.addLiteral(Literal(bp,  true ));reachability_clauses.push_back(c); }
                            { Clause c; c.addLiteral(Literal(eqp, false));
                                        c.addLiteral(Literal(ap,   true ));
                                        c.addLiteral(Literal(bp,   false));
                                        c.addLiteral(Literal(wp,   true )); reachability_clauses.push_back(c); }
                        }

                        // Out(u->v) => OR_p LTW(p)
                        { Clause c; c.addLiteral(Literal(OUT, false));
                                    for (int p = B-1; p >= 0; --p)
                                        c.addLiteral(Literal(LTW(i,j,k,dir,p), true));
                        reachability_clauses.push_back(c); }
                    }
                }
            }
        }


        // Helper methods for encoding turn limit constraints

        void turn_limit_constraints(int i, int j, int k) {
            // Implement turn limit constraints for cell (i, j) and line k
            
            string Tij = __turn_lit_str(i, j, k); track_literal(Tij);
            string Xij = __occ_lit_str(i, j, k); track_literal(Xij);

            vector<int> dirs; for(int d = 1; d <= 4; d++) if(!out_of_bounds(i,j,d)) dirs.push_back(d);

            vector<int> in_dirs, out_dirs; // Valid incoming and outgoing directions
            for(int d : dirs) {
                if(!out_of_bound_rev(i,j,d)) in_dirs.push_back(d);
                if(!out_of_bounds(i,j,d)) out_dirs.push_back(d);
            } 

            // Build IN/OUT name lists (only in-bounds dirs)
            vector<string> INs, OUTs;
            for(int d : in_dirs)  { string s = __in_lit_str(i,j,k,d); track_literal(s); INs.push_back(s); }
            for(int d : out_dirs) { string s = __out_lit_str(i,j,k,d); track_literal(s); OUTs.push_back(s); }

            // 1) Turn pairs imply T: for all d1,d2 with d1 != opp(d2)
            for(int d1 : in_dirs) {
                for(int d2 : out_dirs) {
                    if(d1 == getOppositeDirIndex(d2)) continue; // No turn
                    // (¬In(i,j,d1) ∨ ¬Out(i,j,d2) ∨ T(i,j))
                    Clause c; 
                    c.addLiteral(Literal(__in_lit_str(i,j,k,d1), false));
                    c.addLiteral(Literal(__out_lit_str(i,j,k,d2), false));
                    c.addLiteral(Literal(Tij, true));
                    turn_clauses.push_back(c);
                }
            }

            // 2) Straight forbids T: for each straight pair d vs opp(d)
            for (int d1 : in_dirs) {
                int d2 = getOppositeDirIndex(d1);
                if (out_of_bounds(i,j,d2)) continue;
                // (¬In(i,j,d1) ∨ ¬Out(i,j,d2) ∨ ¬T(i,j))
                Clause c;
                c.addLiteral(Literal(__in_lit_str(i,j,k,d1), false));
                c.addLiteral(Literal(__out_lit_str(i,j,k,d2), false));
                c.addLiteral(Literal(Tij, false));
                turn_clauses.push_back(c);
            }

            // 3) Tie T to occupancy as well as In and Out 

            Clause c_occ; c_occ.addLiteral(Literal(Tij, false)); c_occ.addLiteral(Literal(Xij, true)); turn_clauses.push_back(c_occ);

            Clause c_in;  c_in.addLiteral(Literal(Tij, false));  for (auto &s: INs)  c_in.addLiteral(Literal(s, true));  turn_clauses.push_back(c_in);
            Clause c_out; c_out.addLiteral(Literal(Tij, false)); for (auto &s: OUTs) c_out.addLiteral(Literal(s, true)); turn_clauses.push_back(c_out);

            return;

        }   

        void sinz_amo_J_turns(int k, vector<string> &Tks, int J) {
            int n = Tks.size();
            
            if (J == 0) {
                // All must be false 
                for (auto &x : Tks) {
                    Clause c; c.addLiteral(Literal(x, false)); turn_clauses.push_back(c);
                    return;
                }
            }

            vector<vector<string>> S_lit_str (n+1, vector<string>(J+1));

            
            // Build s(i,t) literals for i = 1..n, t = 1..J

            for(int i = 1; i <= n; i++) {
                for(int t = 1; t <= J; t++) {
                    string sinz_name = __turn_seq_name(k,i,t);
                    track_literal(sinz_name);
                    S_lit_str[i][t] = sinz_name;
                }
            }

            auto S = [&](int i, int t){ return S_lit_str[i][t]; };

            // Initialisation x1 => s(1,1) and forbid s(1,t) for t>1
            {
                Clause c; c.addLiteral(Literal(Tks[0], false)); c.addLiteral(Literal(S(1,1), true)); turn_clauses.push_back(c);
                for(int t = 2; t <= J; t++) {
                    Clause c2; c2.addLiteral(Literal(S(1, t), false)); turn_clauses.push_back(c2);
                }
            }

            // Relation for i = 2..

            for(int i = 2; i <= n; i++) {
                // xi => s(i,1)
                {
                    Clause c; c.addLiteral(Literal(Tks[i-1], false)); c.addLiteral(Literal(S(i,1), true)); turn_clauses.push_back(c);
                }

                // s(i-1,1) => s(i,1) 
                {
                    Clause c; c.addLiteral(Literal(S(i-1,1), false)); c.addLiteral(Literal(S(i,1), true)); turn_clauses.push_back(c);
                }

                // (xi & s(i-1,t-1)) => s(i,t) for t = 2..J
                for(int t = 2; t <= J; t++) {
                    // s(i-1,t) => s(i,t)
                    { Clause c; c.addLiteral(Literal(S(i-1,t), false)); c.addLiteral(Literal(S(i,t), true));
                    turn_clauses.push_back(c); }
                    // (x_i ∧ s(i-1,t-1)) => s(i,t)  as (¬x_i ∨ ¬s(i-1,t-1) ∨ s(i,t))
                    { Clause c; c.addLiteral(Literal(Tks[i-1], false)); c.addLiteral(Literal(S(i-1,t-1), false)); c.addLiteral(Literal(S(i,t), true));
                    turn_clauses.push_back(c); }
                }

                // Forbid s(i, J+1) implicitly: (x_i ∧ s(i-1,J)) ⇒ false => (¬x_i ∨ ¬s(i-1,J))
                { Clause c; c.addLiteral(Literal(Tks[i-1], false)); c.addLiteral(Literal(S(i-1,J), false));
                turn_clauses.push_back(c); }
                
            }

        }

        void encodeTurnLimit() {
            // Encode turn limit constraints here
            // Ensure that no line exceeds the turn limit
            // Placeholder implementation
            for(int k = 0; k < K; k++) {
                vector<string> Tks; Tks.reserve(M*N); // Collect all T(i,j,k) literals for line k
                for (int i = 0; i < M; i++) {
                    for(int j = 0; j < N; j++) {
                        // Example clause (to be replaced with actual turn limit logic)
                        turn_limit_constraints(i, j, k);

                        // Setup T(i,j,k) literal
                        string Tij = __turn_lit_str(i, j, k);
                        Tks.push_back(Tij);
                        // No need to track here, done in turn_limit_constraints()
                    }
                }

                sinz_amo_J_turns(k, Tks, J);
            }
        }


        // Top level function to encode the problem into CNF
        void encode() {
            encodeOccupancy();
            encodeReachability();
            // encodeTurnLimit();
            encodeAcyclicityBinary();

            // Combine all clauses
            clauses.insert(clauses.end(), occupancy_clauses.begin(), occupancy_clauses.end());
            clauses.insert(clauses.end(), reachability_clauses.begin(), reachability_clauses.end());
            clauses.insert(clauses.end(), directionality_clauses.begin(), directionality_clauses.end());
            clauses.insert(clauses.end(), turn_clauses.begin(), turn_clauses.end());
        }

        // Output CNF in DIMACS format
        void outputDIMACS(const string &filename) {
            ofstream file(filename);
            if (!file.is_open()) {
                cerr << "Error: Cannot create CNF file: " << filename << endl;
                return;
            }

            // Write header
            file << "p cnf " << var_count << " " << clauses.size() << endl;

            // Write clauses
            for (const auto &clause : clauses) {
                for (const auto &literal : clause.literals) {
                    string name = literal.getName();
                    if (literal_var_map.find(name) != literal_var_map.end()) {
                        int var_num = literal_var_map[name];
                        if (!literal.isPositive()) {
                            file << "-";
                        }
                        file << var_num << " ";
                    }
                }
                file << "0" << endl;
            }

            file.close();
            cout << "CNF written to " << filename << " with " << var_count << " variables and " << clauses.size() << " clauses." << endl;
        }

        // Create mapping file for decoder
        void outputVariableMapping(const string &filename) {
            ofstream file(filename);
            if (!file.is_open()) {
                cerr << "Error: Cannot create mapping file: " << filename << endl;
                return;
            }

            for (const auto &pair : literal_var_map) {
                file << pair.second << " " << pair.first << endl;
            }

            file.close();
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