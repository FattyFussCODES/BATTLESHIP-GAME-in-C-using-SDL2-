#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h> // Add SDL_image for loading images
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

// Increased window size to fit both grids properly
#define GRID_SIZE 10
#define NUM_SHIPS 5
#define CELL_SIZE 35 // Smaller cells to fit better
#define GRID_OFFSET_X 150
#define GRID_OFFSET_Y 160 // Increased Y offset to make room for background image
#define WINDOW_WIDTH 1200
#define WINDOW_HEIGHT 720
#define MESSAGE_DURATION 2000 // Duration of feedback messages in milliseconds
#define BANNER_HEIGHT 100 // Height of the banner area above the grids

typedef enum { WATER = '~', SHIP = 'S', HIT = 'H', MISS = 'M' } CellState;

typedef struct {
    int size;
    char name[20];
    int hitCount; // Track hits for individual ship tracking
    int isSunk;   // Flag to track if ship is sunk
    int startRow;
    int startCol;
    int isHorizontal;
} Ship;

typedef struct {
    CellState grid[GRID_SIZE][GRID_SIZE];
    Ship ships[NUM_SHIPS];
    int numPlacedShips;
} Board;

typedef struct {
    char text[100];
    SDL_Color color;
    Uint32 startTime;
    int isActive;
} FeedbackMessage;

typedef struct {
    SDL_Window* window;
    SDL_Renderer* renderer;
    TTF_Font* fontLarge;
    TTF_Font* fontMedium;
    TTF_Font* fontSmall;
    TTF_Font* fontTitle; // New larger font for the title
    SDL_Texture* waterTexture;
    SDL_Texture* shipTexture;
    SDL_Texture* hitTexture;
    SDL_Texture* missTexture;
    SDL_Texture* backgroundTexture; // New texture for background image
    int currentShipIndex;
    int isPlacingShips;
    int isHorizontal;
    int gameOver;
    int playerWon;
    int playerTurn;
    FeedbackMessage message;
} GameState;

void initBoard(Board* board) {
    for (int i = 0; i < GRID_SIZE; i++) {
        for (int j = 0; j < GRID_SIZE; j++) {
            board->grid[i][j] = WATER;
        }
    }
    board->numPlacedShips = 0;
}

int isValidCoord(int row, int col) {
    return row >= 0 && row < GRID_SIZE && col >= 0 && col < GRID_SIZE;
}

int canPlaceShip(Board* board, int row, int col, int size, int horizontal) {
    if (horizontal) {
        if (col + size > GRID_SIZE) return 0;
        for (int i = 0; i < size; i++)
            if (board->grid[row][col + i] != WATER) return 0;
    } else {
        if (row + size > GRID_SIZE) return 0;
        for (int i = 0; i < size; i++)
            if (board->grid[row + i][col] != WATER) return 0;
    }
    return 1;
}

void placeShip(Board* board, int row, int col, int size, int horizontal, int shipIndex) {
    if (shipIndex < 0 || shipIndex >= NUM_SHIPS) return;
    
    // Store ship details for tracking
    board->ships[shipIndex].startRow = row;
    board->ships[shipIndex].startCol = col;
    board->ships[shipIndex].isHorizontal = horizontal;
    board->ships[shipIndex].hitCount = 0;
    board->ships[shipIndex].isSunk = 0;
    
    if (horizontal) {
        for (int i = 0; i < size; i++)
            board->grid[row][col + i] = SHIP;
    } else {
        for (int i = 0; i < size; i++)
            board->grid[row + i][col] = SHIP;
    }
    
    board->numPlacedShips++;
}

void placeBotShips(Board* board, Ship* shipTemplates) {
    for (int i = 0; i < NUM_SHIPS; i++) {
        // Copy ship data from template
        board->ships[i].size = shipTemplates[i].size;
        strcpy(board->ships[i].name, shipTemplates[i].name);
        board->ships[i].hitCount = 0;
        board->ships[i].isSunk = 0;
        
        int placed = 0;
        while (!placed) {
            int row = rand() % GRID_SIZE;
            int col = rand() % GRID_SIZE;
            int horizontal = rand() % 2;
            if (canPlaceShip(board, row, col, shipTemplates[i].size, horizontal)) {
                placeShip(board, row, col, shipTemplates[i].size, horizontal, i);
                placed = 1;
            }
        }
    }
}

