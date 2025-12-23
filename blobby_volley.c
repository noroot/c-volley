/*******************************************************************************************
*
*   C-volley - classic volley game, written in C.
*   A fun volleyball game featuring plain C with raylib. Made just for fun and aestethics.
*
*   Author: Dmitry R <public@falsetrue.io>
*   Version: 1.0
*   License: GPL-3.0
*
*   Dependencies:
*   - This game has been created using raylib (www.raylib.com)
*   - raylib is licensed under an unmodified zlib/libpng license (View raylib.h for details)
*
*   Copyright: 2025 (c) Dmitry R. <public@falsetrue.io>
********************************************************************************************/

#include "raylib.h"
#include <math.h>

#if defined(PLATFORM_WEB)
    #include <emscripten/emscripten.h>
#endif

//----------------------------------------------------------------------------------
// Defines
//----------------------------------------------------------------------------------

#define APP_NAME "C-Volley"
#define COPYRIGHT "C-Volley v1.0, dmth (c) 2025"

#define SCREEN_WIDTH 1024
#define SCREEN_HEIGHT 768

// Physics constants
#define PLAYER_GRAVITY 0.8f
#define BALL_GRAVITY 0.4f
#define BALL_AIR_RESISTANCE 0.99f  // Horizontal velocity damping per frame
#define BALL_BOUNCE_DAMPING 1.0f   // Energy loss on wall/ceiling bounce
#define GROUND_LEVEL (SCREEN_HEIGHT - 50)
#define PLAYER_RADIUS 50.0f
#define BALL_RADIUS 35.0f

// Movement constants
#define PLAYER_MOVE_SPEED 4.0f
#define PLAYER_JUMP_FORCE -12.0f
#define PLAYER_MAX_VELOCITY_Y 15.0f
#define BALL_INITIAL_SPEED_X 4.0f
#define BALL_INITIAL_SPEED_Y -6.0f
#define BALL_MAX_SPEED 15.0f

// Court layout
#define NET_X (SCREEN_WIDTH / 2)
#define NET_HEIGHT 140.0f
#define NET_WIDTH 10.0f

// Game rules
#define WIN_SCORE 10
#define TRAIL_LENGTH 3

// AI constants
#define AI_REACTION_DISTANCE 150.0f
#define AI_JUMP_THRESHOLD 60.0f
#define AI_POSITION_TOLERANCE 20.0f
#define AI_JUMP_COOLDOWN 30

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
typedef enum GameState {
    MENU = 0,
    PLAYING,
    GAMEOVER,
    CREDITS
} GameState;

typedef enum GameMode {
    SINGLE_PLAYER = 0,
    TWO_PLAYER
} GameMode;

typedef enum PlayerSide {
    LEFT = 0,
    RIGHT
} PlayerSide;

typedef struct Player {
    Vector2 position;
    Vector2 velocity;
    float radius;
    PlayerSide side;
    Color color;
    int score;
    bool onGround;
} Player;

typedef struct Ball {
    Vector2 position;
    Vector2 velocity;
    float radius;
    Vector2 trail[TRAIL_LENGTH];
    int trailCount;
    float rotation;  // Rotation angle in degrees
} Ball;

typedef struct Particle {
    Vector2 position;
    Vector2 velocity;
    Color color;
    float alpha;
    float life;  // 0.0 to 1.0
    bool active;
} Particle;

#define MAX_PARTICLES 100

//------------------------------------------------------------------------------------
// Global Variables Declaration
//------------------------------------------------------------------------------------
static const int screenWidth = SCREEN_WIDTH;
static const int screenHeight = SCREEN_HEIGHT;

static GameState gameState = MENU;
static GameMode gameMode = SINGLE_PLAYER;
static bool pause = false;
static int framesCounter = 0;

static Player player1 = { 0 };
static Player player2 = { 0 };
static Ball ball = { 0 };

// Particle system
static Particle particles[MAX_PARTICLES] = { 0 };

// AI state
static int aiJumpCooldown = 0;

// Serving state
static PlayerSide servingSide = LEFT;

// Score delay (2 seconds at 60fps = 120 frames)
static int scoreDelayTimer = 0;
#define SCORE_DELAY_FRAMES 120

// Menu selection
static int menuSelection = 0;

// Credits scroll position
static float creditsScroll = 0;

// Exit flag
static bool shouldExitGame = false;

// Match timer (in frames, 60fps)
static int matchTimer = 0;

// Audio (commented - structure ready for future sound files)
static Sound fxJump;
static Sound fxBallBounce;
static Sound fxScore;
static Sound fxGameOver;

// Music
static Music menuMusic;
static Music creditsMusic;

// Textures
static Texture2D backgroundTexture;
static Texture2D ballTexture;

//------------------------------------------------------------------------------------
// Module Functions Declaration (local)
//------------------------------------------------------------------------------------
static void InitGame(void);
static void UpdateGame(void);
static void DrawGame(void);
static void UnloadGame(void);
static void UpdateDrawFrame(void);

// Helper functions
static void ResetBall(void);
static void UpdatePlayerControls(void);
static void UpdateAI(void);
static void UpdateBallTrail(void);
static void DrawBallTrail(void);
static void DrawSpinningBall(void);
static void DrawNet(void);
static void DrawScore(void);
static void DrawMenu(void);
static void DrawPlayerShadow(Player player);
static void DrawGround(void);
static void DrawCredits(void);

// Particle system functions
static void SpawnGroundParticles(Vector2 position, int count);
static void UpdateParticles(void);
static void DrawParticles(void);

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
int main(void)
{
    InitWindow(screenWidth, screenHeight, APP_NAME);
    SetExitKey(KEY_NULL);  // Disable default Escape key to close window

    InitGame();

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(UpdateDrawFrame, 60, 1);
#else
    SetTargetFPS(60);

    while (!WindowShouldClose() && !shouldExitGame)
    {
        UpdateDrawFrame();
    }
#endif

    UnloadGame();
    CloseWindow();

    return 0;
}

