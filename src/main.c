#include <SFML/Graphics.h>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

// NOTE: One tick is equal to 1/50th of a second or 0.02 seconds.
#define FRAMERATE_LIMIT 50

#define MAX_SCORE 1024
#define MAX_HIGH_SCORES 3

#define SCORE_TEXT_OFFSET 16
#define SCORE_MAX_LENGTH 128

#define PATTERN_SHOW_TIME 30
#define PATTERN_HIDE_TIME 8
#define PATTERN_TIME (PATTERN_SHOW_TIME + PATTERN_HIDE_TIME)

#define FULL_HIGHLIGHT_TIME 12

#define GAME_OVER_TEXT_VERTICAL_OFFSET -20
#define RETRY_TEXT_VERTICAL_OFFSET 50

#define MIN(A, B) (A > B ? B : A)
#define MAX(A, B) (A > B ? A : B)

#define bool uint8_t
#define TRUE 1
#define FALSE 0

enum GameState {
	STATE_PATTERN,
	STATE_PLAYING,
	STATE_OVER,
	STATE_MAX,
};

enum RectType {
	RECT_NONE = -1,
	RECT_RED,
	RECT_BLUE,
	RECT_GREEN,
	RECT_YELLOW,
	RECT_ALL,
	RECT_GAME_OVER,
	RECT_MAX,
};

typedef struct {
	sfRectangleShape* redRect;
	sfRectangleShape* blueRect;
	sfRectangleShape* greenRect;
	sfRectangleShape* yellowRect;
	sfRectangleShape* highlightRect;
	uint8_t highlighted;
	int8_t* scoreString;
	sfText* scoreText;
	sfFont* scoreFont;
	sfText* gameOverText;
	sfFont* gameOverFont;
	sfText* retryText;
	sfText* highscoreLabel;
	sfText* highscoreText[MAX_HIGH_SCORES];
} Visuals;

typedef struct {
	int32_t state;
    int32_t score;
	int32_t timer;
	int32_t highScores[MAX_HIGH_SCORES];
	int32_t quadrants[MAX_SCORE];
	Visuals visuals;
} Game;

typedef struct {
	sfVector2u windowSize;
	sfBool mousePressed;
	sfVector2i mousePosition;
	uint32_t tick;
} Engine;

int32_t* read_highscores(const char* filename, size_t* score_count) {
	FILE* file = fopen(filename, "r");
	if (file == NULL) {
		printf("No highscore file found\n");

		score_count = 0;
		return NULL;
	} else {
		size_t count = 0;
		int32_t temp = 0;
		while(!feof(file)) {
			fscanf(file, "%d", &temp);
			count += 1;
		}

		// fclose(file);
		// file = fopen(filename, "r");
		fseek(file, 0, 0);

		int32_t* scores = malloc(sizeof(int32_t) * count);
		
		int32_t score = 0;
		size_t i = 0;
		while(!feof(file)) {
			fscanf(file, "%d", &score);
			scores[i] = score;
			i += 1;
		}

		fclose(file);

		*score_count = count;
		return scores;
	}
}

void write_highscores(const char* filename, int32_t* highscores) {
	FILE* file = fopen(filename, "w");
	
	size_t count = sizeof(highscores) / sizeof(int32_t);
	for (size_t i = 0; i < count; i++) {
		char buffer[SCORE_MAX_LENGTH];
		int32_t score = highscores[i];
		if (score >= 0) {
			int32_t bytes = sprintf(buffer, "%d\n", score);
			if (bytes > 0) {
				fwrite(buffer, bytes, sizeof(char), file);
			}
		}
	}

	fclose(file);
}


