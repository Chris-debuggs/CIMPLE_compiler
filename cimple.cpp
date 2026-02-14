// cimplec.cpp
// Final merged & improved Cimple → C++ transpiler
// Supports: types, multi-arg print, typed def, range-for, indentation-aware blocks, etc.
// Compile with: g++ cimplec.cpp -o cimplec -std=c++17

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <map>
#include <algorithm>
#include <cstdlib>

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

// Better print argument splitter (respects quotes)
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
// Main program
// ────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cerr << "Usage: cimplec <file.cimp> [--no-run]\n";
        return 1;
    }

    string input_path = argv[1];
    bool should_run = true;
    if (argc >= 3 && string(argv[2]) == "--no-run") {
        should_run = false;
    }

    ifstream in(input_path);
    if (!in.is_open()) {
        cerr << "[ERROR] Cannot open: " << input_path << endl;
        return 1;
    }

    string base = input_path;
    size_t dot = base.find_last_of('.');
    if (dot != string::npos) base = base.substr(0, dot);
    string cpp_out = base + ".cpp";
    string exe_out = base;

    ofstream out(cpp_out);
    if (!out.is_open()) {
        cerr << "[ERROR] Cannot create: " << cpp_out << endl;
        return 1;
    }

    out << "#include <iostream>\n#include <string>\nusing namespace std;\n\n";

    vector<int> indent_stack = {0};
    int line_num = 0;
    string line;

    out << "int main() {\n";

    while (getline(in, line)) {
        ++line_num;
        string original = line;
        string trimmed = trim(line);
        if (trimmed.empty() || starts_with(trimmed, "//")) continue;

        // Calculate current indentation level
        size_t indent_len = line.find_first_not_of(" \t");
        if (indent_len == string::npos) continue;
        int this_indent = indent_len;

        // Dedent if necessary
        while (indent_stack.size() > 1 && this_indent < indent_stack.back()) {
            indent_stack.pop_back();
            out << string((indent_stack.size() - 1) * 4, ' ') << "}\n";
        }

        string indent_str((indent_stack.size() - 1) * 4, ' ');

        trimmed = replace_keywords(trimmed);

        // ── print(...) ──
        if (starts_with(trimmed, "print(")) {
            size_t start = trimmed.find('(') + 1;
            size_t end = trimmed.rfind(')');
            if (end == string::npos || end <= start) {
                cerr << "[Warning line " << line_num << "] Invalid print\n";
                continue;
            }
            string args_str = trimmed.substr(start, end - start);
            auto args = split_print_args(args_str);

            out << indent_str << "cout";
            for (const string& a : args) out << " << " << a;
            out << " << endl;\n";
            continue;
        }

        // ── cin(var) ──
        if (starts_with(trimmed, "cin(")) {
            size_t start = trimmed.find('(') + 1;
            size_t end = trimmed.rfind(')');
            string var = trim(trimmed.substr(start, end - start));
            out << indent_str << "cin >> " << var << ";\n";
            continue;
        }

        // ── def name(param: type, ...) ──
        if (starts_with(trimmed, "def ")) {
            string sig = trim(trimmed.substr(4));
            size_t po = sig.find('('), pc = sig.rfind(')');
            if (po == string::npos || pc == string::npos) continue;

            string name = trim(sig.substr(0, po));
            string params_str = trim(sig.substr(po + 1, pc - po - 1));

            vector<pair<string, string>> params;
            if (!params_str.empty()) {
                stringstream ss(params_str);
                string token;
                while (getline(ss, token, ',')) {
                    token = trim(token);
                    size_t colon = token.find(':');
                    if (colon != string::npos) {
                        string t = trim(token.substr(0, colon));
                        string n = trim(token.substr(colon + 1));
                        if (t == "int" || t == "float" || t == "bool" || t == "double") {
                            params.emplace_back(t, n);
                        } else {
                            params.emplace_back("string", n);
                        }
                    } else {
                        params.emplace_back("string", trim(token));
                    }
                }
            }

            out << "void " << name << "(";
            for (size_t i = 0; i < params.size(); ++i) {
                if (i > 0) out << ", ";
                out << params[i].first << " " << params[i].second;
            }
            out << ") {\n";

            indent_stack.push_back(this_indent + 4); // assume next line indented
            continue;
        }

        // ── for var in range(...) ──
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

            indent_stack.push_back(this_indent + 4);
            continue;
        }

        // ── if / elif / else / while ──
        if (ends_with(trimmed, ":")) {
            string stmt = trim(trimmed.substr(0, trimmed.size() - 1));
            string keyword = stmt.substr(0, stmt.find(' '));
            string cond = trim(stmt.substr(keyword.size()));

            if (keyword == "if" || keyword == "while") {
                out << indent_str << keyword << " (" << cond << ") {\n";
                indent_stack.push_back(this_indent + 4);
            }
            else if (keyword == "elif") {
                // close previous block and open else if
                if (indent_stack.size() > 1) {
                    out << string((indent_stack.size() - 2) * 4, ' ') << "} else if (" << cond << ") {\n";
                }
            }
            continue;
        }

        if (trimmed == "else") {
            if (indent_stack.size() > 1) {
                out << string((indent_stack.size() - 2) * 4, ' ') << "} else {\n";
            }
            continue;
        }

        // ── end (optional now) ──
        if (trimmed == "end") {
            if (indent_stack.size() > 1) {
                indent_stack.pop_back();
                out << string((indent_stack.size() - 1) * 4, ' ') << "}\n";
            }
            continue;
        }

        // ── declarations / assignments / expressions ──
        if (!trimmed.empty()) {
            bool is_decl = starts_with(trimmed, "int ") || starts_with(trimmed, "float ") ||
                           starts_with(trimmed, "double ") || starts_with(trimmed, "bool ") ||
                           starts_with(trimmed, "string ");
            out << indent_str << trimmed;
            if (trimmed.back() != ';' && trimmed.back() != '{' && trimmed.back() != '}') {
                out << ";";
            }
            out << "\n";
        }
    }

    // Close all remaining blocks
    while (indent_stack.size() > 1) {
        indent_stack.pop_back();
        out << string((indent_stack.size() - 1) * 4, ' ') << "}\n";
    }

    out << "    return 0;\n}\n";

    out.close();
    in.close();

    cout << "[OK] Generated → " << cpp_out << endl;

    if (should_run) {
        string cmd = "g++ \"" + cpp_out + "\" -o \"" + exe_out + "\" -std=c++17";
        cout << "[Compiling] " << cmd << endl;
        if (system(cmd.c_str()) != 0) {
            cerr << "[Compilation failed]\n";
            return 1;
        }

        // Use PowerShell on Windows so './exe' works; otherwise use native './exe'
        // Runtime OS detection: prefer checking the `OS` environment variable
        // (on Windows it's usually "Windows_NT"). Fall back to compile-time
        // `_WIN32` if `OS` is not present.
        const char* os_env_p = getenv("OS");
        string os_env = os_env_p ? string(os_env_p) : string();
        bool is_windows_runtime = false;
    #ifdef _WIN32
        is_windows_runtime = true;
    #endif
        if (!os_env.empty()) {
            string os_l = os_env;
            transform(os_l.begin(), os_l.end(), os_l.begin(), ::tolower);
            if (os_l.find("windows") != string::npos) is_windows_runtime = true;
            else if (os_l.find("linux") != string::npos || os_l.find("unix") != string::npos) is_windows_runtime = false;
        }

        string run_cmd;
        if (is_windows_runtime) {
            string exe_path = exe_out;
            bool has_drive = exe_path.size() >= 2 && isalpha(static_cast<unsigned char>(exe_path[0])) && exe_path[1] == ':';
            bool has_slash = exe_path.find('/') != string::npos || exe_path.find('\\') != string::npos;
            string ps_target = (has_drive || has_slash) ? exe_path : string("./") + exe_path;
            for (char &c : ps_target) if (c == '/') c = '\\';
            run_cmd = string("powershell -NoProfile -ExecutionPolicy Bypass -Command \"") + "& '" + ps_target + "'" + "\"";
        } else {
            string target = exe_out;
            if (target.find('/') == string::npos) target = string("./") + target;
            run_cmd = string("/bin/sh -c \"") + target + string("\"");
        }
        cout << "[Running] " << run_cmd << "\n" << string(40, '-') << "\n";
        system(run_cmd.c_str());
    }

    return 0;
}