//------------------------------------------------------------------------------------
// Module Functions Definitions (local)
//------------------------------------------------------------------------------------

// Initialize game variables
void InitGame(void)
{
    // Set initial game state
    gameState = MENU;
    menuSelection = 0;
    pause = false;
    framesCounter = 0;
    aiJumpCooldown = 0;
    servingSide = LEFT;
    scoreDelayTimer = 0;
    matchTimer = 0;

    // Initialize Player 1 (left side - blue)
    player1.position = (Vector2){ SCREEN_WIDTH / 4, GROUND_LEVEL - PLAYER_RADIUS };
    player1.velocity = (Vector2){ 0, 0 };
    player1.radius = PLAYER_RADIUS;
    player1.side = LEFT;
    player1.color = BLUE;
    player1.score = 0;
    player1.onGround = true;

    // Initialize Player 2 (right side - red)
    player2.position = (Vector2){ SCREEN_WIDTH * 3 / 4, GROUND_LEVEL - PLAYER_RADIUS };
    player2.velocity = (Vector2){ 0, 0 };
    player2.radius = PLAYER_RADIUS;
    player2.side = RIGHT;
    player2.color = RED;
    player2.score = 0;
    player2.onGround = true;

    // Initialize Ball
    ball.radius = BALL_RADIUS;
    ball.trailCount = 0;
    ResetBall();

    // SFX initialization 
    InitAudioDevice();
    fxJump = LoadSound("resources/jump.wav");
    fxBallBounce = LoadSound("resources/bounce.wav");
    fxScore = LoadSound("resources/score.wav");
    fxGameOver = LoadSound("resources/gameover.wav");

    menuMusic = LoadMusicStream("resources/hymn_to_aurora.mod");
    SetMusicVolume(menuMusic, 0.5f);

    creditsMusic = LoadMusicStream("resources/space_debris.mod");
    SetMusicVolume(creditsMusic, 0.5f);

    // Load textures
    backgroundTexture = LoadTexture("resources/background.png");
    ballTexture = LoadTexture("resources/ball.png");
}

// Reset ball on serving player's side
void ResetBall(void)
{
    if (servingSide == LEFT)
    {
        ball.position = (Vector2){ SCREEN_WIDTH / 4, 100 };
    }
    else
    {
        ball.position = (Vector2){ SCREEN_WIDTH * 3 / 4, 100 };
    }

    ball.velocity.x = 0;
    ball.velocity.y = 0;
    ball.trailCount = 0;
    ball.rotation = 0;
}

// Update player controls
void UpdatePlayerControls(void)
{
    // Player 1 (Left side) - W/A/D controls
    if (IsKeyDown(KEY_A))
    {
        player1.position.x -= PLAYER_MOVE_SPEED;
        player1.velocity.x = -PLAYER_MOVE_SPEED;
    }
    else if (IsKeyDown(KEY_D))
    {
        player1.position.x += PLAYER_MOVE_SPEED;
        player1.velocity.x = PLAYER_MOVE_SPEED;
    }
    else
    {
        player1.velocity.x = 0;
    }

    // Jump
    if (IsKeyPressed(KEY_W) && player1.onGround)
    {
        player1.velocity.y = PLAYER_JUMP_FORCE;
        player1.onGround = false;
        /* NOTE: Need better jump sound 
           PlaySound(fxJump); */
    }

    // Keep player on their side (left of net)
    if (player1.position.x - player1.radius < 0)
    {
        player1.position.x = player1.radius;
    }
    if (player1.position.x + player1.radius > NET_X - NET_WIDTH / 2)
    {
        player1.position.x = NET_X - NET_WIDTH / 2 - player1.radius;
    }

    // Player 2 (Right side) - Only if TWO_PLAYER mode
    if (gameMode == TWO_PLAYER)
    {
        if (IsKeyDown(KEY_LEFT))
        {
            player2.position.x -= PLAYER_MOVE_SPEED;
            player2.velocity.x = -PLAYER_MOVE_SPEED;
        }
        else if (IsKeyDown(KEY_RIGHT))
        {
            player2.position.x += PLAYER_MOVE_SPEED;
            player2.velocity.x = PLAYER_MOVE_SPEED;
        }
        else
        {
            player2.velocity.x = 0;
        }

        if (IsKeyPressed(KEY_UP) && player2.onGround)
        {
            player2.velocity.y = PLAYER_JUMP_FORCE;
            player2.onGround = false;
            /* NOTE: Need better jump sound PlaySound(fxJump); */
        }

        // Keep player on their side (right of net)
        if (player2.position.x - player2.radius < NET_X + NET_WIDTH / 2)
        {
            player2.position.x = NET_X + NET_WIDTH / 2 + player2.radius;
        }
        if (player2.position.x + player2.radius > SCREEN_WIDTH)
        {
            player2.position.x = SCREEN_WIDTH - player2.radius;
        }
    }
}

