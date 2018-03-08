#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <math.h>
#include <limits>
#include <unordered_map>
#include <map>
#include <sstream>
#include <set>

//#define MAX_INT numeric_limits<int>::max()

using namespace std;

enum STRATEGY
{
    NONE=0,
    ATTACK_FRONTLINE,
    COLLECT_GOLD,
    DEFEND_TOWER,
    ATTACK_ENEMY_TOWER,
    SWARM_ATTACK_ENEMY_HERO,
    UPGRADE,
    UPGRADE_HEALTH,
    SELF_DEFENCE,
    END
};


string StratText[END]={"NONE", "A_F", "C_G", "D_T", "A_E_T", "S_A_E_H", "U", "U_H", "S_D"};

struct Point{
    Point():x(0),y(0){}
    Point(int _x, int _y):x(_x), y(_y){}
    void operator=(const Point& o){x=o.x; y=o.y;}

    int distanceTo(Point& other) const
    {
        return sqrt(pow(x-other.x, 2) + pow(y-other.y, 2));
    }

    int distance2To(Point& other)
    {
        return sqrt(pow(x-other.x, 2) + pow(y-other.y, 2));
    }
    int x;
    int y;
};

ostream& operator<<(ostream& os, const Point& p)
{
    os << p.x << " " << p.y << endl;
}

struct SpawnPoint: public Point
{
    string entityType; // BUSH, from wood1 it can also be SPAWN
    int radius;
};

struct Item
{
    //    string itemName; // contains keywords such as BRONZE, SILVER and BLADE, BOOTS connected by "_" to help you sort easier
    //    int itemCost; // BRONZE items have lowest cost, the most expensive items are LEGENDARY
    //    int damage; // keyword BLADE is present if the most important item stat is damage
    //    int health;
    //    int maxHealth;
    //    int mana;
    //    int maxMana;
    //    int moveSpeed; // keyword BOOTS is present if the most important item stat is moveSpeed
    //    int manaRegeneration;
    //    int isPotion; // 0 if it's not instantly consumed

    string itemName;
    map<string, int> values;

    friend ostream& operator<<(ostream& os, const Item& i);
};



ostream& operator<<(ostream& os, const Item& i)
{
    //os <<i.values["itemName"] <<" cost: " << i.values["itemCost"] <<" damage: " << i.values["damage"] <<" health: " << i.values["health"] <<" maxHealth " << i.values["maxHealth"] <<" mana " << i.values["mana"] <<" moveSpeed " << i.values["moveSpeed"] <<" manaRegeneration " << i.values["manaRegeneration"] <<" isPotion " << i.values["isPotion"] << endl;
    os << i.itemName;
    for(auto& it: i.values)
    {
        os << " " << it.first << ": " << it.second;
    }
    os << endl;
}

struct Entity: public Point
{
    Entity():actioned(false)
    {
        countDown.resize(3);
        nearestNeutral = nearestOpponentUnit = 5000;//Arbitrary large val
        enemiesAround = 0;
    }

    bool isUnderAttack(){return enemiesAround > 0;}

    int unitId;
    int team;
    string unitType; // UNIT, HERO, TOWER, can also be GROOT from wood1
    int attackRange;
    float health;
    float maxHealth;
    int shield; // useful in bronze
    int attackDamage;
    int movementSpeed;
    int stunDuration; // useful in bronze
    int goldValue;
    vector<int> countDown;
    int mana;
    int maxMana;
    int manaRegeneration;
    string heroType; // DEADPOOL, VALKYRIE, DOCTOR_STRANGE, HULK, IRONMAN
    int isVisible; // 0 if it isn't
    int itemsOwned; // useful from wood1

    int distanceFromHome;
    int distanceToOpponentTower;
    int enemiesAround;
    bool actioned;

    int nearestNeutral;
    int nearestEnemyId;
    int nearestOpponentUnit;

    STRATEGY Strategy;

    friend ostream& operator<<(ostream& os, const Entity& i);

};

//Globals
int myTeam;
int oppTeam;
int neutralTeam;
int bushAndSpawnPointCount;// usefrul from wood1, represents the number of bushes and the number of places where neutral units can spawn
int itemCount; // useful from wood2

