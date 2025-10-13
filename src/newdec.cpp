// Decoder compatible with newenc.cpp variable scheme
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
    map<string, bool> assignment; // variable name -> truth value

    // Get single outgoing direction at (i=row, j=col) from global OUT variables
    // Returns dir in {1..4}, or 0 if none/invalid
    int getOutgoingDir(int i, int j) const {
        int count = 0, chosen = 0;
        for (int d = 1; d <= 4; ++d) {
            string name = string("OUT_") + to_string(i) + "_" + to_string(j) + "_" + to_string(d);
            auto it = assignment.find(name);
            if (it != assignment.end() && it->second) {
                ++count; chosen = d;
                if (count > 1) break;
            }
        }
        if (count == 1) return chosen;
        return 0;
    }

    // Robust SAT output parsing using mapping file (var id -> name)
    bool parseSATOutputWithMapping(const string &sat_output_file, const map<int,string> &id2name) {
        ifstream in(sat_output_file);
        if (!in.is_open()) {
            cerr << "Error: Cannot open SAT output file: " << sat_output_file << endl;
            return false;
        }

        string line; bool got_status = false; bool is_sat = false;
        while (getline(in, line)) {
            if (line.empty()) continue;
            if (!got_status) {
                if (line == "SAT") { is_sat = true; got_status = true; continue; }
                if (line == "UNSAT") { is_sat = false; got_status = true; break; }
                if (line.size() >= 1 && line[0] == 's') {
                    if (line.find("UNSAT") != string::npos) { is_sat = false; got_status = true; break; }
                    if (line.find("SATISFIABLE") != string::npos) { is_sat = true; got_status = true; continue; }
                }
                // If not a status line, proceed to parse integers as a model
                got_status = true; // be forgiving
            }

            stringstream ss(line); string tok;
            while (ss >> tok) {
                if (tok == "v") continue;
                if (tok == "0") break;
                bool pos = true; if (!tok.empty() && tok[0] == '-') { pos = false; tok.erase(0,1); }
                char *endp = nullptr; long val = strtol(tok.c_str(), &endp, 10);
                if (endp == tok.c_str() || *endp != '\0') continue; // skip non-integers
                int var = (int)val;
                auto it = id2name.find(var);
                if (it != id2name.end()) assignment[it->second] = pos;
            }
        }
        in.close();
        if (!is_sat) {
            cout << "Problem is UNSATISFIABLE" << endl;
            return false;
        }
        return true;
    }

    bool inBounds(int x, int y) const { return (0 <= x && x < N && 0 <= y && y < M); }

    // Reconstruct path for line k following global OUT_ directions from its start to end
    string reconstructPath(int k) const {
        auto &starts = metro_map.getLineStarts(k);
        auto &ends   = metro_map.getLineEnds(k);
        if (starts.empty() || ends.empty()) return string("0\n");

        int sx = starts[0].first, sy = starts[0].second;
        int ex = ends[0].first,   ey = ends[0].second;

        string path;
        int x = sx, y = sy;

        // detect loops robustly
        unordered_set<long long> seen;
        auto key = [&](int xx, int yy){ return (static_cast<long long>(yy) << 32) ^ static_cast<unsigned long long>(xx); };

        for (int steps = 0; steps <= M * N; ++steps) {
            if (x == ex && y == ey) break;

            int dir = getOutgoingDir(y, x); // names use i=row=y, j=col=x
            if (dir == 0) {
                cerr << "Error: No/ambiguous OUT at (" << x << "," << y << ") for line " << k << endl;
                break;
            }

            int nx = x, ny = y;
            if (dir == 1) { path += MOVE_R; nx += 1; }
            else if (dir == 2) { path += MOVE_U; ny -= 1; }
            else if (dir == 3) { path += MOVE_L; nx -= 1; }
            else /*dir==4*/    { path += MOVE_D; ny += 1; }

            if (!inBounds(nx, ny)) {
                cerr << "Error: Step goes out of bounds from (" << x << "," << y << ") to (" << nx << "," << ny << ") for line " << k << endl;
                break;
            }

            // Optional guidance: ensure neighbor is reachable for this line
            string rname = string("R_") + to_string(ny) + "_" + to_string(nx) + "_" + to_string(k);
            auto itR = assignment.find(rname);
            if (itR != assignment.end() && !itR->second) {
                cerr << "Warning: Moving into cell without R true for line " << k
                     << ": (" << nx << "," << ny << ")" << endl;
            }

            long long kk = key(nx, ny);
            if (seen.count(kk)) {
                cerr << "Error: Loop detected at (" << nx << "," << ny << ") for line " << k << endl;
                break;
            }
            seen.insert(kk);

            x = nx; y = ny;
        }

        path += ZERO;
        return path;
    }

public:
    SATDecoder(MetroMap &m) : metro_map(m) {
        N = metro_map.getColNum();
        M = metro_map.getRowNum();
        K = metro_map.getLineNum();
    }

    bool parseWithMappingFile(const string &sat_output_file, const string &mapping_file) {
        map<int,string> id2name;
        ifstream mf(mapping_file);
        if (!mf.is_open()) {
            cerr << "Error: Cannot open mapping file: " << mapping_file << endl;
            return false;
        }
        string line;
        while (getline(mf, line)) {
            if (line.empty()) continue;
            stringstream ss(line);
            int id; string name;
            if (ss >> id >> name) id2name[id] = name;
        }
        mf.close();
        return parseSATOutputWithMapping(sat_output_file, id2name);
    }

    vector<string> generateSolution() const {
        vector<string> all;
        all.reserve(K);
        for (int k = 0; k < K; ++k) all.push_back(reconstructPath(k));
        return all;
    }

    void writeOutput(const string &output_file, bool was_sat) {
        ofstream out(output_file);
        if (!out.is_open()) {
            cerr << "Error: Cannot create output file: " << output_file << endl;
            return;
        }
        if (!was_sat) {
            out << "0\n";
        } else {
            auto sol = generateSolution();
            for (auto &s : sol) out << s;
        }
        out.close();
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

    ifstream in(input_file);
    if (!in.is_open()) {
        cerr << "Error: Cannot open input file: " << input_file << endl;
        return 1;
    }
    MetroMap metro = parseInputFile(in);
    in.close();

    SATDecoder dec(metro);
    bool was_sat = dec.parseWithMappingFile(sat_output_file, mapping_file);
    dec.writeOutput(output_file, was_sat);
    return 0;
}