// Update AI (controls player2 in single-player mode)
void UpdateAI(void)
{
    // Cooldown management
    if (aiJumpCooldown > 0) aiJumpCooldown--;

    // Only react if ball is on AI's side or coming toward it
    bool ballComingToward = (ball.velocity.x > 0 && ball.position.x < NET_X) ||
                            (ball.position.x >= NET_X);

    if (!ballComingToward)
    {
        // Return to center of side
        float targetX = NET_X + (SCREEN_WIDTH - NET_X) / 2;
        if (player2.position.x < targetX - AI_POSITION_TOLERANCE)
        {
            player2.position.x += PLAYER_MOVE_SPEED * 0.6f;
        }
        else if (player2.position.x > targetX + AI_POSITION_TOLERANCE)
        {
            player2.position.x -= PLAYER_MOVE_SPEED * 0.6f;
        }
        player2.velocity.x = 0;
        return;
    }

    // Calculate horizontal distance to ball
    float distanceX = ball.position.x - player2.position.x;

    // Move toward ball's X position
    if (distanceX < -AI_POSITION_TOLERANCE)
    {
        player2.position.x -= PLAYER_MOVE_SPEED * 0.8f;
        player2.velocity.x = -PLAYER_MOVE_SPEED * 0.8f;
    }
    else if (distanceX > AI_POSITION_TOLERANCE)
    {
        player2.position.x += PLAYER_MOVE_SPEED * 0.8f;
        player2.velocity.x = PLAYER_MOVE_SPEED * 0.8f;
    }
    else
    {
        player2.velocity.x = 0;
    }

    // Keep AI on their side
    if (player2.position.x - player2.radius < NET_X + NET_WIDTH / 2)
    {
        player2.position.x = NET_X + NET_WIDTH / 2 + player2.radius;
    }
    if (player2.position.x + player2.radius > SCREEN_WIDTH)
    {
        player2.position.x = SCREEN_WIDTH - player2.radius;
    }

    // Jump decision logic
    float distanceY = player2.position.y - ball.position.y;
    float horizontalDist = fabsf(distanceX);

    // Jump conditions
    bool shouldJump = (horizontalDist < AI_REACTION_DISTANCE) &&
                      (distanceY > -AI_JUMP_THRESHOLD) &&
                      (distanceY < 100.0f) &&
                      (aiJumpCooldown == 0) &&
                      (player2.onGround);

    // Add randomness to make AI beatable (20% chance to miss)
    if (shouldJump && GetRandomValue(0, 100) > 20)
    {
        player2.velocity.y = PLAYER_JUMP_FORCE * 0.9f;
        player2.onGround = false;
        aiJumpCooldown = AI_JUMP_COOLDOWN;
        /* NOTE: Need better jump sound,  PlaySound(fxJump); */
    }
}

// Update ball trail effect
void UpdateBallTrail(void)
{
    for (int i = TRAIL_LENGTH - 1; i > 0; i--)
    {
        ball.trail[i] = ball.trail[i - 1];
    }

    ball.trail[0] = ball.position;

    if (ball.trailCount < TRAIL_LENGTH)
    {
        ball.trailCount++;
    }
}

