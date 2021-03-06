#include <iostream>
#include <memory>
#include <sstream>

#include "build.h"
#include "symboltable.h"

struct ObjectDefSpecialization {
    std::string name;
    int classId;
    const char *requiredProperties[10];
};

void Parser::parseTokens(std::list<Token>::iterator start, std::list<Token>::iterator end) {
    cur = start;
    ObjectDefSpecialization objectTypes[] = {
        { "sex",       ocSex,       { "name", "object", "reflexive", "adjective", "possessive" } },
        { "species",   ocSpecies,   { "name" } },
        { "action",    ocAction,    { "name" } },
        { "item",      ocItem,      { "article", "name", "plural" } },
        { "character", ocCharacter, { "article", "name", "sex", "species" } },
        { "scene",     ocScene,     { "body" } },
        { "event",     ocEvent,     { "count", "body" } },
        { "" }
    };

    while (cur != end) {
        const Origin &origin = cur->origin;

        if (!matches(Token::Identifier)) {
            std::stringstream ss;
            ss << "Expected identifier at top level, but found " << cur->type << ".";
            throw BuildError(origin, ss.str());
        }

        if (matches("node")) {
            doNode();
        } else if (matches("constant")) {
            doConstant();
        } else if (matches("skill")) {
            doSkill();
        } else if (matches("damage-types")) {
            doDamageTypes();
        } else if (matches("object")) {
            doObject();
        } else {
            bool foundType = false;

            for (const auto &type : objectTypes) {
                if (cur->text == type.name) {
                    ++cur;
                    doObjectClass(origin, type);
                    foundType = true;
                    break;
                }
            }

            if (!foundType) {
                std::stringstream ss;
                ss << "Expected top level construct, but found " << cur->type;
                if (cur->type == Token::Identifier) {
                    ss << " ~" << cur->text << '~';
                }
                throw BuildError(origin, ss.str());
            }
        }
    }
}


void Parser::doConstant() {
    const Origin &origin = cur->origin;
    require("constant");
    require(Token::Identifier);
    const std::string &name = cur->text;
    symbols.add(origin, name, SymbolDef::Constant);
    ++cur;

    if (matches(Token::Integer)) {
        gameData.constants.insert(std::make_pair(name, Value(cur->value)));
    } else if (matches(Token::String)) {
        std::string labelName = gameData.addString(cur->text, symbols);
        gameData.constants.insert(std::make_pair(name, Value(labelName)));
    } else {
        throw BuildError(origin, "Constant value must be integer literal or string.");
    }
    ++cur;
    require(Token::Semicolon, true);
}

void Parser::doNode() {
    const Origin &origin = cur->origin;
    require("node");
    require(Token::Identifier);

    std::string nodeName = cur->text;
    symbols.add(origin, nodeName, SymbolDef::Node);

    ++cur;
    std::shared_ptr<Block> block(doBlock());
    std::shared_ptr<Node> node(new Node);
    node->name = nodeName;
    node->block = block;
    node->origin = origin;

    gameData.nodes.push_back(node);
}

void Parser::doDamageTypes() {
    const Origin &origin = cur->origin;
    require("damage-types");
    require(Token::OpenBrace, true);
    unsigned typeNumber = 0;
    while (!matches(Token::CloseBrace)) {
        require(Token::Identifier);
        const std::string &typeName = cur->text;
        ++cur;
        require(Token::String);
        std::string typeRef = gameData.addString(cur->text, symbols);
        ++cur;

        symbols.add(origin, typeName, SymbolDef::DamageType);
        gameData.constants.insert(std::make_pair(typeName, Value(typeNumber++)));
        gameData.damageTypes.push_back(typeRef);
    }
    ++cur;
}

void Parser::doObject() {
    const Origin &origin = cur->origin;
    require("object");
    std::shared_ptr<ObjectDef> obj = doObjectCore(origin);

    gameData.dataItems.push_back(obj);
}

std::shared_ptr<ObjectDef> Parser::doObjectCore(const Origin &origin) {
    require(Token::Identifier);
    std::shared_ptr<ObjectDef> obj(new ObjectDef);
    obj->origin = origin;
    obj->name = cur->text;
    symbols.add(origin, cur->text, SymbolDef::ObjectDef);
    ++cur;
    require(Token::OpenBrace, true);
    while (!matches(Token::CloseBrace)) {
        require(Token::Identifier);
        const std::string &propName = cur->text;
        ++cur;

        Value value = doProperty(obj->name, propName);

        std::uint32_t propId = ObjectDef::getPropertyIdent(propName);
        if (propId == propInternalName) {
            throw BuildError(origin, "Cannot set object internal name");
        }
        auto iter = obj->properties.find(propId);
        if (iter != obj->properties.end()) {
            throw BuildError(origin, "Duplicate property " + propName);
        } else {
            obj->properties.insert(std::make_pair(propId, value));
        }
    }
    ++cur;

    auto identIter = obj->properties.find(propIdent);
    if (identIter == obj->properties.end()) {
        obj->properties.insert(std::make_pair(propIdent, Value(ObjectDef::nextIdent)));
        ++ObjectDef::nextIdent;
    } else {
        if (identIter->second.type != Value::Integer) {
            throw BuildError(origin, "Ident property must be integer.");
        }
        std::uint32_t identValue = identIter->second.value;
        if (identValue < ObjectDef::nextIdent) {
            throw BuildError(origin, "Manual ident property too low.");
        }
        ObjectDef::nextIdent = identValue + 1;
    }
    obj->properties.insert(std::make_pair(propInternalName, Value(gameData.addString(obj->name, symbols))));
    return obj;
}

