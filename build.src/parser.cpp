#include <iostream>
#include <memory>
#include <sstream>

#include "build.h"

void Parser::checkSymbol(const Origin &origin, const std::string &name, SymbolDef::Type type) {
    for (const auto &i : symbols) {
        if (i.name == name) {
            std::stringstream ss;
            ss << "Symbol " << name << " was already defined at " << i.origin << ".";
            throw BuildError(origin, ss.str());
        }
    }
    symbols.push_back(SymbolDef(origin, name, type));
}

void Parser::parseTokens(std::list<Token>::iterator start, std::list<Token>::iterator end) {
    cur = start;

    while (cur != end) {

        if (matches("NODE")) {
            doNode();
        } else if (matches("TITLE")) {
            doTitle();
        } else if (matches("BYLINE")) {
            doByline();
        } else if (matches("VERSION")) {
            doVersion();
        } else if (matches("CONSTANT")) {
            doConstant();
        } else if (matches("ITEM")) {
            doItemDef();
        } else if (matches("SEX")) {
            doSex();
        } else if (matches("SPECIES")) {
            doSpecies();
        } else if (matches("SKILL")) {
            doSkill();
        } else if (matches("CHARACTER")) {
            doCharacter();
        } else {
            std::stringstream ss;
            throw BuildError(cur->origin, "Expected top level construct");
        }
    }

}

void Parser::doTitle() {
    require("TITLE");
    require(Token::String);
    gameData.title = gameData.addString(cur->text);
    ++cur;
    require(Token::Semicolon, true);
}

void Parser::doByline() {
    require("BYLINE");
    require(Token::String);
    gameData.byline = gameData.addString(cur->text);
    ++cur;
    require(Token::Semicolon, true);
}

void Parser::doVersion() {
    require("VERSION");
    require(Token::String);
    gameData.version = gameData.addString(cur->text);
    ++cur;
    require(Token::Semicolon, true);
}

void Parser::doConstant() {
    const Origin &origin = cur->origin;
    require("CONSTANT");
    require(Token::Identifier);
    const std::string &name = cur->text;
    checkSymbol(origin, name, SymbolDef::Constant);
    ++cur;

    require(Token::Integer);
    gameData.constants.insert(std::make_pair(name, cur->value));
    ++cur;
    require(Token::Semicolon, true);
}

void Parser::doNode() {
    const Origin &origin = cur->origin;
    require("NODE");
    require(Token::Identifier);

    std::string nodeName = cur->text;
    checkSymbol(origin, nodeName, SymbolDef::Node);

    ++cur;
    std::shared_ptr<Block> block(doBlock());
    std::shared_ptr<Node> node(new Node);
    node->name = nodeName;
    node->block = block;
    node->origin = origin;

    gameData.nodes.push_back(node);
}

void Parser::doItemDef() {
    const Origin &origin = cur->origin;
    require("ITEM");
    require(Token::Identifier);
    const std::string &name = cur->text;
    checkSymbol(origin, name, SymbolDef::Item);
    ++cur;

    std::shared_ptr<ItemDef> item(new ItemDef);
    item->origin = origin;
    item->name = name;
    require(Token::OpenBrace, true);
    require(Token::String);
    item->article = gameData.addString(cur->text);
    ++cur;
    require(Token::String);
    item->singular = gameData.addString(cur->text);
    ++cur;
    require(Token::String);
    item->plural = gameData.addString(cur->text);
    ++cur;

    item->flags = doFlags();

    while (!matches(Token::CloseBrace)) {
        const Origin &pOrigin = cur->origin;
        require(Token::Identifier);
        const std::string &pName = cur->text;
        ++cur;

        if (pName == "onUse") {
            const Value &value = doProperty(item->name);
            item->onUse = value;
        } else if (pName == "canEquip") {
            const Value &value = doProperty(item->name);
            item->canEquip = value;
        } else if (pName == "onEquip") {
            const Value &value = doProperty(item->name);
            item->onEquip = value;
        } else if (pName == "onRemove") {
            const Value &value = doProperty(item->name);
            item->onRemove = value;
        } else if (pName == "slot") {
            const Value &value = doValue();
            item->slot = value;
        } else {
            throw BuildError(pOrigin, "Unknown item property " + pName);
        }
    }
    ++cur;

    gameData.dataItems.push_back(item);
}