// Returns ship index if sunk, -1 otherwise
int checkShipSunk(Board* board, int row, int col) {
    // For each ship, check if this hit was the final one
    for (int i = 0; i < board->numPlacedShips; i++) {
        if (board->ships[i].isSunk) continue; // Skip already sunk ships
        
        // Calculate ship positions
        int startRow = board->ships[i].startRow;
        int startCol = board->ships[i].startCol;
        int size = board->ships[i].size;
        int horizontal = board->ships[i].isHorizontal;
        
        // Check if hit was on this ship
        int isOnShip = 0;
        if (horizontal) {
            if (row == startRow && col >= startCol && col < startCol + size)
                isOnShip = 1;
        } else {
            if (col == startCol && row >= startRow && row < startRow + size)
                isOnShip = 1;
        }
        
        if (isOnShip) {
            board->ships[i].hitCount++;
            
            // Check if ship is sunk
            if (board->ships[i].hitCount >= size) {
                board->ships[i].isSunk = 1;
                return i; // Return ship index
            }
            break;
        }
    }
    return -1;
}

int attack(Board* board, int row, int col, char* feedbackMsg, SDL_Color* msgColor) {
    if (board->grid[row][col] == SHIP) {
        board->grid[row][col] = HIT;
        
        // Check if a ship was sunk
        int sunkShipIndex = checkShipSunk(board, row, col);
        if (sunkShipIndex >= 0) {
            sprintf(feedbackMsg, "%s sunk!", board->ships[sunkShipIndex].name);
            *msgColor = (SDL_Color){255, 0, 0, 255}; // Red for sunk ship
        } else {
            sprintf(feedbackMsg, "Hit!");
            *msgColor = (SDL_Color){255, 140, 0, 255}; // Orange for hit
        }
        
        return 1;
    } else if (board->grid[row][col] == WATER) {
        board->grid[row][col] = MISS;
        sprintf(feedbackMsg, "Miss!");
        *msgColor = (SDL_Color){30, 30, 150, 255}; // Blue for miss
    }
    return 0;
}

int allShipsSunk(Board* board) {
    for (int i = 0; i < GRID_SIZE; i++)
        for (int j = 0; j < GRID_SIZE; j++)
            if (board->grid[i][j] == SHIP) return 0;
    return 1;
}

SDL_Texture* createTextTexture(SDL_Renderer* renderer, TTF_Font* font, const char* text, SDL_Color color) {
    if (!font || !renderer) return NULL;
    
    SDL_Surface* surface = TTF_RenderText_Solid(font, text, color);
    if (!surface) return NULL;
    
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    return texture;
}

void renderText(SDL_Renderer* renderer, TTF_Font* font, const char* text, int x, int y, SDL_Color color) {
    if (!font || !renderer || !text) return;
    
    SDL_Surface* surface = TTF_RenderText_Solid(font, text, color);
    if (!surface) return;
    
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) {
        SDL_FreeSurface(surface);
        return;
    }
    
    SDL_Rect rect;
    rect.x = x;
    rect.y = y;
    rect.w = surface->w;
    rect.h = surface->h;
    
    SDL_RenderCopy(renderer, texture, NULL, &rect);
    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}

SDL_Texture* loadTexture(SDL_Renderer* renderer, SDL_Color color) {
    if (!renderer) return NULL;
    
    SDL_Surface* surface = SDL_CreateRGBSurface(0, CELL_SIZE, CELL_SIZE, 32, 0, 0, 0, 0);
    if (!surface) return NULL;
    
    SDL_FillRect(surface, NULL, SDL_MapRGB(surface->format, color.r, color.g, color.b));
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    return texture;
}

// New function to load image textures
SDL_Texture* loadImageTexture(SDL_Renderer* renderer, const char* path) {
    if (!renderer || !path) return NULL;
    
    SDL_Surface* surface = IMG_Load(path);
    if (!surface) {
        printf("Failed to load image %s! SDL_image Error: %s\n", path, IMG_GetError());
        return NULL;
    }
    
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) {
        printf("Failed to create texture from %s! SDL Error: %s\n", path, SDL_GetError());
    }
    
    SDL_FreeSurface(surface);
    return texture;
}

