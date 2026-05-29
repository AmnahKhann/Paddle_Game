#include "raylib.h"
#include <vector>
#include <cmath>
#include <cstdlib>
#include <ctime>


//  SCREEN CONSTANTS

static const int   SCR_W          = 1100;
static const int   SCR_H          = 800;
static const float LEVEL_DURATION = 20.0f;
static const float MAX_SPEED      = 860.0f;


//  COLOR PALETTE

static const Color PAL_BG    = {  6,   9,  24, 255 };  // obsidian navy
static const Color PAL_DOT   = { 40,  60, 120,  90 };  // dot-grid
static const Color PAL_GHOST = { 70,  95, 170, 255 };  // dim bg text
static const Color PAL_DIM   = {110, 140, 220, 255 };  // brighter bg text
static const Color PAL_GOLD  = {245, 197,  24, 255 };  // amber gold
static const Color PAL_ICE   = { 99, 170, 252, 255 };  // ice blue
static const Color PAL_WHITE = {232, 240, 255, 255 };  // cold white
static const Color PAL_RED   = {239,  68,  68, 255 };  // danger / lives


//  BALL TYPES

struct BallType { const char *l1, *l2; Color tint; };
static const BallType BT[] = {
	{ "ENA",  "QUIZ",  { 239,  68,  68, 255 } },   // 0  red
	{ "ENA",  "ASSN",  { 249, 115,  22, 255 } },   // 1  orange
	{ "LA",   "QUIZ",  { 168,  85, 247, 255 } },   // 2  purple
	{ "LA",   "ASSN",  { 236,  72, 153, 255 } },   // 3  pink
	{ "FOP",  "QUIZ",  {  34, 211, 238, 255 } },   // 4  cyan
	{ "FOP",  "ASSN",  {  16, 185, 129, 255 } },   // 5  emerald
	{ "OOP",  "QUIZ",  { 234, 179,   8, 255 } },   // 6  amber
	{ "DS",   "ASSN",  { 244,  63,  94, 255 } },   // 7  rose
};
static const int   NBT         = 8;
static const float BALL_RADIUS = 26.f;


//  STRUCTS  (kept close to your original naming style)

struct Ball {
	Vector2 position = { 0, 0 };
	Vector2 velocity = { 0, 0 };
	float   radius   = BALL_RADIUS;
	int     type     = 0;
};

struct Paddle {
	Vector2 position = { 0, 0 };
	Vector2 size     = { 220, 28 };
	float   speed    = 520.f;
};

struct Gameobjects {
	Paddle            player;
	std::vector<Ball> balls;
	int   lives        = 3;
	int   level        = 1;
	float timer        = LEVEL_DURATION;
	bool  flash        = false;
	float flashT       = 0.f;
	int   newBallType  = 0;   // type index of the ball that just spawned on level-up
};

struct BGPhrase {
	const char* txt;
	float x, y, rot;
	Color col;
	int   fs;
};

enum GameState { MENU, PLAYING, GAME_OVER };


//  GLOBALS

static GameState    state   = MENU;
static Gameobjects  gm;
static Texture2D    btex[NBT];
static Sound        sWall, sPad, sLife, sLvl, sOver;
static BGPhrase     phrases[28];
static int          phraseN = 0;


//  PROCEDURAL BEEP GENERATOR

static Sound make_beep(float f0, float f1, float dur, float vol = 0.40f)
{
	int sr = 44100, n = (int)(sr * dur);
	short* d = (short*)RL_MALLOC(n * sizeof(short));
	for (int i = 0; i < n; i++) {
		float t   = (float)i / sr;
		float pct = (float)i / n;
		float f   = f0 + (f1 - f0) * pct;
		float env = pct < 0.05f ? pct / 0.05f : 1.f - pct;
		d[i] = (short)(32000.f * vol * env * sinf(2.f * PI * f * t));
	}
	Wave w = { (unsigned)n, (unsigned)sr, 16, 1, d };
	Sound s = LoadSoundFromWave(w);
	RL_FREE(d);
	return s;
}


