#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "build.h"


BuildError::BuildError(const std::string &msg)
: std::runtime_error(msg), errorMessage("")
{
    strncpy(errorMessage, msg.c_str(), errorMessageLength-1);
}
BuildError::BuildError(const Origin &origin, const std::string &msg)
: std::runtime_error(msg)
{
    std::stringstream ss;
    ss << origin << ' ' << msg;

    strncpy(errorMessage, ss.str().c_str(), errorMessageLength-1);
}
const char* BuildError::what() const throw() {
    return errorMessage;
}

std::string GameData::addString(const std::string &text) {
    if (strings.count(text) > 0) {
        return strings[text];
    } else {
        std::stringstream name;
        name << "__s" << nextString++;
        strings.insert(std::make_pair(text, name.str()));
        return name.str();
    }
}

std::string readFile(const std::string &file) {
    std::ifstream inf(file);
    if (!inf) {
        throw BuildError(Origin(), "Could not open file "+file+".");
    }
    std::string content( (std::istreambuf_iterator<char>(inf)),
                         std::istreambuf_iterator<char>() );
    return content;
}

std::string formatForDump(std::string text, size_t maxlen) {
    size_t pos = text.find_first_of('\n');
    while (pos != std::string::npos) {
        text[pos] = '^';
        pos = text.find_first_of('\n');
    }
    if (text.size() >= maxlen) {
        text = text.substr(0, maxlen - 3);
        text += "...";
        return text;
    } else {
        return text;
    }
}

int main(int argc, char *argv[]) {
    GameData gameData;
    Lexer lexer;

    std::vector<std::string> sourceFiles;
    std::string outputFile = "game.bin";

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-o") == 0) {
            ++i;
            if (i >= argc) {
                std::cerr << "-o options requires output filename\n";
                return 1;
            }
            outputFile = argv[i];
        } else {
            sourceFiles.push_back(argv[i]);
        }
    }

    if (sourceFiles.empty()) {
        std::cerr << "No source files provided!\n";
        return 1;
    }

    try {
        std::vector<SymbolDef> symbols;

        Parser parser(gameData, symbols);
        for (const std::string &file : sourceFiles) {
            lexer.doFile(file);
            parser.parseTokens(lexer.tokens.begin(), lexer.tokens.end());
        }

        bool hasStartSymbol = false;
        for (const auto &symbol : symbols) {
            if (symbol.name == "start") {
                if (symbol.type != SymbolDef::Node) {
                    std::cerr << symbol.origin << " Start node must be node.\n";
                    return 1;
                }
                hasStartSymbol = true;
            }
        }
        if (!hasStartSymbol) {
            std::cerr << "FATAL: No start node defined.\n";
            return 1;
        }
    } catch (BuildError &e) {
        std::cerr << e.what() << "\n";
        return 1;
    }

    try {
        for (std::shared_ptr<DataType> i : gameData.dataItems) {
            std::shared_ptr<SkillSet> cd = std::dynamic_pointer_cast<SkillSet>(i);
            if (cd) {
                for (unsigned i = 0; i < sklCount; ++i) {
                    if (cd->setDefaults) {
                        if (i >= gameData.skills.size()) {
                            cd->skills[i] = 0;
                        } else {
                            cd->skills[i] = gameData.skills[i]->defaultValue;
                        }
                    } else {
                        cd->skills[i] = 0;
                    }
                }

                for (auto &i : cd->skillMap) {
                    const auto &v = gameData.constants.find(i.first);
                    if (v != gameData.constants.end()) {
                        cd->skills[v->second] = i.second.value;
                    }
                }
                cd->skillMap.clear();
            }
        }

        make_bin(gameData, outputFile);
    } catch (BuildError &e) {
        std::cerr << e.what() << "\n";
    }


    /* ************************************************************************
     * DUMP GLOBAL SYMBOLS TO FILE                                            */
    std::ofstream out_defs("dbg_defs.txt");

    out_defs << "FOUND " << std::dec << gameData.dataItems.size() << " DATA ITEMS\n" << std::hex << std::uppercase;
    for (auto &item : gameData.dataItems) {
        out_defs << "    " << *item << '\n';
    }

    out_defs << "\nFOUND " << gameData.nodes.size() << " NODES\n";
    for (auto &node : gameData.nodes) {
        out_defs << "    " << std::setw(32) << node->name << ' ' << node->origin << '\n';
    }

    out_defs << "\nFOUND " << gameData.skills.size() << " SKILLS\n" << std::left;
    for (auto &skill : gameData.skills) {
        out_defs << std::left << "    " << std::setw(32);
        if (skill->name.empty()) {
            out_defs << "(unnamed)";
        } else {
            out_defs << skill->name;
        }
        out_defs << "   " << skill->origin << '\n';
    }

    out_defs << "\nFOUND " << gameData.constants.size() << " CONSTANTS:\n" << std::left;
    for (auto &constant : gameData.constants) {
        out_defs << "    " << std::setw(32) << constant.first << "   ";
        out_defs << std::setw(13) << constant.second;
        // << origin
        out_defs << '\n';
    }

    out_defs << "\nFOUND " << gameData.strings.size() << " STRINGS:\n";
    for (auto &str : gameData.strings) {
        out_defs << "    " << std::setw(8) << str.second << "  ";
        std::stringstream ss;
        ss << "~" << formatForDump(str.first, 35) << "~";
        out_defs << std::setw(37) << ss.str();
        // << origin
        out_defs << " \n";
    }

    out_defs.close();

    /* ************************************************************************
     * DUMP DATA ITEMS TO FILE                                                */
    std::ofstream out("dbg_nodes.txt");

    out << "FOUND " << gameData.nodes.size() << " NODES\n";
    for (auto &node : gameData.nodes) {
        out << '\n' << node->name << '\n';
        for (auto &stmt : node->block->statements) {
            out << "   ";
            for (auto &text : stmt->parts) {
                out << ' ' << text;
            }
            out << ";\n";
        }
    }

    /* ********************************************************************** */
    return 0;
}