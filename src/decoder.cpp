#include <bits/stdc++.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include "utils.hpp"
using namespace std;

class SATDecoder {
private:
    MetroMap &metro_map;
    int N, M, K; // Columns, Rows, Lines
    map<string, bool> variable_assignments;
    vector<vector<Cell>> solution_grid;

    // Parse variable name to extract components
    bool parseOccupancyVar(const string &var_name, int &i, int &j, int &k) {
        if (var_name.substr(0, 2) != "X_") return false;
        
        stringstream ss(var_name.substr(2));
        string token;
        vector<int> parts;
        
        while (getline(ss, token, '_')) {
            parts.push_back(stoi(token));
        }
        
        if (parts.size() != 3) return false;
        i = parts[0]; j = parts[1]; k = parts[2];
        return true;
    }

    bool parseDirectionVar(const string &var_name, string &type, int &i, int &j, int &k, int &dir) {
        if (var_name.substr(0, 4) == "Out_") {
            type = "Out";
            stringstream ss(var_name.substr(4));
            string token;
            vector<int> parts;
            
            while (getline(ss, token, '_')) {
                parts.push_back(stoi(token));
            }
            
            if (parts.size() != 4) return false;
            i = parts[0]; j = parts[1]; k = parts[2]; dir = parts[3];
            return true;
        }
        return false;
    }

    // Reconstruct path for a specific line
    string reconstructPath(int line_k) {
        auto &starts = metro_map.getLineStarts(line_k);
        auto &ends = metro_map.getLineEnds(line_k);
        
        if (starts.empty() || ends.empty()) return "0\n";
        
        int start_x = starts[0].first, start_y = starts[0].second;
        int end_x = ends[0].first, end_y = ends[0].second;
        
        string path = "";
        int curr_x = start_x, curr_y = start_y;
        
        // Follow the path using Out_ variables
        while (curr_x != end_x || curr_y != end_y) {
            bool found_direction = false;
            
            // Check all four directions to find which Out_ variable is true
            for (int dir = 1; dir <= 4; dir++) {
                string out_var = "Out_" + to_string(curr_y) + "_" + to_string(curr_x) + "_" + to_string(line_k) + "_" + to_string(dir);
                
                if (variable_assignments.find(out_var) != variable_assignments.end() && 
                    variable_assignments[out_var]) {
                    
                    // Move in this direction
                    if (dir == RIGHT) {
                        curr_x += 1;
                        path += MOVE_R;
                    } else if (dir == UP) {
                        curr_y -= 1;
                        path += MOVE_U;
                    } else if (dir == LEFT) {
                        curr_x -= 1;
                        path += MOVE_L;
                    } else if (dir == DOWN) {
                        curr_y += 1;
                        path += MOVE_D;
                    }
                    
                    found_direction = true;
                    break;
                }
            }
            
            if (!found_direction) {
                cerr << "Error: No valid direction found at (" << curr_x << "," << curr_y << ") for line " << line_k << endl;
                break;
            }
            
            // Safety check to prevent infinite loops
            if (path.length() > 2 * (M + N) * 4) { // Each move is 2 chars, max path length estimate
                cerr << "Error: Path too long, possible infinite loop for line " << line_k << endl;
                break;
            }
        }
        
        path += ZERO;
        return path;
    }

public:
    SATDecoder(MetroMap &m) : metro_map(m) {
        N = metro_map.getColNum();
        M = metro_map.getRowNum();
        K = metro_map.getLineNum();
        
        // Initialize solution grid
        solution_grid.resize(M, vector<Cell>(N, Cell()));
    }

    // Parse SAT solver output
    bool parseSATOutput(const string &sat_output_file) {
        ifstream file(sat_output_file);
        if (!file.is_open()) {
            cerr << "Error: Cannot open SAT output file: " << sat_output_file << endl;
            return false;
        }

        string line;
        bool satisfiable = false;
        
        // Read first line to check if SAT or UNSAT
        if (getline(file, line)) {
            if (line == "SAT") {
                satisfiable = true;
            } else if (line == "UNSAT") {
                cout << "Problem is UNSATISFIABLE" << endl;
                return false;
            } else {
                cerr << "Error: Invalid SAT solver output format" << endl;
                return false;
            }
        }

        if (!satisfiable) return false;

        // Read variable assignments
        while (getline(file, line)) {
            if (line.empty() || line == "0") continue;
            
            stringstream ss(line);
            string var_str;
            
            while (ss >> var_str) {
                if (var_str == "0") break;
                
                bool is_positive = true;
                string var_name = var_str;
                
                if (var_str[0] == '-') {
                    is_positive = false;
                    var_name = var_str.substr(1);
                }
                
                // For simplicity, we'll assume variable names are preserved
                // In practice, you might need a mapping from numbers to variable names
                variable_assignments[var_name] = is_positive;
            }
        }

        file.close();
        return true;
    }

    // Parse SAT output with variable mapping file
    bool parseSATOutputWithMappingFile(const string &sat_output_file, const string &mapping_file) {
        // First load the variable mapping
        map<int, string> var_map;
        ifstream map_file(mapping_file);
        if (!map_file.is_open()) {
            cerr << "Warning: Cannot open mapping file: " << mapping_file << endl;
            // Try parsing without mapping (assuming variable names are preserved)
            return parseSATOutput(sat_output_file);
        }

        string line;
        while (getline(map_file, line)) {
            if (line.empty()) continue;
            stringstream ss(line);
            int var_num;
            string var_name;
            if (ss >> var_num >> var_name) {
                var_map[var_num] = var_name;
            }
        }
        map_file.close();

        cout << "Loaded " << var_map.size() << " variable mappings." << endl;

        // Now parse SAT output using the mapping
        return parseSATOutputWithMapping(sat_output_file, var_map);
    }