Game* game_create(Engine engine) {
	Game* game = malloc(sizeof(Game));
	
	game->state = STATE_PATTERN;
    game->score = 0;
	game->timer = -50;

	// Default highscores
	for (size_t i = 0; i < MAX_HIGH_SCORES; i++) {
		game->highScores[i] = -1;
	}

	// Load highscores from file
	size_t score_count = 0;
	int32_t* highscores = read_highscores("highscores.txt", &score_count);
	for (size_t i = 0; i < MIN(MAX_HIGH_SCORES, score_count); i++) {
		game->highScores[i] = highscores[i];
	}
	free(highscores);

	srand(time(NULL));
	for (size_t i = 0; i < MAX_SCORE; i++) {
		game->quadrants[i] = random() % 4;
	}

	game->visuals.blueRect = sfRectangleShape_create();
	game->visuals.greenRect = sfRectangleShape_create();
	game->visuals.redRect = sfRectangleShape_create();
	game->visuals.yellowRect = sfRectangleShape_create();
	game->visuals.highlightRect = sfRectangleShape_create();
	game->visuals.highlighted = RECT_NONE;

	sfRectangleShape_setFillColor(game->visuals.blueRect, sfBlue);
	sfRectangleShape_setFillColor(game->visuals.greenRect, sfGreen);
	sfRectangleShape_setFillColor(game->visuals.redRect, sfRed);
	sfRectangleShape_setFillColor(game->visuals.yellowRect, sfYellow);
	sfRectangleShape_setFillColor(game->visuals.highlightRect, sfColor_fromRGBA(255, 255, 255, 128));

	game->visuals.scoreFont = sfFont_createFromFile("res/FiraCode-SemiBold.ttf");
	if (!game->visuals.scoreFont) {
		return NULL;
	}

	game->visuals.scoreText = sfText_create();

	sfText_setFont(game->visuals.scoreText, game->visuals.scoreFont);
	sfText_setCharacterSize(game->visuals.scoreText, 32);
	sfText_setPosition(game->visuals.scoreText, (sfVector2f) { SCORE_TEXT_OFFSET, SCORE_TEXT_OFFSET });
	
	game->visuals.scoreString = malloc(SCORE_MAX_LENGTH);
	if (snprintf(game->visuals.scoreString, SCORE_MAX_LENGTH, "Score: %d", game->score) < 0) {
		return NULL;
	}
	sfText_setString(game->visuals.scoreText, game->visuals.scoreString);

	game->visuals.gameOverFont = sfFont_createFromFile("res/FiraCode-Bold.ttf");
	if (!game->visuals.gameOverFont) {
		return NULL;
	}

	game->visuals.gameOverText = sfText_create();

	sfText_setFont(game->visuals.gameOverText, game->visuals.gameOverFont);
	sfText_setCharacterSize(game->visuals.gameOverText, 64);
	sfText_setPosition(game->visuals.gameOverText, (sfVector2f) { engine.windowSize.x / 2.0f, engine.windowSize.y / 2.0f });
	sfText_setString(game->visuals.gameOverText, "Game Over");
	
	sfFloatRect rect = sfText_getLocalBounds(game->visuals.gameOverText);
	sfVector2f centered = (sfVector2f) { rect.width / 2.0, rect.height / 2.0 - GAME_OVER_TEXT_VERTICAL_OFFSET };
	sfText_setOrigin(game->visuals.gameOverText, centered);

	game->visuals.retryText = sfText_create();

	sfText_setFont(game->visuals.retryText, game->visuals.scoreFont);
	sfText_setCharacterSize(game->visuals.retryText, 32);
	sfText_setPosition(game->visuals.retryText, (sfVector2f) { engine.windowSize.x / 2.0f, engine.windowSize.y / 2.0f });
	sfText_setString(game->visuals.retryText, "Click to Retry");
	
	rect = sfText_getLocalBounds(game->visuals.retryText);
	centered = (sfVector2f) { rect.width / 2.0, rect.height / 2.0 - RETRY_TEXT_VERTICAL_OFFSET };
	sfText_setOrigin(game->visuals.retryText, centered);

	game->visuals.highscoreLabel = sfText_create();

	sfText_setFont(game->visuals.highscoreLabel, game->visuals.scoreFont);
	sfText_setCharacterSize(game->visuals.highscoreLabel, 32);
	sfText_setPosition(game->visuals.highscoreLabel, (sfVector2f) { engine.windowSize.x / 2.0f, engine.windowSize.y / 2.0f });
	sfText_setString(game->visuals.highscoreLabel, "Highscores");
	
	rect = sfText_getLocalBounds(game->visuals.highscoreLabel);
	centered = (sfVector2f) { rect.width / 2.0, rect.height / 2.0 - RETRY_TEXT_VERTICAL_OFFSET };
	sfText_setOrigin(game->visuals.highscoreLabel, centered);

	for (size_t i = 0; i < MAX_HIGH_SCORES; i++) {
		sfText* highscoreText = sfText_create();
		sfText_setFont(highscoreText, game->visuals.scoreFont);
		sfText_setCharacterSize(highscoreText, 18);
		sfText_setPosition(highscoreText, (sfVector2f) { engine.windowSize.x / 2.0f, engine.windowSize.y * 0.75f + (float)i * 24.0f });
		char* highScoreString = malloc(SCORE_MAX_LENGTH);
		if (snprintf(highScoreString, SCORE_MAX_LENGTH, "%d: %d", i + 1, game->highScores[i]) < 0) {
			return NULL;
		}
		sfText_setString(highscoreText, highScoreString);
		rect = sfText_getLocalBounds(highscoreText);
		centered = (sfVector2f) { rect.width / 2.0, rect.height / 2.0 };
		sfText_setOrigin(highscoreText, centered);

		game->visuals.highscoreText[i] = highscoreText;
	}

	return game;
}