void Parser::doObjectClass(const Origin &origin, const ObjectDefSpecialization &def) {
    std::shared_ptr<ObjectDef> newObj = doObjectCore(origin);
    newObj->properties.insert(std::make_pair(propClass, Value(def.classId)));

    for (int i = 0; def.requiredProperties[i] != nullptr; ++i) {
        const char *name = def.requiredProperties[i];
        if (!newObj->hasProperty(name)) {
            std::stringstream ss;
            ss << "Object " << newObj->name << " requires property " << name;
            throw BuildError(newObj->origin, ss.str());
        }
    }

    gameData.dataItems.push_back(newObj);
}


void Parser::doList(const std::string &myName) {
    const Origin &origin = cur->origin;

    require(Token::OpenParan, true);
    std::shared_ptr<DataList> list(new DataList);
    list->origin = origin;
    list->name = myName;
    symbols.add(origin, list->name, SymbolDef::List);
    while (!matches(Token::CloseParan)) {
        Value v = doValue();
        list->values.push_back(v);
    }
    ++cur;

    gameData.dataItems.push_back(list);
}

void Parser::doMap(const std::string &myName) {
    const Origin &origin = cur->origin;

    require(Token::OpenParan, true);
    std::shared_ptr<DataMap> skillMap(new DataMap);
    skillMap->origin = origin;
    skillMap->name = myName;
    symbols.add(origin, skillMap->name, SymbolDef::Map);
    while (!matches(Token::CloseParan)) {
        const Value &name = doValue();
        const Value &v = doValue();
        require(Token::Semicolon, true);
        skillMap->mapData.insert(std::make_pair(name, v));
    }
    ++cur;

    gameData.dataItems.push_back(skillMap);
}

void Parser::doSkill() {
    const Origin &origin = cur->origin;
    require("skill");
    require(Token::Identifier);
    const std::string &name = cur->text;
    symbols.add(origin, name, SymbolDef::Skill);
    ++cur;

    // create constant with skill name
    gameData.constants.insert(std::make_pair(name, Value(skillCounter++)));

    std::shared_ptr<SkillDef> skill(new SkillDef);
    skill->origin = origin;
    skill->name = name;

    skill->statSkill = doValue();

    require(Token::String);
    skill->displayName = gameData.addString(cur->text, symbols);
    ++cur;

    require(Token::Integer);
    skill->defaultValue = cur->value;
    ++cur;

    if (matches(Token::Integer)) {
        skill->recoveryRate = cur->value;
        ++cur;
    }

    skill->flags = doFlags();

    require(Token::Semicolon, true);
    gameData.skills.push_back(skill);
}

Value Parser::doProperty(const std::string &forName, const std::string &propName) {
    const Origin &origin = cur->origin;
    std::stringstream anonymousName;
    anonymousName << "__prop_" << forName << "__" << propName;

    if (matches(Token::String)) {
        std::string label = gameData.addString(cur->text, symbols);
        ++cur;
        require(Token::Semicolon, true);
        return Value(label);
    } else if (matches(Token::Identifier)) {
        std::string &name = cur->text;
        ++cur;
        if (matches(Token::OpenParan)) {
            if (name == "list") {
                name = anonymousName.str();
                doList(name);
            } else if (name == "map") {
                name = anonymousName.str();
                doMap(name);
            } else if (name == "flags") {
                Value flagSet(Value::FlagSet);
                flagSet.mFlagSet = doFlags();
                require(Token::Semicolon, true);
                return flagSet;
            } else {
                throw BuildError(origin, "Unknown property type name " + name + ".");
            }
        } else if (name[0] == '$') {
            std::uint16_t ident = ObjectDef::getPropertyIdent(name.substr(1));
            require(Token::Semicolon, true);
            return Value(ident);
        }
        require(Token::Semicolon, true);
        return Value(name);
    } else if (matches(Token::OpenBrace)) {
        std::shared_ptr<Node> node(new Node);
        node->block = std::shared_ptr<Block>(doBlock());
        node->name = anonymousName.str();
        gameData.nodes.push_back(node);
        require(Token::Semicolon, true);
        symbols.add(origin, node->name, SymbolDef::Node);
        return Value(node->name);
    } else if (matches(Token::Integer)) {
        int v = cur->value;
        ++cur;
        require(Token::Semicolon, true);
        return Value(v);
    } else {
        throw BuildError(origin, "Expected property value.");
    }
}