void drawGrid(SDL_Renderer* renderer, Board* board, int x, int y, int hideShips, 
              SDL_Texture* waterTex, SDL_Texture* shipTex, SDL_Texture* hitTex, SDL_Texture* missTex, TTF_Font* font) {
    if (!renderer || !board) return;

    // Draw grid labels
    SDL_Color black = {0, 0, 0, 255};
    
    // Draw column labels (A-J)
    for (int i = 0; i < GRID_SIZE; i++) {
        char label[2] = {(char)('A' + i), '\0'};
        if (font) {
            renderText(renderer, font, label, x + i * CELL_SIZE + CELL_SIZE/2 - 5, y - 25, black);
        }
    }
    
    // Draw row labels (1-10)
    for (int i = 0; i < GRID_SIZE; i++) {
        char label[3];
        sprintf(label, "%d", i + 1);
        if (font) {
            renderText(renderer, font, label, x - 20, y + i * CELL_SIZE + CELL_SIZE/2 - 10, black);
        }
    }

    // Draw grid cells
    for (int i = 0; i < GRID_SIZE; i++) {
        for (int j = 0; j < GRID_SIZE; j++) {
            SDL_Rect cellRect = { x + j * CELL_SIZE, y + i * CELL_SIZE, CELL_SIZE, CELL_SIZE };
            
            // Fill cell with appropriate texture based on state
            CellState cellState = board->grid[i][j];
            if (hideShips && cellState == SHIP) {
                cellState = WATER;
            }
            
            SDL_Texture* currentTexture = NULL;
            switch (cellState) {
                case WATER:
                    currentTexture = waterTex;
                    break;
                case SHIP:
                    currentTexture = shipTex;
                    break;
                case HIT:
                    currentTexture = hitTex;
                    break;
                case MISS:
                    currentTexture = missTex;
                    break;
            }
            
            if (currentTexture) {
                SDL_RenderCopy(renderer, currentTexture, NULL, &cellRect);
            } else {
                // Fallback if texture is missing - use colors directly
                switch (cellState) {
                    case WATER:
                        SDL_SetRenderDrawColor(renderer, 100, 150, 255, 255);
                        break;
                    case SHIP:
                        SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
                        break;
                    case HIT:
                        SDL_SetRenderDrawColor(renderer, 255, 80, 80, 255);
                        break;
                    case MISS:
                        SDL_SetRenderDrawColor(renderer, 200, 200, 255, 255);
                        break;
                }
                SDL_RenderFillRect(renderer, &cellRect);
            }
            
            // Draw grid borders
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderDrawRect(renderer, &cellRect);
        }
    }
}

void drawShipIndicators(SDL_Renderer* renderer, TTF_Font* font, Board* board, int x, int y, int isPlayerBoard) {
    if (!renderer || !font || !board) return;
    
    SDL_Color textColor = {0, 0, 0, 255};
    SDL_Color statusColor;
    char statusText[50];
    
    // Draw ship status indicators
    for (int i = 0; i < board->numPlacedShips; i++) {
        if (board->ships[i].isSunk) {
            statusColor = (SDL_Color){255, 0, 0, 255}; // Red for sunk
            sprintf(statusText, "%s - SUNK", board->ships[i].name);
        } else if (isPlayerBoard) {
            statusColor = (SDL_Color){0, 100, 0, 255}; // Green for active
            sprintf(statusText, "%s - ACTIVE", board->ships[i].name);
        } else {
            statusColor = (SDL_Color){50, 50, 50, 255}; // Gray for enemy ships
            sprintf(statusText, "%s - ?", board->ships[i].name);
        }
        
        renderText(renderer, font, statusText, x, y + i * 25, statusColor);
    }
}