//  BALL TEXTURE  (radial gradient + specular highlight)
static Texture2D make_ball_texture(int r, Color c)
{int sz = r * 2;
	Image img = GenImageColor(sz, sz, BLANK);
	for (int y = 0; y < sz; y++) {
		for (int x = 0; x < sz; x++) {
			float dx = x - r, dy = y - r;
			float d  = sqrtf(dx * dx + dy * dy);
			if (d > r) continue;
			float t  = d / r;
			float br = 1.f - t * 0.55f;
			Color px;
			px.r = (unsigned char)fminf(255, c.r * br + 255 * (1.f - t) * 0.45f);
			px.g = (unsigned char)fminf(255, c.g * br + 255 * (1.f - t) * 0.42f);
			px.b = (unsigned char)fminf(255, c.b * br + 255 * (1.f - t) * 0.38f);
			px.a = 255;
			// specular highlight (top-left)
			float hx = dx + r * 0.36f, hy = dy + r * 0.36f;
			float hd = sqrtf(hx * hx + hy * hy);
			if (hd < r * 0.36f) {
				float hl = 1.f - hd / (r * 0.36f);
				px.r = (unsigned char)fminf(255, px.r + 210 * hl * hl);
				px.g = (unsigned char)fminf(255, px.g + 210 * hl * hl);
				px.b = (unsigned char)fminf(255, px.b + 210 * hl * hl);
			}
			ImageDrawPixel(&img, x, y, px);
		}
	}
	Texture2D tex = LoadTextureFromImage(img);
	UnloadImage(img);
	return tex;
}


//  BACKGROUND PHRASES  (life of a CE student)

static void init_phrases()
{
	struct R { const char* t; float x, y, rot; int bright, fs; } raw[] = {
		{ "why wont it compile",          55,   48,  -9, 0, 13 },
		{ "3 AM debugging session",      720,   30,   5, 0, 12 },
		{ "deadline:  11:59 PM",         290,  118,  -5, 1, 14 },
		{ "segfault  (core dumped)",      890,  175,   8, 0, 11 },
		{ "git push origin main",          45,  248,  -7, 0, 12 },
		{ "stack overflow  tab #47",      640,  215,   9, 1, 11 },
		{ "undefined behavior",           195,  345, -12, 0, 13 },
		{ "coffee++  ;  sleep--",         825,  360,   4, 1, 14 },
		{ "O(n^2) is fine right?",        415,  432,  -7, 0, 11 },
		{ "it works on my machine",        28,  502,   6, 0, 12 },
		{ "read chapters 5 to 12",        755,  468,  -5, 1, 13 },
		{ "NullPointerException",         305,  578,   8, 0, 11 },
		{ "one more energy drink",        895,  555,  -9, 1, 12 },
		{ "ctrl+z  ctrl+z  ctrl+z",       100,  652,   3, 0, 13 },
		{ "bhai circuit kyun nahi bana",  540,  622, -10, 0, 10 },
		{ "semester mai kuch nahi aya",   195,  722,   7, 0, 11 },
		{ "ENA viva kal hai yaar",        700,  705,  -6, 1, 12 },
		{ "LA matrix phir nahi aaya",     400,  762,   4, 0, 11 },
		{ "FOP recursion phir se",        855,  745,  -8, 1, 13 },
		{ "todo:  understand everything",  48,  762,   5, 0, 10 },
		{ "sleep is a myth",             370,  310,  -8, 0, 15 },
		{ "have you tried restarting",    820,  640,  10, 0, 10 },
		{ "LIFE OF A CE STUDENT",          80,  195, -18, 1, 32 },
		{ "bechara student",             590,  490,  12, 1, 24 },
	};
	phraseN = (int)(sizeof(raw) / sizeof(raw[0]));
	for (int i = 0; i < phraseN; i++)
		phrases[i] = { raw[i].t, raw[i].x, raw[i].y, raw[i].rot,
		               raw[i].bright ? PAL_DIM : PAL_GHOST, raw[i].fs };
}


