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

// Canonicalize (i,j,dir) to a unique undirected edge representation
static inline bool canonicalEdge(int i, int j, int dir, int& ci, int& cj, int& cdir) {
    if (dir == DIR_RIGHT) { ci = i; cj = j; cdir = 0; return true; }
    if (dir == DIR_DOWN)  { ci = i; cj = j; cdir = 1; return true; }
    if (dir == DIR_LEFT)  { ci = i; cj = j - 1; cdir = 0; return cj >= 0; }
    if (dir == DIR_UP)    { ci = i - 1; cj = j; cdir = 1; return ci >= 0; }
    return false;
}

static inline string edgeName(int i, int j, int k, int dir) {
    int ci, cj, cdir;
    if (!canonicalEdge(i, j, dir, ci, cj, cdir)) return "";
    ostringstream ss;
    ss << "E_" << ci << "_" << cj << "_" << k << "_" << cdir;
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
        // At most one metro line uses a given cell (i,j) across all k).
        for (int i = 0; i < M; ++i) {
            for (int j = 0; j < N; ++j) {
                // Build derived presence vars P(i,j,k) from incident edges
                vector<Literal> Pk; Pk.reserve(K);
                for (int k = 0; k < K; ++k) {
                    vector<pair<int,Literal>> edges;
                    collectIncidentCanonicalEdges(i, j, k, edges);

                    vector<Literal> incE; incE.reserve(edges.size());
                    for (auto &pr: edges) incE.push_back(pr.second);

                    if (incE.empty()) continue; // cell unusable for this k

                    // Define P(i,j,k) ↔ OR incident edges (directionless presence)
                    definePresenceVarFromEdges(i, j, k, incE);
                    Pk.push_back(presenceVar(i,j,k));
                }
                // Run AMO (sequential) across the P(i,j,k)
                // addAtMostOneSequential(Pk);
                addAtMostOne(Pk);
            }
        }
    }


    void encodeLineFlow(int k) {
        auto &starts = metro_map.getLineStarts(k);
        auto &ends   = metro_map.getLineEnds(k);
        int src_i = starts.second, src_j = starts.first;
        int sink_i = ends.second,   sink_j = ends.first;

        for (int i = 0; i < M; ++i) {
            for (int j = 0; j < N; ++j) {
                bool is_source = (i == src_i && j == src_j);
                bool is_sink   = (i == sink_i && j == sink_j);

                vector<pair<int,Literal>> edges;
                collectIncidentCanonicalEdges(i, j, k, edges);

                vector<Literal> E; E.reserve(edges.size());
                for (auto &pr: edges) E.push_back(pr.second);

                if (is_source || is_sink) {
                    // Endpoints must have degree exactly 1
                    addAtLeastOne(E);       // deg ≥ 1
                    // pairwise AMO on endpoint is tiny (≤4); keep it simple
                    addAtMostOne(E);        // deg ≤ 1
                    // No TURN clause at endpoints (optional, doesn’t hurt)
                    continue;
                }

                // Interiors:
                if ((int)E.size() >= 2) {
                    // Edge-only degree=2 semantics for *used* cells:
                    // Forbid degree 1 and 3; allow degree 0 or 2.
                    interiorDegreeExactlyTwo_edgeOnly(E);
                    // TURN from edges only
                    encodeTurnFromEdges_edgeOnly(i, j, k, edges);
                }
                // If E.size()<2, the cell can’t be used — nothing to constrain.
            }
        }
    }


    // ---- NEW: Linear AMO (Sinz / ladder) ----
    void addAtMostOneSequential(const vector<Literal>& lits) {
        const int n = (int)lits.size();
        if (n <= 1) return;
        // s_1,...,s_{n-1}
        vector<int> s(n - 1);
        for (int i = 0; i < n - 1; ++i) {
            ostringstream ss; ss << "S_AMO_" << clauses.size() << "_" << i;
            s[i] = getVar(ss.str());
        }
        // (¬x1 ∨ s1)
        addClause(Clause{ lits[0].neg(), Literal(s[0]) });
        // For i=2..n-1:
        for (int i = 1; i < n - 1; ++i) {
            // (¬x_{i+1} ∨ s_i)
            addClause(Clause{ lits[i].neg(), Literal(s[i]) });
            // (¬s_{i-1} ∨ s_i)
            addClause(Clause{ Literal(s[i-1]).neg(), Literal(s[i]) });
            // (¬x_i ∨ ¬s_{i-1})
            addClause(Clause{ lits[i].neg(), Literal(s[i-1]).neg() });
        }
        // (¬x_n ∨ ¬s_{n-2})
        addClause(Clause{ lits[n-1].neg(), Literal(s[n-2]).neg() });
    }

    // ---- NEW: Incident canonical edge literals around (i,j) for line k ----
    void collectIncidentCanonicalEdges(int i, int j, int k,
                                    vector<pair<int,Literal>>& edges) {
        edges.clear(); edges.reserve(4);
        auto push_if = [&](int dir) {
            int ci, cj, cdir;
            if (!canonicalEdge(i, j, dir, ci, cj, cdir)) return;
            // Neighbor must be in-bounds for the original step
            int ni, nj; if (!step(i, j, dir, ni, nj)) return;
            string en = edgeName(i, j, k, dir);
            if (en.empty()) return;
            edges.push_back({dir, litByName(en)});
        };
        push_if(DIR_RIGHT);
        push_if(DIR_LEFT);
        push_if(DIR_UP);
        push_if(DIR_DOWN);
    }

    // ---- NEW: Edge-only interior constraints (no X guard) ----
    void interiorDegreeExactlyTwo_edgeOnly(const vector<Literal>& E) {
        const int d = (int)E.size(); // d<=4
        if (d < 2) return;           // Cell simply cannot be used; nothing to constrain

        // No degree-1: for each e, (¬e ∨ (∨ others))
        for (int t = 0; t < d; ++t) {
            Clause c; c.addLiteral(E[t].neg());
            for (int u = 0; u < d; ++u) if (u != t) c.addLiteral(E[u]);
            addClause(c);
        }
        // AtMostTwo: forbid any triple of edges (d<=4 => at most 4 clauses)
        for (int a = 0; a < d; ++a)
            for (int b = a+1; b < d; ++b)
                for (int c = b+1; c < d; ++c)
                    addClause(Clause{ E[a].neg(), E[b].neg(), E[c].neg() });
    }

    // ---- NEW: TURN from edges only (no X) ----
    Literal turnVar(int i,int j,int k){ return litByName(turnName(i,j,k)); }

    void encodeTurnFromEdges_edgeOnly(int i,int j,int k,
                                    const vector<pair<int,Literal>>& edges) {
        // Identify available directions
        Literal Er(0,false), El(0,false), Eu(0,false), Ed(0,false);
        bool hr=false, hl=false, hu=false, hd=false;
        for (auto &pr: edges) {
            if (pr.first==DIR_RIGHT){ Er=pr.second; hr=true; }
            if (pr.first==DIR_LEFT ){ El=pr.second; hl=true; }
            if (pr.first==DIR_UP   ){ Eu=pr.second; hu=true; }
            if (pr.first==DIR_DOWN ){ Ed=pr.second; hd=true; }
        }
        bool hasH = (hr||hl), hasV=(hu||hd);
        if (!(hasH && hasV)) return;

        Literal T = turnVar(i,j,k);

        // Straight ⇒ ¬T
        if (hu && hd) addClause(Clause{ Eu.neg(), Ed.neg(), T.neg() });
        if (hl && hr) addClause(Clause{ El.neg(), Er.neg(), T.neg() });

        // Any orthogonal pair ⇒ T
        if (hu && hl) addClause(Clause{ Eu.neg(), El.neg(), T });
        if (hu && hr) addClause(Clause{ Eu.neg(), Er.neg(), T });
        if (hd && hl) addClause(Clause{ Ed.neg(), El.neg(), T });
        if (hd && hr) addClause(Clause{ Ed.neg(), Er.neg(), T });

        // Track once per cell
        if (line_turn_vars[k].empty() || line_turn_vars[k].back()!=T.id)
            line_turn_vars[k].push_back(T.id);
    }

    // ---- NEW: helper for a derived presence var P(i,j,k) used for AMO/popular cities
    // P(i,j,k) is TRUE iff any incident edge of line k touches (i,j).
    Literal presenceVar(int i,int j,int k){
        ostringstream ss; ss << "P_"<<i<<"_"<<j<<"_"<<k;
        return litByName(ss.str());
    }
    void definePresenceVarFromEdges(int i,int j,int k,
                                    const vector<Literal>& incE) {
        if (incE.empty()) return; // unreachable cell
        Literal P = presenceVar(i,j,k);
        // (P → OR E)
        {
            Clause c; c.addLiteral(P.neg());
            for (auto &e: incE) c.addLiteral(e);
            addClause(c);
        }
        // (E → P) for all incident edges
        for (auto &e: incE) addClause(Clause{ e.neg(), P });
    }

    // Collect available edges (real 4 dirs). Wire E→X, E→X(neigh), using canonical edge representation.
    void collectEdgesAndWire(int i, int j, int k, Literal X,
                         vector<pair<int, Literal>> &edges) {
        for (int dir = DIR_RIGHT; dir <= DIR_DOWN; ++dir) {
            int ni, nj;
            if (!step(i, j, dir, ni, nj)) continue;

            // Canonical edge var (same ID from either endpoint / direction)
            string edge_name = edgeName(i, j, k, dir);
            if (edge_name.empty()) continue;
            Literal E  = litByName(edge_name);
            Literal Xn = litByName(occName(ni, nj, k));

            // Always expose this direction to the degree/turn code:
            edges.push_back({dir, E});

            // BUT emit the edge->occupancy implications only once per undirected edge:
            const bool is_canonical_here = (dir == DIR_RIGHT || dir == DIR_DOWN);
            if (is_canonical_here) {
                addImplication(E, X);   // E -> X(i,j,k)
                addImplication(E, Xn);  // E -> X(ni,nj,k)
            }
        }
    }

    // Interiors: enforce degree exactly 2 when X is true, on the *available* sides.
    void interiorDegreeExactlyTwo(Literal X, const vector<pair<int, Literal>> &edges) {
        vector<Literal> E; E.reserve(edges.size());
        for (auto &pr : edges) E.push_back(pr.second);

        // X → (ΣE ≥ 1)
        addGuardedAtLeastOne(X, E);

        // Forbid degree = 1 when X is true:
        // (X ∧ E_d) → (∨_{d'≠d} E_{d'})  →  (¬X ∨ ¬E_d ∨ E_o1 ∨ E_o2 ∨ E_o3)
        for (size_t t = 0; t < E.size(); ++t) {
            Clause c; c.addLiteral(X.neg()); c.addLiteral(E[t].neg());
            for (size_t u = 0; u < E.size(); ++u) if (u != t) c.addLiteral(E[u]);
            addClause(c);
        }

        // At most two: forbid any triple of edges simultaneously true.
        for (size_t a = 0; a < E.size(); ++a)
            for (size_t b = a + 1; b < E.size(); ++b)
                for (size_t c = b + 1; c < E.size(); ++c)
                    addClause(Clause{ E[a].neg(), E[b].neg(), E[c].neg() });
    }

    // Turn = 1  iff one vertical edge and one horizontal edge are chosen (under deg=2).
    void encodeTurnFromEdges(int i, int j, int k,
                            const vector<pair<int, Literal>> &edges, Literal X) {
        // Find per-direction edges if present.
        Literal Er(0,false), El(0,false), Eu(0,false), Ed(0,false);
        bool hr=false, hl=false, hu=false, hd=false;
        for (auto &[dir, E] : edges) {
            if (dir == DIR_RIGHT) { Er = E; hr = true; }
            if (dir == DIR_LEFT)  { El = E; hl = true; }
            if (dir == DIR_UP)    { Eu = E; hu = true; }
            if (dir == DIR_DOWN)  { Ed = E; hd = true; }
        }
        bool hasH = (hr || hl), hasV = (hu || hd);
        if (!(hasH && hasV)) return;  // Without both orientations, a turn is impossible.

        Literal T = litByName(turnName(i, j, k));
        addImplication(T, X); // helpful propagation

        // Straight ⇒ ¬T
        if (hu && hd) addClause(Clause{ Eu.neg(), Ed.neg(), T.neg() });
        if (hl && hr) addClause(Clause{ El.neg(), Er.neg(), T.neg() });

        // Any orthogonal pair ⇒ T  (enumerate only edges that exist)
        if (hu && hl) addClause(Clause{ Eu.neg(), El.neg(), T });
        if (hu && hr) addClause(Clause{ Eu.neg(), Er.neg(), T });
        if (hd && hl) addClause(Clause{ Ed.neg(), El.neg(), T });
        if (hd && hr) addClause(Clause{ Ed.neg(), Er.neg(), T });

        // Track for per-line turn budget
        line_turn_vars[k].push_back(T.id);
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
        int Pgroups = metro_map.getPopularCitiesCount();
        if (Pgroups <= 0) return;

        auto &groups = metro_map.getPopularCities();
        for (int p = 0; p < Pgroups; ++p) {
            Clause coverage; // big OR over all k and incident edges at those cells
            for (auto &cell : groups) {
                int col = cell.first, row = cell.second;
                if (!inBounds(row, col)) continue;
                for (int k = 0; k < K; ++k) {
                    vector<pair<int,Literal>> edges;
                    collectIncidentCanonicalEdges(row, col, k, edges);
                    for (auto &pr: edges) coverage.addLiteral(pr.second);
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
        // out << "c Re-implemented SAT encoding" << '\n';
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
