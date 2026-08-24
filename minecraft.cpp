#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <chrono>
#include <thread>
#include <algorithm>
#include <map>
#include <random>

// --- INCLUDES NCURSES ---
#include <ncurses.h>

using namespace std;

// --- CONSTANTES ---
const int WORLD_WIDTH = 300;
const int WORLD_HEIGHT = 50;
const int VIEW_WIDTH = 100; 
const int VIEW_HEIGHT = 30;

enum BlockType { AIR = 0, DIRT, GRASS, STONE, WATER, WOOD, LEAVES, GOLD, DIAMOND };
enum MobType { NONE, ZOMBIE, CREEPER };

struct Block {
    BlockType type;
    char symbol;
};

struct Vec2 { 
    float x, y; 
    float velY = 0; 
};

struct Mob {
    Vec2 pos;
    MobType type;
    int health;
    bool alive;
};

class TerrainGenerator {
public:
    static float getNoise(float x) {
        return (sinf(x * 0.1f) * 5.0f) + (sinf(x * 0.3f) * 2.0f);
    }
};

class MinecraftMaster {
private:
    Block world[WORLD_WIDTH][WORLD_HEIGHT];
    Vec2 player; 
    int playerHealth;
    bool isJumping;
    bool gameOver = false;

    int mineDir = 0; // 0: Bas, 1: Haut, 2: Gauche, 3: Droite
    map<BlockType, int> inventory;
    vector<Mob> mobs;

    int camX = 0, camY = 0;
    mt19937 rng;

public:
    MinecraftMaster() : player{40.0f, 15.0f, 0}, playerHealth(100), isJumping(false), rng(12345) {
        generateWorld();
        spawnMobs(8);
    }

    void placeTree(int x, int y) {
        for (int h = 1; h <= 4; h++) {
            if (y - h >= 0) world[x][y-h] = {WOOD, 'T'};
        }
        for (int ox = -1; ox <= 1; ox++) {
            for (int oy = -2; oy <= 0; oy++) {
                int tx = x + ox, ty = (y - 4) + oy;
                if (tx >= 0 && tx < WORLD_WIDTH && ty >= 0 && ty < WORLD_HEIGHT)
                    if (world[tx][ty].type == AIR) world[tx][ty] = {LEAVES, 'L'};
            }
        }
    }

    void generateWorld() {
        for (int x = 0; x < WORLD_WIDTH; x++) {
            float noise1 = (sinf(x * 0.1f) * 5.0f);
            float biomeShift = (sinf(x * 0.01f) * 15.0f);
            float elevation = noise1 + biomeShift;
            int groundLevel = 25 + (int)elevation;

            for (int y = 0; y < WORLD_HEIGHT; y++) {
                if (y > groundLevel + 15) {
                    int chance = rand() % 100;
                    if (chance < 2) world[x][y] = {DIAMOND, 'D'};
                    else if (chance < 7) world[x][y] = {GOLD, 'O'};
                    else world[x][y] = {STONE, '.'};
                }
                else if (y > groundLevel) world[x][y] = {DIRT, '#'};
                else if (y == groundLevel) world[x][y] = {GRASS, 'G'};
                else if (y > 45) world[x][y] = {WATER, 'w'};
                else world[x][y] = {AIR, ' '};
            }
        }

        for (int x = 5; x < WORLD_WIDTH - 5; x++) {
            if (rand() % 100 < 4) { 
                int groundY = 0;
                for(int y=0; y<WORLD_HEIGHT; y++) { 
                    if (world[x][y].type == GRASS) { groundY = y; break; } 
                }
                if (groundY > 0) placeTree(x, groundY);
            }
        }
    }

    void spawnMobs(int count) {
        for (int i = 0; i < count; i++) {
            Mob m;
            m.pos = {(float)(rand() % (WORLD_WIDTH-2) + 1), 5.0f, 0};
            m.type = (rand() % 2 == 0) ? ZOMBIE : CREEPER;
            m.health = (m.type == ZOMBIE) ? 25 : 10;
            m.alive = true;
            mobs.push_back(m);
        }
    }

    bool isSolid(float x, float y) {
        int ix = (int)round(x);
        int iy = [this](float val){ return (int)round(val); }(y); // Inline lambda to avoid scope issues
        if (ix < 0 || ix >= WORLD_WIDTH || iy < 0 || iy >= WORLD_HEIGHT) return true;
        BlockType type = world[ix][iy].type;
        return (type == STONE || type == DIRT || type == GRASS);
    }

