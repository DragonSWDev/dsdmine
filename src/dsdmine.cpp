/*
Copyright 2023 DragonSWDev

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <SDL.h>
//#include <SDL_image.h>

#include <iostream>
#include <random>
#include <chrono>
#include <filesystem>
#include <string>

#include "SDL_render.h"
#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_sdlrenderer.h"

#include "mini/ini.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#define GAME_VERSION "1.0"

#define TILE_SIZE 16
#define FACE_SIZE 24
#define DISPLAY_WIDTH 13
#define DISPLAY_HEIGHT 23

#if !SDL_VERSION_ATLEAST(2,0,17)
#error This backend requires SDL 2.0.17+ because of SDL_RenderGeometry() function
#endif

#if defined(WIN32) || defined(_WIN32)
	#define PATH_SEPARATOR "\\"
#else
	#define PATH_SEPARATOR "/"
#endif

enum FaceState { NORMAL, NORMAL_CLICK, FIELD_CLICK, GAME_WON, GAME_LOST };
enum GameMode { BEGINNER, ADVANCED, EXPERT, CUSTOM };
enum WindowType { CUSTOM_GAME, BEST_SCORES, ABOUT, NEW_TIME };
enum GameState { INITIALIZED, STARTED, WON, LOST };

struct FieldType
{
	bool isVisible; //Field visible
	bool isMine; //Field with mine
	bool isFlag; //Field with flag
	bool isUnknown; //Unknown field
	bool isClicked; //Field that user clicks (draw pushed button instead of normal)
	int mineCount; //Mine count around field (should be 0 if field is mine or with no mines around)
};

struct BestTimes
{
	std::string playerName;
	int bestTime;
};

struct StbImage
{
    unsigned char* pixels;
    int width;
    int height;
    int channels;
};

GameMode gameMode = GameMode::BEGINNER;
GameState gameState = GameState::INITIALIZED;
bool marksEnabled = true;
FieldType **fieldArray = NULL;

int fieldWidth, fieldHeight, fieldMines, windowWidth, windowHeight, gameTime, flagCount;
std::mt19937 randomEngine;

//Get random value in range
int getRandomNumber(int min, int max)
{
	return std::uniform_int_distribution<int>{min, max}(randomEngine);
}

//Draw display textures with provided values (get width to put right display in right border of the window)
void drawDisplay(SDL_Renderer* renderer, SDL_Texture* displayTexture, int time, int flags, int width)
{
	if (time > 999)
	{
		time = 999;
	}

	if (flags > 999)
	{
		flags = 999;
	}

	if (flags < -99)
	{
		flags = -99;
	}

	std::string valueStr;
	int valueStrSize;

	//Time display conversion and drawing
	valueStr = std::to_string(time);

	//Fill with zeros if number has less than three digits
	valueStrSize = valueStr.size();

	if (valueStrSize < 3)
	{
		for (int i = 0; i < 3 - valueStrSize; i++)
		{
			valueStr = "0" + valueStr;
		}
	}

	for (int i = 0; i < valueStr.size(); i++) {
		SDL_Rect srcRect, dstRect;
		srcRect.w = DISPLAY_WIDTH;
		srcRect.h = DISPLAY_HEIGHT;
		srcRect.x = 0;
		srcRect.y = (valueStr[i] - 48) * 23; //48 is ASCII for 0

		dstRect.x = 5 + 13 * i;
		dstRect.y = 20;
		dstRect.w = DISPLAY_WIDTH;
		dstRect.h = DISPLAY_HEIGHT;

		SDL_RenderCopy(renderer, displayTexture, &srcRect, &dstRect);
	}

	//Flags display conversion and drawing
	valueStr = std::to_string(flagCount);

	if (flags >= 0)
	{
		valueStrSize = valueStr.size();

		//Fill with zeros if number has less than three digits (only for positive numbers)
		if (valueStrSize < 3)
		{
			for (int i = 0; i < 3 - valueStrSize; i++)
			{
				valueStr = "0" + valueStr;
			}
		}
	}
	else if (flags < 0 && flags > -10)
	{
		valueStr = std::to_string(flagCount * (-1)); //Change flag count to positive number and convert again
		valueStr = "-0" + valueStr;
	}

	for (int i = 0; i < valueStr.size(); i++) {
		SDL_Rect srcRect, dstRect;
		srcRect.w = DISPLAY_WIDTH;
		srcRect.h = DISPLAY_HEIGHT;
		srcRect.x = 0;

		if (valueStr[i] != '-')
		{
			srcRect.y = (valueStr[i] - 48) * 23; //48 is ASCII for 0
		}
		else
		{
			srcRect.y = 10 * 23;
		}

		dstRect.x = (width - 39 - 5) + 13 * i;
		dstRect.y = 20;
		dstRect.w = DISPLAY_WIDTH;
		dstRect.h = DISPLAY_HEIGHT;

		SDL_RenderCopy(renderer, displayTexture, &srcRect, &dstRect);
	}
}

//Draw face on status bar
void drawFace(SDL_Renderer* renderer, SDL_Texture* faceTexture, FaceState state)
{
	SDL_Rect srcRect, dstRect;

	//Face
	srcRect.w = FACE_SIZE;
	srcRect.h = FACE_SIZE;
	srcRect.x = 0;

	//Get face texture to draw
	//State enum order is the same as face texture tiles order
	srcRect.y = state * srcRect.h;

	dstRect.w = FACE_SIZE;
	dstRect.h = FACE_SIZE;
	dstRect.y = 20;

	//Window with is 16 (field tile width) * field width + 10 (5px margin on each side)
	//Point (0, 0) is top left corner so substract half of the width to make it centered
	dstRect.x = (16 * fieldWidth + 10) / 2 - (srcRect.w / 2);

	SDL_RenderCopy(renderer, faceTexture, &srcRect, &dstRect);
}

//Draw mine field
void drawField(SDL_Renderer* renderer, SDL_Texture* fieldTexture)
{
	SDL_Rect srcRect, dstRect;

	srcRect.w = TILE_SIZE;
	srcRect.h = TILE_SIZE;
	srcRect.x = 0;

	dstRect.w = TILE_SIZE;
	dstRect.h = TILE_SIZE;

	for (int row = 0; row < fieldHeight; row++)
	{
		for (int col = 0; col < fieldWidth; col++)
		{
			if (!fieldArray[row][col].isVisible && fieldArray[row][col].isClicked) //Clicked hidden tile without mark
			{
				srcRect.y = 1 * TILE_SIZE;
			}
			else if (fieldArray[row][col].isVisible && !fieldArray[row][col].isMine && fieldArray[row][col].mineCount == 0) //Visible empty tile (same as pushed tile)
			{
				srcRect.y = 1 * TILE_SIZE;
			}
			else if (fieldArray[row][col].isFlag //Tile with flag (visible on all tiles if game is running or on tiles with mines if game ended)
					&& (gameState != GameState::LOST || (gameState == GameState::LOST && fieldArray[row][col].isMine)))
			{
					srcRect.y = 2 * TILE_SIZE;
			}
			else if (!fieldArray[row][col].isVisible && fieldArray[row][col].isUnknown && !fieldArray[row][col].isClicked) //Hidden tile with question mark
			{
				srcRect.y = 3 * TILE_SIZE;
			}
			else if (!fieldArray[row][col].isVisible && fieldArray[row][col].isUnknown && fieldArray[row][col].isClicked) //Clicked hidden tile with question amrk
			{
				srcRect.y = 4 * TILE_SIZE;
			}
			else if (fieldArray[row][col].isVisible && fieldArray[row][col].mineCount > 0) //Visible tile with number
			{
				srcRect.y = (4 + fieldArray[row][col].mineCount) * TILE_SIZE;
			}
			else if (fieldArray[row][col].isVisible && fieldArray[row][col].isMine && fieldArray[row][col].isClicked) //Visible tile with clicked
			{
				srcRect.y = 15 * TILE_SIZE;
			}
			else if (fieldArray[row][col].isVisible && fieldArray[row][col].isMine) //Visible tile with mine
			{
				srcRect.y = 13 * TILE_SIZE;
			}
			else if (!fieldArray[row][col].isMine && fieldArray[row][col].isFlag) //Tile with wrong flag (visible after game over instead of normal tile)
			{
				srcRect.y = 14 * TILE_SIZE;
			}
			else if (!fieldArray[row][col].isVisible && !fieldArray[row][col].isClicked) //Hidden tile without mark and not clicked
			{
				srcRect.y = 0;
			}

			dstRect.x = 5 + col * TILE_SIZE;
			dstRect.y = 50 + row * TILE_SIZE;

			SDL_RenderCopy(renderer, fieldTexture, &srcRect, &dstRect);
		}
	}
}

//Prepare new game with selected mode
void prepareGame(int customWidth = 0, int customHeight = 0, int customMines = 0)
{
	int oldHeight = fieldHeight; //Save old height to use it in array deletion

	switch (gameMode) //Predefined game modes
	{
		case GameMode::BEGINNER:
			fieldWidth = 9;
			fieldHeight = 9;
			fieldMines = 10;
		break;

		case GameMode::ADVANCED:
			fieldWidth = 16;
			fieldHeight = 16;
			fieldMines = 40;
		break;

		case GameMode::EXPERT:
			fieldWidth = 30;
			fieldHeight = 16;
			fieldMines = 99;
		break;

		case GameMode::CUSTOM:
			fieldWidth = customWidth;
			fieldHeight = customHeight;
			fieldMines = customMines;
		break;
	}

	//Check if custom values are valid
	if (gameMode == GameMode::CUSTOM)
	{
		if (fieldWidth < 9)
			fieldWidth = 9;

		if (fieldHeight < 9)
			fieldHeight = 9;

		if (fieldWidth > 100)
			fieldWidth = 100;

		if (fieldHeight > 100)
			fieldHeight = 100;

		if (fieldMines < 10)
			fieldMines = 10;

		if (fieldMines > (fieldWidth*fieldHeight) / 2) //At least half of fields should be empty
			fieldMines = (fieldWidth*fieldHeight) / 2;
	}

	flagCount = fieldMines;

	//If array was created before delete it
	if (fieldArray != NULL)
	{
		for (int row = 0; row < oldHeight; row++)
		{
			delete [] fieldArray[row];
		}

		delete [] fieldArray;
	}

	//Create new array
	fieldArray = new FieldType*[fieldHeight];

	for (int i = 0; i < fieldHeight; i++)
	{
		fieldArray[i] = new FieldType[fieldWidth];
	}

	//Init created array
	//Initaly all fields will be empty and hidden
	for (int row = 0; row < fieldHeight; row++)
	{
		for (int col = 0; col < fieldWidth; col++)
		{
			fieldArray[row][col].isVisible = false;
			fieldArray[row][col].isMine = false;
			fieldArray[row][col].isFlag = false;
			fieldArray[row][col].isUnknown = false;
			fieldArray[row][col].isClicked = false;
			fieldArray[row][col].mineCount = 0;
		}
	}
}

//Check if field is mine
bool isMine(int row, int column)
{
	if (row < 0 || row > fieldHeight - 1 || column < 0 || column > fieldWidth - 1) //Index out of bonds
	{
		return false;
	}

	return fieldArray[row][column].isMine;
}

//Generate new minefield (generates after first click so get position to prevent generating mine on this field)
void generateField(int selectedRow, int selectedColumn)
{
	//Setup mines
	int mineRow = getRandomNumber(0, fieldHeight - 1);
	int mineColumn = getRandomNumber(0, fieldWidth - 1);

	for (int i = 0; i < fieldMines; i++)
	{
		//Keep getting random number as long we dont get empty tile
		//Also don't set mine on selected field
		while ((mineRow == selectedRow && mineColumn == selectedColumn) || fieldArray[mineRow][mineColumn].isMine)
		{
			mineRow = getRandomNumber(0, fieldHeight - 1);
			mineColumn = getRandomNumber(0, fieldWidth - 1);
		}

		//Set mine on selected field
		fieldArray[mineRow][mineColumn].isMine = true;
	}

	//Setup mine count
	for (int row = 0; row < fieldHeight; row++)
	{
		for (int col = 0; col < fieldWidth; col++)
		{
			if (fieldArray[row][col].isMine) //Ignore fields with mine
			{
				continue;
			}

			int mineCount = 0;

			if (isMine(row - 1, col - 1)) //Top left
			{
				mineCount++;
			}

			if (isMine(row - 1, col)) //Top
			{
				mineCount++;
			}

			if (isMine(row - 1, col + 1)) //Top right
			{
				mineCount++;
			}

			if (isMine(row, col - 1)) //Left
			{
				mineCount++;
			}

			if (isMine(row, col + 1)) //Right
			{
				mineCount++;
			}

			if (isMine(row + 1, col - 1)) //Bottom left
			{
				mineCount++;
			}

			if (isMine(row + 1, col)) //Bottom
			{
				mineCount++;
			}

			if (isMine(row + 1, col + 1)) //Bottom right
			{
				mineCount++;
			}

			fieldArray[row][col].mineCount = mineCount;
		}
	}
}

//Recursively uncover all neighbourg empty tiles
void floodFill(int row, int column)
{
	if (row < 0 || row > fieldHeight - 1 || column < 0 || column > fieldWidth - 1) //Index out of bonds
	{
		return;
	}

	//Skip tiles that are not hidden, with mine or with flag
	if (fieldArray[row][column].isVisible || fieldArray[row][column].isMine || fieldArray[row][column].isFlag)
	{
		return;
	}

	fieldArray[row][column].isVisible = true;

	//If field is count then return after making it visible
	if (fieldArray[row][column].mineCount > 0)
	{
		return;
	}

	floodFill(row - 1, column - 1); //Top left
	floodFill(row - 1, column); //Top
	floodFill(row - 1, column + 1); //Top right
	floodFill(row, column - 1); //Left
	floodFill(row, column + 1); //Right
	floodFill(row + 1, column - 1); //Bottom left
	floodFill(row + 1, column); //Bottom
	floodFill(row + 1, column + 1); //Bottom right
}

//Uncover selected tile
//Also set game state if player won or lost
void uncoverTile(int row, int column)
{
	if (fieldArray[row][column].isMine) //Clicked on field with mine so game over
	{
		gameState = GameState::LOST;
		return;
	}

	floodFill(row, column);

	//Check if player won game (only mine tiles are left)
	//All safe tiles should be visible
	//Count visible tiles and check if their number is equal to number of tiles minus number of mines
	int fieldCount = 0;

	for (int r = 0; r < fieldHeight; r++)
	{
		for (int c = 0; c < fieldWidth; c++)
		{
			if (fieldArray[r][c].isVisible)
			{
				fieldCount++;
			}
		}
	}

	if (fieldCount == fieldWidth * fieldHeight - fieldMines)
	{
		gameState = GameState::WON;
	}
}

//Expose field after game over
void exposeField()
{
	for (int row = 0; row < fieldHeight; row++)
	{
		for (int col = 0; col < fieldWidth; col++)
		{
			if (fieldArray[row][col].isMine)
			{
				fieldArray[row][col].isVisible = true;
			}
		}
	}
}

//Set flag or question mark (if enabled and already flag) on the field
void markTile(int row, int column)
{
	//Can't mark visible fields
	if (fieldArray[row][column].isVisible)
	{
		return;
	}

	if (fieldArray[row][column].isUnknown)
	{
		fieldArray[row][column].isUnknown = false;
		return;
	}

	if (!fieldArray[row][column].isFlag)
	{
		fieldArray[row][column].isFlag = true;
		flagCount--;
		return;
	}

	if (fieldArray[row][column].isFlag)
	{
		fieldArray[row][column].isFlag = false;
		flagCount++;

		if (marksEnabled)
		{
			fieldArray[row][column].isUnknown = true;
		}

		return;
	}
}

//Check if selected tile is selectable
bool isSelectable(int row, int column)
{
	//Can't select fields that are visible or with flag
	if (fieldArray[row][column].isVisible || fieldArray[row][column].isFlag)
	{
		return false;
	}

	return true;
}

//Create SDL_Surface from image loaded by stb_image
SDL_Surface* surfaceFromStbImage(StbImage image)
{
    int pitch = image.width * image.channels;

    Uint32 redMask, greenMask, blueMask, alphaMask;

    #if SDL_BYTEORDER == SDL_BIG_ENDIAN
        int shift = (image.channels == 4) ? 0 : 8;
        redMask = 0xff000000 >> shift;
        greenMask = 0x00ff0000 >> shift;
        blueMask = 0x0000ff00 >> shift;
        alphaMask = 0x000000ff >> shift;
    #else
        redMask = 0x000000ff;
        greenMask = 0x0000ff00;
        blueMask = 0x00ff0000;
        alphaMask = (image.channels == 4) ? 0xff000000 : 0;
    #endif

    return SDL_CreateRGBSurfaceFrom(image.pixels, image.width, image.height, image.channels*8, pitch, redMask, greenMask, blueMask, alphaMask);
}

int main(int argc, char* argv[])
{
	SDL_Window* window = NULL;
	SDL_Renderer* renderer = NULL;
	SDL_Texture* fields = NULL, *faces = NULL, *display = NULL;
	std::string basePath, prefPath;

	if (SDL_Init(SDL_INIT_VIDEO) != 0)
	{
		fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
		return EXIT_FAILURE;
	}

	basePath = SDL_GetBasePath();

	windowWidth = 154; //Initial window size for beginner mode
	windowHeight = 199;

	window = SDL_CreateWindow("dsdmine", 100, 100, windowWidth, windowHeight, SDL_WINDOW_SHOWN);

	if (window == NULL)
	{
		fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
		return EXIT_FAILURE;
	}

	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);

	if(renderer == NULL)
	{
		fprintf(stderr, "SDL_CreateRenderer Error: %s\n", SDL_GetError());

		return EXIT_FAILURE;
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.IniFilename = NULL; //Don't create ImGui ini file

	ImGui::StyleColorsDark();

	// Setup Platform/Renderer backends
	ImGui_ImplSDL2_InitForSDLRenderer(window);
	ImGui_ImplSDLRenderer_Init(renderer);

	//Load assets from base directory
	StbImage fieldsImage, facesImage, displayImage;
	fieldsImage.pixels = stbi_load((basePath + "assets"+PATH_SEPARATOR+"tiles.png").c_str(), &fieldsImage.width, &fieldsImage.height, &fieldsImage.channels, 0);
	facesImage.pixels = stbi_load((basePath + "assets"+PATH_SEPARATOR+"faces.png").c_str(), &facesImage.width, &facesImage.height, &facesImage.channels, 0);
	displayImage.pixels = stbi_load((basePath + "assets"+PATH_SEPARATOR+"display.png").c_str(), &displayImage.width, &displayImage.height, &displayImage.channels, 0);

	if (fieldsImage.pixels != NULL && facesImage.pixels != NULL && displayImage.pixels != NULL)
	{
		fields = SDL_CreateTextureFromSurface(renderer, surfaceFromStbImage(fieldsImage));
		faces = SDL_CreateTextureFromSurface(renderer, surfaceFromStbImage(facesImage));
		display = SDL_CreateTextureFromSurface(renderer, surfaceFromStbImage(displayImage));
	}

	if (fields == NULL || faces == NULL || display == NULL)
	{
		fprintf(stderr, "Failed loading assets!\n");

		return EXIT_FAILURE;
	}

	//Try to load icon
	StbImage windowIconImage;
	windowIconImage.pixels = stbi_load((basePath + "assets"+PATH_SEPARATOR+"icon.png").c_str(), &windowIconImage.width, &windowIconImage.height, &windowIconImage.channels, 0);
	SDL_Surface* windowIcon;

	if (windowIconImage.pixels != NULL)
	{
		windowIcon = surfaceFromStbImage(windowIconImage);
	}

	if (windowIcon)
	{
		SDL_SetWindowIcon(window, windowIcon);
	}

	//Check if config directory is present and load it, try to create it otherwise
	bool loadConfig = true;

	if (argc > 1)
	{
		if (strcmp(argv[1], "-p") || strcmp(argv[1], "--portable")) //With "-p" or "--portable" command line flag don't try to create or load config
		{
			loadConfig = false;
		}
	}

	if (loadConfig) //If load config is enabled then try to create pref path
	{
		char* prefLocation = SDL_GetPrefPath("DragonSWDev", "dsdmine"); //Try to create pref directory if it doesn't exists

		if (prefLocation)
		{
			prefPath.append(prefLocation);
			SDL_free(prefLocation);
		}
		else
		{
			loadConfig = false;
		}
	}

	mINI::INIFile configFile(prefPath + "besttimes.ini");
	mINI::INIStructure iniStructure;
	BestTimes* bestTimes;

	if (loadConfig && !std::filesystem::exists(prefPath + "besttimes.ini")) //Check if besttimes.ini file exists
	{
		//Create config file
		iniStructure["Beginner"]["Name"] = "Unknown";
		iniStructure["Beginner"]["Time"] = "999";

		iniStructure["Advanced"]["Name"] = "Unknown";
		iniStructure["Advanced"]["Time"] = "999";

		iniStructure["Expert"]["Name"] = "Unknown";
		iniStructure["Expert"]["Time"] = "999";

		loadConfig = configFile.generate(iniStructure);
	}

	if (loadConfig && configFile.read(iniStructure)) //Config loaded, get values
	{
		bestTimes = new BestTimes[3];

		bestTimes[0].playerName = iniStructure["Beginner"]["Name"];
		bestTimes[1].playerName = iniStructure["Advanced"]["Name"];
		bestTimes[2].playerName = iniStructure["Expert"]["Name"];

		try
		{
			bestTimes[0].bestTime = std::stoi(iniStructure["Beginner"]["Time"]);
			bestTimes[1].bestTime = std::stoi(iniStructure["Advanced"]["Time"]);
			bestTimes[2].bestTime = std::stoi(iniStructure["Expert"]["Time"]);
		}
		catch (...)
		{
			std::cerr << "Reading config failed" << std::endl;
			loadConfig = false;
		}
	}
	else
	{
		loadConfig = false;
	}

	unsigned timeSeed = std::chrono::system_clock::now().time_since_epoch().count();
	randomEngine.seed(timeSeed);

	gameMode = GameMode::BEGINNER;
	prepareGame();

	ImVec4 clear_color = ImVec4(0.75f, 0.75f, 0.75f, 1.00f);

	bool isRunning = true, popupWindow = false, changeMode = false, gameMenuVisible = false, helpMenuVisible = false;
	int customWidth = fieldWidth, customHeight = fieldHeight, customMines = fieldMines, clickedRow = -1, clickedColumn = -1, startTime;
	gameTime = 0;
	WindowType windowType; //Decide which window should be drawn
	FaceState faceState = FaceState::NORMAL, oldFaceState = FaceState::NORMAL;
	char inputName[14] = "Unknown";

	while(isRunning)
	{
		SDL_Event event;

		while (SDL_PollEvent(&event))
		{
			ImGui_ImplSDL2_ProcessEvent(&event);

			if (event.type == SDL_QUIT)
			{
				isRunning = false;
			}

			if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
			{
				isRunning = false;
			}

			//Handle mouse button down
			//If popup window is visible then ignore that to prevent accidental clicks
			//Same goes for game or help menu
			if (event.type == SDL_MOUSEBUTTONDOWN && !popupWindow && !gameMenuVisible && !helpMenuVisible)
			{
				//Get mouse cursor location
				int x, y;
				SDL_GetMouseState(&x, &y);

				//Check if player is clicking face (using left mouse button)
				if (event.button.button == SDL_BUTTON_LEFT &&
					y >= 20 && y <= 20 + FACE_SIZE && x >= (16 * fieldWidth + 10) / 2 - FACE_SIZE / 2 && x <= (16 * fieldWidth + 10) / 2 + FACE_SIZE / 2)
				{
					oldFaceState = faceState;
					faceState = FaceState::NORMAL_CLICK;
				}

				//Check if player is clicking field (only if game is not finished)
				if ((gameState == GameState::INITIALIZED || gameState == GameState::STARTED) 
					&& y >= 50 && y <= windowHeight - 5 && x >= 5 && x <= windowWidth - 5)
				{
					//Set mouse position relative to the field (top left of the field will be 0,0)
					x -= 5;
					y -= 50;

					//Calculate which tile was clicked
					int row, column;

					row = y / TILE_SIZE;
					column = x / TILE_SIZE;

					//Check if tile is selectable (if it was clicked with left mouse button)
					if (event.button.button == SDL_BUTTON_LEFT && isSelectable(row, column))
					{
						clickedRow = row;
						clickedColumn = column;

						fieldArray[row][column].isClicked = true; //Mark tile as clicked

						faceState = FaceState::FIELD_CLICK;
					}
					else if (event.button.button == SDL_BUTTON_RIGHT) //Mark tile
					{
						markTile(row, column);
					}
				}
			}

			//Handle mouse button up
			if (event.type == SDL_MOUSEBUTTONUP && !popupWindow)
			{
				//Get mouse cursor location
				int x, y;
				SDL_GetMouseState(&x, &y);

				//If face was clicked check if cursor is still on face
				if (faceState == FaceState::NORMAL_CLICK)
				{
					//Check if cursor is still on face
					if (y >= 20 && y <= 20 + FACE_SIZE && x >= (16 * fieldWidth + 10) / 2 - FACE_SIZE / 2 && x <= (16 * fieldWidth + 10) / 2 + FACE_SIZE / 2)
					{
						changeMode = true;
					}

					faceState = oldFaceState;
				}

				//Reset face
				if (faceState == FaceState::FIELD_CLICK)
				{
					faceState = FaceState::NORMAL;
				}

				//Some tile was clicked - check if cursor is still on that field
				if (clickedRow >= 0 && clickedColumn >= 0)
				{
					//Check if mouse is still on field
					if (y >= 50 && y <= windowHeight - 5 && x >= 5 && x <= windowWidth - 5)
					{
						x -= 5;
						y -= 50;

						int row, column;

						row = y / TILE_SIZE;
						column = x / TILE_SIZE;

						//Still the same field - perform action
						if (row == clickedRow && column == clickedColumn)
						{
							if (gameState == GameState::INITIALIZED) //Field is not generated - generate new
							{
								generateField(row, column);
								gameState = GameState::STARTED;
								startTime = SDL_GetTicks();
							}

							uncoverTile(row, column);

							if (gameState == GameState::LOST)
							{
								exposeField();
								faceState = FaceState::GAME_LOST;
							}
							else if (gameState == GameState::WON)
							{
								faceState = FaceState::GAME_WON;

								if (gameMode != GameMode::CUSTOM && loadConfig)
								{
									if (gameTime < bestTimes[gameMode].bestTime)
									{
										popupWindow = true;
										windowType = WindowType::NEW_TIME;
									}
								}
							}

							if (gameState != GameState::LOST) //Leave field clicked after game over to show it after exposing field
							{
								fieldArray[clickedRow][clickedColumn].isClicked = false;
							}
						}
						else
						{
							fieldArray[clickedRow][clickedColumn].isClicked = false;
						}
					}
					else //Mouse outside field - clear tile that was clicked
					{
						fieldArray[clickedRow][clickedColumn].isClicked = false;
					}

					clickedRow = -1;
					clickedColumn = -1;
				}
				else if (clickedRow >= 0 && clickedColumn >= 0)
				{
					clickedRow = -1;
					clickedColumn = -1;
				}
			}
		}

		//Change window size to fit selected mode
		//Also change game mode
		if (changeMode)
		{
			if (gameMode != GameMode::CUSTOM)
			{
				prepareGame();
			}
			else
			{
				prepareGame(customWidth, customHeight, customMines);
			}

			//Setup custom values to show them on custom window
			customWidth = fieldWidth;
			customHeight = fieldHeight;
			customMines = fieldMines;

			//Window width is supposed to be field tile width * field width + 10 (5px margin on each side)
			//Window hight is same but top margin should be bigger to make room for display and face
			windowWidth = TILE_SIZE * fieldWidth + 10;
			windowHeight = TILE_SIZE * fieldHeight + 10 + 45;

			SDL_SetWindowSize(window, windowWidth, windowHeight);

			gameState = GameState::INITIALIZED;
			faceState = FaceState::NORMAL;
			gameTime = 0;

			changeMode = false;
		}

		//Update time
		if (gameState == GameState::STARTED && gameTime <= 999)
		{
			gameTime = SDL_GetTicks() - startTime;
			gameTime /= 1000;
		}

		SDL_SetRenderDrawColor(renderer, 
			(Uint8)(clear_color.x * 255), 
			(Uint8)(clear_color.y * 255), 
			(Uint8)(clear_color.z * 255), 
			(Uint8)(clear_color.w * 255));
		
		SDL_RenderClear(renderer);

		 // Start the Dear ImGui frame
		ImGui_ImplSDLRenderer_NewFrame();
		ImGui_ImplSDL2_NewFrame(window);
		ImGui::NewFrame();

		if (ImGui::BeginMainMenuBar())
		{
			if (ImGui::BeginMenu("Game"))
			{
				gameMenuVisible = true;

				if (ImGui::MenuItem("New"))
				{
					changeMode = true;
				}

				ImGui::Separator();

				if (ImGui::MenuItem("Unknown (?)", NULL, marksEnabled, true))
				{
					marksEnabled = !marksEnabled;
				}

				ImGui::Separator();

				if (ImGui::MenuItem("Beginner", NULL, (gameMode == GameMode::BEGINNER), true))
				{
					if (gameMode != GameMode::BEGINNER)
					{
						gameMode = GameMode::BEGINNER;
						changeMode = true;
					}
				}

				if (ImGui::MenuItem("Advanced", NULL, (gameMode == GameMode::ADVANCED), true))
				{
					if (gameMode != GameMode::ADVANCED)
					{
						gameMode = GameMode::ADVANCED;
						changeMode = true;
					}
				}

				if (ImGui::MenuItem("Expert", NULL, (gameMode == GameMode::EXPERT), true))
				{
					if (gameMode != GameMode::EXPERT)
					{
						gameMode = GameMode::EXPERT;
						changeMode = true;
					}
				}

				if (ImGui::MenuItem("Custom", NULL, (gameMode == GameMode::CUSTOM), !popupWindow))
				{
					popupWindow = true;
					windowType = WindowType::CUSTOM_GAME;
				}

				ImGui::Separator();

				if (ImGui::MenuItem("Quit"))
				{
					isRunning = false;
				}

					ImGui::EndMenu();
			}
			else
			{
				gameMenuVisible = false;
			}

			if (ImGui::BeginMenu("Info"))
			{
				helpMenuVisible = true;

				if (ImGui::MenuItem("Best times", NULL, false, !popupWindow))
				{
					popupWindow = true;
					windowType = WindowType::BEST_SCORES;
				}

				ImGui::Separator();

				if (ImGui::MenuItem("About", NULL, false, !popupWindow))
				{
					popupWindow = true;
					windowType = WindowType::ABOUT;
				}

				ImGui::EndMenu();
			}
			else
			{
				helpMenuVisible = false;
			}

				ImGui::EndMainMenuBar();
		}

		if (popupWindow)
		{
			//Setup new window sizes and position for beginner size to make sure they will fit
			if (gameMode == GameMode::BEGINNER)
			{
				ImGui::SetNextWindowPos(ImVec2(0, 0));
				ImGui::SetNextWindowSize(ImVec2(154, 170));
			}
			else
			{
				ImGui::SetNextWindowPos(ImVec2(10, 10));
				ImGui::SetNextWindowSize(ImVec2(250, 170));
			}

			if (windowType == CUSTOM_GAME)
			{
				ImGui::Begin("Custom game");

				ImGui::Text("Custom field");

				ImGui::InputInt("Width", &customWidth);
				ImGui::InputInt("Height", &customHeight);
				ImGui::InputInt("Mines", &customMines);

				if (ImGui::Button("Ok"))
				{
					gameMode = GameMode::CUSTOM;
					changeMode = true;
					popupWindow = false;
				}

				if (ImGui::Button("Cancel"))
				{
					popupWindow = false;
				}

				ImGui::End();
			}
			else if (windowType == BEST_SCORES) //Best scores
			{
				ImGui::Begin("Best times");

				ImGui::Text("Best times");

				if (!loadConfig)
				{
					ImGui::Text("Beginner: 999s Unknown");
					ImGui::Text("Advanced: 999s Unknown");
					ImGui::Text("Expert: 999s Unknown");
				}
				else
				{
					ImGui::Text(("Beginner: %ds " + bestTimes[0].playerName).c_str(), bestTimes[0].bestTime);
					ImGui::Text(("Advanced: %ds " + bestTimes[1].playerName).c_str(), bestTimes[1].bestTime);
					ImGui::Text(("Expert: %ds " + bestTimes[2].playerName).c_str(), bestTimes[2].bestTime);
				}

				if (ImGui::Button("Ok"))
				{
					popupWindow = false;
				}

				if (ImGui::Button("Reset") && loadConfig)
				{
					for (int i = 0; i < 3; i++)
					{
						bestTimes[i].playerName = "Unknown";
						bestTimes[i].bestTime = 999;
					}
				}

				ImGui::End();
			}
			else if (windowType == WindowType::ABOUT)
			{
				ImGui::Begin("dsdmine");

				ImGui::Text("Version:");
				ImGui::SameLine();
				ImGui::Text(GAME_VERSION);
				ImGui::Text("By DragonSWDev");

				if (ImGui::Button("Ok"))
				{
					popupWindow = false;
				}

				ImGui::End();
			}
			else if (windowType == WindowType::NEW_TIME)
			{
				ImGui::Begin("New best time");

				ImGui::Text("New best time!");
				ImGui::Text("Please enter your name.");
				ImGui::InputText("Name", inputName, IM_ARRAYSIZE(inputName));

				if (ImGui::Button("Ok"))
				{
					if (loadConfig)
					{
						bestTimes[gameMode].playerName = inputName;
						bestTimes[gameMode].bestTime = gameTime;
					}

					popupWindow = false;
				}

				ImGui::End();
			}
		}

		// Rendering
		ImGui::Render();

		drawDisplay(renderer, display, gameTime, flagCount, windowWidth);

		drawFace(renderer, faces, faceState);

		drawField(renderer, fields);

		ImGui_ImplSDLRenderer_RenderDrawData(ImGui::GetDrawData());

		SDL_RenderPresent(renderer);
	}

	//Config loaded, store settings before ending game
	if (loadConfig)
	{
		iniStructure["Beginner"]["Name"] = bestTimes[0].playerName;
		iniStructure["Beginner"]["Time"] = std::to_string(bestTimes[0].bestTime);

		iniStructure["Advanced"]["Name"] = bestTimes[1].playerName;
		iniStructure["Advanced"]["Time"] = std::to_string(bestTimes[1].bestTime);

		iniStructure["Expert"]["Name"] = bestTimes[2].playerName;
		iniStructure["Expert"]["Time"] = std::to_string(bestTimes[2].bestTime);

		configFile.write(iniStructure);
	}

	if (windowIcon)
	{
		SDL_FreeSurface(windowIcon);
	}

	SDL_DestroyTexture(fields);
	SDL_DestroyTexture(faces);
	SDL_DestroyTexture(display);
	stbi_image_free(fieldsImage.pixels);
	stbi_image_free(facesImage.pixels);
	stbi_image_free(displayImage.pixels);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();

	return EXIT_SUCCESS;
}
