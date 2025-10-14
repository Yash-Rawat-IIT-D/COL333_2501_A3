#include <bits/stdc++.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include "utils.hpp"
using namespace std;

const int DIR_RIGHT = 1;
const int DIR_UP    = 2;
const int DIR_LEFT  = 3;
const int DIR_DOWN  = 4;

// Use the same canonicalization as the encoder: a single variable per
// undirected edge, anchored on RIGHT/DOWN from a canonical cell.
static inline bool canonicalEdge(int i, int j, int dir, int &ci, int &cj, int &cdir) {
    if (dir == DIR_RIGHT) { ci = i; cj = j;     cdir = 0; return true; }
    if (dir == DIR_DOWN)  { ci = i; cj = j;     cdir = 1; return true; }
    if (dir == DIR_LEFT)  { ci = i; cj = j - 1; cdir = 0; return (cj >= 0); }
    if (dir == DIR_UP)    { ci = i - 1; cj = j; cdir = 1; return (ci >= 0); }
    return false;
}

static inline string edgeNameCanonical(int i, int j, int k, int dir) {
    int ci, cj, cdir;
    if (!canonicalEdge(i, j, dir, ci, cj, cdir)) return string();
    ostringstream ss; ss << "E_" << ci << "_" << cj << "_" << k << "_" << cdir; return ss.str();
}

static inline void step_by_dir(int x, int y, int dir, int &nx, int &ny) {
    nx = x; ny = y;
    switch (dir) {
        case DIR_RIGHT: nx++; break;
        case DIR_LEFT:  nx--; break;
        case DIR_UP:    ny--; break;
        case DIR_DOWN:  ny++; break;
    }
}

class SATDecoder {
private:
    MetroMap &metro_map;
    int N, M, K; // Columns, Rows, Lines
    unordered_map<string, bool> true_assignments;
    vector<vector<Cell>> solution_grid;

    bool parseOccupancyVar(const string &var_name, int &i, int &j, int &k) {
        if (var_name.rfind("X_", 0) != 0) return false;
        string rest = var_name.substr(2);
        string token;
        vector<int> parts;
        stringstream ss(rest);
        while (getline(ss, token, '_')) {
            if (token.empty()) return false;
            parts.push_back(stoi(token));
        }
        if (parts.size() != 3) return false;
        i = parts[0];
        j = parts[1];
        k = parts[2];
        return true;
    }

    bool parseEdgeVar(const string &var_name, int &i, int &j, int &k, int &dir) {
        if (var_name.rfind("E_", 0) != 0) return false;
        string rest = var_name.substr(2);
        string token; vector<int> parts; stringstream ss(rest);
        while (getline(ss, token, '_')) {
            if (token.empty()) return false;
            parts.push_back(stoi(token));
        }
        if (parts.size() != 4) return false;
        i = parts[0]; j = parts[1]; k = parts[2]; dir = parts[3];
        return true;
    }

    bool parseOutVar(const string &var_name, int &i, int &j, int &k, int &dir) {
        if (var_name.rfind("OUT_", 0) != 0) return false;
        string rest = var_name.substr(4);
        string token;
        vector<int> parts;
        stringstream ss(rest);
        while (getline(ss, token, '_')) {
            if (token.empty()) return false;
            parts.push_back(stoi(token));
        }
        if (parts.size() != 4) return false;
        i = parts[0];
        j = parts[1];
        k = parts[2];
        dir = parts[3];
        return true;
    }

    // CellDir directionForCell(int i, int j, int k) const {
    //     for (int dir = 1; dir <= 4; ++dir) {
    //         string out_var = "OUT_" + to_string(i) + "_" + to_string(j) + "_" +
    //                          to_string(k) + "_" + to_string(dir);
    //         auto it = true_assignments.find(out_var);
    //         if (it != true_assignments.end() && it->second) {
    //             return makeCellDir(dir);
    //         }
    //     }
    //     return EMPTY;
    // }

    CellDir directionForCell(int i, int j, int k) const {
        // For display only. If degree=1 (endpoint), show that dir.
        // If degree=2 (interior), pick the first true dir.
        int chosen_dir = -1;
        for (int dir = 1; dir <= 4; ++dir) {
            string evar = edgeNameCanonical(i, j, k, dir);
            if (evar.empty()) continue;
            auto it = true_assignments.find(evar);
            if (it != true_assignments.end() && it->second) {
                chosen_dir = dir; break;
            }
        }
        if (chosen_dir == -1) return EMPTY;
        return makeCellDir(chosen_dir);
    }