void game_draw(Game* game, Engine engine, sfRenderWindow* window) {
	float halfWidth = engine.windowSize.x / 2.0f;
	float halfHeight = engine.windowSize.y / 2.0f;
	sfVector2f rectSize = {halfWidth, halfHeight};

	sfRectangleShape_setPosition(game->visuals.blueRect, (sfVector2f) {0.0f, 0.0f});
	sfRectangleShape_setPosition(game->visuals.greenRect, (sfVector2f) {halfWidth, 0.0f});
	sfRectangleShape_setPosition(game->visuals.redRect, (sfVector2f) {0.0f, halfHeight});
	sfRectangleShape_setPosition(game->visuals.yellowRect, (sfVector2f) {halfWidth, halfHeight});

	sfRectangleShape_setSize(game->visuals.blueRect, rectSize);
	sfRectangleShape_setSize(game->visuals.greenRect, rectSize);
	sfRectangleShape_setSize(game->visuals.redRect, rectSize);
	sfRectangleShape_setSize(game->visuals.yellowRect, rectSize);
	sfRectangleShape_setSize(game->visuals.highlightRect, rectSize);

	sfRectangleShape_setFillColor(game->visuals.highlightRect, sfColor_fromRGBA(255, 255, 255, 128));

	sfVector2f highlightedPos;
	switch (game->visuals.highlighted)
	{
	case RECT_RED:
		highlightedPos = (sfVector2f) {0.0f, 0.0f};
		break;
	case RECT_BLUE:
		highlightedPos = (sfVector2f) {halfWidth, 0.0f};
		break;
	case RECT_GREEN:
		highlightedPos = (sfVector2f) {0.0f, halfHeight};
		break;
	case RECT_YELLOW:
		highlightedPos = (sfVector2f) {halfWidth, halfHeight};
		break;
	case RECT_ALL:
		highlightedPos = (sfVector2f) {0.0f, 0.0f};
		sfRectangleShape_setSize(game->visuals.highlightRect, (sfVector2f) {(float)engine.windowSize.x, (float)engine.windowSize.y});
		break;
	case RECT_GAME_OVER:
		highlightedPos = (sfVector2f) {0.0f, 0.0f};
		sfRectangleShape_setSize(game->visuals.highlightRect, (sfVector2f) {(float)engine.windowSize.x, (float)engine.windowSize.y});
		sfRectangleShape_setFillColor(game->visuals.highlightRect, sfColor_fromRGBA(0, 0, 0, 196));
		break;
	default:
		highlightedPos = (sfVector2f){0.0f, 0.0f};
		sfRectangleShape_setSize(game->visuals.highlightRect, (sfVector2f) {0.0f, 0.0f});
		break;
	}
	sfRectangleShape_setPosition(game->visuals.highlightRect, highlightedPos);

	sfRenderWindow_drawRectangleShape(window, game->visuals.blueRect, NULL);
	sfRenderWindow_drawRectangleShape(window, game->visuals.greenRect, NULL);
	sfRenderWindow_drawRectangleShape(window, game->visuals.redRect, NULL);
	sfRenderWindow_drawRectangleShape(window, game->visuals.yellowRect, NULL);

	sfRenderWindow_drawRectangleShape(window, game->visuals.highlightRect, NULL);

	snprintf(game->visuals.scoreString, SCORE_MAX_LENGTH, "Score: %d", game->score);
	sfText_setString(game->visuals.scoreText, game->visuals.scoreString);

	sfRenderWindow_drawText(window, game->visuals.scoreText, NULL);

	if (game->state == STATE_OVER) {
		sfRenderWindow_drawText(window, game->visuals.gameOverText, NULL);
		sfRenderWindow_drawText(window, game->visuals.retryText, NULL);

		for (size_t i = 0; i < MAX_HIGH_SCORES; i++) {
			sfText* highscoreText = game->visuals.highscoreText[i];

			sfRenderWindow_drawText(window, highscoreText, NULL);
		}
	}
}