int gold;
int enemyGold;
int roundType; // a positive value will show the number of heroes that await a command

int entityCount;
Entity homeTower;
Entity enemyTower;
Entity nearestEnemyInFrontLine;


vector<SpawnPoint> spawnPoints;
vector<Item> items;
vector<Entity> allEntities;

vector<Entity> enemyHeros;
int enemyIdToSwarm;
unordered_map<int, vector<string>> itemsOwnedMap;

unordered_map<int, Entity> prevHeroStates;
int heroCount;
unordered_map<int, int> entityIndex;


struct Hero : public Entity
{
    Hero(const Entity& E)
        :Entity(E)
    {

    }

    void printMove(string& move)
    {
        if(!actioned)
        {
            cout << move << "; "<< StratText[Strategy] << endl;
            actioned = true;
        }
    }
    void printMove(stringstream& move)
    {
        if(!actioned)
        {
            cout << move.str() << "; "<< StratText[Strategy] << endl;
            actioned = true;
        }
    }

    void buyItems(string skill)
    {
        cerr << "buyitems: skill, itemsOwned, max cost: " << skill << " " << itemsOwned << " " << gold << endl;
        sort(items.begin(), items.end(), [&skill](Item& i1, Item& i2)-> bool{return i1.values[skill] > i2.values[skill];});

        if(itemsOwned > 3)
        {
            sellItems();
            return;
        }

        for(Item& i: items)
        {
            cerr << i;
            if(i.values["itemCost"] < gold && i.values[skill] > 0)
            {
                gold -= i.values["itemCost"];
                string o;
                o = "BUY " + i.itemName;
                printMove(o);

                itemsOwnedMap[unitId].push_back(i.itemName);
                break;
            }
        }
    }

    int sellItems()
    {
        cerr << "sellItems: itemsOwned: gold: itemsOwned[unitId].size()" << itemsOwned << " " << gold << itemsOwnedMap[unitId].size() << endl;

        if(itemsOwnedMap[unitId].size() > 0)
        {
            string item = itemsOwnedMap[unitId][0];
            itemsOwnedMap[unitId].erase(itemsOwnedMap[unitId].begin());

            stringstream o;
            o << "SELL " << item;
            printMove(o);
        }
    }

    int findNeutralToAttack()
    {
        int leastNeutralDist = 50000;
        int id = -1;
        int neutralHealth = 5000;
        for(Entity& e: allEntities)
        {
            if(e.team == neutralTeam)
            {
                if(e.health < neutralHealth && e.health < health)
                {
                    id = e.unitId; neutralHealth = e.health;
                }
                else if(e.health == neutralHealth && e.health < health)
                {
                    int d = distanceTo(e);
                    if(d < leastNeutralDist)
                    {
                        leastNeutralDist = d; id = e.unitId;
                    }
                }
            }
        }
        return id;
    }

    void defaultMove()
    {
        stringstream o;
        Point p;
        p.x = nearestEnemyInFrontLine.x;
        p.y = nearestEnemyInFrontLine.y - 10;
        p.y += unitId%2 == 0 ? attackRange : -attackRange;
        p.y = min(p.y, 740);

        o << "MOVE_ATTACK " << p.x << " " << p.y << " " << nearestEnemyInFrontLine.unitId << "; Default";
        printMove(o);
    }

    Point findNearestPointToHide()
    {
        int ansD = 50000;
        Point ans;
        for(SpawnPoint& p:  spawnPoints)
        {
            if(p.entityType == "BUSH")
            {
                int d = distanceTo(p);
                if(d < ansD)
                {
                    ansD = d;
                    ans.x = p.x, ans.y = p.y;
                }
            }
        }

        if(ans.x == 0 && ans.y == 0)
            ans.x = homeTower.x, ans.y = homeTower.y;

        return ans;
    }

    void hide()
    {
        stringstream o;
        Point p = findNearestPointToHide();
        o << "MOVE " << p.x << " " << p.y;
    }