// Update game (one frame)
void UpdateGame(void)
{
    framesCounter++;

    // Update music streams
    UpdateMusicStream(menuMusic);
    UpdateMusicStream(creditsMusic);

    // Control music based on game state
    if (gameState == MENU)
    {
        if (!IsMusicStreamPlaying(menuMusic))
        {
            PlayMusicStream(menuMusic);
        }
        if (IsMusicStreamPlaying(creditsMusic))
        {
            StopMusicStream(creditsMusic);
        }
    }
    else if (gameState == CREDITS)
    {
        if (!IsMusicStreamPlaying(creditsMusic))
        {
            PlayMusicStream(creditsMusic);
        }
        if (IsMusicStreamPlaying(menuMusic))
        {
            StopMusicStream(menuMusic);
        }
    }
    else
    {
        if (IsMusicStreamPlaying(menuMusic))
        {
            StopMusicStream(menuMusic);
        }
        if (IsMusicStreamPlaying(creditsMusic))
        {
            StopMusicStream(creditsMusic);
        }
    }

    switch (gameState)
    {
        case MENU:
        {
            // Menu navigation
            if (IsKeyPressed(KEY_UP))
            {
                menuSelection--;
                if (menuSelection < 0) menuSelection = 3;
            }
            if (IsKeyPressed(KEY_DOWN))
            {
                menuSelection++;
                if (menuSelection > 3) menuSelection = 0;
            }

            // Start game, show credits, or exit
            if (IsKeyPressed(KEY_ENTER))
            {
                if (menuSelection == 0 || menuSelection == 1)
                {
                    gameMode = (menuSelection == 0) ? SINGLE_PLAYER : TWO_PLAYER;
                    gameState = PLAYING;

                    // Reset scores
                    player1.score = 0;
                    player2.score = 0;
                    matchTimer = 0;
                    ResetBall();
                }
                else if (menuSelection == 2)
                {
                    // Show credits
                    gameState = CREDITS;
                    creditsScroll = SCREEN_HEIGHT;
                }
                else if (menuSelection == 3)
                {
                    // Exit game
                    shouldExitGame = true;
                }
            }
        } break;

        case PLAYING:
        {
            if (IsKeyPressed(KEY_P))
            {
                pause = !pause;
            }

            // Return to menu on Escape
            if (IsKeyPressed(KEY_ESCAPE))
            {
                gameState = MENU;
                menuSelection = 0;
                break;
            }

            if (!pause)
            {
                // Increment match timer
                matchTimer++;

                // Update particles
                UpdateParticles();

                // Update controls
                UpdatePlayerControls();

                // Update AI if single player
                if (gameMode == SINGLE_PLAYER)
                {
                    UpdateAI();
                }

                // Apply physics to players
                player1.position.x += player1.velocity.x;
                player1.position.y += player1.velocity.y;
                player2.position.x += player2.velocity.x;
                player2.position.y += player2.velocity.y;

                player1.velocity.y += PLAYER_GRAVITY;
                player2.velocity.y += PLAYER_GRAVITY;

                // Clamp max velocity
                if (player1.velocity.y > PLAYER_MAX_VELOCITY_Y)
                    player1.velocity.y = PLAYER_MAX_VELOCITY_Y;
                if (player2.velocity.y > PLAYER_MAX_VELOCITY_Y)
                    player2.velocity.y = PLAYER_MAX_VELOCITY_Y;

                // Player ground collision
                if (player1.position.y + player1.radius >= GROUND_LEVEL)
                {
                    player1.position.y = GROUND_LEVEL - player1.radius;
                    player1.velocity.y = 0;
                    player1.onGround = true;
                }
                else
                {
                    player1.onGround = false;
                }

                if (player2.position.y + player2.radius >= GROUND_LEVEL)
                {
                    player2.position.y = GROUND_LEVEL - player2.radius;
                    player2.velocity.y = 0;
                    player2.onGround = true;
                }
                else
                {
                    player2.onGround = false;
                }

                // Handle score delay timer
                if (scoreDelayTimer > 0)
                {
                    scoreDelayTimer--;
                    if (scoreDelayTimer == 0)
                    {
                        ResetBall();
                    }
                }

                // Update ball physics 
                ball.position.x += ball.velocity.x;
                ball.position.y += ball.velocity.y;
                ball.velocity.y += BALL_GRAVITY;

                // NOTE: Air resistance, turns out not very useful now
                //ball.velocity.x *= BALL_AIR_RESISTANCE;

                // NOTE: ball rotation based on speed
                float speed = sqrtf(ball.velocity.x * ball.velocity.x); // + ball.velocity.y * ball.velocity.y);
                ball.rotation += (speed / ball.radius) * 35.0f;

                // Update trail every 2nd frame
                if (framesCounter % 2 == 0)
                {
                    UpdateBallTrail();
                }

                // Ball wall collision (left and right)
                if (ball.position.x - ball.radius <= 0)
                {
                    ball.position.x = ball.radius;
                    ball.velocity.x *= -BALL_BOUNCE_DAMPING;
                }
                if (ball.position.x + ball.radius >= SCREEN_WIDTH)
                {
                    ball.position.x = SCREEN_WIDTH - ball.radius;
                    ball.velocity.x *= -BALL_BOUNCE_DAMPING;
                }

                // Ball ceiling collision
                if (ball.position.y - ball.radius <= 0)
                {
                    ball.position.y = ball.radius;
                    ball.velocity.y *= -BALL_BOUNCE_DAMPING;
                }

                // Ball-net collision
                Rectangle netRect = {
                    NET_X - NET_WIDTH / 2,
                    GROUND_LEVEL - NET_HEIGHT,
                    NET_WIDTH,
                    NET_HEIGHT
                };

                if (CheckCollisionCircleRec(ball.position, ball.radius, netRect))
                {
                    // Realistic net collision with energy loss
                    ball.velocity.x *= -BALL_BOUNCE_DAMPING;
                    ball.velocity.y *= 0.9f;  // Slight vertical damping on net hit

                    // Push ball out of net
                    if (ball.position.x < NET_X)
                    {
                        ball.position.x = netRect.x - ball.radius;
                    }
                    else
                    {
                        ball.position.x = netRect.x + netRect.width + ball.radius;
                    }
                    PlaySound(fxBallBounce);
                }

                // Ball-Player collision with velocity transfer (skip during score delay)
                if (scoreDelayTimer == 0 && CheckCollisionCircles(ball.position, ball.radius,
                                         player1.position, player1.radius))
                {
                    // Calculate collision normal
                    Vector2 normal = {
                        ball.position.x - player1.position.x,
                        ball.position.y - player1.position.y
                    };

                    // Normalize
                    float length = sqrtf(normal.x * normal.x + normal.y * normal.y);
                    if (length > 0)
                    {
                        normal.x /= length;
                        normal.y /= length;
                    }

                    // Reflect ball velocity with realistic energy retention
                    float dotProduct = ball.velocity.x * normal.x + ball.velocity.y * normal.y;
                    ball.velocity.x = ball.velocity.x - 2 * dotProduct * normal.x;
                    ball.velocity.y = ball.velocity.y - 2 * dotProduct * normal.y;

                    // Apply slight energy loss on collision
                    ball.velocity.x *= 0.95f;
                    ball.velocity.y *= 0.95f;

                    // Add player's velocity influence
                    ball.velocity.x += player1.velocity.x * 0.7f;
                    ball.velocity.y += player1.velocity.y * 0.5f;

                    // Special case: if blob is jumping, add upward boost
                    if (player1.velocity.y < -5.0f)
                    {
                        ball.velocity.y -= 3.0f;
                    }

                    // Clamp ball speed
                    float speed = sqrtf(ball.velocity.x * ball.velocity.x +
                                       ball.velocity.y * ball.velocity.y);
                    if (speed > BALL_MAX_SPEED)
                    {
                        ball.velocity.x = (ball.velocity.x / speed) * BALL_MAX_SPEED;
                        ball.velocity.y = (ball.velocity.y / speed) * BALL_MAX_SPEED;
                    }

                    // Push ball out of blob
                    ball.position.x = player1.position.x + normal.x * (player1.radius + ball.radius);
                    ball.position.y = player1.position.y + normal.y * (player1.radius + ball.radius);

                    PlaySound(fxBallBounce);
                }

                // Ball-Player2 collision (skip during score delay)
                if (scoreDelayTimer == 0 && CheckCollisionCircles(ball.position, ball.radius,
                                         player2.position, player2.radius))
                {
                    // Calculate collision normal
                    Vector2 normal = {
                        ball.position.x - player2.position.x,
                        ball.position.y - player2.position.y
                    };

                    // Normalize
                    float length = sqrtf(normal.x * normal.x + normal.y * normal.y);
                    if (length > 0)
                    {
                        normal.x /= length;
                        normal.y /= length;
                    }

                    // Reflect ball velocity with realistic energy retention
                    float dotProduct = ball.velocity.x * normal.x + ball.velocity.y * normal.y;
                    ball.velocity.x = ball.velocity.x - 2 * dotProduct * normal.x;
                    ball.velocity.y = ball.velocity.y - 2 * dotProduct * normal.y;

                    // Apply slight energy loss on collision
                    ball.velocity.x *= 0.95f;
                    ball.velocity.y *= 0.95f;

                    // Add player's velocity influence
                    ball.velocity.x += player2.velocity.x * 0.7f;
                    ball.velocity.y += player2.velocity.y * 0.5f;

                    // Special case: if blob is jumping, add upward boost
                    if (player2.velocity.y < -5.0f)
                    {
                        ball.velocity.y -= 3.0f;
                    }

                    // Clamp ball speed
                    float speed = sqrtf(ball.velocity.x * ball.velocity.x +
                                       ball.velocity.y * ball.velocity.y);
                    if (speed > BALL_MAX_SPEED)
                    {
                        ball.velocity.x = (ball.velocity.x / speed) * BALL_MAX_SPEED;
                        ball.velocity.y = (ball.velocity.y / speed) * BALL_MAX_SPEED;
                    }

                    // Push ball out of blob
                    ball.position.x = player2.position.x + normal.x * (player2.radius + ball.radius);
                    ball.position.y = player2.position.y + normal.y * (player2.radius + ball.radius);

                    PlaySound(fxBallBounce);
                }

                // Ball ground collision
                if (ball.position.y + ball.radius >= GROUND_LEVEL)
                {
                    // Bounce ball off ground
                    ball.position.y = GROUND_LEVEL - ball.radius;
                    ball.velocity.y *= -BALL_BOUNCE_DAMPING;

                    // Spawn ground particles on impact
                    Vector2 impactPos = { ball.position.x, GROUND_LEVEL };
                    SpawnGroundParticles(impactPos, 15);

                    // Only process scoring if not in delay 
                    if (scoreDelayTimer == 0)
                    {
                        // Determine which side scored
                        if (ball.position.x < NET_X)
                        {
                            // Ball landed on left side, right player scores
                            player2.score++;
                            servingSide = RIGHT;  // Winner gets the serve
                        }
                        else
                        {
                            // Ball landed on right side, left player scores
                            player1.score++;
                            servingSide = LEFT;  // Winner gets the serve
                        }
                        PlaySound(fxScore);

                        // Check win condition
                        if (player1.score >= WIN_SCORE || player2.score >= WIN_SCORE)
                        {
                            gameState = GAMEOVER;
                            PlaySound(fxGameOver);
                        }
                        else
                        {
                            // Start score delay timer instead of immediately resetting
                            scoreDelayTimer = SCORE_DELAY_FRAMES;
                        }
                    }
                }
            }
        } break;

        case GAMEOVER:
        {
            // Return to menu
            if (IsKeyPressed(KEY_ENTER))
            {
                gameState = MENU;
                menuSelection = 0;
                player1.score = 0;
                player2.score = 0;
                matchTimer = 0;
            }
        } break;

        case CREDITS:
        {
            // Scroll credits up
            creditsScroll -= 2.0f;

            if (creditsScroll <= -800) {
                creditsScroll = -800;
            }

            // Return to menu on ESC or ENTER, or when credits finish
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE)) // || creditsScroll < -800)
            {
                gameState = MENU;
                menuSelection = 0;
            }
        } break;
    }
}