int initGameState(GameState* state) {
    if (!state) return 0;
    
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 0;
    }
    
    // Initialize SDL_ttf
    if (TTF_Init() < 0) {
        printf("SDL_ttf could not initialize! TTF_Error: %s\n", TTF_GetError());
        SDL_Quit();
        return 0;
    }
    
    // Initialize SDL_image
    int imgFlags = IMG_INIT_PNG | IMG_INIT_JPG;
    if (!(IMG_Init(imgFlags) & imgFlags)) {
        printf("SDL_image could not initialize! IMG_Error: %s\n", IMG_GetError());
        TTF_Quit();
        SDL_Quit();
        return 0;
    }
    
    // Create window
    state->window = SDL_CreateWindow("Battleship", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                    WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    if (!state->window) {
        fprintf(stderr,"Window could not be created! SDL_Error: %s\n", SDL_GetError());
        IMG_Quit();
        TTF_Quit();
        SDL_Quit();
        return 0;
    }
    
    // Create renderer
    state->renderer = SDL_CreateRenderer(state->window, -1, SDL_RENDERER_SOFTWARE);
    if (!state->renderer) {
        printf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(state->window);
        IMG_Quit();
        TTF_Quit();
        SDL_Quit();
        return 0;
    }
    
    // Try to load fonts in different sizes
    state->fontLarge = TTF_OpenFont("/fonts/ARIAL.ttf", 28);
    state->fontMedium = TTF_OpenFont("/fonts/ARIAL.ttf", 22);
    state->fontSmall = TTF_OpenFont("/fonts/ARIAL.ttf", 18);
    state->fontTitle = TTF_OpenFont("/fonts/ARIAL.ttf", 48); // Larger title font
    
    if (!state->fontMedium) {
        // Try alternate fonts or built-in fonts
        state->fontLarge = TTF_OpenFont("/home/aviral/Desktop/c/fonts/FreeSans.ttf", 28);
        state->fontMedium = TTF_OpenFont("/home/aviral/Desktop/c/fonts/FreeSans.ttf", 22);
        state->fontSmall = TTF_OpenFont("/home/aviral/Desktop/c/fonts/FreeSans.ttf", 18);
        state->fontTitle = TTF_OpenFont("/home/aviral/Desktop/c/fonts/FreeSans.ttf", 48);
        
        if (!state->fontMedium) {
            printf("Warning: Font could not be loaded! TTF_Error: %s\n", TTF_GetError());
            printf("Continuing without text rendering...\n");
            // We'll continue without a font
        }
    }
    
    // Create textures for different cell states
    SDL_Color waterColor = {100, 150, 255, 255};  // Light blue for water
    SDL_Color shipColor = {80, 80, 80, 255};      // Gray for ships
    SDL_Color hitColor = {255, 80, 80, 255};      // Red for hits
    SDL_Color missColor = {200, 200, 255, 255};   // Light color for misses
    
    state->waterTexture = loadTexture(state->renderer, waterColor);
    state->shipTexture = loadTexture(state->renderer, shipColor);
    state->hitTexture = loadTexture(state->renderer, hitColor);
    state->missTexture = loadTexture(state->renderer, missColor);
    
    // Load background image
    state->backgroundTexture = loadImageTexture(state->renderer, "ocean_background.jpg");
    if (!state->backgroundTexture) {
        printf("Warning: Background image could not be loaded! Creating a default background.\n");
        // Create a default background texture
        SDL_Surface* bgSurface = SDL_CreateRGBSurface(0, WINDOW_WIDTH, BANNER_HEIGHT, 32, 0, 0, 0, 0);
        if (bgSurface) {
            // Create a gradient-like background
            for (int y = 0; y < BANNER_HEIGHT; y++) {
                SDL_Rect line = {0, y, WINDOW_WIDTH, 1};
                // Gradient from dark blue to lighter blue
                int blueValue = 120 + y / 3;
                if (blueValue > 220) blueValue = 220;
                SDL_FillRect(bgSurface, &line, SDL_MapRGB(bgSurface->format, 0, 50 + y/4, blueValue));
            }
            state->backgroundTexture = SDL_CreateTextureFromSurface(state->renderer, bgSurface);
            SDL_FreeSurface(bgSurface);
        }
    }
    
    // Check if textures were created successfully
    if (!state->waterTexture || !state->shipTexture || !state->hitTexture || !state->missTexture) {
        printf("Warning: Some textures could not be created. Using direct color rendering.\n");
    }
    
    // Game state initialization
    state->currentShipIndex = 0;
    state->isPlacingShips = 1;
    state->isHorizontal = 1;
    state->gameOver = 0;
    state->playerWon = 0;
    state->playerTurn = 1;
    
    // Initialize feedback message
    state->message.isActive = 0;
    
    return 1;
}