    virtual void playMove()
    {
        cerr << "Play move" << heroType << endl;
        stringstream o;

        if(!actioned)
        {
            switch(Strategy)
            {
            case COLLECT_GOLD:{
                int neutralId = findNeutralToAttack();
                o << "ATTACK " << neutralId;
                printMove(o);
                break;
            }
            case DEFEND_TOWER:{
                if(abs(homeTower.x - x) > 400)
                    o << "MOVE " << homeTower.x << " " << homeTower.y;
                else
                    o << "ATTACK " << nearestOpponentUnit;
                printMove(o);
                break;
            }
            case ATTACK_ENEMY_TOWER:{
                if(abs(enemyTower.x - x) > 400)
                {
                    o << "MOVE " << enemyTower.x << " " << enemyTower.y;
                }
                else
                {
                    o << "ATTACK " << enemyTower.unitId;
                }
                printMove(o);
                break;
            }
            case UPGRADE:{
                if(itemsOwned > 3)
                    sellItems();
                else if(itemsOwned == 0)
                    buyItems("damage");
                else if(itemsOwned == 1)
                    buyItems("moveSpeed");
                else
                    buyItems("mana");

                break;
            }
            case UPGRADE_HEALTH:{
                buyItems("health");
                if(!actioned)
                {
                    if(distanceFromHome < 200 && itemsOwned > 0)
                    {
                        sellItems();
                    }
                    else
                    {
                        o << "MOVE " << homeTower.x << " " << homeTower.y;
                        printMove(o);
                    }
                }
            }
                break;

            }
        }

    }
};

vector<Hero*> myHeroes;

struct Hulk : public Hero
{
    Hulk(const Entity& E)
        :Hero(E)
    {}

    virtual void  playMove()
    {
        Hero::playMove();

        // If its not a common move then
        enum skills {CHARGE = 0, EXPLOSIVESHIELD = 1, BASH = 2};
        vector<int> manaCost = {20,35,50};

        stringstream o;
        if(!actioned)
        {
            switch(Strategy){
            case ATTACK_FRONTLINE:{
                sort(allEntities.begin(), allEntities.end(), [](Entity& i1, Entity& i2)-> bool{return i1.health < i2.health;});

                stringstream o;
                for(Entity& e: allEntities)
                {
                    int d = distanceTo(e);
                    if(e.health < attackDamage)
                    {
                        if(d < attackRange)
                        {
                            o << "ATTACK " << e.unitId << "; Nearby weak1";
                            printMove(o);
                            break;
                        }
                        else if(d < 500 && mana > manaCost[CHARGE] && countDown[CHARGE] == 0)
                        {
                            o << "CHARGE " << e.unitId << "; Faraway weak1";
                            printMove(o);
                            break;
                        }
                    }
                }
                break;
            }

            case SWARM_ATTACK_ENEMY_HERO:{
                Entity enemy = enemyHeros[0].unitId == enemyIdToSwarm ? enemyHeros[0] : enemyHeros[1];
                int d = distanceTo(enemy);

                if(d < 300 && mana > manaCost[CHARGE] && countDown[CHARGE] == 0)
                    o << "CHARGE " << enemyIdToSwarm;
                else
                    o << "ATTACK " << enemyIdToSwarm;

                printMove(o);
                break;
            }

            case SELF_DEFENCE:{
                if(enemiesAround == 1 && allEntities[nearestEnemyId].health < health)
                    o << "ATTACK " << nearestEnemyId;
                else
                    o << "MOVE " << homeTower.x << " " << homeTower.y;
                printMove(o);
                break;
            }

            }
        }

        //If still no move found then default
        if(!actioned)
        {
            defaultMove();
        }
    }
};

struct Valkyrie : public Hero
{
    Valkyrie(const Entity& E)
        :Hero(E)
    {}