    // string reconstructPath(int line_k) const {
    //     const auto &starts = metro_map.getLineStarts(line_k);
    //     const auto &ends   = metro_map.getLineEnds(line_k);
    //     if (starts.empty() || ends.empty()) return string("0\n");

    //     int start_x = starts[0].first;
    //     int start_y = starts[0].second;
    //     int end_x   = ends[0].first;
    //     int end_y   = ends[0].second;

    //     string path;
    //     int curr_x = start_x, curr_y = start_y;

    //     for (int steps = 0; steps <= M * N; ++steps) {
    //         if (curr_x == end_x && curr_y == end_y) break;

    //         int direction_count = 0;
    //         int chosen_dir = -1;
    //         for (int dir = 1; dir <= 4; ++dir) {
    //             string out_var = "OUT_" + to_string(curr_y) + "_" + to_string(curr_x) +
    //                              "_" + to_string(line_k) + "_" + to_string(dir);
    //             auto it = true_assignments.find(out_var);
    //             if (it != true_assignments.end() && it->second) {
    //                 direction_count++;
    //                 chosen_dir = dir;
    //             }
    //         }

    //         if (direction_count == 0) {
    //             cerr << "Error: No outgoing direction from (" << curr_x << "," << curr_y
    //                  << ") for line " << line_k << endl;
    //             break;
    //         }
    //         if (direction_count > 1) {
    //             cerr << "Error: Multiple outgoing directions from (" << curr_x << "," << curr_y
    //                  << ") for line " << line_k << endl;
    //             break;
    //         }

    //         if (chosen_dir == RIGHT) { curr_x += 1; path += MOVE_R; }
    //         else if (chosen_dir == UP)   { curr_y -= 1; path += MOVE_U; }
    //         else if (chosen_dir == LEFT) { curr_x -= 1; path += MOVE_L; }
    //         else { curr_y += 1; path += MOVE_D; }
    //     }

    //     path += ZERO;
    //     return path;
    // }

    // bool loadVariableMapping(const string &mapping_file,
    //                          unordered_map<int, string> &var_map) const {
    //     ifstream map_stream(mapping_file);
    //     if (!map_stream.is_open()) {
    //         cerr << "Error: Cannot open mapping file: " << mapping_file << endl;
    //         return false;
    //     }

    //     string line;
    //     while (getline(map_stream, line)) {
    //         if (line.empty()) continue;
    //         stringstream ss(line);
    //         int var_id;
    //         string var_name;
    //         if (ss >> var_id >> var_name) {
    //             var_map[var_id] = var_name;
    //         }
    //     }
    //     return true;
    // }

    string reconstructPath(int line_k) const {
        const auto &starts = metro_map.getLineStarts(line_k);
        const auto &ends   = metro_map.getLineEnds(line_k);
        if (starts.empty() || ends.empty()) return string("0\n");

        int start_x = starts[0].first;   // column (j)
        int start_y = starts[0].second;  // row    (i)
        int end_x   = ends[0].first;
        int end_y   = ends[0].second;

        string path;
        int curr_x = start_x, curr_y = start_y;
        int prev_x = -1000000, prev_y = -1000000;  // sentinel

        for (int steps = 0; steps <= M * N; ++steps) {
            if (curr_x == end_x && curr_y == end_y) break;

            // Read true edges using canonical names at current (i=curr_y, j=curr_x)
            vector<int> dirs_true;
            for (int dir = 1; dir <= 4; ++dir) {
                string evar = edgeNameCanonical(curr_y, curr_x, line_k, dir);
                if (evar.empty()) continue;
                auto it = true_assignments.find(evar);
                if (it != true_assignments.end() && it->second) dirs_true.push_back(dir);
            }

            if (dirs_true.empty()) {
                cerr << "Error: No edges from (" << curr_x << "," << curr_y
                    << ") for line " << line_k << endl;
                break;
            }

            // Choose the edge that does NOT go back to (prev_x, prev_y).
            int chosen_dir = -1;
            for (int d : dirs_true) {
                int nx, ny; step_by_dir(curr_x, curr_y, d, nx, ny);
                if (!(nx == prev_x && ny == prev_y)) {
                    if (chosen_dir != -1) {
                        // More than one forward option means the model isn't a simple path.
                        // Your encoding should prevent this; treat as error to avoid ambiguity.
                        cerr << "Error: Ambiguous forward edges from (" << curr_x << "," << curr_y
                            << ") for line " << line_k << endl;
                    }
                    chosen_dir = d;
                }
            }

            // If we're at the source, degree=1 ⇒ exactly one true edge, so chosen_dir is set.
            // If still -1, we must go back (dead-end), which shouldn't happen.
            if (chosen_dir == -1) {
                cerr << "Error: Only back edge available at (" << curr_x << "," << curr_y
                    << ") for line " << line_k << endl;
                break;
            }

            // Move and emit symbol
            int nx, ny; step_by_dir(curr_x, curr_y, chosen_dir, nx, ny);
            if (chosen_dir == DIR_RIGHT) path += MOVE_R;
            else if (chosen_dir == DIR_LEFT) path += MOVE_L;
            else if (chosen_dir == DIR_UP) path += MOVE_U;
            else path += MOVE_D;

            prev_x = curr_x; prev_y = curr_y;
            curr_x = nx;      curr_y = ny;
        }

        path += ZERO;
        return path;
    }