    // Corrected isSolid to use simple integer casting for safety
    bool isSolidSafe(float x, float y) {
        int ix = (int)floor(x + 0.5f);
        int iy = (int)floor(y + 0.5f);
        if (ix < 0 || ix >= WORLD_WIDTH || iy < 0 || iy >= WORLD_HEIGHT) return true;
        BlockType type = world[ix][iy].type;
        return (type == STONE || type == DIRT || type == GRASS);
    }

    void update() {
        if (gameOver) return;
        player.velY += 0.25f;
        player.y += player.velY;

        if (isSolidSafe(player.x, player.y)) {
            if (player.velY > 0) {
                int iy = (int)round(player.y);
                if (iy >= 0 && iy < WORLD_HEIGHT) player.y = (float)iy - 1.0f;
            }
            player.velY = 0; isJumping = false;
        }

        for (auto& m : mobs) {
            if (!m.alive) continue;
            m.pos.velY += 0.2f; m.pos.y += m.pos.velY;
            int mix = (int)round(m.pos.x), miy = (int)round(m.pos.y);
            if (mix >= 0 && mix < WORLD_WIDTH && miy >= 0 && miy < WORLD_HEIGHT) {
                if (world[mix][miy].type != AIR && world[mix][miy].type != WATER) {
                    m.pos.velY = 0; m.pos.y = (float)miy - 1.0f;
                }
            }
            float dx = player.x - m.pos.x, dy = player.y - m.pos.y;
            if (sqrt(dx*dx + dy*dy) < 1.2f) {
                if (m.type == ZOMBIE) playerHealth -= 0.5f;
                else if (m.type == CREEPER) { m.alive = false; playerHealth -= 20; }
            }
            if (sqrt(dx*dx + dy*dy) < 15.0f) {
                float speed = (m.type == ZOMBIE) ? 0.1f : 0.07f;
                if (dx > 0.2f) m.pos.x += speed; else if (dx < -0.2f) m.pos.x -= speed;
                if (dy > 0.2f) m.pos.y += speed; else if (dy < -0.2f) m.pos.y -= speed;
            }
        }

        if (playerHealth <= 0) { playerHealth = 0; gameOver = true; }
        camX = (int)player.x - VIEW_WIDTH / 2;
        if (camX < 0) camX = 0; if (camX > WORLD_WIDTH - VIEW_WIDTH) camX = WORLD_WIDTH - VIEW_WIDTH;
        camY = (int)player.y - VIEW_HEIGHT / 2;
        if (camY < 0) camY = 0; if (camY > WORLD_HEIGHT - VIEW_HEIGHT) camY = WORLD_HEIGHT - VIEW_HEIGHT;
    }

    void input(int ch) {
        if (gameOver) return;
        // Movement - uses standard keys
        if (ch == 'a' || ch == 'A' || ch == KEY_LEFT) { if (!isSolidSafe(player.x - 1, player.y)) player.x -= 1; }
        else if (ch == 'd' || ch == 'D' || ch == KEY_RIGHT) { if (!isSolidSafe(player.x + 1, player.y)) player.x += 1; }
        else if (mobs.empty()) {} // dummy
        
        // Directional Control (Only changes cursor, not movement)
        if (ch == KEY_DOWN || ch == 's' || ch == 'S') mineDir = 0;
        else if (ch == KEY_UP || ch == 'w' || ch == 'W') mineDir = 1;
        else if (ch == KEY_LEFT || ch == 'a' || ch == 'A') mineDir = 2;
        else if (ch == KEY_RIGHT || ch == 'd' || ch == 'D') mineDir = 3;
        // Special jump handling to prevent conflict with direction switching
        else if (ch == 'w' || ch == 'W') { 
            if (!isJumping) { player.velY = -1.3f; isJumping = true; }
            mineDir = 1; 
        }

        // Jump (if not already jumping)
        if ((ch == 'w' || ch == 'W') && !isJumping) { player.velY = -1.3f; isJumping = true; }

        // Mining
        else if (ch == 'x' || ch == 'X') {
            int tx = (int)round(player.x), ty = (int)round(player.y);
            if (mineDir == 0) ty += 1; else if (mineDir == 1) ty -= 1;
            else if (mineDir == 2) tx -= 1; else if (mineDir == 3) tx += 1;

            if (tx >= 0 && tx < WORLD_WIDTH && ty >= 0 && ty < WORLD_HEIGHT) {
                if (world[tx][ty].type != AIR && world[tx][ty].type != WATER) {
                    inventory[world[tx][ty].type]++; 
                    world[tx][ty] = {AIR, ' '};
                }
            }
        }
        // Placement
        else if (ch == 'p' || ch == 'P') {
            int tx = (int)round(player.x), ty = (int)round(player.y - 1);
            if (tx >= 0 && tx < WORLD_WIDTH && ty >= 0 && ty < WORLD_HEIGHT)
                if (world[tx][ty].type == AIR) world[tx][ty] = {DIRT, '#'};
        }
        // Combat
        else if (ch == 'f' || ch == 'F') {
            for (auto& m : mobs) {
                if (m.alive) {
                    float dx = player.x - m.pos.x, dy = player.y - m.pos.y;
                    if (sqrt(dx*dx + dy*dy) <= 3.5f) {
                        m.health -= 12; if (m.health <= 0) m.alive = false;
                    }
                }
            }
        }
    }