std::vector<Value> Parser::doFlags() {
    std::vector<Value> flags;

    if (!matches(Token::OpenParan)) {
        return flags;
    }
    ++cur;

    while (!matches(Token::CloseParan)) {
        flags.push_back(doValue());
    }
    ++cur;

    return flags;
}


std::shared_ptr<Block> Parser::doBlock() {
    std::shared_ptr<Block> block(new Block);
    require(Token::OpenBrace, true);
    while (!matches(Token::CloseBrace)) {
        doStatement(block);
    }
    std::shared_ptr<Statement> doneStmt(new Statement);
    doneStmt->parts.push_back(Value("end"));
    block->statements.push_back(doneStmt);
    ++cur;
    return block;
}

void Parser::doStatement(std::shared_ptr<Block> forBlock) {
    const Origin &origin = cur->origin;
    std::shared_ptr<Statement> statement(new Statement);
    statement->origin = origin;

    if (matches(Token::Identifier)) {
        const Command *cmd = getCommand(cur->text);
        statement->commandInfo = cmd;
        if (cmd == nullptr) {
            statement->parts.push_back(Value("push"));
            statement->parts.push_back(doValue());
        } else {
            statement->parts.push_back(doValue());
            for (int i = 0; i < cmd->argCount; ++i) {
                statement->parts.push_back(doValue());
            }
        }
        forBlock->statements.push_back(statement);
    } else if (matches(Token::String) || matches(Token::Integer)) {
        statement->parts.push_back(Value("push"));
        statement->parts.push_back(doValue());
        forBlock->statements.push_back(statement);
    } else if (matches(Token::Indirection)) {
        ++cur;
        statement->parts.push_back(Value("push"));
        statement->parts.push_back(doValue());
        forBlock->statements.push_back(statement);
        std::shared_ptr<Statement> secondStmt(new Statement);
        secondStmt->parts.push_back(Value("fetch"));
        forBlock->statements.push_back(secondStmt);
    } else if (matches(Token::Say)) {
        ++cur;
        statement->parts.push_back(Value("push"));
        Value valueToSay = doValue();
        statement->parts.push_back(valueToSay);
        forBlock->statements.push_back(statement);
        std::shared_ptr<Statement> secondStmt(new Statement);
        if (valueToSay.type == Value::Integer) {
            secondStmt->parts.push_back(Value("say-number"));
        } else {
            secondStmt->parts.push_back(Value("say"));
        }
        forBlock->statements.push_back(secondStmt);
    } else {
        std::stringstream ss;
        ss << "Unhandled token type " << cur->type << " in statement.";
        throw BuildError(origin, ss.str());
    }
}

Value Parser::doValue() {
    if (matches(Token::String)) {
        std::string label = gameData.addString(cur->text, symbols);
        ++cur;
        return Value(label);
    } else if (matches(Token::Identifier)) {
        std::string label;
        if (cur->text[0] == '#') {
            std::string label = cur->text.substr(1);
            ++cur;
            return Value(Value::Global, label);
        } else if (cur->text[0] == '$') {
            std::uint16_t ident = ObjectDef::getPropertyIdent(cur->text.substr(1));
            ++cur;
            return Value(ident);
        } else {
            std::string label = cur->text;
            ++cur;
            return Value(Value::Identifier, label);
        }
    } else if (matches(Token::Integer)) {
        int v = cur->value;
        ++cur;
        return Value(v);
    } else {
        std::stringstream ss;
        ss << "Expected value type, but found " << cur->type << '.';
        throw BuildError(cur->origin, ss.str());
    }
}



void Parser::require(Token::Type type, bool advance) {
    if (cur->type != type) {
        std::stringstream ss;
        ss << "Expected " << type << " but found " << cur->type;
        throw BuildError(cur->origin, ss.str());
    }
    if (advance) ++cur;
}

void Parser::require(const std::string &text) {
    if (cur->type != Token::Identifier || cur->text != text) {
        std::stringstream ss;
        ss << "Expected \"" << text << "\" but found ";
        if (cur->type == Token::Identifier) {
            ss << '"' << cur->text << '"';
        } else {
            ss << cur->type;
        }
        ss << '.';
        throw BuildError(cur->origin, ss.str());
    }
    ++cur;
}

bool Parser::matches(Token::Type type) {
    return cur->type == type;
}

bool Parser::matches(const std::string &text) {
    return (cur->type == Token::Identifier && cur->text == text);
}