    bool loadVariableMapping(const string &mapping_file,
                             unordered_map<int, string> &var_map) const {
        ifstream map_stream(mapping_file);
        if (!map_stream.is_open()) {
            cerr << "Error: Cannot open mapping file: " << mapping_file << endl;
            return false;
        }

        string line;
        while (getline(map_stream, line)) {
            if (line.empty()) continue;
            stringstream ss(line);
            int var_id;
            string var_name;
            if (ss >> var_id >> var_name) {
                var_map[var_id] = var_name;
            }
        }
        return true;
    }

    bool parseSATOutput(const string &sat_output_file,
                        const unordered_map<int, string> &var_map) {
        ifstream sat_stream(sat_output_file);
        if (!sat_stream.is_open()) {
            cerr << "Error: Cannot open SAT output file: " << sat_output_file << endl;
            return false;
        }

        string line;
        bool status_determined = false;
        bool is_sat = false;

        while (getline(sat_stream, line)) {
            if (line.empty()) continue;
            if (line[0] == 'c') continue;

            if (!status_determined) {
                if (line == "SAT") {
                    is_sat = true;
                    status_determined = true;
                    continue;
                }
                if (line == "UNSAT") {
                    cout << "Problem is UNSATISFIABLE" << endl;
                    return false;
                }
                if (line.size() > 1 && line[0] == 's') {
                    if (line.find("UNSAT") != string::npos) {
                        cout << "Problem is UNSATISFIABLE" << endl;
                        return false;
                    }
                    if (line.find("SAT") != string::npos) {
                        is_sat = true;
                        status_determined = true;
                        continue;
                    }
                }
                // No explicit status yet; assume SAT if model follows immediately.
                is_sat = true;
                status_determined = true;
            }

            stringstream ss(line);
            string tok;
            while (ss >> tok) {
                if (tok == "v" || tok == "0") continue;
                int lit_value = stoi(tok);
                int var_id = abs(lit_value);
                auto it = var_map.find(var_id);
                if (it == var_map.end()) continue;
                if (lit_value > 0) {
                    true_assignments[it->second] = true;
                }
            }
        }

        if (!is_sat) {
            cout << "Problem is UNSATISFIABLE" << endl;
            return false;
        }
        return true;
    }

public:
    SATDecoder(MetroMap &m) : metro_map(m) {
        N = metro_map.getColNum();
        M = metro_map.getRowNum();
        K = metro_map.getLineNum();
        solution_grid.resize(M, vector<Cell>(N, Cell()));
    }

    bool decodeModel(const string &sat_output_file, const string &mapping_file) {
        unordered_map<int, string> var_map;
        if (!loadVariableMapping(mapping_file, var_map)) return false;
        return parseSATOutput(sat_output_file, var_map);
    }

