#include <bits/stdc++.h>
#include <fstream>
#include <iostream>
using namespace std;

// Y/y -> Rows,  X/x -> Columns

const string MOVE_R = "R ";
const string MOVE_L = "L ";
const string MOVE_U = "U ";
const string MOVE_D = "D ";
const string ZERO = "0\n";

enum CellMode {
    EMPTY = 0,
    LEFT = 1,
    RIGHT = 2,
    UP = 3,
    DOWN = 4,
};

class Cell {
    private :
        int line_no;
        CellMode mode;
    public :
        Cell() {
            line_no = -1;
            mode = EMPTY;
        }
        
        Cell(int ln, CellMode m) {
            line_no = ln;
            mode = m;
        }

        int getLineNo() {
            return line_no;
        }

        CellMode getMode() {
            return mode;
        }
};

class MetroMap {
    private:
        // Input Parsing
        int rows, cols, lines, turn_limit, pop_cities_sz;
        vector<vector<Cell>> grid;
        vector<vector<pair<int,int>>> line_starts;
        vector<vector<pair<int,int>>> line_ends;
        vector<vector<pair<int,int>>> popular_cities; 

        // SAT MAGIC
        // Solution Representation - For each line, store the coordinates of the cells it occupies
        // Suppose the grid is setup after parsing the sat_output file

        vector<string> line_path;

    public:
        MetroMap(int r, int c, int l, int t, int p) {
            rows = r;
            cols = c;
            lines = l;
            turn_limit = t;
            pop_cities_sz = p;
        }

        void setGrid() {
            grid.resize(rows, vector<Cell>(cols, Cell()));
        }

        void setLineStarts(vector<vector<pair<int,int>>> ls) {
            line_starts.resize(lines);
            line_starts = ls;
        }

        void setLineEnds(vector<vector<pair<int,int>>> le) {
            line_ends.resize(lines);
            line_ends = le;
        }

        void setPopularCities(vector<vector<pair<int,int>>> ps) {
            popular_cities.resize(pop_cities_sz);
            popular_cities = ps;
        }

        void setLinePath() {
            // Assumes that the grid is already setup with Proper Cell Values and that the Solution is SAT
            line_path.resize(lines);
            for (int i = 0; i < lines; i++) {
                pair<int,int> start = line_starts[i][0];
                pair<int,int> end = line_ends[i][0]; 
                pair<int,int> curr = start;
                while (curr != end) {
                    CellMode mode = grid[curr.second][curr.first].getMode();
                    if (mode == LEFT) {
                        curr.first -= 1;
                        line_path[i] += MOVE_L;
                    } else if (mode == RIGHT) {
                        line_path[i] += MOVE_R;
                        curr.first += 1;
                    } else if (mode == UP) {
                        line_path[i] += MOVE_U;
                        curr.second -= 1;
                    } else if (mode == DOWN) {
                        line_path[i] += MOVE_D;
                        curr.second += 1;
                    } else {
                        cerr << "Error: Invalid Cell Mode while tracing line " << i+1 << endl
                        << "Current Position: (" << curr.first << "," << curr.second << ")" << endl;
                        break;
                    }
                }

                line_path[i] += ZERO;

            }
        }

        void printInfo() {
            cout << "Rows: " << rows << ", Columns: " << cols << ", Lines: " << lines << ", Turn Limit: " << turn_limit << ", Popular Cities: " << pop_cities_sz << endl;
            cout << "Line Starts: (col, row)" << endl;
            for (int i = 0; i < lines; i++) {
                cout << "Line " << i+1 << ": ";
                for (auto &p : line_starts[i]) {
                    cout << "(" << p.first << "," << p.second << ") ";
                }
                cout << endl;
            }
            cout << "Line Ends: (col, row)" << endl;
            for (int i = 0; i < lines; i++) {
                cout << "Line " << i+1 << ": ";
                for (auto &p : line_ends[i]) {
                    cout << "(" << p.first << "," << p.second << ") ";
                }
                cout << endl;
            }
            cout << "Popular Cities:" << endl;
            for (int i = 0; i < pop_cities_sz; i++) {
                cout << "Cities " << i+1 << ": ";
                for (auto &p : popular_cities[i]) {
                    cout << "(" << p.first << "," << p.second << ") ";
                }
                cout << endl;
            }
        }

        // Public Getters

        int getLineNum() {
            return lines;
        }

        string& getLineCells(int i) {
            if (i < 0 || i >= lines) {
                throw out_of_range("Invalid line number in getLineCells");
            }
            return line_path[i];
        }
};      

MetroMap parseInputFile(ifstream &input_file_stream) {

    int mode, cols, rows, lines, turn_limit, pop_stations_sz = 0;
    input_file_stream >> mode;

    if (mode == 1) {
        input_file_stream >> cols >> rows >> lines >> turn_limit;
    } else if (mode == 2) {
        input_file_stream >> cols >> rows >> lines >> turn_limit >> pop_stations_sz;
    } else {
        cerr << "Error: Invalid mode in input file" << endl;
        return MetroMap(0, 0, 0, 0, 0);
    }

    MetroMap metro_map(rows, cols, lines, turn_limit, pop_stations_sz);
    // metro_map.setGrid();

    vector<vector<pair<int, int>>> line_starts(lines);
    vector<vector<pair<int, int>>> line_ends(lines);

    for (int i = 0; i < lines; i++) {
        int start_x, start_y, end_x, end_y;
        input_file_stream >> start_x >> start_y >> end_x >> end_y;
        line_starts[i].emplace_back(start_x, start_y);
        line_ends[i].emplace_back(end_x, end_y);
    }

    metro_map.setLineStarts(line_starts);
    metro_map.setLineEnds(line_ends);

    if (mode == 2 && pop_stations_sz > 0) {
        vector<vector<pair<int, int>>> popular_stations(pop_stations_sz);
        for (int i = 0; i < pop_stations_sz; i++) {
            int pop_x, pop_y;
            input_file_stream >> pop_x >> pop_y;
            popular_stations[i].emplace_back(pop_x, pop_y);
        }
        metro_map.setPopularCities(popular_stations);
    }

    return metro_map;
}

void parseOutputFile(ofstream &output_file_stream, MetroMap &metro_map, bool was_sat) {
    // Assumes that the setLineCells has been called if was_sat is true
    if (!was_sat) {
        output_file_stream << 0 << endl;
        output_file_stream.close();
    }

    int lines = metro_map.getLineNum();

    if (!was_sat) {
        output_file_stream << 0 << endl;
        return;
    }

    for (int i = 0; i < lines; i++) {
        string path = metro_map.getLineCells(i);
        output_file_stream << path << endl;
    }

}