    virtual void  playMove()
    {
        Hero::playMove();

        enum skills {SPEARFLIP = 0, JUMP = 1, POWERUP = 2};
        vector<int> manaCost = {20,35,50};
        stringstream o;

        if(!actioned)
        {
            switch(Strategy){
            case ATTACK_FRONTLINE:{
                sort(allEntities.begin(), allEntities.end(), [](Entity& i1, Entity& i2)-> bool{return i1.health < i2.health;});

                for(Entity& e: allEntities)
                {
                    int d = distanceTo(e);
                    if(e.health < attackDamage)
                    {
                        if(d < attackRange)
                        {
                            o << "ATTACK " << e.unitId << "; Nearby weak1";
                            printMove(o);
                            break;
                        }
                        else if(d < 250 && mana > manaCost[JUMP] && countDown[JUMP] == 0)
                        {
                            o << "JUMP " << e.x << " " << e.y << "; Faraway weak1";
                            printMove(o);
                            break;
                        }
                    }
                }
                break;
            }


            case SWARM_ATTACK_ENEMY_HERO:{
                Entity enemy = enemyHeros[0].unitId == enemyIdToSwarm ? enemyHeros[0] : enemyHeros[1];
                if(distanceTo(enemy) < 250 && mana > manaCost[JUMP] && countDown[JUMP] == 0)
                    o << "JUMP " << enemy.x << " " << enemy.y;
                else
                    o << "ATTACK " << enemyIdToSwarm;

                printMove(o);
                break;
            }

            case SELF_DEFENCE:{
                if(enemiesAround == 1 && allEntities[nearestEnemyId].health < health)
                    o << "ATTACK " << nearestEnemyId;
                else
                    o << "MOVE " << homeTower.x << " " << homeTower.y;

                printMove(o);
                break;
            }

            }
        }

        //If still no move found then default
        if(!actioned)
        {
            defaultMove();
        }

    }

};

struct Ironman : public Hero
{
    Ironman(const Entity& E)
        :Hero(E)
    {}

    void playMove()
    {
        enum skills {BLINK = 0, FIREBALL = 1, BURNING = 2};
        vector<int> manaCost = {16,60,50};

        stringstream o;
        if(!actioned)
        {
            switch(Strategy){
            case ATTACK_FRONTLINE:{
                sort(allEntities.begin(), allEntities.end(), [](Entity& i1, Entity& i2)-> bool{return i1.health < i2.health;});


                for(Entity& e: allEntities)
                {
                    int d = distanceTo(e);
                    if(e.health < attackDamage)
                    {
                        if(d < attackRange)
                        {
                            o << "ATTACK " << e.unitId << "; Nearby weak1";
                            printMove(o);
                            break;
                        }
                    }
                }
                break;
            }

            case SWARM_ATTACK_ENEMY_HERO:{
                Entity enemy = enemyHeros[0].unitId == enemyIdToSwarm ? enemyHeros[0] : enemyHeros[1];
                int d = distanceTo(enemy);

                Point p; p.y = enemy.y;
                p.x = myTeam == 0 ? min(enemy.x - attackRange + 10, 0) : max(enemy.x + attackRange - 10, homeTower.x);

                if(d <= 900 && mana > manaCost[FIREBALL] && countDown[FIREBALL] == 0)
                    o << "FIREBALL " << enemy.x << " " << enemy.y;
                else
                    o << "MOVE_ATTACK " << p.x << " " << p.y << " " << enemy.unitId;


                printMove(o);
                break;
            }

            case SELF_DEFENCE:{
                if(enemiesAround == 1 && allEntities[nearestEnemyId].health < health)
                    o << "ATTACK " << nearestEnemyId;
                else
                    o << "MOVE " << homeTower.x << " " << homeTower.y;

                printMove(o);
                break;
            }

            }
        }

        //If still no move found then default
        if(!actioned)
        {
            defaultMove();
        }
    }
};



ostream& operator<<(ostream& os, const Entity& e)
{
    os << "team: " << e.team<< " unitId: "<< e.unitId << " unitType: " << e.unitType << " heroType "<< e.heroType << " health " << e.health << " mana " << e.mana <<" isVisible " <<e.isVisible<<" itemsOwned "<<e.itemsOwned  << " attackRange: " << e.attackRange << endl;
}

struct Game{

    Game()
    {neutralTeam = -1;}

    void clearLoopStates(){
        allEntities.clear(); prevHeroStates.clear(); entityIndex.clear(); enemyHeros.clear();

        for(Hero* p: myHeroes)
            delete p;

        myHeroes.clear();
    }