void Parser::doSex() {
    const Origin &origin = cur->origin;
    require("SEX");
    require(Token::Identifier);
    const std::string &name = cur->text;
    checkSymbol(origin, name, SymbolDef::Sex);
    ++cur;

    std::shared_ptr<SexDef> sex(new SexDef);
    sex->origin = origin;
    sex->name = name;
    require(Token::OpenBrace, true);

    require(Token::String);
    sex->displayName = gameData.addString(cur->text);
    ++cur;

    sex->flags = doFlags();

    require(Token::String);
    sex->subject = gameData.addString(cur->text);
    ++cur;
    require(Token::String);
    sex->object = gameData.addString(cur->text);
    ++cur;
    require(Token::String);
    sex->possess = gameData.addString(cur->text);
    ++cur;
    require(Token::String);
    sex->adject = gameData.addString(cur->text);
    ++cur;
    require(Token::String);
    sex->reflex = gameData.addString(cur->text);
    ++cur;

    require(Token::CloseBrace, true);
    gameData.dataItems.push_back(sex);
}

void Parser::doSpecies() {
    const Origin &origin = cur->origin;
    require("SPECIES");
    require(Token::Identifier);
    const std::string &name = cur->text;
    checkSymbol(origin, name, SymbolDef::Species);
    ++cur;

    std::shared_ptr<SpeciesDef> species(new SpeciesDef);
    species->origin = origin;
    species->name = name;
    require(Token::OpenBrace, true);

    require(Token::String);
    species->displayName = gameData.addString(cur->text);
    ++cur;

    species->flags = doFlags();

    require(Token::CloseBrace, true);
    gameData.dataItems.push_back(species);
}

void Parser::doSkill() {
    const Origin &origin = cur->origin;
    require("SKILL");
    require(Token::Identifier);
    const std::string &name = cur->text;
    checkSymbol(origin, name, SymbolDef::Skill);
    ++cur;

    // create constant with skill name
    gameData.constants.insert(std::make_pair(name, skillCounter++));

    std::shared_ptr<SkillDef> skill(new SkillDef);
    skill->origin = origin;
    skill->name = name;

    skill->statSkill = doValue();

    require(Token::String);
    skill->displayName = gameData.addString(cur->text);
    ++cur;

    require(Token::Integer);
    skill->defaultValue = cur->value;
    ++cur;

    skill->flags = doFlags();

    require(Token::Semicolon, true);
    gameData.skills.push_back(skill);
}

void Parser::doCharacter() {
    const Origin &origin = cur->origin;
    require("CHARACTER");
    require(Token::Identifier);
    const std::string &name = cur->text;
    checkSymbol(origin, name, SymbolDef::Character);
    ++cur;

    std::shared_ptr<CharacterDef> character(new CharacterDef);
    character->origin = origin;
    character->name = name;
    require(Token::OpenBrace, true);

    require(Token::String);
    character->article = gameData.addString(cur->text);
    ++cur;
    require(Token::String);
    character->displayName = gameData.addString(cur->text);
    ++cur;

    character->sex = doValue();
    character->species = doValue();

    character->flags = doFlags();

    while (!matches(Token::CloseBrace)) {
        require(Token::Identifier);
        if (cur->text == "faction") {
            ++cur;
            character->faction = doValue();
        } else if (cur->text == "skills") {
            ++cur;
            require(Token::OpenParan);
            ++cur;
            while (!matches(Token::CloseParan)) {
                require(Token::Identifier);
                const std::string &name = cur->text;
                ++cur;
                const Value &v = doValue();
                require(Token::Semicolon, true);
                character->skillMap.insert(std::make_pair(name, v));
            }
            ++cur;
        } else if (cur->text == "gear") {
            ++cur;
            require(Token::OpenParan);
            ++cur;
            while (!matches(Token::CloseParan)) {
                require(Token::String);
                const std::string &name = gameData.addString(cur->text);
                ++cur;
                require(Token::Identifier);
                const std::string &v = cur->text;
                ++cur;
                require(Token::Semicolon, true);
                character->gear.insert(std::make_pair(name, v));
            }
            ++cur;
        } else {
            throw BuildError(origin, "Unknown character property " + cur->text);
        }
    }

    require(Token::CloseBrace, true);
    gameData.dataItems.push_back(character);
}