void game_update_pattern(Game* game, Engine engine) {
	if (game->timer < FULL_HIGHLIGHT_TIME * 2) {
		if (game->timer < FULL_HIGHLIGHT_TIME) {
			game->visuals.highlighted = RECT_ALL;
		}
		game->timer += 1;
	} else {
		size_t timer = game->timer - (FULL_HIGHLIGHT_TIME * 2);
		size_t index = timer / PATTERN_TIME;
		
		size_t highlighted = game->quadrants[index];
		float half = (float)timer / (float)PATTERN_TIME;
		half -= (float)index;

		float time = (float)PATTERN_SHOW_TIME / (float)PATTERN_TIME;
		if (half > time - (time / 2.0) && half < time + (time / 2.0)) {
			game->visuals.highlighted = highlighted;
		} else {
			game->visuals.highlighted = RECT_NONE;
		}

		game->timer += 1;
		if (index > game->score) {
			game->state = STATE_PLAYING;
			game->timer = 0;
			game->visuals.highlighted = RECT_NONE;
			printf("Changing state to STATE_PLAYING\n");
		}
	}
}

void game_update_playing(Game* game, Engine engine) {
	if (engine.mousePressed) {
		int32_t pressed = 0;
		int32_t halfWidth = engine.windowSize.x / 2.0;
		int32_t halfHeight = engine.windowSize.y / 2.0;
		if (engine.mousePosition.x > halfWidth) {
			pressed += 1;
		}
		if (engine.mousePosition.y > halfHeight) {
			pressed += 2;
		}

		game->visuals.highlighted = pressed;

		if (game->quadrants[game->timer] == pressed) {
			game->timer += 1;

			if (game->timer == (game->score + 1)) {
				game->score += 1;
				game->state = STATE_PATTERN;
				game->timer = -30;
				// game->visuals.highlighted = RECT_NONE;
				
				printf("Changing state to STATE_PATTERN\n");
			}
		} else {
			game->state = STATE_OVER;
			game->timer = 0;
			game->visuals.highlighted = RECT_NONE;

			uint8_t index = -1;
			for (size_t i = 0; i < MAX_HIGH_SCORES; i++) {
				if (game->score > game->highScores[i]) {
					index = i;
					break;
				}
			}

			int32_t newHighscores[MAX_HIGH_SCORES];

			size_t offset = 0;
			for (size_t i = 0; i < MAX_HIGH_SCORES; i++) {
				if (i == index) {
					newHighscores[i] = game->score;
					offset += 1;
				} else if (i > index) {
					newHighscores[i] = game->highScores[i - offset];
				} else {
					newHighscores[i] = game->highScores[i];
				}
			}

			for (size_t i = 0; i < MAX_HIGH_SCORES; i++) {
				game->highScores[i] = newHighscores[i];
			}

			const char* places[3] = {
				"1st",
				"2nd",
				"3rd"
			};

			for (size_t i = 0; i < MAX_HIGH_SCORES; i++) {
				sfText* highscoreText = game->visuals.highscoreText[i];
				
				char* highScoreString = malloc(SCORE_MAX_LENGTH);
				if (snprintf(highScoreString, SCORE_MAX_LENGTH, "%s: %d", places[i], game->highScores[i]) < 0) {
					exit(1);
				}
				sfText_setString(highscoreText, highScoreString);
				
				sfFloatRect rect = sfText_getLocalBounds(highscoreText);
				sfVector2f centered = (sfVector2f) { rect.width / 2.0, rect.height / 2.0 };
				sfText_setOrigin(highscoreText, centered);
			}

			write_highscores("highscores.txt", game->highScores);

			printf("Changing state to STATE_OVER\n");
		}
	}
}

