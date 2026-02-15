// cimplec.cpp
// Cimple (.cimp) → C++ transpiler
// Only translates, does NOT compile or run anything
// Usage:   ./cimplec program.cimp
// Output:  program.cpp
//
// Compile this transpiler with:
//     g++ cimplec.cpp -o cimplec -std=c++17

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>

using namespace std;

// ────────────────────────────────────────────────
// Utilities
// ────────────────────────────────────────────────

string trim(const string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, last - first + 1);
}

bool starts_with(const string& s, const string& prefix) {
    return s.rfind(prefix, 0) == 0;
}

bool ends_with(const string& s, const string& suffix) {
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

string replace_keywords(string s) {
    size_t pos = 0;
    while ((pos = s.find(" and ", pos)) != string::npos) { s.replace(pos, 5, " && "); pos += 3; }
    pos = 0;
    while ((pos = s.find(" or ", pos)) != string::npos)  { s.replace(pos, 4, " || ");  pos += 3; }
    pos = 0;
    while ((pos = s.find("not ", pos)) != string::npos)  { s.replace(pos, 4, "! ");    pos += 2; }
    return s;
}

vector<string> split_print_args(const string& s) {
    vector<string> args;
    string current;
    bool in_string = false;
    char quote = 0;
    for (char c : s) {
        if (in_string) {
            current += c;
            if (c == quote) in_string = false;
        } else {
            if (c == '"' || c == '\'') {
                in_string = true;
                quote = c;
                current += c;
            } else if (c == ',') {
                args.push_back(trim(current));
                current.clear();
            } else {
                current += c;
            }
        }
    }
    if (!current.empty()) args.push_back(trim(current));
    return args;
}

// ────────────────────────────────────────────────
// Main logic — only translation
// ────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    if (argc != 2) {
        cerr << "Usage: ./cimplec <file.cimp>\n";
        cerr << "   Writes output to <file>.cpp\n";
        return 1;
    }

    string input_path = argv[1];
    ifstream in(input_path);
    if (!in.is_open()) {
        cerr << "[ERROR] Cannot open input file: " << input_path << endl;
        return 1;
    }

    string base_name = input_path;
    size_t dot = base_name.find_last_of('.');
    if (dot != string::npos) base_name = base_name.substr(0, dot);

    string output_path = base_name + ".cpp";

    ofstream out(output_path);
    if (!out.is_open()) {
        cerr << "[ERROR] Cannot create output file: " << output_path << endl;
        return 1;
    }

    // Header
    out << "#include <iostream>\n";
    out << "#include <string>\n";
    out << "using namespace std;\n\n";

    vector<int> indent_stack = {0};
    int line_num = 0;
    string line;

    out << "int main() {\n";

    while (getline(in, line)) {
        ++line_num;
        string trimmed = trim(line);
        if (trimmed.empty() || starts_with(trimmed, "//")) continue;

        // Detect indentation level
        size_t indent_pos = line.find_first_not_of(" \t");
        if (indent_pos == string::npos) continue;
        int this_indent = static_cast<int>(indent_pos);

        // Auto-deduct when indentation decreases
        while (indent_stack.size() > 1 && this_indent < indent_stack.back()) {
            indent_stack.pop_back();
            out << string((indent_stack.size() - 1) * 4, ' ') << "}\n";
        }

        string indent_str((indent_stack.size() - 1) * 4, ' ');

        trimmed = replace_keywords(trimmed);

        // print(...)
        if (starts_with(trimmed, "print(")) {
            size_t start = trimmed.find('(') + 1;
            size_t end = trimmed.rfind(')');
            if (end > start) {
                string args_part = trimmed.substr(start, end - start);
                auto args = split_print_args(args_part);
                out << indent_str << "cout";
                for (const string& arg : args) {
                    out << " << " << arg;
                }
                out << " << endl;\n";
            }
            continue;
        }

        // cin(...)
        if (starts_with(trimmed, "cin(")) {
            size_t start = trimmed.find('(') + 1;
            size_t end = trimmed.rfind(')');
            if (end > start) {
                string var = trim(trimmed.substr(start, end - start));
                out << indent_str << "cin >> " << var << ";\n";
            }
            continue;
        }

        // def name(param: type, ...)
        if (starts_with(trimmed, "def ")) {
            string sig = trim(trimmed.substr(4));
            size_t po = sig.find('('), pc = sig.rfind(')');
            if (po == string::npos || pc == string::npos) continue;

            string fname = trim(sig.substr(0, po));
            string params_str = trim(sig.substr(po + 1, pc - po - 1));

            vector<pair<string, string>> params;
            if (!params_str.empty()) {
                stringstream ss(params_str);
                string token;
                while (getline(ss, token, ',')) {
                    token = trim(token);
                    size_t colon = token.find(':');
                    string type = "string";
                    string name = token;
                    if (colon != string::npos) {
                        type = trim(token.substr(0, colon));
                        name = trim(token.substr(colon + 1));
                        if (type != "int" && type != "float" && type != "double" && type != "bool") {
                            type = "string";
                        }
                    }
                    params.emplace_back(type, name);
                }
            }

            out << "void " << fname << "(";
            for (size_t i = 0; i < params.size(); ++i) {
                if (i > 0) out << ", ";
                out << params[i].first << " " << params[i].second;
            }
            out << ") {\n";

            if (this_indent + 4 > indent_stack.back()) {
                indent_stack.push_back(this_indent + 4);
            }
            continue;
        }

        // for ... in range(...)
        if (starts_with(trimmed, "for ") && trimmed.find(" in range(") != string::npos) {
            size_t in_pos = trimmed.find(" in range(");
            string var = trim(trimmed.substr(4, in_pos - 4));

            size_t rstart = trimmed.find("range(") + 6;
            size_t rend = trimmed.rfind(')');
            string rargs = trimmed.substr(rstart, rend - rstart);

            vector<string> nums;
            stringstream ss(rargs);
            string tok;
            while (getline(ss, tok, ',')) nums.push_back(trim(tok));

            string init = "0", limit, step = "1";
            if (nums.size() == 1)       limit = nums[0];
            else if (nums.size() == 2) { init = nums[0]; limit = nums[1]; }
            else if (nums.size() == 3) { init = nums[0]; limit = nums[1]; step = nums[2]; }

            out << indent_str << "for (int " << var << " = " << init << "; "
                << var << " < " << limit << "; " << var << " += " << step << ") {\n";

            if (this_indent + 4 > indent_stack.back()) {
                indent_stack.push_back(this_indent + 4);
            }
            continue;
        }

        // C-style for(...)
        if (starts_with(trimmed, "for(") && ends_with(trimmed, ":")) {
            string inside = trimmed.substr(4, trimmed.size() - 5);
            replace(inside.begin(), inside.end(), ',', ';');
            out << indent_str << "for(" << inside << ") {\n";

            if (this_indent + 4 > indent_stack.back()) {
                indent_stack.push_back(this_indent + 4);
            }
            continue;
        }

        // if / while / elif / else :
        if (ends_with(trimmed, ":")) {
            string header = trim(trimmed.substr(0, trimmed.size() - 1));
            size_t sp = header.find(' ');
            string keyword = (sp != string::npos) ? header.substr(0, sp) : header;
            string cond = (sp != string::npos) ? trim(header.substr(sp)) : "";

            if (keyword == "if" || keyword == "while") {
                out << indent_str << keyword << " (" << cond << ") {\n";
                if (this_indent + 4 > indent_stack.back()) {
                    indent_stack.push_back(this_indent + 4);
                }
            }
            else if (keyword == "elif") {
                out << string((indent_stack.size() - 2) * 4, ' ') << "} else if (" << cond << ") {\n";
            }
            continue;
        }

        if (trimmed == "else") {
            out << string((indent_stack.size() - 2) * 4, ' ') << "} else {\n";
            continue;
        }

        if (trimmed == "end") {
            if (indent_stack.size() > 1) {
                indent_stack.pop_back();
                out << string((indent_stack.size() - 1) * 4, ' ') << "}\n";
            }
            continue;
        }

        // Everything else: variables, assignments, expressions, function calls...
        if (!trimmed.empty()) {
            out << indent_str << trimmed;
            // Add semicolon when it looks like a statement
            if (trimmed.back() != ';' && trimmed.back() != '{' && trimmed.back() != '}') {
                if (trimmed.find('=') != string::npos ||
                    trimmed.find("++") != string::npos ||
                    trimmed.find("--") != string::npos ||
                    trimmed.find('(') != string::npos) {
                    out << ";";
                }
            }
            out << "\n";
        }
    }

    // Close remaining open blocks
    while (indent_stack.size() > 1) {
        indent_stack.pop_back();
        out << string((indent_stack.size() - 1) * 4, ' ') << "}\n";
    }

    out << "    return 0;\n}\n";

    out.close();
    in.close();

    cout << "[OK]  Written to:  " << output_path << endl;

    return 0;
}
