#ifndef PLAY_H
#define PLAY_H

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "constants.h"

#include "playerror.h"

struct SkillDef {
    unsigned baseSkill;
    unsigned nameAddress;
    unsigned flags;
    unsigned defaultValue;
    unsigned recoveryRate;

    bool testFlags(unsigned testFor) const {
        return (flags & testFor) == testFor;
    }
};

struct DamageType {
    unsigned nameAddress;
};

class Game {
public:
    struct Character {
        std::uint32_t def;
        std::uint32_t sex, species;
        std::map<unsigned, int> resistAdj;
        std::map<unsigned, int> skillAdj;
        std::map<unsigned, int> skillCur;
        std::map<std::uint32_t, std::uint32_t> gear;
    };


    class Option {
    public:
        Option()
        : dest(0)
        { }
        Option(std::uint32_t name, std::uint32_t destination, std::uint32_t extra = 0)
        : name(name), dest(destination), extra(extra)
        { }

        std::uint32_t name;
        std::uint32_t dest;
        std::uint32_t extra;
    };

    class CarriedItem {
    public:
        CarriedItem()
        : qty(0), itemIdent(0)
        { }
        CarriedItem(int qty, int itemIdent)
        : qty(qty), itemIdent(itemIdent)
        { }

        int qty;
        std::uint32_t itemIdent;
    };

    Game()
    : gameStarted(false), locationName(0), isRunning(false), data(nullptr),
      gameTime(0), inCombat(false), startedCombat(false)
    { }
    ~Game() {
        delete[] data;
    }

    // ////////////////////////////////////////////////////////////////////////
    // Game Engine Startup                                                   //
    void loadDataFromFile(const std::string &filename);
    void setDataAs(uint8_t *data, size_t size);

    // ////////////////////////////////////////////////////////////////////////
    // Fetching game data                                                    //
    int getSkillCount() const;
    const SkillDef* getSkillDef(unsigned skillNo) const;
    int getDamageTypeCount() const;
    const DamageType* getDamageType(unsigned damageTypeNo) const;

    // ////////////////////////////////////////////////////////////////////////
    // Fetching game state                                                   //
    std::string getTimeString(bool exact = false);
    std::string getOutput() const;

    std::uint32_t getObjectProperty(std::uint32_t objRef, std::uint16_t propId);
    bool objectHasProperty(std::uint32_t objRef, std::uint16_t propId);
    std::string getNameOf(std::uint32_t address);
    std::string getPronoun(std::uint32_t cRef, int pronounType);

    Character* getCharacter(std::uint32_t address);
    const Character* getCharacter(std::uint32_t address) const;
    void doRest(int forTime);
    bool isKOed(std::uint32_t cRef);
    int skillRecoveryRate(int skillNo);
    int getSkillMax(std::uint32_t cRef, int skillNo);
    int getSkillCur(std::uint32_t cRef, int skillNo);
    void adjResistance(std::uint32_t cRef, int damageType, int amount);
    int getResistance(std::uint32_t cRef, int damageType);
    std::vector<std::uint32_t> getActions(std::uint32_t cRef);

    std::uint8_t readByte(std::uint32_t pos) const;
    std::uint16_t readShort(std::uint32_t pos) const;
    std::uint32_t readWord(std::uint32_t pos) const;

    // ////////////////////////////////////////////////////////////////////////
    // Player action commands                                                //
    bool actionAllowed() const;
    bool isInCombat() const;
    int combatStatus();
    void doOption(int optionNumber);
    void useItem(int itemNumber);
    void equipItem(std::uint32_t whoIdent, int itemNumber);
    void unequipItem(std::uint32_t whoIdent, std::uint32_t slotIdent);
    void doAction(std::uint32_t cRef, std::uint32_t action);


    // ////////////////////////////////////////////////////////////////////////
    // Public game state data                                                //
    bool gameStarted;
    unsigned currentCombatant, combatRound;
    std::vector<Option> options;
    std::vector<CarriedItem> inventory;
    std::uint32_t locationName;
    std::vector<std::uint32_t> party;
    std::vector<std::uint32_t> combatants;
private:
    // ////////////////////////////////////////////////////////////////////////
    // Miscellaneous                                                         //
    void doGameSetup();
    static int roll(int dice, int sides);

    // ////////////////////////////////////////////////////////////////////////
    // Raw Data Management                                                   //
    int getType(std::uint32_t address) const;
    bool isType(std::uint32_t address, uint8_t type) const;
    const char *getString(std::uint32_t address) const;
    std::uint32_t getFromMap(std::uint32_t address, std::uint32_t value) const;
    bool mapHasValue(std::uint32_t address, std::uint32_t value) const;

    // ////////////////////////////////////////////////////////////////////////
    // Character Management                                                  //
    void resetCharacter(std::uint32_t cRef);
    void restoreCharacter(std::uint32_t cRef);
    void doDamage(std::uint32_t cRef, int amount, int to, int type);
    int doSkillCheck(std::uint32_t cRef, int skill, int modifiers, int target);
    void adjSkillMax(std::uint32_t cRef, int skillNo, int adjustment);
    void adjSkillCur(std::uint32_t cRef, int skillNo, int adjustment);

    // ////////////////////////////////////////////////////////////////////////
    // combat methods                                                        //
    void doCombatLoop();
    void advanceCombatant();
    void doCombatOptions();

    // ////////////////////////////////////////////////////////////////////////
    // node execution                                                        //
    void doScene(std::uint32_t address);
    std::uint32_t call(std::uint32_t sceneOrNode, bool clearAfter, bool clearBefore);
    std::uint32_t doNode(std::uint32_t address);

    // ////////////////////////////////////////////////////////////////////////
    // Output manipulation                                                   //
    void clearOutput();
    void say(const std::string &text);
    void say(int number);
    void sayError(const std::string &errorMessage);

    // ////////////////////////////////////////////////////////////////////////
    // inventory management                                                  //
    bool addItems(int qty, std::uint32_t itemIdent);
    bool removeItems(int qty, std::uint32_t itemIdent);
    int itemQty(std::uint32_t itemIdent);

    // ////////////////////////////////////////////////////////////////////////
    // stack and stored data management                                      //
    uint32_t fetch(uint32_t key) const;
    void setTemp(unsigned tempNo, std::uint32_t value);


    // ////////////////////////////////////////////////////////////////////////
    // Private data storage                                                  //
    bool isRunning;
    std::map<std::uint32_t, std::uint32_t> storage;
    std::uint32_t location;
    bool inLocation;
    bool newLocation;
    uint8_t *data;
    size_t dataSize;
    std::map<std::uint32_t, Character*> characters;
    std::string outputBuffer;
    unsigned gameTime;
    bool inCombat, startedCombat;
    std::uint32_t afterCombatNode;

    std::vector<SkillDef> skillDefs;
    std::vector<DamageType> damageTypes;
};

std::string toTitleCase(std::string text);
std::string toUpperFirst(std::string text);
std::string trim(std::string text);
std::vector<std::string> explodeString(const std::string &text, int onChar = '\n');
std::vector<std::string> wrapString(const std::string &text, unsigned width);
std::string& tidyString(std::string &text);

const int hoursPerDay = 24;
const int minutesPerDay = 1440;
const int minutesPerHour = 60;

#endif