//  DRAW BACKGROUND

static void draw_background()
{
	ClearBackground(PAL_BG);
	// dot grid
	for (int x = 44; x < SCR_W; x += 55)
		for (int y = 44; y < SCR_H; y += 55)
			DrawPixel(x, y, PAL_DOT);
	// scattered CE-life phrases
	Font fnt = GetFontDefault();
	for (int i = 0; i < phraseN; i++) {
		BGPhrase& p = phrases[i];
		float hw = MeasureText(p.txt, p.fs) * 0.5f;
		float hh = p.fs * 0.5f;
		DrawTextPro(fnt, p.txt, { p.x + hw, p.y + hh }, { hw, hh },
		            p.rot, (float)p.fs, 1.f, p.col);
	}
}


//  DRAW BALL

static void draw_ball(const Ball& ball)
{
	Color tc = BT[ball.type].tint;
	// glow ring
	DrawCircleGradient(
		(int)ball.position.x, (int)ball.position.y,
		ball.radius + 18,
		ColorAlpha(tc, 0.28f),
		ColorAlpha(tc, 0.f)
	);
	// textured sphere
	DrawTexture(btex[ball.type],
	            (int)(ball.position.x - ball.radius),
	            (int)(ball.position.y - ball.radius), WHITE);
	// readable subject labels — drawn with outline for contrast
	int fs = 11;
	const char* l1 = BT[ball.type].l1;
	const char* l2 = BT[ball.type].l2;
	int x1 = (int)ball.position.x - MeasureText(l1, fs) / 2;
	int x2 = (int)ball.position.x - MeasureText(l2, fs) / 2;
	int y1 = (int)ball.position.y - fs - 1;
	int y2 = (int)ball.position.y + 2;
	// dark outline for readability
	DrawText(l1, x1 + 1, y1 + 1, fs, BLACK);
	DrawText(l2, x2 + 1, y2 + 1, fs, BLACK);
	DrawText(l1, x1, y1, fs, PAL_WHITE);
	DrawText(l2, x2, y2, fs, PAL_WHITE);
}


//  DRAW PADDLE  — carbon-fibre professional style

static void draw_paddle()
{
	float px = gm.player.position.x, py = gm.player.position.y;
	float pw = gm.player.size.x,     ph = gm.player.size.y;
	Rectangle r = { px, py, pw, ph };

	// layer 1: broad soft shadow
	DrawRectangleRounded({ px + 5, py + 7, pw, ph }, 0.50f, 8, { 0, 0, 0, 120 });

	// layer 2: deep gunmetal base
	DrawRectangleRounded(r, 0.50f, 8, { 16, 18, 38, 255 });

	// layer 3: carbon-fibre texture — fine vertical slots
	for (int i = 0; i < (int)(pw / 8); i++) {
		float lx = px + 6 + i * 8.f;
		DrawLine((int)lx, (int)(py + 3), (int)lx, (int)(py + ph - 3),
		         { 255, 255, 255, 9 });
	}
	// cross-hatch horizontal pass
	for (int i = 1; i <= 2; i++) {
		float ly = py + ph * i / 3.f;
		DrawLine((int)(px + 6), (int)ly, (int)(px + pw - 6), (int)ly,
		         { 255, 255, 255, 7 });
	}

	// layer 4: top-edge specular (lit from above)
	DrawRectangleRounded({ px + 2, py + 2, pw - 4, ph * 0.38f }, 0.50f, 8,
	                     { 100, 120, 255, 28 });
	DrawLineEx({ px + 10, py + 2 }, { px + pw - 10, py + 2 }, 1.5f,
	            { 220, 230, 255, 90 });

	// layer 5: neon left accent bar (ice blue) + glow
	DrawRectangleRounded({ px - 3, py - 2, 14, ph + 4 }, 0.60f, 4,
	                     { 60, 160, 255, 40 });
	DrawRectangleRounded({ px, py, 10, ph }, 0.60f, 4, { 60, 160, 255, 255 });

	// layer 6: neon right accent bar (gold) + glow
	DrawRectangleRounded({ px + pw - 11, py - 2, 14, ph + 4 }, 0.60f, 4,
	                     { 245, 190, 20, 40 });
	DrawRectangleRounded({ px + pw - 10, py, 10, ph }, 0.60f, 4,
	                     { 245, 190, 20, 255 });

	// layer 7: sharp outer rim
	DrawRectangleRoundedLines(r, 0.50f, 8, { 80, 100, 220, 160 });

	// layer 8: label with drop shadow
	const char* lbl = "bechara student";
	int fs = 13, lw = MeasureText(lbl, fs);
	DrawText(lbl, (int)(px + pw / 2 - lw / 2) + 1, (int)(py + ph / 2 - fs / 2) + 1,
	         fs, { 0, 0, 0, 120 });
	DrawText(lbl, (int)(px + pw / 2 - lw / 2), (int)(py + ph / 2 - fs / 2),
	         fs, { 190, 205, 255, 230 });
}