void cleanupGameState(GameState* state) {
    if (!state) return;
    
    // Free textures if they exist
    if (state->waterTexture) SDL_DestroyTexture(state->waterTexture);
    if (state->shipTexture) SDL_DestroyTexture(state->shipTexture);
    if (state->hitTexture) SDL_DestroyTexture(state->hitTexture);
    if (state->missTexture) SDL_DestroyTexture(state->missTexture);
    if (state->backgroundTexture) SDL_DestroyTexture(state->backgroundTexture);
    
    // Close fonts if they exist
    if (state->fontLarge) TTF_CloseFont(state->fontLarge);
    if (state->fontMedium) TTF_CloseFont(state->fontMedium);
    if (state->fontSmall) TTF_CloseFont(state->fontSmall);
    if (state->fontTitle) TTF_CloseFont(state->fontTitle);
    
    // Destroy renderer and window
    if (state->renderer) SDL_DestroyRenderer(state->renderer);
    if (state->window) SDL_DestroyWindow(state->window);
    
    // Quit SDL subsystems
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
}

void showFeedbackMessage(GameState* state, const char* message, SDL_Color color) {
    if (!state) return;
    
    strncpy(state->message.text, message, sizeof(state->message.text) - 1);
    state->message.color = color;
    state->message.startTime = SDL_GetTicks();
    state->message.isActive = 1;
}

void handleMouseClick(int x, int y, Board* playerBoard, Board* botBoard, GameState* state, Ship* shipTemplates) {
    if (!playerBoard || !botBoard || !state || !shipTemplates) return;
    
    // Calculate grid coordinates for left (player) grid
    int playerGridLeft = GRID_OFFSET_X;
    int playerGridTop = GRID_OFFSET_Y;
    int playerGridRight = playerGridLeft + GRID_SIZE * CELL_SIZE;
    int playerGridBottom = playerGridTop + GRID_SIZE * CELL_SIZE;
    
    // Calculate grid coordinates for right (bot) grid
    int botGridLeft = WINDOW_WIDTH / 2 + 50;
    int botGridTop = GRID_OFFSET_Y;
    int botGridRight = botGridLeft + GRID_SIZE * CELL_SIZE;
    int botGridBottom = botGridTop + GRID_SIZE * CELL_SIZE;
    
    // Check if click is in player's grid
    if (x >= playerGridLeft && x < playerGridRight && y >= playerGridTop && y < playerGridBottom && state->isPlacingShips) {
        int gridX = (x - playerGridLeft) / CELL_SIZE;
        int gridY = (y - playerGridTop) / CELL_SIZE;
        
        if (canPlaceShip(playerBoard, gridY, gridX, shipTemplates[state->currentShipIndex].size, state->isHorizontal)) {
            // Copy ship details
            playerBoard->ships[state->currentShipIndex].size = shipTemplates[state->currentShipIndex].size;
            strcpy(playerBoard->ships[state->currentShipIndex].name, shipTemplates[state->currentShipIndex].name);
            
            placeShip(playerBoard, gridY, gridX, shipTemplates[state->currentShipIndex].size, state->isHorizontal, state->currentShipIndex);
            
            char message[100];
            sprintf(message, "%s placed!", shipTemplates[state->currentShipIndex].name);
            SDL_Color green = {0, 150, 0, 255};
            showFeedbackMessage(state, message, green);
            
            state->currentShipIndex++;
            
            if (state->currentShipIndex >= NUM_SHIPS) {
                state->isPlacingShips = 0;
                placeBotShips(botBoard, shipTemplates);
                showFeedbackMessage(state, "All ships placed! Attack the enemy fleet!", (SDL_Color){0, 100, 150, 255});
            }
        } else {
            showFeedbackMessage(state, "Cannot place ship there!", (SDL_Color){255, 0, 0, 255});
        }
    }
    // Check if click is in bot's grid and it's player's turn
    else if (x >= botGridLeft && x < botGridRight && y >= botGridTop && y < botGridBottom && !state->isPlacingShips && state->playerTurn && !state->gameOver) {
        int gridX = (x - botGridLeft) / CELL_SIZE;
        int gridY = (y - botGridTop) / CELL_SIZE;
        
        CellState cell = botBoard->grid[gridY][gridX];
        if (cell != HIT && cell != MISS) {
            char feedbackMsg[100];
            SDL_Color msgColor;
            
            if (attack(botBoard, gridY, gridX, feedbackMsg, &msgColor)) {
                showFeedbackMessage(state, feedbackMsg, msgColor);
                
                if (allShipsSunk(botBoard)) {
                    state->gameOver = 1;
                    state->playerWon = 1;
                    showFeedbackMessage(state, "You Win! All enemy ships sunk!", (SDL_Color){0, 150, 0, 255});
                }
            } else {
                showFeedbackMessage(state, feedbackMsg, msgColor);
                state->playerTurn = 0;  // Switch to bot's turn after miss
            }
        } else {
            showFeedbackMessage(state, "Already attacked there!", (SDL_Color){150, 150, 0, 255});
        }
    }
}