// Draw game (one frame)
void DrawGame(void)
{
    BeginDrawing();
    ClearBackground(RAYWHITE);

    // Draw background image
    if (backgroundTexture.id > 0)
    {
        DrawTexture(backgroundTexture, 0, 0, GRAY);
    }

    switch (gameState)
    {
        case MENU:
        {
            DrawMenu();
        } break;

        case PLAYING:
        {
            // Draw ground
            DrawGround();

            // Draw net
            DrawNet();

            // Draw player shadows
            DrawPlayerShadow(player1);
            DrawPlayerShadow(player2);

            // Draw players with borders and highlights
            // Subtle pulsing effect
            float pulse = 0.4f + sinf(framesCounter * 0.05f) * 0.1f;

            // Player 1
            DrawCircleV(player1.position, player1.radius, player1.color);
            DrawCircleLines((int)player1.position.x, (int)player1.position.y, player1.radius, BLACK);
            DrawCircleLines((int)player1.position.x, (int)player1.position.y, player1.radius - 2, Fade(WHITE, 0.3f));
            // Natural highlight with movement
            float offset1X = -player1.radius * 0.35f + player1.velocity.x * 0.5f;
            float offset1Y = -player1.radius * 0.35f - fabsf(player1.velocity.y) * 0.3f;
            Vector2 highlight1 = { player1.position.x + offset1X, player1.position.y + offset1Y };
            DrawCircleGradient((int)highlight1.x, (int)highlight1.y, player1.radius * 0.25f,
                             Fade(WHITE, pulse * 0.8f), Fade(WHITE, 0.0f));

            // Player 2
            DrawCircleV(player2.position, player2.radius, player2.color);
            DrawCircleLines((int)player2.position.x, (int)player2.position.y, player2.radius, BLACK);
            DrawCircleLines((int)player2.position.x, (int)player2.position.y, player2.radius - 2, Fade(WHITE, 0.3f));
            // Natural highlight with movement
            float offset2X = -player2.radius * 0.35f + player2.velocity.x * 0.5f;
            float offset2Y = -player2.radius * 0.35f - fabsf(player2.velocity.y) * 0.3f;
            Vector2 highlight2 = { player2.position.x + offset2X, player2.position.y + offset2Y };
            DrawCircleGradient((int)highlight2.x, (int)highlight2.y, player2.radius * 0.25f,
                             Fade(WHITE, pulse * 0.8f), Fade(WHITE, 0.0f));

            // Draw particles
            DrawParticles();

            // Draw ball with trail and spinning animation
            DrawBallTrail();
            DrawSpinningBall();

            // Draw score
            DrawScore();

            // Draw pause indicator
            if (pause)
            {
                DrawText("PAUSED", SCREEN_WIDTH / 2 - 60, SCREEN_HEIGHT / 2, 40, GRAY);
                DrawText("Press P to continue",
                        SCREEN_WIDTH / 2 - 100, SCREEN_HEIGHT / 2 + 50, 20, LIGHTGRAY);
            }
        } break;

        case GAMEOVER:
        {
            // Draw final state
            DrawGround();
            DrawNet();

            // Draw players with borders and highlights
            // Subtle pulsing effect
            float pulse = 0.4f + sinf(framesCounter * 0.05f) * 0.1f;

            // Player 1
            DrawCircleV(player1.position, player1.radius, player1.color);
            DrawCircleLines((int)player1.position.x, (int)player1.position.y, player1.radius, BLACK);
            DrawCircleLines((int)player1.position.x, (int)player1.position.y, player1.radius - 2, Fade(WHITE, 0.3f));
            // Natural highlight
            Vector2 highlight1 = { player1.position.x - player1.radius * 0.35f, player1.position.y - player1.radius * 0.35f };
            DrawCircleGradient((int)highlight1.x, (int)highlight1.y, player1.radius * 0.25f,
                             Fade(WHITE, pulse * 0.8f), Fade(WHITE, 0.0f));

            // Player 2
            DrawCircleV(player2.position, player2.radius, player2.color);
            DrawCircleLines((int)player2.position.x, (int)player2.position.y, player2.radius, BLACK);
            DrawCircleLines((int)player2.position.x, (int)player2.position.y, player2.radius - 2, Fade(WHITE, 0.3f));
            // Natural highlight
            Vector2 highlight2 = { player2.position.x - player2.radius * 0.35f, player2.position.y - player2.radius * 0.35f };
            DrawCircleGradient((int)highlight2.x, (int)highlight2.y, player2.radius * 0.25f,
                             Fade(WHITE, pulse * 0.8f), Fade(WHITE, 0.0f));

            DrawSpinningBall();
            DrawScore();

            // Winner announcement
            const char *winner = (player1.score >= WIN_SCORE) ?
                                "PLAYER 1 WINS!" : "PLAYER 2 WINS!";
            int winnerWidth = MeasureText(winner, 60);
            DrawText(winner, SCREEN_WIDTH / 2 - winnerWidth / 2,
                    SCREEN_HEIGHT / 2 - 80, 60, GOLD);

            DrawText("Press ENTER to return to menu",
                    SCREEN_WIDTH / 2 - 150, SCREEN_HEIGHT / 2 + 20, 20, LIGHTGRAY);
        } break;

        case CREDITS:
        {
            DrawCredits();
        } break;
    }

    EndDrawing();
}