//  DRAW HUD

static void draw_hud()
{
	// lives
	DrawText("LIVES", 10, 10, 16, ColorAlpha(PAL_WHITE, 0.50f));
	for (int i = 0; i < 3; i++) {
		Color c = (i < gm.lives) ? PAL_RED : (Color){ 30, 36, 72, 255 };
		DrawCircle(90 + i * 28, 20, 10, c);
	}
	// level
	const char* lvl = TextFormat("LEVEL  %d", gm.level);
	DrawText(lvl, SCR_W / 2 - MeasureText(lvl, 26) / 2, 6, 26, PAL_GOLD);
	// ball count
	DrawText(TextFormat("%d BALLS", (int)gm.balls.size()),
	         SCR_W - 120, 10, 16, ColorAlpha(PAL_ICE, 0.85f));
	// level timer bar
	float ratio = gm.timer / LEVEL_DURATION;
	Color barC  = ratio > 0.50f ? (Color){ 16, 185, 129, 255 }
	            : ratio > 0.25f ? PAL_GOLD : PAL_RED;
	int bw = 290, bx = SCR_W / 2 - bw / 2, by = 36;
	DrawRectangle(bx, by, bw, 5, { 18, 24, 56, 255 });
	DrawRectangle(bx, by, (int)(bw * ratio), 5, barC);
	DrawText("NEXT LEVEL",
	         bx + bw / 2 - MeasureText("NEXT LEVEL", 9) / 2,
	         by - 13, 9, ColorAlpha(PAL_WHITE, 0.30f));
}


//  GAME HELPERS

static void clamp_speed(Ball& ball)
{
	float s = sqrtf(ball.velocity.x * ball.velocity.x + ball.velocity.y * ball.velocity.y);
	if (s > MAX_SPEED) {
		ball.velocity.x = ball.velocity.x / s * MAX_SPEED;
		ball.velocity.y = ball.velocity.y / s * MAX_SPEED;
	}
}

static Ball spawn_ball(int idx, int lvl, float x_hint)
{
	Ball ball;
	ball.type     = idx % NBT;
	ball.radius   = BALL_RADIUS;
	ball.position = { x_hint, (float)SCR_H / 3.f };
	float spd     = 240.f + lvl * 26.f;
	float ang     = (34.f + (float)(rand() % 42)) * DEG2RAD;
	ball.velocity = { spd * sinf(ang) * ((rand() % 2) ? 1.f : -1.f), -spd * cosf(ang) };
	return ball;
}

static Gameobjects make_objects()
{
	Gameobjects obj;
	obj.player.size     = { 220, 28 };
	obj.player.position = { (SCR_W - 220) / 2.f, SCR_H - 38.f };
	obj.player.speed    = 520.f;
	obj.lives = 3; obj.level = 1; obj.timer = LEVEL_DURATION;
	obj.balls.clear();
	obj.balls.push_back(spawn_ball(0, 1, SCR_W / 2.f));
	return obj;
}