    void processEnemyEntities()
    {

        //Update my hero with some stats about opponent and neutrals
        for(auto H: myHeroes)
        {

            int leastNeutralDist = 5000;
            int leastOppEnemyDist = 5000;

            for(Entity& e: allEntities)
            {
                if(e.team != myTeam)
                {
                    int d = H->distanceTo(e);
                    if(d <= e.attackRange)
                        H->enemiesAround += 1;

                    //Neutral
                    if(e.team == neutralTeam && d < leastNeutralDist)
                    {
                        leastNeutralDist = d; H->nearestNeutral = e.unitId;
                    }
                    else if(e.team == oppTeam &&  d < leastOppEnemyDist)
                    {//Opponent
                        leastOppEnemyDist = d; H->nearestEnemyId = e.unitId;
                    }
                }
            }
        }

        //Find nearest enemy from home in frontline
        int dist = numeric_limits<int>::max();
        for(Entity& e: allEntities)
        {
            if(e.team == oppTeam)
            {
                e.distanceFromHome = e.distanceTo(homeTower);
                if(e.distanceFromHome < dist)
                {
                    dist = e.distanceFromHome;
                    nearestEnemyInFrontLine = e;
                }
            }
        }

        //Update Hero distance
        for(auto H: myHeroes)
        {
            H->distanceFromHome = H->distanceTo(homeTower);
            H->distanceToOpponentTower = H->distanceTo(enemyTower);

        }

        for(int i = 0; i < allEntities.size(); ++i)
            entityIndex[allEntities[i].unitId] = i;
    }

    void findStrategy()
    {
        for(auto H: myHeroes)
        {
            if(gold > 200 && !H->isUnderAttack())
            {
                H->Strategy = UPGRADE;
            }
            else if(enemyHeros.size() == 1 && enemyHeros[0].distanceTo(enemyTower) > 500)
            {
                H->Strategy = SWARM_ATTACK_ENEMY_HERO;
                enemyIdToSwarm = enemyHeros[0].unitId;
            }
            else if(enemyHeros.size() == 2 && enemyHeros[1].distanceTo(enemyTower) > 500)
            {
                H->Strategy = SWARM_ATTACK_ENEMY_HERO;
                enemyIdToSwarm = enemyHeros[1].unitId;
            }
            else if(H->isUnderAttack())
            {
                H->Strategy = SELF_DEFENCE;
            }
            else if(gold < 200 && H->findNeutralToAttack() != -1)
            {
                H->Strategy = COLLECT_GOLD;
            }
            else if(H->health < 500 && (H->itemsOwned > 0 || gold > 50))
            {
                H->Strategy = UPGRADE_HEALTH;
            }
            else if(nearestEnemyInFrontLine.distanceFromHome < 400)
            {
                H->Strategy = DEFEND_TOWER;
            }
            else if(enemyTower.health < 1000  || enemyHeros.size() == 0)
            {
                H->Strategy = ATTACK_ENEMY_TOWER;
            }
            else
            {
                H->Strategy = ATTACK_FRONTLINE;
            }
        }
    }

    void play(){
        if(roundType < 0)
        {
            if(heroCount == 0)
                cout << "HULK" << endl;
            else
                cout << "VALKYRIE" << endl;
            ++heroCount;
        }
        else
        {
            processEnemyEntities();
            findStrategy();

            for(auto H: myHeroes)
            {
                //cerr << hero.heroType << endl;
                H->playMove();

            }
        }
    }