// Draw ball trail effect
void DrawBallTrail(void)
{
    for (int i = 0; i < ball.trailCount; i++)
    {
        float alpha = 1.0f - ((float)i / TRAIL_LENGTH);
        float radius = ball.radius * (1.0f - ((float)i / TRAIL_LENGTH) * 0.5f);
        DrawCircleV(ball.trail[i], radius, Fade(LIGHTGRAY, alpha * 0.6f));
    }
}

// Draw ball with spinning animation (volleyball pattern)
void DrawSpinningBall(void)
{
    // Draw shadow for depth (bottom-right)
    Vector2 shadowPos = {
        ball.position.x + ball.radius * 0.15f,
        ball.position.y + ball.radius * 0.15f
    };
    DrawCircleV(shadowPos, ball.radius, Fade(BLACK, 0.15f));

    // If ball texture is loaded, use it; otherwise fall back to procedural drawing
    if (ballTexture.id > 0)
    {
        // Draw rotating ball texture
        float diameter = ball.radius * 2.0f;
        Rectangle source = { 0, 0, (float)ballTexture.width, (float)ballTexture.height };
        Rectangle dest = { ball.position.x, ball.position.y, diameter, diameter };
        Vector2 origin = { ball.radius, ball.radius };

        DrawTexturePro(ballTexture, source, dest, origin, ball.rotation, WHITE);
    }
    else
    {
        // Fallback: Draw base sphere with smooth radial gradient for roundness
        Color centerColor = (Color){ 255, 255, 255, 255 };  // Bright white center
        Color edgeColor = (Color){ 255, 140, 60, 255 };     // Orange edge

        // Main ball with gradient
        DrawCircleGradient((int)ball.position.x, (int)ball.position.y, ball.radius, centerColor, edgeColor);

        // Draw rotating stripes to show ball spin
        Color stripeColor = (Color){ 220, 100, 40, 200 };
        int numStripes = 4;

        for (int i = 0; i < numStripes; i++)
        {
            float angle = (ball.rotation + (i * 360.0f / numStripes)) * DEG2RAD;

            // Draw curved stripe using line segments
            int segments = 16;
            for (int seg = 0; seg < segments - 1; seg++)
            {
                float t1 = (float)seg / (segments - 1);
                float t2 = (float)(seg + 1) / (segments - 1);

                // Create curved stripe across the ball
                float curveAngle1 = (t1 - 0.5f) * 160.0f * DEG2RAD;
                float curveAngle2 = (t2 - 0.5f) * 160.0f * DEG2RAD;

                // Rotate the curve based on ball rotation
                float x1 = cosf(angle + curveAngle1) * ball.radius * (0.85f - fabsf(t1 - 0.5f) * 0.4f);
                float y1 = sinf(angle + curveAngle1) * ball.radius * (0.85f - fabsf(t1 - 0.5f) * 0.4f);
                float x2 = cosf(angle + curveAngle2) * ball.radius * (0.85f - fabsf(t2 - 0.5f) * 0.4f);
                float y2 = sinf(angle + curveAngle2) * ball.radius * (0.85f - fabsf(t2 - 0.5f) * 0.4f);

                Vector2 p1 = { ball.position.x + x1, ball.position.y + y1 };
                Vector2 p2 = { ball.position.x + x2, ball.position.y + y2 };

                // Fade stripe at edges for 3D effect
                float alpha = 1.0f - fabsf(t1 - 0.5f) * 1.2f;
                if (alpha > 0)
                {
                    DrawLineEx(p1, p2, 2.5f, Fade(stripeColor, alpha));
                }
            }
        }

        // Add shading on bottom-right for 3D depth
        Vector2 shadePos = {
            ball.position.x + ball.radius * 0.4f,
            ball.position.y + ball.radius * 0.4f
        };
        DrawCircleGradient((int)shadePos.x, (int)shadePos.y, ball.radius * 0.6f,
                           Fade(BLANK, 0.0f), Fade(ORANGE, 0.3f));

        // Add bright highlight for spherical 3D effect (top-left)
        Vector2 highlightPos = {
            ball.position.x - ball.radius * 0.35f,
            ball.position.y - ball.radius * 0.35f
        };
        DrawCircleV(highlightPos, ball.radius * 0.3f, Fade(WHITE, 0.5f));
        DrawCircleV(highlightPos, ball.radius * 0.18f, Fade(WHITE, 0.7f));
        DrawCircleV(highlightPos, ball.radius * 0.08f, Fade(WHITE, 0.9f));

        // Outer rim for definition
        DrawCircleLines((int)ball.position.x, (int)ball.position.y, ball.radius, Fade(ORANGE, 0.3f));
    }
}