    void render() {
        erase(); 
        if (gameOver) {
            attron(COLOR_PAIR(1) | A_BOLD);
            mvprintw(LINES/2, COLS/2 - 10, "GAME OVER");
            attroff(COLOR_PAIR(1) | A_BOLD);
            refresh(); return;
        }

        for (int y = 0; y < VIEW_HEIGHT; y++) {
            for (int x = 0; x < VIEW_WIDTH; x++) {
                int wx = x + camX, wy = y + camY;
                if (wx < 0 || wx >= WORLD_WIDTH || wy < 0 || wy >= WORLD_HEIGHT) continue;

                bool drawn = false;
                if ((int)round(player.x) == wx && (int)round(player.y) == wy) {
                    attron(COLOR_PAIR(2)); mvaddch(y, x, '@'); attroff(COLOR_PAIR(2));
                    drawn = true;
                } else {
                    for(auto& m : mobs) {
                        if(m.alive && (int)round(m.pos.x) == wx && (int)round(m.pos.y) == wy) {
                            attron(m.type == ZOMBIE ? COLOR_PAIR(1) : COLOR_PAIR(3));
                            mvaddch(y, x, (m.type == ZOMBIE ? 'Z' : 'C'));
                            attroff(m.type == ZOMBIE ? COLOR_PAIR(1) : COLOR_PAIR(3));
                            drawn = true; break;
                        }
                    }
                }

                if (!drawn) {
                    BlockType bt = world[wx][wy].type;
                    int cp = 0; char sym = ' ';
                    if (bt == GRASS) { cp = 4; sym = 'G'; }
                    else if (bt == DIRT) { cp = 5; sym = '#'; }
                    else if (bt == STONE) { cp = 7; sym = '.'; }
                    else if (bt == WATER) { cp = 4; sym = 'w'; }
                    else if (bt == WOOD) { cp = 5; sym = 'T'; }
                    else if (bt == LEAVES) { cp = 4; sym = 'L'; }
                    else if (bt == GOLD) { cp = 6; sym = 'O'; }
                    else if (bt == DIAMOND) { cp = 2; sym = 'D'; }
                    if (cp != 0) { attron(COLOR_PAIR(cp)); mvaddch(y, x, sym); attroff(COLOR_PAIR(cp)); }
                    else { mvaddch(y, x, ' '); }
                }
            }
        }

        string dirStr = (mineDir == 0) ? "DOWN" : (mineDir == 1) ? "UP" : (mineDir == 2) ? "LEFT" : "RIGHT";
        mvprintw(VIEW_HEIGHT + 1, 0, "HP: %d | POS: %d,%d | CURSOR: %s", playerHealth, (int)player.x, (int)player.y, dirStr.c_str());
        mvprintw(VIEW_HEIGHT + 2, 0, "X: Mine | F: Attack (AoE) | P: Place Dirt | Q: Quit");
        refresh();
    }
};

int main() {
    initscr(); noecho(); curs_set(0); keypad(stdscr, TRUE); nodelay(stdscr, TRUE);
    start_color();
    init_pair(1, COLOR_RED, COLOR_BLACK);      
    init_pair(2, COLOR_CYAN, COLOR_BLACK);     
    init_pair(3, COLOR_MAGENTA, COLOR_BLACK);  
    init_pair(4, COLOR_GREEN, COLOR_BLACK);    
    init_pair(5, COLOR_YELLOW, COLOR_BLACK);   
    init_pair(6, COLOR_YELLOW, COLOR_BLACK);   
    init_pair(7, COLOR_WHITE, COLOR_BLACK);    

    MinecraftMaster game;
    while (true) {
        int ch = getch();
        if (ch != ERR) {
            if (ch == 'q' || ch == 'Q') break;
            game.input(ch);
        }
        game.update();
        game.render();
        this_thread::sleep_for(chrono::milliseconds(30)); 
    }

    endwin();
    return 0;
}