    Point findBestPointOfAttack(Entity& H, Entity& Enemy)
    {
        Point ans(Enemy.x, Enemy.y);

        Point n,s,e,w;
        n.x = Enemy.x; n.y = Enemy.y-Enemy.attackRange;
        s.x = Enemy.x; s.y = Enemy.y+Enemy.attackRange;
        e.x = min(Enemy.x-Enemy.attackRange, 0); e.y = Enemy.y;
        w.x = max(Enemy.x + Enemy.attackRange, 1910); w.y = Enemy.y;

        int nNeighbors = 0; int sNeighbors = 0; int eNeighbors = 0; int wNeighbors = 0;
        int nDist = H.distanceTo(n); int sDist = H.distanceTo(s); int eDist = H.distanceTo(e); int wDist = H.distanceTo(w);


        for(Entity& e: allEntities)
        {
            if(e.team == oppTeam)
            {
                if(e.distanceTo(n) < e.attackRange)
                    nNeighbors++;
                if(e.distanceTo(s) < e.attackRange)
                    sNeighbors++;
                if(e.distanceTo(e) < e.attackRange)
                    eNeighbors++;
                if(e.distanceTo(w) < e.attackRange)
                    wNeighbors++;
            }
        }

        int bestNeighborCount = 5000;//random big no
        int bestDistance = 5000;

        if(nNeighbors < bestNeighborCount)
            ans = n, bestNeighborCount = nNeighbors, bestDistance = nDist;

        if(sNeighbors < bestNeighborCount || (sNeighbors == bestNeighborCount && sDist < bestDistance))
            ans = s, bestNeighborCount = sNeighbors, bestDistance = sDist;

        if(eNeighbors < bestNeighborCount || (eNeighbors == bestNeighborCount && eDist < bestDistance))
            ans = e, bestNeighborCount = eNeighbors, bestDistance = eDist;

        if(wNeighbors < bestNeighborCount || (wNeighbors == bestNeighborCount && wDist < bestDistance))
            ans = w;

        return ans;
    }

    void findNearestPointToEvade(Entity& H)
    {
        Point n,s,e,w;
        n.x = H.x; n.y = H.y-H.attackRange;
        s.x = H.x; s.y = H.y+H.attackRange;
        e.x = min(H.x-H.attackRange, 0); e.y = H.y;
        w.x = max(H.x + H.attackRange, 1910); w.y = H.y;

        //Not complete yet
    }

};


/**
 * Made with love by AntiSquid, Illedan and Wildum.
 * You can help children learn to code while you participate by donating to CoderDojo.
 **/
int main()
{
    Game game;
    cin >> myTeam; cin.ignore();
    cin >> bushAndSpawnPointCount; cin.ignore();
    heroCount = 0;
    oppTeam = myTeam == 0 ? 1 : 0;

    //cerr << "My team id: " << myTeam << endl;

    for (int i = 0; i < bushAndSpawnPointCount; i++) {
        SpawnPoint s;
        cin >> s.entityType >> s.x >> s.y >> s.radius; cin.ignore();
        spawnPoints.emplace_back(s);
    }


    cin >> itemCount; cin.ignore();
    for (int _ = 0; _ < itemCount; _++) {
        string itemName; // contains keywords such as BRONZE, SILVER and BLADE, BOOTS connected by "_" to help you sort easier
        int itemCost; // BRONZE items have lowest cost, the most expensive items are LEGENDARY
        int damage; // keyword BLADE is present if the most important item stat is damage
        int health;
        int maxHealth;
        int mana;
        int maxMana;
        int moveSpeed; // keyword BOOTS is present if the most important item stat is moveSpeed
        int manaRegeneration;
        int isPotion; // 0 if it's not instantly consumed

        cin >> itemName >> itemCost >> damage >> health >> maxHealth >> mana >> maxMana >> moveSpeed >> manaRegeneration >> isPotion; cin.ignore();

        Item i;
        i.itemName=itemName; i.values["itemCost"]=itemCost;  i.values["damage"]=damage;
        i.values["health"]=health;     i.values["maxHealth"]=maxHealth;i.values["mana"]=mana;
        i.values["maxMana"]=maxMana;   i.values["moveSpeed"]=moveSpeed;i.values["manaRegeneration"]=manaRegeneration;
        i.values["isPotion"]=isPotion;

        cerr << i;
        items.emplace_back(i);
    }

    // game loop
    while (1) {
        cin >> gold; cin.ignore();
        cin >> enemyGold; cin.ignore();
        cin >> roundType; cin.ignore();


        cin >> entityCount; cin.ignore();
        for (int i = 0; i < entityCount; i++) {
            Entity e;
            cin >> e.unitId >> e.team >> e.unitType >> e.x >> e.y >> e.attackRange >> e.health >> e.maxHealth >> e.shield >> e.attackDamage >> e.movementSpeed >> e.stunDuration >> e.goldValue >> e.countDown[0] >> e.countDown[1] >> e.countDown[2] >> e.mana >> e.maxMana >> e.manaRegeneration >> e.heroType >> e.isVisible >> e.itemsOwned; cin.ignore();

            cerr << e;

            if(myTeam == e.team && e.unitType == "HERO")
            {
                Hero* h = nullptr;

                if(e.heroType == "HULK")
                    h = new Hulk(e);
                else if(e.heroType == "VALKYRIE")
                    h = new Valkyrie(e);
                else if(e.heroType == "IRONMAN")
                    h = new Ironman(e);

                myHeroes.push_back(h);
            }
            else if(oppTeam == e.team && e.unitType == "HERO")
            {
                enemyHeros.push_back(e);
            }
            else if(e.unitType == "TOWER")
            {
                if(e.team == myTeam)
                    homeTower = e;
                else
                    enemyTower = e;
            }

            allEntities.emplace_back(e);
        }
        //        //cerr << "Home: " << game.home.x << " " << game.home.y << endl;
        //        //cerr << "enemyTower: " << game.enemyTower.x << " " << game.enemyTower.y << endl;
        game.play();
        game.clearLoopStates();
    }
}