// Draw net
void DrawNet(void)
{
    // Draw shadow cast on the ground from the pole
    Vector2 shadowStart = { NET_X + NET_WIDTH / 2, GROUND_LEVEL };
    Vector2 shadowEnd = { NET_X + NET_WIDTH / 2 + 15, GROUND_LEVEL };
    DrawLineEx(shadowStart, shadowEnd, 8.0f, Fade(BLACK, 0.3f));

    // Draw pole shadow on left side for 3D depth
    DrawRectangle(NET_X - NET_WIDTH / 2 - 2,
                 GROUND_LEVEL - NET_HEIGHT,
                 2,
                 NET_HEIGHT,
                 Fade(BLACK, 0.4f));

    // Main net post with gradient for roundness
    DrawRectangleGradientH(NET_X - NET_WIDTH / 2,
                          GROUND_LEVEL - NET_HEIGHT,
                          NET_WIDTH,
                          NET_HEIGHT,
                          GRAY,
                          WHITE);

    // Right edge shadow for cylinder effect
    DrawRectangle(NET_X + NET_WIDTH / 2 - 1,
                 GROUND_LEVEL - NET_HEIGHT,
                 1,
                 NET_HEIGHT,
                 Fade(DARKGRAY, 0.5f));

    // Top cap for the pole
    DrawRectangle(NET_X - NET_WIDTH / 2 - 2,
                 GROUND_LEVEL - NET_HEIGHT - 5,
                 NET_WIDTH + 4,
                 5,
                 ORANGE);

    // Top cap highlight
    DrawRectangle(NET_X - NET_WIDTH / 2 - 2,
                 GROUND_LEVEL - NET_HEIGHT - 5,
                 NET_WIDTH + 4,
                 2,
                 LIGHTGRAY);
}

// Draw score
void DrawScore(void)
{
    // Player 1 score (left side)
    DrawText(TextFormat("%d", player1.score),
             SCREEN_WIDTH / 4 - 20,
             30,
             60,
             BLUE);

    // Player 2 score (right side)
    DrawText(TextFormat("%d", player2.score),
             SCREEN_WIDTH * 3 / 4 - 20,
             30,
             60,
             RED);

    // Separator
    DrawText("-", SCREEN_WIDTH / 2 - 10, 30, 60, LIGHTGRAY);

    // Match timer (convert frames to minutes:seconds)
    int totalSeconds = matchTimer / 60;
    int minutes = totalSeconds / 60;
    int seconds = totalSeconds % 60;
    const char *timerText = TextFormat("%02d:%02d", minutes, seconds);
    int timerWidth = MeasureText(timerText, 30);
    DrawText(timerText, SCREEN_WIDTH / 2 - timerWidth / 2, 100, 30, WHITE);
}

// Draw menu
void DrawMenu(void)
{
    // Title
    const char *title = APP_NAME;
    int titleWidth = MeasureText(title, 60);
    DrawText(title, SCREEN_WIDTH / 2 - titleWidth / 2, 80, 60, WHITE);

    // Menu options
    const char *option1 = "Single Player (vs Computer)";
    const char *option2 = "Two Players (Hotseat)";
    const char *option3 = "Credits";
    const char *option4 = "Exit";

    int opt1Width = MeasureText(option1, 30);
    int opt2Width = MeasureText(option2, 30);
    int opt3Width = MeasureText(option3, 30);
    int opt4Width = MeasureText(option4, 30);

    // Highlight selected option
    Color color1 = (menuSelection == 0) ? RED : GRAY;
    Color color2 = (menuSelection == 1) ? RED : GRAY;
    Color color3 = (menuSelection == 2) ? RED : GRAY;
    Color color4 = (menuSelection == 3) ? RED : GRAY;

    DrawText(option1, SCREEN_WIDTH / 2 - opt1Width / 2, 200, 30, color1);
    DrawText(option2, SCREEN_WIDTH / 2 - opt2Width / 2, 250, 30, color2);
    DrawText(option3, SCREEN_WIDTH / 2 - opt3Width / 2, 300, 30, color3);
    DrawText(option4, SCREEN_WIDTH / 2 - opt4Width / 2, 350, 30, color4);

    // Instructions
    DrawText("Use UP/DOWN to select, ENTER to start",
             SCREEN_WIDTH / 2 - MeasureText("Use UP/DOWN to select, ENTER to start", 20) / 2,
             450, 20, LIGHTGRAY);

    // Controls info
    DrawText("P1: W (jump), A/D (move)", 50, SCREEN_HEIGHT - 60, 16, LIGHTGRAY);
    DrawText("P2: UP (jump), LEFT/RIGHT (move)", 50, SCREEN_HEIGHT - 35, 16, LIGHTGRAY);

    DrawText(COPYRIGHT, SCREEN_WIDTH - MeasureText(COPYRIGHT, 16) - 25, SCREEN_HEIGHT-35, 16, BLACK);
}