void botTurn(Board* playerBoard, GameState* state) {
    if (!playerBoard || !state) return;
    
    if (!state->playerTurn && !state->gameOver) {
        // Add delay for bot thinking
        SDL_Delay(600);
        
        int row, col;
        do {
            row = rand() % GRID_SIZE;
            col = rand() % GRID_SIZE;
        } while (playerBoard->grid[row][col] == HIT || playerBoard->grid[row][col] == MISS);
        
        char feedbackMsg[100];
        char coordStr[4];
        sprintf(coordStr, "%c%d", 'A' + col, row + 1);
        
        SDL_Color msgColor;
        char botMessage[100];
        
        if (attack(playerBoard, row, col, feedbackMsg, &msgColor)) {
            sprintf(botMessage, "Enemy attacks %s - %s", coordStr, feedbackMsg);
            showFeedbackMessage(state, botMessage, msgColor);
            
            if (allShipsSunk(playerBoard)) {
                state->gameOver = 1;
                state->playerWon = 0;
                showFeedbackMessage(state, "You Lose! All your ships sunk!", (SDL_Color){150, 0, 0, 255});
            }
        } else {
            sprintf(botMessage, "Enemy attacks %s - %s", coordStr, feedbackMsg);
            showFeedbackMessage(state, botMessage, msgColor);
            state->playerTurn = 1;  // Switch back to player's turn after miss
        }
    }
}