    void buildSolutionGrid() {
        // Clear the grid
        for (int i = 0; i < M; ++i)
            for (int j = 0; j < N; ++j)
                solution_grid[i][j] = Cell();

        // For each cell, pick any line k that has an incident true edge here
        for (int i = 0; i < M; ++i) {
            for (int j = 0; j < N; ++j) {
                int chosen_k = -1;
                CellDir chosen_dir = EMPTY;

                for (int k = 0; k < K; ++k) {
                    vector<pair<int,string>> incident;
                    incident.reserve(4);

                    auto push_if = [&](int dir){
                        int ci, cj, cdir;
                        if (!canonicalEdge(i, j, dir, ci, cj, cdir)) return;

                        // Neighbor in-bounds (match encoder's step())
                        int ni = i, nj = j;
                        if (dir == DIR_RIGHT)      nj = j + 1;
                        else if (dir == DIR_LEFT)  nj = j - 1;
                        else if (dir == DIR_UP)    ni = i - 1;
                        else /* DIR_DOWN */        ni = i + 1;

                        if (ni < 0 || ni >= M || nj < 0 || nj >= N) return;

                        string e = edgeNameCanonical(i, j, k, dir);
                        if (!e.empty()) incident.push_back({dir, e});
                    };

                    push_if(DIR_RIGHT);
                    push_if(DIR_LEFT);
                    push_if(DIR_UP);
                    push_if(DIR_DOWN);

                    // Is any incident edge true for this line k?
                    int dir_true = -1;
                    for (auto &pr : incident) {
                        auto it = true_assignments.find(pr.second);
                        if (it != true_assignments.end() && it->second) {
                            dir_true = pr.first;
                            break;
                        }
                    }
                    if (dir_true == -1) continue;

                    chosen_k = k;
                    chosen_dir = makeCellDir(dir_true);
                    break; // take the first line we find for display
                }

                if (chosen_k != -1) {
                    solution_grid[i][j] = Cell(chosen_k, chosen_dir);
                }
            }
        }
    }



    vector<string> generateSolution() const {
        vector<string> solution;
        solution.reserve(K);
        for (int k = 0; k < K; ++k) {
            solution.push_back(reconstructPath(k));
        }
        return solution;
    }

    void writeOutput(const string &output_file, bool was_sat) const {
        ofstream out(output_file);
        if (!out.is_open()) {
            cerr << "Error: Cannot create output file: " << output_file << endl;
            return;
        }

        if (!was_sat) {
            out << "0" << endl;
        } else {
            auto solution = generateSolution();
            for (const auto &line_path : solution) out << line_path;
        }
    }

    void printSolutionGrid() const {
        cout << "Solution Grid:" << endl;
        for (int i = 0; i < M; ++i) {
            for (int j = 0; j < N; ++j) {
                Cell cell = solution_grid[i][j];
                if (cell.getLineNo() == -1) {
                    cout << ". ";
                } else {
                    char dir_char = '.';
                    switch (cell.getMode()) {
                        case RIGHT: dir_char = 'R'; break;
                        case LEFT:  dir_char = 'L'; break;
                        case UP:    dir_char = 'U'; break;
                        case DOWN:  dir_char = 'D'; break;
                        default:    dir_char = 'S'; break;
                    }
                    cout << cell.getLineNo() << dir_char << " ";
                }
            }
            cout << endl;
        }
    }
};

int main(int argc, char* argv[]) {
    if (argc != 5) {
        cerr << "Usage: " << argv[0]
             << " <input_file> <sat_output_file> <output_file> <mapping_file>" << endl;
        return 1;
    }

    string input_file      = argv[1];
    string sat_output_file = argv[2];
    string output_file     = argv[3];
    string mapping_file    = argv[4];

    ifstream input_stream(input_file);
    if (!input_stream.is_open()) {
        cerr << "Error: Cannot open input file: " << input_file << endl;
        return 1;
    }

    MetroMap metro_map = parseInputFile(input_stream);
    input_stream.close();
    cout << "Input file parsed successfully." << endl;

    SATDecoder decoder(metro_map);
    bool was_sat = decoder.decodeModel(sat_output_file, mapping_file);
    if (was_sat) {
        decoder.buildSolutionGrid();
        cout << "Solution found!" << endl;
        decoder.printSolutionGrid();
    }
    decoder.writeOutput(output_file, was_sat);
    cout << "Output written to " << output_file << endl;
    return 0;
}