static void level_up()
{
	gm.level++;
	gm.timer = LEVEL_DURATION;
	int idx = (int)gm.balls.size();
	gm.newBallType = idx % NBT;
	gm.balls.push_back(spawn_ball(idx, gm.level, (float)(rand() % (SCR_W - 140) + 70)));
	for (auto& ball : gm.balls) {
		ball.velocity.x *= 1.08f;
		ball.velocity.y *= 1.08f;
		clamp_speed(ball);
	}
	if (gm.player.size.x > 90) gm.player.size.x -= 11;
	gm.flash = true; gm.flashT = 1.5f;
	PlaySound(sLvl);
}

//  BALL-BALL COLLISION  (elastic bounce)
static void handle_ball_collisions()
{
	for (int i = 0; i < (int)gm.balls.size(); i++) {
		for (int j = i + 1; j < (int)gm.balls.size(); j++) {
			Ball& a = gm.balls[i];
			Ball& b = gm.balls[j];
			float dx = b.position.x - a.position.x;
			float dy = b.position.y - a.position.y;
			float dist = sqrtf(dx * dx + dy * dy);
			float minDist = a.radius + b.radius;
			if (dist < minDist && dist > 0.001f) {
				// normalised collision axis
				float nx = dx / dist, ny = dy / dist;
				// push apart so they don't overlap
				float overlap = (minDist - dist) * 0.5f;
				a.position.x -= nx * overlap;
				a.position.y -= ny * overlap;
				b.position.x += nx * overlap;
				b.position.y += ny * overlap;
				// swap velocity components along collision axis (equal-mass elastic)
				float dvx = b.velocity.x - a.velocity.x;
				float dvy = b.velocity.y - a.velocity.y;
				float dot  = dvx * nx + dvy * ny;
				if (dot < 0) {       // only resolve if approaching
					a.velocity.x += dot * nx;
					a.velocity.y += dot * ny;
					b.velocity.x -= dot * nx;
					b.velocity.y -= dot * ny;
					clamp_speed(a);
					clamp_speed(b);
					PlaySound(sWall);
				}
			}
		}
	}
}

//  BALL-PADDLE BOUNCE  (fixed — speed must never decrease)
static void handle_paddle_bounce(Ball& ball)
{
	Rectangle paddleRect = { gm.player.position.x, gm.player.position.y,
	                         gm.player.size.x,     gm.player.size.y };

	if (CheckCollisionCircleRec(ball.position, ball.radius, paddleRect)) {
		// get current speed before doing anything
		float spd = sqrtf(ball.velocity.x * ball.velocity.x +
		                  ball.velocity.y * ball.velocity.y);

		// reflect upward
		ball.velocity.y  = -fabsf(ball.velocity.y);
		ball.position.y  = gm.player.position.y - ball.radius;

		// steer based on hit position (how far from paddle centre)
		float hp          = (ball.position.x - gm.player.position.x) / gm.player.size.x;
		float steer       = (hp - 0.5f) * 2.f * 180.f;   // ±180 px/s nudge
		ball.velocity.x  += steer;

		// re-normalise to original speed then apply 5 % boost
		float newSpd = sqrtf(ball.velocity.x * ball.velocity.x +
		                     ball.velocity.y * ball.velocity.y);
		float boost  = spd * 1.05f / newSpd;      // never slower, always 5 % faster
		ball.velocity.x *= boost;
		ball.velocity.y *= boost;
		clamp_speed(ball);
		PlaySound(sPad);
	}
}


//  MAIN