// Draw player shadow cast on ground
void DrawPlayerShadow(Player player)
{
    // Shadow position is on the ground, horizontally aligned with player
    Vector2 shadowPos = {
        player.position.x,
        GROUND_LEVEL - player.radius * 0.3f
    };

    // Shadow size and opacity decrease with height
    float height = GROUND_LEVEL - player.position.y - player.radius;
    float shadowScale = 1.0f - (height / 200.0f);
    if (shadowScale < 0.4f) shadowScale = 0.4f;
    if (shadowScale > 1.0f) shadowScale = 1.0f;
    float shadowAlpha = 0.3f * shadowScale;

    DrawEllipse((int)shadowPos.x, (int)shadowPos.y,
               player.radius * shadowScale * 1.2f,
               player.radius * shadowScale * 0.5f,
               Fade(BLACK, shadowAlpha));
}

// Draw ground
void DrawGround(void)
{
    // Draw ground
    DrawRectangle(0, GROUND_LEVEL, SCREEN_WIDTH,
                 SCREEN_HEIGHT - GROUND_LEVEL, DARKBROWN);

    // Draw court line
    DrawLineEx((Vector2){ 0, GROUND_LEVEL },
              (Vector2){ SCREEN_WIDTH, GROUND_LEVEL },
              3.0f, GREEN);

    DrawText(COPYRIGHT, SCREEN_WIDTH - MeasureText(COPYRIGHT, 16) - 25, SCREEN_HEIGHT-35, 16, GRAY);
}

void text_center(const char *text, int y, int fontSize, Color color) {
    int centerX = SCREEN_WIDTH / 2;
    DrawText(text, centerX - MeasureText(text, fontSize) / 2, y, fontSize, color);
}

// Draw scrolling credits
void DrawCredits(void)
{
    int y = (int)creditsScroll;

    // Title
    text_center(APP_NAME, y, 50, WHITE);
    y += 100;

    // Game Credits
    text_center("CODE AND GRAPHICS BY", y, 30, GRAY);
    y += 50;
    text_center("Dmitry R. (dmth)", y, 40, LIGHTGRAY);
    y += 80;

    text_center("POWERED BY", y, 30, DARKGRAY);
    y += 50;
    text_center("raylib", y, 40, MAROON);
    y += 80;

    text_center("SPECIAL THANKS", y, 30, DARKGRAY);
    y += 50;
    text_center("Ramon Santamaria (@raysan5)", y, 25, GRAY);
    y += 50;
    text_center("raylib community", y, 25, GRAY);
    y += 80;

    text_center("INSPIRED BY", y, 30, GRAY);
    y += 50;
    text_center("Arcade Volley, 1989", y, 25, LIGHTGRAY); 
    y += 50;
    text_center("Blobby Volley, 2000", y, 25, LIGHTGRAY);
    y += 80;

    text_center("MUSIC BY", y, 30, GRAY);
    y += 50;
    text_center("Hymn To Aurora (Main Menu) - Fredrik Skogh aka \"Horace Wimp\"", y, 25, LIGHTGRAY);
    y += 50;
    text_center("Space Debris (Credits) - Markus Captain Kaarlonen", y, 25, LIGHTGRAY);

    y += 100;
    text_center("THANK YOU FOR PLAYING!", y, 40, GOLD);
    y += 80;

    text_center(APP_NAME, y, 50, WHITE);
    y += 100;
    text_center("https://falsetrue.io/projects/c-volley/", y, 25, LIGHTGRAY);
    
    y += 100;
    text_center("Press ENTER or ESC to return", y, 20, LIGHTGRAY);
}

// Spawn ground particles on impact
void SpawnGroundParticles(Vector2 position, int count)
{
    for (int i = 0; i < count && i < MAX_PARTICLES; i++)
    {
        // Find an inactive particle slot
        for (int j = 0; j < MAX_PARTICLES; j++)
        {
            if (!particles[j].active)
            {
                particles[j].active = true;
                particles[j].position = position;

                // Random velocity spread (upward and sideways)
                float angle = GetRandomValue(-60, -120) * DEG2RAD;
                float speed = GetRandomValue(2, 6);
                particles[j].velocity.x = cosf(angle) * speed * (GetRandomValue(0, 1) ? 1 : -1);
                particles[j].velocity.y = sinf(angle) * speed;

                // Use ground color with slight variation
                particles[j].color = DARKBROWN;
                particles[j].alpha = 1.0f;
                particles[j].life = 1.0f;

                break;  // Found a slot, move to next particle
            }
        }
    }
}

// Update all active particles
void UpdateParticles(void)
{
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        if (particles[i].active)
        {
            // Apply physics
            particles[i].velocity.y += 0.3f;  // Gravity
            particles[i].position.x += particles[i].velocity.x;
            particles[i].position.y += particles[i].velocity.y;

            // Fade out over time
            particles[i].life -= 0.02f;
            particles[i].alpha = particles[i].life;

            // Deactivate when life runs out or falls below ground
            if (particles[i].life <= 0.0f || particles[i].position.y > GROUND_LEVEL + 20)
            {
                particles[i].active = false;
            }
        }
    }
}

// Draw all active particles
void DrawParticles(void)
{
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        if (particles[i].active)
        {
            // Draw particle as a small circle
            float size = 3.0f * particles[i].life;  
            DrawCircleV(particles[i].position, size,
                       Fade(particles[i].color, particles[i].alpha));
        }
    }
}

void UnloadGame(void)
{
    UnloadSound(fxJump);
    UnloadSound(fxBallBounce);
    UnloadSound(fxScore);
    UnloadSound(fxGameOver);

    UnloadMusicStream(menuMusic);
    UnloadMusicStream(creditsMusic);

    CloseAudioDevice();

    if (backgroundTexture.id > 0)
    {
        UnloadTexture(backgroundTexture);
    }
    if (ballTexture.id > 0)
    {
        UnloadTexture(ballTexture);
    }
}

void UpdateDrawFrame(void)
{
    UpdateGame();
    DrawGame();
}