    // Alternative: Parse SAT output with variable mapping
    bool parseSATOutputWithMapping(const string &sat_output_file, const map<int, string> &var_map) {
        ifstream file(sat_output_file);
        if (!file.is_open()) {
            cerr << "Error: Cannot open SAT output file: " << sat_output_file << endl;
            return false;
        }

        string line;
        bool satisfiable = false;
        
        // Read first line
        if (getline(file, line)) {
            if (line == "SAT") {
                satisfiable = true;
            } else if (line == "UNSAT") {
                cout << "Problem is UNSATISFIABLE" << endl;
                return false;
            } else {
                cerr << "Error: Invalid SAT solver output format" << endl;
                return false;
            }
        }

        if (!satisfiable) return false;

        // Read variable assignments
        while (getline(file, line)) {
            if (line.empty() || line == "0") continue;
            
            stringstream ss(line);
            string var_str;
            
            while (ss >> var_str) {
                if (var_str == "0") break;
                
                bool is_positive = true;
                int var_num;
                
                if (var_str[0] == '-') {
                    is_positive = false;
                    var_num = stoi(var_str.substr(1));
                } else {
                    var_num = stoi(var_str);
                }
                
                if (var_map.find(var_num) != var_map.end()) {
                    variable_assignments[var_map.at(var_num)] = is_positive;
                }
            }
        }

        file.close();
        return true;
    }

    // Build solution grid from variable assignments
    void buildSolutionGrid() {
        // Initialize all cells as empty
        for (int i = 0; i < M; i++) {
            for (int j = 0; j < N; j++) {
                solution_grid[i][j] = Cell();
            }
        }

        // Set occupied cells and their directions
        for (const auto &assignment : variable_assignments) {
            const string &var_name = assignment.first;
            bool value = assignment.second;

            if (!value) continue; // Only process true variables

            // Check if this is an occupancy variable
            int i, j, k;
            if (parseOccupancyVar(var_name, i, j, k)) {
                // Find the direction for this cell by checking Out_ variables
                CellDir direction = EMPTY;
                
                for (int dir = 1; dir <= 4; dir++) {
                    string out_var = "Out_" + to_string(i) + "_" + to_string(j) + "_" + to_string(k) + "_" + to_string(dir);
                    
                    if (variable_assignments.find(out_var) != variable_assignments.end() && 
                        variable_assignments[out_var]) {
                        direction = makeCellDir(dir);
                        break;
                    }
                }
                
                solution_grid[i][j] = Cell(k, direction);
            }
        }
    }

    // Generate output for all lines
    vector<string> generateSolution() {
        vector<string> solution;
        
        for (int k = 0; k < K; k++) {
            string path = reconstructPath(k);
            solution.push_back(path);
        }
        
        return solution;
    }

    // Write solution to output file
    void writeOutput(const string &output_file, bool was_sat) {
        ofstream file(output_file);
        if (!file.is_open()) {
            cerr << "Error: Cannot create output file: " << output_file << endl;
            return;
        }

        if (!was_sat) {
            file << "0" << endl;
        } else {
            vector<string> solution = generateSolution();
            for (const string &path : solution) {
                file << path;
            }
        }

        file.close();
    }

    // Debug: Print variable assignments
    void printAssignments() {
        cout << "Variable Assignments:" << endl;
        for (const auto &assignment : variable_assignments) {
            cout << assignment.first << " = " << (assignment.second ? "true" : "false") << endl;
        }
    }

    // Debug: Print solution grid
    void printSolutionGrid() {
        cout << "Solution Grid:" << endl;
        for (int i = 0; i < M; i++) {
            for (int j = 0; j < N; j++) {
                Cell cell = solution_grid[i][j];
                if (cell.getLineNo() == -1) {
                    cout << ". ";
                } else {
                    char dir_char = '?';
                    switch (cell.getMode()) {
                        case RIGHT: dir_char = 'R'; break;
                        case UP: dir_char = 'U'; break;
                        case LEFT: dir_char = 'L'; break;
                        case DOWN: dir_char = 'D'; break;
                        default: dir_char = 'X'; break;
                    }
                    cout << cell.getLineNo() << dir_char << " ";
                }
            }
            cout << endl;
        }
    }
};

// Example usage function
int main(int argc, char* argv[]) {
    if (argc < 4 || argc > 5) {
        cerr << "Usage: " << argv[0] << " <input_file> <sat_output_file> <output_file> [mapping_file]" << endl;
        return 1;
    }

    string input_file = argv[1];
    string sat_output_file = argv[2];
    string output_file = argv[3];
    string mapping_file = (argc == 5) ? argv[4] : "";

    // Parse input file
    ifstream input_stream(input_file);
    if (!input_stream.is_open()) {
        cerr << "Error: Cannot open input file: " << input_file << endl;
        return 1;
    }

    MetroMap metro_map = parseInputFile(input_stream);
    input_stream.close();

    cout << "Input file parsed successfully." << endl;

    // Create decoder
    SATDecoder decoder(metro_map);

    // Parse SAT output
    bool was_sat;
    if (!mapping_file.empty()) {
        was_sat = decoder.parseSATOutputWithMappingFile(sat_output_file, mapping_file);
    } else {
        was_sat = decoder.parseSATOutput(sat_output_file);
    }
    
    if (was_sat) {
        decoder.buildSolutionGrid();
        
        // Debug output
        cout << "Solution found!" << endl;
        decoder.printSolutionGrid();
    }

    // Write output
    decoder.writeOutput(output_file, was_sat);
    cout << "Output written to " << output_file << endl;

    return 0;
}