int main()
{
	srand((unsigned)time(nullptr));
	InitWindow(SCR_W, SCR_H, "Bechara Student");
	SetTargetFPS(60);
	InitAudioDevice();

	sWall = make_beep(500, 390, 0.08f, 0.30f);
	sPad  = make_beep(270, 220, 0.11f, 0.42f);
	sLife = make_beep(220,  65, 0.55f, 0.60f);
	sLvl  = make_beep(400, 820, 0.42f, 0.50f);
	sOver = make_beep(140,  50, 1.00f, 0.65f);

	for (int i = 0; i < NBT; i++)
		btex[i] = make_ball_texture((int)BALL_RADIUS, BT[i].tint);

	init_phrases();
	gm = make_objects();

	while (!WindowShouldClose())
	{
		float dt = GetFrameTime();

		// ══ MENU ═════════════════════════════════════
		if (state == MENU)
		{
			if (IsKeyPressed(KEY_ENTER)) { gm = make_objects(); state = PLAYING; }

			BeginDrawing();
			draw_background();

			const char* t1 = "BECHARA"; const char* t2 = "STUDENT";
			int s1 = 108, s2 = 108;
			DrawText(t1, SCR_W / 2 - MeasureText(t1, s1) / 2 + 5, 120, s1, PAL_GHOST);
			DrawText(t2, SCR_W / 2 - MeasureText(t2, s2) / 2 + 5, 235, s2, PAL_GHOST);
			DrawText(t1, SCR_W / 2 - MeasureText(t1, s1) / 2,     116, s1, PAL_GOLD);
			DrawText(t2, SCR_W / 2 - MeasureText(t2, s2) / 2,     231, s2, PAL_WHITE);

			const char* sub = "dodge your quizzes & assignments before they hit the floor";
			DrawText(sub, SCR_W / 2 - MeasureText(sub, 17) / 2, 372, 17,
			         ColorAlpha(PAL_WHITE, 0.50f));
			const char* ctrl = "<-  ->  move        ENTER  start";
			DrawText(ctrl, SCR_W / 2 - MeasureText(ctrl, 22) / 2, 418, 22, PAL_ICE);

			// ball preview row
			for (int i = 0; i < NBT; i++) {
				float bx = SCR_W / 2.f - (NBT * 58) / 2.f + i * 58 + 29;
				DrawCircleGradient((int)bx, 516, 30,
				                   ColorAlpha(BT[i].tint, 0.20f),
				                   ColorAlpha(BT[i].tint, 0.f));
				DrawTexture(btex[i], (int)(bx - BALL_RADIUS), (int)(516 - BALL_RADIUS), WHITE);
				int fx = 9;
				DrawText(BT[i].l1, (int)bx - MeasureText(BT[i].l1, fx) / 2, (int)(516 - fx - 1), fx, PAL_WHITE);
				DrawText(BT[i].l2, (int)bx - MeasureText(BT[i].l2, fx) / 2, (int)(516 + 2),       fx, PAL_WHITE);
			}
			EndDrawing();
		}

		// ══ PLAYING ══════════════════════════════════
		else if (state == PLAYING)
		{
			// move paddle
			if (IsKeyDown(KEY_RIGHT)) gm.player.position.x += gm.player.speed * dt;
			if (IsKeyDown(KEY_LEFT))  gm.player.position.x -= gm.player.speed * dt;
			if (gm.player.position.x < 0)
				gm.player.position.x = 0;
			if (gm.player.position.x + gm.player.size.x > SCR_W)
				gm.player.position.x = SCR_W - gm.player.size.x;

			// level timer
			gm.timer -= dt;
			if (gm.timer <= 0.f) level_up();
			if (gm.flash) { gm.flashT -= dt; if (gm.flashT <= 0.f) gm.flash = false; }

			// update each ball
			for (auto& ball : gm.balls) {
				ball.position.x += ball.velocity.x * dt;
				ball.position.y += ball.velocity.y * dt;

				// wall bounces
				if (ball.position.x - ball.radius <= 0) {
					ball.velocity.x = fabsf(ball.velocity.x);
					ball.position.x = ball.radius;
					PlaySound(sWall);
				}
				if (ball.position.x + ball.radius >= SCR_W) {
					ball.velocity.x = -fabsf(ball.velocity.x);
					ball.position.x = SCR_W - ball.radius;
					PlaySound(sWall);
				}
				if (ball.position.y - ball.radius <= 0) {
					ball.velocity.y = fabsf(ball.velocity.y);
					ball.position.y = ball.radius;
					PlaySound(sWall);
				}

				// paddle bounce (fixed speed logic)
				handle_paddle_bounce(ball);

				// fell off bottom
				if (ball.position.y - ball.radius > SCR_H) {
					gm.lives--;
					PlaySound(sLife);
					float spd      = 240.f + gm.level * 26.f;
					ball.position  = { (float)(rand() % (SCR_W - 140) + 70), (float)SCR_H / 3.f };
					ball.velocity  = { spd * 0.7f * ((rand() % 2) ? 1.f : -1.f), spd };
					if (gm.lives <= 0) { PlaySound(sOver); state = GAME_OVER; }
				}
			}

			// ball-vs-ball collisions
			handle_ball_collisions();

			BeginDrawing();
			draw_background();
			for (auto& ball : gm.balls) draw_ball(ball);
			draw_paddle();
			draw_hud();

			if (gm.flash) {
				float a    = gm.flashT / 1.5f;
				int   ti   = gm.newBallType;
				Color tc   = BT[ti].tint;
				// tinted screen wash matching the new ball's colour
				DrawRectangle(0, 0, SCR_W, SCR_H,
				              { tc.r, tc.g, tc.b, (unsigned char)(30 * a) });

				// "LEVEL X" header
				const char* hdr = TextFormat("LEVEL  %d", gm.level);
				int hfs = 52;
				DrawText(hdr,
				         SCR_W / 2 - MeasureText(hdr, hfs) / 2,
				         SCR_H / 2 - 80, hfs,
				         { 255, 230, 80, (unsigned char)(255 * a) });

				// new ball name  e.g.  "FOP  QUIZ"
				const char* bname = TextFormat("%s  %s", BT[ti].l1, BT[ti].l2);
				int bfs = 46;
				// draw it in the ball's own tint colour
				DrawText(bname,
				         SCR_W / 2 - MeasureText(bname, bfs) / 2,
				         SCR_H / 2 - 16, bfs,
				         { tc.r, tc.g, tc.b, (unsigned char)(255 * a) });

				// small caption below
				const char* cap = "incoming assignment!";
				int cfs = 20;
				DrawText(cap,
				         SCR_W / 2 - MeasureText(cap, cfs) / 2,
				         SCR_H / 2 + 40, cfs,
				         { 255, 255, 255, (unsigned char)(180 * a) });
			}
			EndDrawing();
		}

		// ══ GAME OVER ════════════════════════════════
		else if (state == GAME_OVER)
		{
			if (IsKeyPressed(KEY_ENTER)) state = MENU;

			BeginDrawing();
			draw_background();
			const char* h1 = "SEMESTER FAILED";
			DrawText(h1, SCR_W / 2 - MeasureText(h1, 72) / 2 + 5, 172, 72, PAL_GHOST);
			DrawText(h1, SCR_W / 2 - MeasureText(h1, 72) / 2,     168, 72, PAL_RED);

			const char* l1 = TextFormat("Survived until Level %d", gm.level);
			DrawText(l1, SCR_W / 2 - MeasureText(l1, 28) / 2, 275, 28, PAL_GOLD);

			const char* l2 = TextFormat("Was juggling %d assignments at once", (int)gm.balls.size());
			DrawText(l2, SCR_W / 2 - MeasureText(l2, 20) / 2, 320, 20, ColorAlpha(PAL_WHITE, 0.60f));

			DrawText("ENTER  ->  back to menu",
			         SCR_W / 2 - MeasureText("ENTER  ->  back to menu", 22) / 2,
			         410, 22, PAL_ICE);
			EndDrawing();
		}
	}

	for (int i = 0; i < NBT; i++) UnloadTexture(btex[i]);
	UnloadSound(sWall); UnloadSound(sPad);
	UnloadSound(sLife); UnloadSound(sLvl); UnloadSound(sOver);
	CloseAudioDevice();
	CloseWindow();
	return 0;
}