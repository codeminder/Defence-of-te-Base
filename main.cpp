#include "raylib.h"
#include "raymath.h"
#include <vector>
#include <queue>
#include <unordered_set>
#include <unordered_map>
#include <fstream>
using namespace std;

static float gameTime = 8.0f;
static float timer = 0;
static float timeSpeed = 1.0f;
static float timeOfDay = 8.0f / 24.0f;
static float resourceTimer = 0.0f;
float scale = 1.0f;
float playerSpeed = 100.0f;

unordered_map<string, unordered_map<string, string>> translations;
std::string currentLanguage = "en";
std::string t(const std::string& key) {
    return translations[currentLanguage][key];
}

const int tileSize = 64, mapWidth = 40, mapHeight = 40;
Camera2D camera = {0};
Vector2 dragStart = {0};
bool dragging = false;

enum GameState { 
	STATE_MENU, 
	STATE_CONTINUE,
	STATE_SETTING,
	STATE_GAME 
};

enum BuildingType { 
	BUILD_NONE, 
	BUILD_BASE, 
	BUILD_GOLD_MINE, 
	BUILD_TRANSPORTER, 
	BUILD_SAWMILL, 
	BUILD_ARCHER, 
	BUILD_IRON_MINE,
	BUILD_FACTORY,
	BUILD_CANNON,
};

struct Building { BuildingType type; int x, y; };
GameState currentState = STATE_MENU;
int selectedBuild = BUILD_NONE;
std::vector<Building> buildings;
std::unordered_set<int> connected;
std::unordered_map<int, int> cannonAmmo;
int gold = 0;
int tree = 0;
int iron = 0;
int ammoCore = 0;
bool basePlaced = false;

void InitTranslations() {
	translations["en"] = {
        {"play", "New game"},
        {"continue", "Continue"},
        {"settings", "Settings"},
        {"Setting", "Setting"},
        {"exit", "Exit"},
        {"back", "Back"},
        {"music_on", "Music: On"},
        {"music_off", "Music: Off"},
        {"sfx_on", "SFX: On"},
        {"sfx_off", "SFX: Off"},
        {"language_ua", "Language: Ukrainian"},
        {"language_en", "Language: English"},
        {"title", "Defense of the Base"},
        {"base", "Base"},
        {"gold_mine", "Gold Mine"},
        {"iron_mine", "Iron Mine"},
        {"cannon", "Cannon"},
        {"transporter", "Transporter"},
        {"sawmill", "Sawmill"},
        {"factory", "Factory"},
        {"archer", "Archer"},
        {"menu", "Menu"},
        {"delete_on", "Delete: On"},
        {"delete_off", "Delete: Off"},
        {"pause", "Pause"},
        {"gold", "Gold"},
        {"tree", "Tree"},
        {"iron", "Iron"},
        {"ammoCore", "Ammocore"}
    };
    translations["ua"] = {
        {"play", "Нова гра"},
        {"continue", "Продовжити"},
        {"settings", "Налаштування"},
        {"Setting", "Налаштування"},
        {"exit", "Вихід"},
        {"back", "Назад"},
        {"music_on", "Музика: Вкл."},
        {"music_off", "Музика: Викл."},
        {"sfx_on", "Звуки: Вкл."},
        {"sfx_off", "Звуки: Викл."},
        {"language_ua", "Мова: Українська"},
        {"language_en", "Мова: Англійська"},
        {"title", "Захист Бази"},
        {"base", "База"},
        {"gold_mine", "Золота Шахта"},
        {"iron_mine", "Залізна Шахта"},
        {"cannon", "Гармата"},
        {"transporter", "Транспортер"},
        {"sawmill", "Лісопилка"},
        {"factory", "Фабрика"},
        {"archer", "Лучник"},
        {"menu", "Меню"},
        {"delete_on", "Видалити: Вкл"},
        {"delete_off", "Видалити: Викл"},
        {"pause", "Пауза"},
        {"gold", "Золото"},
        {"tree", "Дерево"},
        {"iron", "Залізо"},
        {"ammoCore", "Ядро"}
    };
}

bool IsInsideMap(int x, int y) {
    return x >= 0 && x < mapWidth && y >= 0 && y < mapHeight;
}