Value Parser::doProperty(const std::string &forName) {
    const Origin &origin = cur->origin;
    if (matches(Token::Identifier)) {
        const std::string &name = cur->text;
        ++cur;
        require(Token::Semicolon, true);
        return Value(name);
    } else if (matches(Token::OpenBrace)) {
        std::stringstream ss;
        ss << "__an_" << forName << "_" << anonymousCounter;
        ++anonymousCounter;
        std::shared_ptr<Node> node(new Node);
        node->block = std::shared_ptr<Block>(doBlock());
        node->name = ss.str();
        gameData.nodes.push_back(node);
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

std::unordered_set<Value> Parser::doFlags() {
    std::unordered_set<Value> flags;

    if (!matches(Token::OpenParan)) {
        return flags;
    }
    ++cur;

    while (!matches(Token::CloseParan)) {
        flags.insert(doValue());
    }
    ++cur;

    return flags;
}


std::shared_ptr<Block> Parser::doBlock() {
    std::shared_ptr<Block> block(new Block);
    require(Token::OpenBrace, true);
    while (!matches(Token::CloseBrace)) {
        std::shared_ptr<Statement> statement = doStatement();
        if (statement) {
            block->statements.push_back(statement);
        }
    }
    std::shared_ptr<Statement> doneStmt(new Statement);
    doneStmt->parts.push_back(Value("end"));
    block->statements.push_back(doneStmt);
    ++cur;
    return block;
}

std::shared_ptr<Statement> Parser::doStatement() {
    const Origin &origin = cur->origin;
    std::shared_ptr<Statement> statement(new Statement);
    statement->origin = origin;
    while (!matches(Token::Semicolon)) {
        statement->parts.push_back(doValue());
    }
    ++cur;
    if (statement->parts.empty()) {
        return nullptr;
    }

    const Value &cmdName = statement->parts.front();
    if (cmdName.type != Value::Identifier) {
        throw BuildError(origin, "Command must be identifier");
    }

    statement->commandInfo = getCommand(cmdName.text);
    if (!statement->commandInfo) {
        throw BuildError(origin, "Unknown command " + cmdName.text);
    }

    if (statement->parts.size() != (unsigned)statement->commandInfo->argCount + 1) {
        std::stringstream ss;
        ss << "Command " << cmdName.text << " expects " << statement->commandInfo->argCount;
        ss << " arguments, but " << (statement->parts.size() - 1) << " were found.";
        throw BuildError(origin, ss.str());
    }


    return statement;
}

Value Parser::doValue() {
    if (matches(Token::String)) {
        std::string label = gameData.addString(cur->text);
        ++cur;
        return Value(label);
    } else if (matches(Token::Identifier)) {
        std::string label;
        if (cur->text[0] == '#') {
            std::string label = cur->text.substr(1);
            ++cur;
            return Value(Value::Global, label);
        } else {
            std::string label = cur->text;
            ++cur;
            return Value(Value::Identifier, label);
        }
    } else if (matches(Token::Integer)) {
        int v = cur->value;
        ++cur;
        return v;
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


std::ostream& operator<<(std::ostream &out, const Value &type) {
    if (type.type == Value::Identifier) {
        out << "T:" << type.text;
    } else {
        out << "I:" << type.value;
    }
    return out;
}