/*
Bronze_Gadget_2     damage: 0 health: 48 isPotion: 0 itemCost: 82 mana: 98 manaRegeneration: 0 maxHealth: 48 maxMana: 98 moveSpeed: 0
Golden_Blade_15     damage: 98 health: 191 isPotion: 0 itemCost: 722 mana: 0 manaRegeneration: 0 maxHealth: 191 maxMana: 0 moveSpeed: 0
Golden_Blade_14     damage: 122 health: 0 isPotion: 0 itemCost: 750 mana: 0 manaRegeneration: 0 maxHealth: 0 maxMana: 0 moveSpeed: 0
Silver_Blade_10     damage: 12 health: 0 isPotion: 0 itemCost: 273 mana: 100 manaRegeneration: 3 maxHealth: 0 maxMana: 100 moveSpeed: 0
mana_potion         damage: 0 health: 0 isPotion: 1 itemCost: 25 mana: 50 manaRegeneration: 0 maxHealth: 0 maxMana: 0 moveSpeed: 0
Bronze_Blade_4      damage: 5 health: 0 isPotion: 0 itemCost: 82 mana: 94 manaRegeneration: 0 maxHealth: 0 maxMana: 94 moveSpeed: 0
Legendary_Gadget_19 damage: 0 health: 389 isPotion: 0 itemCost: 1112 mana: 0 manaRegeneration: 24 maxHealth: 389 maxMana: 0 moveSpeed: 0
xxl_potion          damage: 0 health: 500 isPotion: 1 itemCost: 330 mana: 0 manaRegeneration: 0 maxHealth: 0 maxMana: 0 moveSpeed: 0
Bronze_Boots_3      damage: 0 health: 0 isPotion: 0 itemCost: 96 mana: 0 manaRegeneration: 0 maxHealth: 0 maxMana: 0 moveSpeed: 27
Legendary_Blade_17  damage: 106 health: 811 isPotion: 0 itemCost: 1154 mana: 0 manaRegeneration: 0 maxHealth: 811 maxMana: 0 moveSpeed: 63
Legendary_Blade_18  damage: 296 health: 0 isPotion: 0 itemCost: 1478 mana: 100 manaRegeneration: 9 maxHealth: 0 maxMana: 100 moveSpeed: 0
Bronze_Blade_1      damage: 11 health: 0 isPotion: 0 itemCost: 79 mana: 0 manaRegeneration: 0 maxHealth: 0 maxMana: 0 moveSpeed: 0
Bronze_Boots_5      damage: 0 health: 0 isPotion: 0 itemCost: 113 mana: 0 manaRegeneration: 1 maxHealth: 0 maxMana: 0 moveSpeed: 18
Legendary_Boots_16  damage: 0 health: 0 isPotion: 0 itemCost: 1491 mana: 100 manaRegeneration: 50 maxHealth: 0 maxMana: 100 moveSpeed: 59
Silver_Gadget_6     damage: 0 health: 262 isPotion: 0 itemCost: 178 mana: 0 manaRegeneration: 0 maxHealth: 262 maxMana: 0 moveSpeed: 0
Silver_Boots_8      damage: 0 health: 125 isPotion: 0 itemCost: 340 mana: 0 manaRegeneration: 0 maxHealth: 125 maxMana: 0 moveSpeed: 76
Silver_Boots_9      damage: 0 health: 95 isPotion: 0 itemCost: 384 mana: 0 manaRegeneration: 0 maxHealth: 95 maxMana: 0 moveSpeed: 96
Golden_Boots_11     damage: 5 health: 270 isPotion: 0 itemCost: 630 mana: 100 manaRegeneration: 8 maxHealth: 270 maxMana: 100 moveSpeed: 11
Legendary_Boots_20  damage: 0 health: 1670 isPotion: 0 itemCost: 1486 mana: 100 manaRegeneration: 19 maxHealth: 1670 maxMana: 100 moveSpeed: 150
larger_potion       damage: 0 health: 100 isPotion: 1 itemCost: 70 mana: 0 manaRegeneration: 0 maxHealth: 0 maxMana: 0 moveSpeed: 0
Silver_Boots_7      damage: 0 health: 0 isPotion: 0 itemCost: 215 mana: 100 manaRegeneration: 0 maxHealth: 0 maxMana: 100 moveSpeed: 48
Golden_Blade_13     damage: 51 health: 58 isPotion: 0 itemCost: 625 mana: 100 manaRegeneration: 5 maxHealth: 58 maxMana: 100 moveSpeed: 0
Golden_Blade_12     damage: 148 health: 0 isPotion: 0 itemCost: 909 mana: 100 manaRegeneration: 0 maxHealth: 0 maxMana: 100 moveSpeed: 0


team: 0 unitId: 1 unitType: TOWER heroType - health 3000 mana 0 isVisible 1 itemsOwned 0 attackRange: 400
team: 1 unitId: 2 unitType: TOWER heroType - health 3000 mana 0 isVisible 1 itemsOwned 0 attackRange: 400
team: 0 unitId: 3 unitType: HERO heroType HULK health 1330 mana 90 isVisible 1 itemsOwned 0 attackRange: 95
team: 1 unitId: 4 unitType: HERO heroType HULK health 1415 mana 90 isVisible 1 itemsOwned 3 attackRange: 95
team: 0 unitId: 5 unitType: HERO heroType VALKYRIE health 1300 mana 155 isVisible 1 itemsOwned 0 attackRange: 130
team: 1 unitId: 6 unitType: HERO heroType IRONMAN health 820 mana 200 isVisible 1 itemsOwned 3 attackRange: 270
team: 0 unitId: 7 unitType: UNIT heroType - health 400 mana 0 isVisible 1 itemsOwned 0 attackRange: 90
team: 0 unitId: 8 unitType: UNIT heroType - health 400 mana 0 isVisible 1 itemsOwned 0 attackRange: 90
team: 0 unitId: 9 unitType: UNIT heroType - health 400 mana 0 isVisible 1 itemsOwned 0 attackRange: 90
team: 0 unitId: 10 unitType: UNIT heroType - health 250 mana 0 isVisible 1 itemsOwned 0 attackRange: 300
team: 1 unitId: 11 unitType: UNIT heroType - health 400 mana 0 isVisible 1 itemsOwned 0 attackRange: 90
team: 1 unitId: 12 unitType: UNIT heroType - health 300 mana 0 isVisible 1 itemsOwned 0 attackRange: 90
team: 1 unitId: 13 unitType: UNIT heroType - health 400 mana 0 isVisible 1 itemsOwned 0 attackRange: 90
team: 1 unitId: 14 unitType: UNIT heroType - health 250 mana 0 isVisible 1 itemsOwned 0 attackRange: 300
team: -1 unitId: 15 unitType: GROOT heroType - health 400 mana 0 isVisible 1 itemsOwned 0 attackRange: 150
team: -1 unitId: 16 unitType: GROOT heroType - health 400 mana 0 isVisible 1 itemsOwned 0 attackRange: 150
team: -1 unitId: 17 unitType: GROOT heroType - health 400 mana 0 isVisible 1 itemsOwned 0 attackRange: 150
team: -1 unitId: 18 unitType: GROOT heroType - health 120 mana 0 isVisible 1 itemsOwned 0 attackRange: 150

1920X750 field
*/