const char *chars =
    "АБВГҐДЕЄЖЗИІЇЙКЛМНОПРСТУФХЦЧШЩЬЮЯ"
    "абвгґдеєжзиіїйклмнопрстуфхцчшщьюя"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789 .,!?:;-_()[]{}|←✓";
    
int PosToIndex(int x, int y) { return y * mapWidth + x; }

void UpdateConnections() {
    connected.clear();
    std::queue<std::pair<Building*, int>> q;
    for (auto &b : buildings)
        if (b.type == BUILD_BASE) {
            connected.insert(PosToIndex(b.x, b.y));
            q.push({&b, 0});
        }
    while (!q.empty()) {
        auto [u, depth] = q.front(); q.pop();
        if (depth >= 5) continue;
        for (auto &t : buildings) {
            if (t.type != BUILD_TRANSPORTER) continue;
            int idx = PosToIndex(t.x, t.y);
            if (connected.count(idx)) continue;
            int d = abs(u->x - t.x) + abs(u->y - t.y);
            if (d <= 5) {connected.insert(idx);
                q.push({&t, depth + 1});
            }
        }
    }
    for (auto &b : buildings) {
        if (b.type == BUILD_GOLD_MINE || b.type == BUILD_SAWMILL || b.type == BUILD_ARCHER || b.type == BUILD_IRON_MINE || b.type == BUILD_FACTORY || b.type == BUILD_CANNON) {
            for (auto &t : buildings) {
                int d = abs(b.x - t.x) + abs(b.y - t.y);
                if (t.type == BUILD_TRANSPORTER && d <= 5 &&
                    connected.count(PosToIndex(t.x, t.y))) {
                    connected.insert(PosToIndex(b.x, b.y));
                    break;
                }
            }
        }
    }
}