void render(SDL_Renderer* renderer, Board* playerBoard, Board* botBoard, GameState* state, Ship* ships) {
    if (!renderer || !playerBoard || !botBoard || !state || !ships) return;
    
    // Clear the screen
    SDL_SetRenderDrawColor(renderer, 230, 230, 230, 255);
    SDL_RenderClear(renderer);
    
    // Draw background image at the top
    if (state->backgroundTexture) {
        SDL_Rect backgroundRect = {0, 0, WINDOW_WIDTH, BANNER_HEIGHT};
        SDL_RenderCopy(renderer, state->backgroundTexture, NULL, &backgroundRect);
    }
    
    // Title - larger and positioned above the background
    SDL_Color titleColor = {255, 255, 255, 255}; // White title for contrast against background
    if (state->fontTitle) {
        renderText(renderer, state->fontTitle, "BATTLESHIP", WINDOW_WIDTH / 2 - 150, 30, titleColor);
    }
    
    // Calculate grid positions - properly centered
    int playerGridX = GRID_OFFSET_X;
    int botGridX = WINDOW_WIDTH / 2 + 50;
    int gridY = GRID_OFFSET_Y;
    
    // Draw both grids
    drawGrid(renderer, playerBoard, playerGridX, gridY, 0, 
             state->waterTexture, state->shipTexture, state->hitTexture, state->missTexture, state->fontSmall);
    drawGrid(renderer, botBoard, botGridX, gridY, 1,
             state->waterTexture, state->shipTexture, state->hitTexture, state->missTexture, state->fontSmall);
    
    // Labels for grids
    if (state->fontMedium) {
        renderText(renderer, state->fontMedium, "Your Fleet", playerGridX + (GRID_SIZE * CELL_SIZE / 2) - 50, gridY - 50, titleColor);
        renderText(renderer, state->fontMedium, "Enemy Fleet", botGridX + (GRID_SIZE * CELL_SIZE / 2) - 50, gridY - 50, titleColor);
    }
    
    // Draw ship status indicators
    if (state->fontSmall) {
        drawShipIndicators(renderer, state->fontSmall, playerBoard, 50, gridY + GRID_SIZE * CELL_SIZE + 20, 1);
        drawShipIndicators(renderer, state->fontSmall, botBoard, botGridX, gridY + GRID_SIZE * CELL_SIZE + 20,1);
    }
    if (state->fontMedium) {
        SDL_Color instructionColor = {30, 30, 150, 255};
        if (state->isPlacingShips) {
            char instruction[100];
            sprintf(instruction, "Place your %s (%d cells) - %s", 
                    ships[state->currentShipIndex].name, 
                    ships[state->currentShipIndex].size,
                    state->isHorizontal ? "Horizontal" : "Vertical");
            renderText(renderer, state->fontMedium, instruction, 50, WINDOW_HEIGHT - 60, instructionColor);
            renderText(renderer, state->fontMedium, "Press SPACE to rotate ship", 50, WINDOW_HEIGHT - 30, instructionColor);
        } else if (state->gameOver) {
            SDL_Color resultColor = state->playerWon ? (SDL_Color){0, 150, 0, 255} : (SDL_Color){150, 0, 0, 255};
            renderText(renderer, state->fontLarge, state->playerWon ? "You Win!" : "Bot Wins!", WINDOW_WIDTH / 2 - 60, WINDOW_HEIGHT - 60, resultColor);
            renderText(renderer, state->fontMedium, "Press ESCAPE to exit", WINDOW_WIDTH / 2 - 80, WINDOW_HEIGHT - 30, titleColor);
        } else {
            renderText(renderer, state->fontMedium, state->playerTurn ? 
                      "Your turn - Click on enemy grid to attack" : 
                      "Bot is thinking...", 
                      WINDOW_WIDTH / 2 - 150, WINDOW_HEIGHT - 30, instructionColor);
        }
    }
    
    // Render active feedback message
    if (state->message.isActive) {
        Uint32 currentTime = SDL_GetTicks();
        if (currentTime - state->message.startTime < MESSAGE_DURATION) {
            if (state->fontMedium) {
                renderText(renderer, state->fontMedium, state->message.text, 
                          WINDOW_WIDTH / 2 - 100, WINDOW_HEIGHT - 90, 
                          state->message.color);
            }
        } else {
            state->message.isActive = 0;
        }
    }
    
    // Present the render
    SDL_RenderPresent(renderer);
}

int main(int argc, char* argv[]) {
    srand(time(NULL));
    printf("Starting Battleship game...\n");
    
    // Game setup
    Board playerBoard, botBoard;
    initBoard(&playerBoard);
    initBoard(&botBoard);
    
    Ship ships[NUM_SHIPS] = {
        {5, "Carrier"},
        {4, "Battleship"},
        {3, "Cruiser"},
        {3, "Submarine"},
        {2, "Destroyer"}
    };
    
    // Initialize SDL and game state
    GameState state;
    if (!initGameState(&state)) {
        printf("Failed to initialize game. Exiting.\n");
        return 1;
    }
    
    printf("Game initialized successfully. Starting main loop.\n");
    
    // Main game loop
    int running = 1;
    SDL_Event event;
    
    while (running) {
        // Handle events
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = 0;
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        handleMouseClick(event.button.x, event.button.y, &playerBoard, &botBoard, &state, ships);
                    }
                    break;
                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_SPACE && state.isPlacingShips) {
                        state.isHorizontal = !state.isHorizontal;
                    } else if (event.key.keysym.sym == SDLK_ESCAPE) {
                        running = 0;
                    }
                    break;
            }
        }
        
        // Bot's turn logic
        botTurn(&playerBoard, &state);
        
        // Render game
        render(state.renderer, &playerBoard, &botBoard, &state, ships);
        
        // Control frame rate
        SDL_Delay(16);  // ~60 FPS
    }
    
    // Cleanup
    cleanupGameState(&state);
    printf("Game closed normally.\n");
    
    return 0;
}