void game_update_over(Game* game, Engine engine) {
	game->visuals.highlighted = RECT_GAME_OVER;

	if (engine.mousePressed) {
		game->visuals.highlighted = RECT_NONE;
		game->score = 0;
		game->state = STATE_PATTERN;

		srand(time(NULL));
		for (size_t i = 0; i < MAX_SCORE; i++) {
			game->quadrants[i] = random() % 4;
		}
	}
}

void game_update(Game* game, Engine engine) {
	if (game->timer < 0) {
		game->timer += 1;
		return;
	}

	switch (game->state)
	{
	case STATE_PATTERN:
		game_update_pattern(game, engine);
		break;
	case STATE_PLAYING:
		game_update_playing(game, engine);
		break;
	case STATE_OVER:
		game_update_over(game, engine);
		break;
	default:
		break;
	}
}

void game_destroy(Game* game) {
	sfRectangleShape_destroy(game->visuals.blueRect);
	sfRectangleShape_destroy(game->visuals.greenRect);
	sfRectangleShape_destroy(game->visuals.redRect);
	sfRectangleShape_destroy(game->visuals.yellowRect);
	sfRectangleShape_destroy(game->visuals.highlightRect);

	for (size_t i = 0; i < MAX_HIGH_SCORES; i++) {
		sfText_destroy(game->visuals.highscoreText[i]);
	}

	sfText_destroy(game->visuals.gameOverText);
	sfText_destroy(game->visuals.scoreText);
	sfText_destroy(game->visuals.retryText);
	sfFont_destroy(game->visuals.gameOverFont);
	sfFont_destroy(game->visuals.scoreFont);

	free(game);
}

int32_t main(int32_t argc, int8_t** argv) {
	Engine engine = {
		(sfVector2u) { 800, 600 },
		sfFalse,
		(sfVector2i) { 0, 0 },
		0
	};

	sfVideoMode mode = {
		engine.windowSize.x,
		engine.windowSize.y,
		32
	};

	sfRenderWindow* window;
	sfView* view;
	sfEvent event;

	Game* game = game_create(engine);

	window = sfRenderWindow_create(mode, "Simon Says", sfClose, NULL);
	if (!window) {
		return EXIT_FAILURE;
	}
	sfRenderWindow_setFramerateLimit(window, 60);

	view = sfView_createFromRect((sfFloatRect) { 0, 0, engine.windowSize.x, engine.windowSize.y });
	sfRenderWindow_setView(window, view);

	while (sfRenderWindow_isOpen(window)) {
		engine.mousePressed = sfFalse;

		while (sfRenderWindow_pollEvent(window, &event)) {
			if (event.type == sfEvtClosed) {
				sfRenderWindow_close(window);
			}
            if (event.type == sfEvtMouseButtonPressed) {
                engine.mousePressed = sfTrue;

				engine.mousePosition.x = event.mouseButton.x;
				engine.mousePosition.y = event.mouseButton.y;
            }
			if (event.type == sfEvtResized) {
				engine.windowSize.x = event.size.width;
				engine.windowSize.y = event.size.height;

				sfView_setSize(view, (sfVector2f) {engine.windowSize.x, engine.windowSize.y});
				sfView_setCenter(view, (sfVector2f) {engine.windowSize.x / 2.0f, engine.windowSize.y / 2.0f});
				
				sfRenderWindow_setView(window, view);
			}
		}
		sfRenderWindow_clear(window, sfBlack);

		game_update(game, engine);
		game_draw(game, engine, window);

		sfRenderWindow_display(window);

		engine.tick += 1;
	}

	game_destroy(game);

	sfView_destroy(view);
	sfRenderWindow_destroy(window);

	return EXIT_SUCCESS;
}