bool CheckButton(Rectangle btn, const char *text, Font font, float scale) {
    bool hov = CheckCollisionPointRec(GetMousePosition(), btn);
    DrawRectangleRec(btn, hov ? DARKGREEN : GRAY);
    DrawRectangleLinesEx(btn, 2, WHITE);
    float fontSize = 20 * scale;
    Vector2 sz = MeasureTextEx(font, text, fontSize, 1);
    DrawTextEx(font, text, {btn.x + (btn.width - sz.x)/2, btn.y + (btn.height - sz.y)/2}, fontSize, 1, WHITE);
    return hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

bool IsFreeAround(const std::vector<std::vector<int>> &map, int x, int y);
void GenerateClusters(std::vector<std::vector<int>> &map, int resourceId, int clusters, int size, int radius);

bool IsFreeAround(const std::vector<std::vector<int>> &map, int x, int y) {
	for (int dy = -1; dy <= 1; dy++) {
		for (int dx = -1; dx <= 1; dx++) {
			int nx = x + dx;
			int ny = y + dy;
			if (nx >= 0 && nx < map[0].size() && ny >= 0 && ny < map.size()) {
				if (map[ny][nx] != 0) return false;
			}
		}
	}
	return true;
}

void GenerateIsolatedResources(std::vector<std::vector<int>> &map, int resourceId, int count) {
	int width = map[0].size();
	int height = map.size();
	int placed = 0;
	int attempts = 0;
	while (placed < count && attempts < count * 50) {
		int x = GetRandomValue(0, width - 1);
		int y = GetRandomValue(0, height - 1);
		if (map[y][x] == 0 && IsFreeAround(map, x, y)) {
			map[y][x] = resourceId;
			placed++;
		}
		attempts++;
	}
}

void SaveGame(const std::vector<std::vector<int>>& map) {
	std::ofstream save("save.dat");
	for (const auto& row : map) {
		for (int tile : row) {
			save << tile << " ";
		}
		save << "\n";
	}
	save.close();
}

void LoadGame(std::vector<std::vector<int>>& map) {
	std::ifstream save("save.dat");
	if (!save.is_open()) return;
	for (int y = 0; y < map.size(); y++) {
		for (int x = 0; x < map[y].size(); x++) {
			save >> map[y][x];
		}
	}
	save.close();
}

void NewGame(std::vector<std::vector<int>> &map) {
	for (auto &row : map)
		std::fill(row.begin(), row.end(), 0);
	gameTime = 8.0f;
	timeOfDay = 8.0f / 24.0f;
	timer = 0.0f;
	resourceTimer = 0.0f;
	timeSpeed = 1.0f;
	timer += GetFrameTime() * timeSpeed;
	float adjustedDelta = GetFrameTime() * timeSpeed;
	gameTime += adjustedDelta;
	int hour = (int)(gameTime);
	int minutes = (int)((gameTime - hour) * 60.0f);
	Vector2 playerPos = {100.0f, 100.0f};
	camera.target = playerPos;
	GenerateIsolatedResources(map, 1, 50);
	GenerateIsolatedResources(map, 2, 20);
	GenerateIsolatedResources(map, 3, 20);
	buildings.clear();
	connected.clear();
	cannonAmmo.clear();
	gold = 0;
	iron = 0;
	tree = 0;
	ammoCore = 0;
	basePlaced = false;
}

int main() {
	InitTranslations();
	InitWindow(GetMonitorWidth(0), GetMonitorHeight(0), "Defence Of The Base");
	ToggleFullscreen();
	int screenWidth = GetScreenWidth();
	int screenHeight = GetScreenHeight();
	Vector2 playerPos = {100.0f, 100.0f};
	scale = screenHeight / 720.0f;
	SetTargetFPS(60);
	std::vector<std::vector<int>> map(mapHeight, std::vector<int>(mapWidth, 0));
	GenerateIsolatedResources(map, 1, 50);
	GenerateIsolatedResources(map, 2, 40);
	GenerateIsolatedResources(map, 3, 30);
	int cpCount = 0;
	int *cp = LoadCodepoints(chars, &cpCount);
	Font font = LoadFontEx("fonts/Ubuntu-R.ttf", 128, cp, cpCount);
	camera.target = Vector2{(float)screenWidth / 2.0f, (float)screenHeight / 2.0f};
	camera.offset = camera.target;
	camera.zoom = 1.0f;
	const int dxs[4] = {-1, 1, 0, 0};
	const int dys[4] = {0, 0, -1, 1};
	while (!WindowShouldClose()) {
		BeginDrawing();
		ClearBackground(DARKGRAY);
		float titleSize = 100 * scale;
		float btnWidth = 360 * scale;
		float btnHeight = 80 * scale;
		float spacing = 20 * scale;
		float totalHeight = titleSize + spacing + 3 * (btnHeight + spacing);
		float startY = (screenHeight - totalHeight) / 2.0f;
		float btnYStart = startY + titleSize + spacing;
		float btnYOffset = btnHeight + spacing;
		if (currentState == STATE_MENU) {
			float btnX = (screenWidth - btnWidth) / 18.0f;
			Vector2 titlePos = {(screenWidth - MeasureTextEx(font, t("title").c_str(), titleSize, 1).x) / 2.0f, startY - 100};
			Vector2 titlePos1 = {(screenWidth - MeasureTextEx(font, "RGR1909", titleSize, 1).x) + 225.0f, startY + 450.0f};
			DrawTextEx(font, t("title").c_str(), titlePos, titleSize, 1, WHITE);
			DrawTextEx(font, "RGR1909", titlePos1, titleSize / 4.0f, 1, WHITE);
			Rectangle play = {20, 240, 300, 70};
			Rectangle continue1 = {20, 330, 300, 70};
			Rectangle setting = {20, 420, 300, 70};
			Rectangle quit = {20, 510, 300, 70};
			if (CheckButton(play, t("play").c_str(), font, 2)) {
				NewGame(map);
				currentState = STATE_GAME;
			}
			if (CheckButton(continue1, t("continue").c_str(), font, 2)) currentState = STATE_CONTINUE;
			if (CheckButton(setting, t("settings").c_str(), font, 2)) currentState = STATE_SETTING;
			if (CheckButton(quit, t("exit").c_str(), font, 2)) break;
		}
		else if (currentState == STATE_CONTINUE) {
			LoadGame(map);
			currentState = STATE_GAME;
		}
		else if (currentState == STATE_SETTING) {
			float btnX = (screenWidth - btnWidth) / 18.0f;
			static bool musicOn = true;
			static bool sfxOn = true;
			static int langIndex = 0;
			std::vector<std::string> langList = {"ua", "en"};
			Vector2 titlePos = {(screenWidth - MeasureTextEx(font, t("Setting").c_str(), titleSize, 1).x) / 2.0f, startY - 100};
			DrawTextEx(font, t("Setting").c_str(), titlePos, titleSize, 1, WHITE);
			Rectangle musicBtn = {20, 135, 200, 50};
			if (CheckButton(musicBtn, musicOn ? t("music_on").c_str() : t("music_off").c_str(), font, 1)) 
				musicOn = !musicOn;
			Rectangle sfxBtn = {240, 135, 200, 50};
			if (CheckButton(sfxBtn, sfxOn ? t("sfx_on").c_str() : t("sfx_off").c_str(), font, 1)) 
				sfxOn = !sfxOn;
			Rectangle langBtn = {460, 135, 200, 50};
			if (CheckButton(langBtn, currentLanguage == "en" ? "Language: English" : "Мова: Українська", font, 1)) {
				langIndex = (langIndex + 1) % langList.size();
				currentLanguage = langList[langIndex];
			}
			Rectangle backBtn = {410, 525, 200, 50};
			if (CheckButton(backBtn, t("back").c_str(), font, 1)) currentState = STATE_MENU;
		}
		else if (currentState == STATE_GAME) {
			timer += GetFrameTime() * timeSpeed;
			float adjustedDelta = GetFrameTime() * timeSpeed;
			gameTime += adjustedDelta;
			if (gameTime >= 24.0f) gameTime -= 24.0f;
			resourceTimer += adjustedDelta;
			playerPos.x += playerSpeed * adjustedDelta;
			timeOfDay += adjustedDelta * 0.05f; 
			if (timeOfDay > 1.0f) timeOfDay -= 1.0f;
			float brightness = 1.0f;
			float nightFade = 0.0f;
			if (gameTime >= 23.0f || gameTime < 6.0f) {
				if (gameTime >= 23.0f) {
					float t = gameTime - 23.0f;
					brightness = 1.0f - t;
					nightFade = t;
				} else {
					brightness = 0.0f;
					nightFade = 1.0f;
				}
			}
			else if (gameTime >= 6.0f && gameTime < 7.0f){
				float t = (gameTime - 6.0f) / 1.0f;
				brightness = t;
				nightFade = 1.0f - t; 
			}
			else {
				brightness = 1.0f;
				nightFade = 0.0f;
			}
			if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { dragStart = GetMousePosition(); dragging = true; }
			if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && dragging) {
				Vector2 cur = GetMousePosition();
				camera.target = Vector2Add(camera.target, Vector2Subtract(dragStart, cur));
				dragStart = cur;
			}
			if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) dragging = false;
			camera.zoom = Clamp(camera.zoom + GetMouseWheelMove() * 0.1f, 0.5f, 3.0f);
			UpdateConnections();
			BeginMode2D(camera);
			for (int y = 0; y < mapHeight; y++)
				for (int x = 0; x < mapWidth; x++) {
					Vector2 pos = {(float)x * tileSize, (float)y * tileSize};
					DrawRectangleV(pos, {tileSize - 2, tileSize - 2}, LIGHTGRAY);
					if (map[y][x] == 1) DrawCircle(pos.x + tileSize / 2, pos.y + tileSize / 2, 15, GREEN);
					if (map[y][x] == 2) DrawCircle(pos.x + tileSize / 2, pos.y + tileSize / 2, 15, GOLD); 
					if (map[y][x] == 3) DrawCircle(pos.x + tileSize / 2, pos.y + tileSize / 2, 15, DARKGRAY);
				}
			Color dayColor = {
				(unsigned char)(brightness * 80), 
				(unsigned char)(brightness * 80), 
				(unsigned char)(brightness * 80), 
				255
			};
			ClearBackground(dayColor);
			Color tint = {0, 0, 0, (unsigned char)(nightFade * 150)};
			DrawRectangleV(
				{camera.target.x - screenWidth / (2 * camera.zoom), camera.target.y - screenHeight / (2 * camera.zoom)},
				{(float)screenWidth / camera.zoom, (float)screenHeight / camera.zoom},
				tint
			);
			for (auto &a : buildings)
				for (auto &b : buildings) {
					if (&a == &b) continue;
					int idxA = PosToIndex(a.x, a.y), idxB = PosToIndex(b.x, b.y);
					if (!connected.count(idxA) || !connected.count(idxB)) continue;
					int dist = abs(a.x - b.x) + abs(a.y - b.y);
					bool valid = false;
					if ((a.type == BUILD_ARCHER && b.type == BUILD_TRANSPORTER) || (b.type == BUILD_ARCHER && a.type == BUILD_TRANSPORTER)) {
						valid = dist <= 5;
					}
					else if ((a.type == BUILD_BASE && b.type == BUILD_TRANSPORTER) || (b.type == BUILD_BASE && a.type == BUILD_TRANSPORTER)) {
						valid = dist <= 5;
					}
					else if ((a.type == BUILD_CANNON && b.type == BUILD_TRANSPORTER) || (b.type == BUILD_CANNON && a.type == BUILD_TRANSPORTER)) {
						valid = dist <= 5;
					}
					else if ((a.type == BUILD_FACTORY && b.type == BUILD_TRANSPORTER) || (b.type == BUILD_FACTORY && a.type == BUILD_TRANSPORTER)) {
						valid = dist <= 5;
					}
					else if ((a.type == BUILD_GOLD_MINE && b.type == BUILD_TRANSPORTER) || (b.type == BUILD_GOLD_MINE && a.type == BUILD_TRANSPORTER)) {
						valid = dist <= 5;
					}
					else if ((a.type == BUILD_IRON_MINE && b.type == BUILD_TRANSPORTER) || (b.type == BUILD_IRON_MINE && a.type == BUILD_TRANSPORTER)) {
						valid = dist <= 5;
					}
					else if ((a.type == BUILD_SAWMILL && b.type == BUILD_TRANSPORTER) || (b.type == BUILD_SAWMILL && a.type == BUILD_TRANSPORTER)) {
						valid = dist <= 5;
					}
					else if (a.type == BUILD_TRANSPORTER && b.type == BUILD_TRANSPORTER) {
						valid = dist <= 5;
					}
					if (valid) {
						Vector2 p1 = {(float)a.x * tileSize + tileSize / 2, (float)a.y * tileSize + tileSize / 2};
						Vector2 p2 = {(float)b.x * tileSize + tileSize / 2, (float)b.y * tileSize + tileSize / 2};
						DrawLineV(p1, p2, Fade(LIME, 0.4f));
					}
				}
			for (auto &b : buildings) {
				float size = tileSize - 16;
				Color col = WHITE;
				switch (b.type) {
					case BUILD_BASE: col = YELLOW; break;
					case BUILD_GOLD_MINE: col = GOLD; break;
					case BUILD_TRANSPORTER: col = SKYBLUE; size = (tileSize - 16) / 3.0f; break;
					case BUILD_SAWMILL: col = BROWN; break;
					case BUILD_ARCHER: col = PURPLE; break;
					case BUILD_CANNON: col = RED; break;
					case BUILD_IRON_MINE: col = GRAY; break;
					case BUILD_FACTORY: col = GREEN; break;
				}
				Vector2 pos = {
					(float)b.x * tileSize + (tileSize - size) / 2, 
					(float)b.y * tileSize + (tileSize - size) / 2 
				};
				DrawRectangle(pos.x, pos.y, size, size, col);
				DrawRectangleLinesEx({pos.x, pos.y, size, size}, 4, BLACK);
				if (b.type == BUILD_ARCHER && connected.count(PosToIndex(b.x, b.y))) {
					Vector2 txt = {(float)b.x * tileSize + 10, (float)b.y * tileSize + 10};
					DrawTextEx(font, "0", txt, 20, 1, WHITE);
				}
				if (b.type == BUILD_CANNON && connected.count(PosToIndex(b.x, b.y))) {
					int ammo = cannonAmmo[PosToIndex(b.x, b.y)];
					Vector2 txt = {(float)b.x * tileSize + 10, (float)b.y * tileSize + 10};
						DrawTextEx(font, TextFormat("%d", ammo), txt, 20, 1, WHITE);
				}
			}
			EndMode2D();

			Rectangle buildBar = {0, 475, 1100, 150};
			DrawRectangleRec(buildBar, Fade(BLACK, 0.8f));
			std::vector<std::string> labels = {
				t("base"), t("gold_mine"), t("transporter"), t("sawmill"),
				t("iron_mine"), t("cannon"), t("factory"), t("archer")
			};
			const char* lbls[8];
			for (int i = 0; i < 8; ++i) {
				lbls[i] = labels[i].c_str();
			}
			Rectangle customButtons[8] = {
				{15, 500, 140, 40},  	// base
				{170, 500, 140, 40}, 	// gold_mine
				{325, 500, 140, 40}, 	// transporter
				{480, 500, 140, 40}, 	// sawmill
				{15, 550, 140, 40}, 	// iron_mine
				{170, 550, 140, 40},	// cannon
				{325, 550, 140, 40}, 	// factory
				{480, 550, 140, 40} 	// archer
			};
			for (int i = 0; i < 8; i++) {
				Rectangle btn = customButtons[i];
				bool disabled = (!basePlaced && i != 0) || (i == 0 && basePlaced);
				Color frame = disabled ? DARKGRAY : (selectedBuild == i + 1 ? YELLOW : WHITE);
				if (!disabled && CheckButton(btn, lbls[i], font, 1)) {
					selectedBuild = i + 1;
				}
				DrawRectangleLinesEx(btn, 3.0f * scale, frame);
				if (disabled) DrawRectangleRec(btn, Fade(BLACK, 0.6f));
			}
			Rectangle menuBtn = {945, 5, 70, 30};
			if (CheckButton(menuBtn, t("menu").c_str(), font, 1)) currentState = STATE_MENU;
			static bool deleteMode = false;
			Rectangle delBtn = {785, 5, 150, 30};
			if (CheckButton(delBtn, deleteMode ? t("delete_on").c_str() : t("delete_off").c_str(), font, 1))
				deleteMode = !deleteMode;
			if (deleteMode && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
				Vector2 w = GetScreenToWorld2D(GetMousePosition(), camera);
				int tx = w.x / tileSize, ty = w.y / tileSize;
				for (int i = 0; i < buildings.size(); i++) {
					if (buildings[i].x == tx && buildings[i].y == ty && buildings[i].type != BUILD_BASE) {
						buildings.erase(buildings.begin() + i);
						break;
					}
				}
			}
			int hour = (int)(gameTime);
			int minutes = (int)((gameTime - hour) * 60.0f);
			DrawText(TextFormat("%02d:%02d", hour, minutes), 455, 30, 40, WHITE);
			Rectangle pauseBtn = {785, 40, 70, 30};
			Rectangle speed1xBtn = {865, 40, 70, 30};
			Rectangle speed3xBtn = {945, 40, 70, 30};
			if (CheckButton(pauseBtn, t("pause").c_str(), font, 1)) timeSpeed = 0.0f;
			if (CheckButton(speed1xBtn, "1x", font, 1)) timeSpeed = 1.0f;
			if (CheckButton(speed3xBtn, "3x", font, 1)) timeSpeed = 3.0f;
			if (!deleteMode && selectedBuild != BUILD_NONE && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
				Vector2 w = GetScreenToWorld2D(GetMousePosition(), camera);
				int tx = w.x / tileSize, ty = w.y / tileSize;
				if (IsInsideMap(tx, ty)) {
					bool free = true;
					for (auto &bb : buildings) 
						if (bb.x == tx && bb.y == ty) 
							free = false;
					bool ok = free && map[ty][tx] == 0;
					if (selectedBuild == BUILD_BASE && basePlaced)
						ok = false;
					if (ok) {
						if (selectedBuild == BUILD_GOLD_MINE) {
							bool nearGold = false;
							for (int d = 0; d < 4; d++) {
								int nx = tx + dxs[d];
								int ny = ty + dys[d];
									if (nx >= 0 && nx < mapWidth && ny >= 0 && ny < mapHeight && map[ny][nx] == 2)
										nearGold = true;
							}
							ok = nearGold;
						}
						if (selectedBuild == BUILD_SAWMILL) {
							bool nearTree = false;
							for (int d = 0; d < 4; d++) {
								int nx = tx + dxs[d];
								int ny = ty + dys[d];
									if (nx >= 0 && nx < mapWidth && ny >= 0 && ny < mapHeight && map[ny][nx] == 1)
										nearTree = true;
							}
							ok = nearTree;
						}
						if (selectedBuild == BUILD_IRON_MINE) {
							bool nearIron = false;
							for (int d = 0; d < 4; d++) {
								int nx = tx + dxs[d];
								int ny = ty + dys[d];
									if (nx >= 0 && nx < mapWidth && ny >= 0 && ny < mapHeight && map[ny][nx] == 3)
										nearIron = true;
							}
							ok = nearIron;
						}
						if (ok) {
							buildings.push_back({(BuildingType)selectedBuild, tx, ty});
							if (selectedBuild == BUILD_BASE) 
								basePlaced = true;
						}
					}
				}
			}
            if (timer >= 1.0f) {
                for (auto &b : buildings) {
                    int idx = PosToIndex(b.x, b.y);
                    if (!connected.count(idx)) continue;
                    if (b.type == BUILD_GOLD_MINE) {
                        bool nearGold = false;
                        for (int d = 0; d < 4; d++) {
							int nx = b.x + dxs[d];
							int ny = b.y + dys[d];
                                if (nx >= 0 && nx < mapWidth && ny >= 0 && ny < mapHeight && map[ny][nx] == 2)
                                    nearGold = true;
                        }
                        if (nearGold) gold += 5;
                    }
                    if (b.type == BUILD_SAWMILL) {
                        bool nearTree = false;
                        for (int d = 0; d < 4; d++) {
							int nx = b.x + dxs[d];
							int ny = b.y + dys[d];
                                if (nx >= 0 && nx < mapWidth && ny >= 0 && ny < mapHeight && map[ny][nx] == 1)
                                    nearTree = true;
                        }
						if (nearTree) tree += 2;
                    }
                    if (b.type == BUILD_IRON_MINE) {
                        bool nearIron = false;
                        for (int d = 0; d < 4; d++) {
							int nx = b.x + dxs[d];
							int ny = b.y + dys[d];
								if (nx >= 0 && nx < mapWidth && ny >= 0 && ny < mapHeight && map[ny][nx] == 3)
									nearIron = true;
						}
						if (nearIron) iron += 1;
					}
				}
				for (auto &b : buildings) {
					int idx = PosToIndex(b.x, b.y);
					if (!connected.count(idx)) continue;
					if (b.type == BUILD_FACTORY) {
						if (iron > 0) {
							iron--;
							ammoCore++;
						}
					}
					if (b.type == BUILD_CANNON) {
						if (!connected.count(idx)) cannonAmmo[idx] = 0;
						while (cannonAmmo[idx] < 20 && ammoCore > 0) {
							cannonAmmo[idx]++;
							ammoCore--;
						}
					}
				}
				timer -= 1.0f;
			}
			float resFontSize = 30 * scale;
			float resSpacing = resFontSize + 10 * scale;
			float x =10, y = 10;
			DrawTextEx(font, TextFormat("%s: %d", t("gold").c_str(), gold), {x, y}, resFontSize + 15, 2, RED);
			DrawTextEx(font, TextFormat("%s: %d", t("tree").c_str(), tree), {x, y + 40}, resFontSize, 1, RED);
			DrawTextEx(font, TextFormat("%s: %d", t("iron").c_str(), iron), {x, y + 70}, resFontSize, 3, RED);
			DrawTextEx(font, TextFormat("%s: %d", t("ammoCore").c_str(), ammoCore), {x, y + 100}, resFontSize, 4, RED);
		}
		SaveGame(map);
		EndDrawing();
	}
	UnloadCodepoints(cp);
	UnloadFont(font);
	CloseWindow();
	return 0;
}
