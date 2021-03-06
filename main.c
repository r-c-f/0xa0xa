#define _GNU_SOURCE
#include <fcntl.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curses.h>
#include <stdbool.h>
#include <stdarg.h>
#include <unistd.h>
#include <time.h>
#include "cursutil.h"
#include "xmem.h"
#include "sopt.h"

#define RND_IMPLEMENTATION
#include "rnd.h"

enum piece_type {
	PIECE_OVERLAP=0,
	PIECE_BLANK=1,
	PIECE_PLUS,
	PIECE_BOX1,
	PIECE_BOX2,
	PIECE_BOX3,
	PIECE_BAR2,
	PIECE_BAR3,
	PIECE_BAR4,
	PIECE_BAR5,
	PIECE_L2,
	PIECE_L3,
	PIECE_TYPE_COUNT
};

int piece_ch[PIECE_TYPE_COUNT] = {0};

#define GRID_SIZE 10

int base_grid[GRID_SIZE][GRID_SIZE] = {0};
#define BANK_COUNT 3
struct piece {
	int grid[GRID_SIZE][GRID_SIZE];
	int l;
	int r;
	int t;
	int b;
};

struct piece piece_base[] = {
	/* two blanks */
	{0},
	{0},
	/* plus */
	{
		{
			{PIECE_BLANK,PIECE_PLUS,PIECE_BLANK},
			{PIECE_PLUS,PIECE_PLUS,PIECE_PLUS},
			{PIECE_BLANK,PIECE_PLUS, PIECE_BLANK},
		},
		0,
		3,
		0,
		3,
	},
	/* box1 */
	{
		{
			{PIECE_BOX1},
		},
		0,
		1,
		0,
		1
	},
	/* box2 */
	{
		{
			{PIECE_BOX2, PIECE_BOX2},
			{PIECE_BOX2, PIECE_BOX2},
		},
		0,
		2,
		0,
		2
	},
	/* box3 */
	{
		{
			{PIECE_BOX3, PIECE_BOX3, PIECE_BOX3},
			{PIECE_BOX3, PIECE_BOX3, PIECE_BOX3},
			{PIECE_BOX3, PIECE_BOX3, PIECE_BOX3},
		},
		0,
		3,
		0,
		3
	},
	/* bar2 */
	{
		{
			{PIECE_BAR2, PIECE_BAR2},
		},
		0,
		1,
		0,
		2
	},
	{
		{
			{PIECE_BAR2},
			{PIECE_BAR2},
		},
		0,
		2,
		0,
		1
	},
	/* bar3 */
	{
		{
			{PIECE_BAR3, PIECE_BAR3, PIECE_BAR3},
		},
		0,
		1,
		0,
		3
	},
	{
		{
			{PIECE_BAR3},
			{PIECE_BAR3},
			{PIECE_BAR3},
		},
		0,
		3,
		0,
		1
	},
	/* bar4 */
	{
		{
			{PIECE_BAR4, PIECE_BAR4, PIECE_BAR4, PIECE_BAR4},
		},
		0,
		1,
		0,
		4
	},
	{
		{
			{PIECE_BAR4},
			{PIECE_BAR4},
			{PIECE_BAR4},
			{PIECE_BAR4},
		},
		0,
		4,
		0,
		1
	},
	/* bar5 */
	{
		{
			{PIECE_BAR5, PIECE_BAR5, PIECE_BAR5, PIECE_BAR5, PIECE_BAR5},
		},
		0,
		1,
		0,
		5
	},
	{
		{
			{PIECE_BAR5},
			{PIECE_BAR5},
			{PIECE_BAR5},
			{PIECE_BAR5},
			{PIECE_BAR5},
		},
		0,
		5,
		0,
		1
	},
	/* l2 */
	{
		{
			{PIECE_L2, PIECE_L2},
			{PIECE_L2},
		},
		0,
		2,
		0,
		2
	},
	{
		{
			{PIECE_L2},
			{PIECE_L2, PIECE_L2},
		},
		0,
		2,
		0,
		2
	},
	{
		{
			{PIECE_L2, PIECE_L2},
			{PIECE_BLANK, PIECE_L2},
		},
		0,
		2,
		0,
		2
	},
	{
		{
			{PIECE_BLANK, PIECE_L2},
			{PIECE_L2, PIECE_L2},
		},
		0,
		2,
		0,
		2
	},
	/* l3 */
	{
		{
			{PIECE_L3, PIECE_L3, PIECE_L3},
			{PIECE_L3},
			{PIECE_L3},
		},
		0,
		3,
		0,
		3,
	},
	{
		{
			{PIECE_L3},
			{PIECE_L3},
			{PIECE_L3, PIECE_L3, PIECE_L3},
		},
		0,
		3,
		0,
		3,
	},
	{
		{
			{PIECE_L3, PIECE_L3, PIECE_L3},
			{PIECE_BLANK, PIECE_BLANK, PIECE_L3},
			{PIECE_BLANK, PIECE_BLANK, PIECE_L3},
		},
		0,
		3,
		0,
		3,
	},
	{
		{
			{PIECE_BLANK, PIECE_BLANK, PIECE_L3},
			{PIECE_BLANK, PIECE_BLANK, PIECE_L3},
			{PIECE_L3, PIECE_L3, PIECE_L3},
		},
		0,
		3,
		0,
		3,
	}
};

#define PIECE_BASE_LEN (sizeof(piece_base)/sizeof(*piece_base))

struct piece piece_bank[BANK_COUNT];
bool piece_bank_stat[BANK_COUNT];
struct piece piece_sel;
int piece_bank_pos;
rnd_pcg_t pcg;
WINDOW *win_piece[BANK_COUNT];
int color_count = -1;

void piece_bank_fill(void)
{
	int i;
	for (i = 0; i < BANK_COUNT; ++i) {
		memmove(piece_bank + i, piece_base + rnd_pcg_range(&pcg, 2, PIECE_BASE_LEN - 1), sizeof(*piece_bank));
		piece_bank_stat[i] = true;
	}
}

void fill_grid_blanks(int grid[GRID_SIZE][GRID_SIZE])
{
	int x, y;
	for (x = 0; x < GRID_SIZE; ++x) {
		for (y = 0; y < GRID_SIZE; ++y) {
			if (!grid[x][y])
				grid[x][y] = PIECE_BLANK;
		}
	}
}


void draw_ch_grid(WINDOW *w, int grid[GRID_SIZE][GRID_SIZE])
{
	int x, y, i, j;
	for (x = 0; x < GRID_SIZE; ++x) {
		for (y = 0; y < GRID_SIZE; ++y) {
			for (i = 1; i < 4; ++i) {
				for (j = 1; j < 3; ++j) {
					mvwaddch(w, (y * 2) + j, (x * 3) + i, grid[x][y]);
				}
			}
		}
	}
}

void draw_grid_overlay(WINDOW *w, int a[GRID_SIZE][GRID_SIZE], int b[GRID_SIZE][GRID_SIZE])
{
	int x, y;
	int ch[GRID_SIZE][GRID_SIZE] = {0};

	for (x = 0; x < GRID_SIZE; ++x) {
		for (y = 0; y < GRID_SIZE; ++y) {
			if ((a[x][y] == PIECE_BLANK) && (b[x][y] == PIECE_BLANK)) {
				ch[x][y] = piece_ch[PIECE_BLANK];
			} else if ((a[x][y] == PIECE_BLANK) || (b[x][y] == PIECE_BLANK)) {
				ch[x][y] = piece_ch[a[x][y] + b[x][y] - 1];
			} else {
				ch[x][y] = piece_ch[PIECE_OVERLAP];

			}
		}
	}
	draw_ch_grid(w, ch);
}

void draw_grid(WINDOW *w, int grid[GRID_SIZE][GRID_SIZE])
{
	int x, y;
	int ch[GRID_SIZE][GRID_SIZE] = {0};
	for (x = 0; x < GRID_SIZE; ++x) {
		for (y = 0; y < GRID_SIZE; ++y) {
			ch[x][y] = piece_ch[grid[x][y]];
		}
	}
	draw_ch_grid(w, ch);
}

bool grid_add_valid(int dst[GRID_SIZE][GRID_SIZE], int src[GRID_SIZE][GRID_SIZE])
{
	int x, y;

	for (x = 0; x < GRID_SIZE; ++x) {
		for (y = 0; y < GRID_SIZE; ++y) {
			if (!((dst[x][y] == PIECE_BLANK) || (src[x][y] == PIECE_BLANK))) {
				return false;
			}
		}
	}
	return true;
}

void draw_piece(WINDOW *w, struct piece *p)
{
	int x, y, i;
	for (x = 0; x < 5; ++x) {
		for (y = 0; y < 5; ++y) {
			for (i = 1; i < 3; ++i) {
				mvwaddch(w, y + 1, (2 * x) + i, ' ' | piece_ch[p->grid[x][y]]);
			}
		}
	}
	wrefresh(w);
}

void draw_piece_bank(void)
{
	int i;
	struct piece blank = {0};
	fill_grid_blanks(blank.grid);
	for (i = 0; i < BANK_COUNT; ++i) {
		if (piece_bank_stat[i]) {
			draw_piece(win_piece[i], piece_bank + i);
		} else {
			draw_piece(win_piece[i], &blank);
		}

		if (piece_bank_pos == i) {
			box(win_piece[i], 0, 0);
		} else {
			unbox(win_piece[i]);
		}
		refresh();
		wrefresh(win_piece[i]);
	}
}

int grid_add(int dst[GRID_SIZE][GRID_SIZE], int src[GRID_SIZE][GRID_SIZE])
{
	int x, y, points = 0;

	if (!grid_add_valid(dst, src))
		return 0;

	for (x = 0; x < GRID_SIZE; ++x) {
		for (y = 0; y < GRID_SIZE; ++y) {
			if (src[x][y] != PIECE_BLANK) {
				++points;
				dst[x][y] = src[x][y];
			}
		}
	}
	return points;
}

int grid_clear_lines(int grid[GRID_SIZE][GRID_SIZE])
{
	int x, y;
	int points = 0;
	bool vert_lines[GRID_SIZE];
	bool hor_lines[GRID_SIZE];
	for (x = 0; x < GRID_SIZE; ++x) {
		vert_lines[x] = true;
		for (y = 0; y < GRID_SIZE; ++y) {
			if (grid[x][y] == PIECE_BLANK)
				vert_lines[x] = false;
		}
		if (vert_lines[x])
			++points;
	}
	for (y = 0; y < GRID_SIZE; ++y) {
		hor_lines[y] = true;
		for (x = 0; x < GRID_SIZE; ++x) {
			if (grid[x][y] == PIECE_BLANK)
				hor_lines[y] = false;
		}
		if (hor_lines[y])
			++points;
	}
	for (x = 0; x < GRID_SIZE; ++x) {
		if (vert_lines[x]) {
			for (y = 0; y < GRID_SIZE; ++y) {
				grid[x][y] = PIECE_BLANK;
			}
		}
	}
	for (y = 0; y < GRID_SIZE; ++y) {
		if (hor_lines[y]) {
			for (x = 0; x < GRID_SIZE; ++x) {
				grid[x][y] = PIECE_BLANK;
			}
		}
	}
	return (5 * points) * ( points + 1);
}

bool piece_move(struct piece *piece, int dx, int dy)
{
	int x, y;
	int tmp[GRID_SIZE][GRID_SIZE] = {0};
	fill_grid_blanks(tmp);
	if (piece->r + dx > GRID_SIZE || piece->l + dx < 0) {
		return false;
	}
	if (piece->b + dy > GRID_SIZE || piece->t + dy < 0) {
		return false;
	}
	piece->r += dx;
	piece->l += dx;
	piece->t += dy;
	piece->b += dy;
	for (x = 0; x < GRID_SIZE; ++x) {
		for (y = 0; y < GRID_SIZE; ++y) {
			if ((x + dx >= 0) && ((y + dy >= 0)) && (x + dx < GRID_SIZE) && (y + dy < GRID_SIZE))
				tmp[(x + dx)][(y + dy)] = piece->grid[x][y];
		}
	}
	memmove(piece->grid, tmp, GRID_SIZE * GRID_SIZE * sizeof(piece->grid[0][0]));
	return true;
}

bool game_valid(void)
{
	int i, x, y, y_dir = -1;
	int max_y, max_x;
	struct piece sel;
	bool bank_valid = false;
	for (i = 0; i < BANK_COUNT; ++i) {
		if (!piece_bank_stat[i])
			continue;
		bank_valid = true;
		memmove(&sel, piece_bank + i, sizeof(sel));
		max_y = 1 + GRID_SIZE - sel.b;
		max_x = 1 + GRID_SIZE - sel.r;
		for (x = 0; x < max_x; ++x) {
			y_dir *= -1;
			for (y = 0; y < max_y; ++y) {
				if (grid_add_valid(base_grid, sel.grid))
					return true;
				piece_move(&sel, 0, y_dir);
			}
			piece_move(&sel, 1, 0);
		}
	}
	return !bank_valid;
}

int score, high_score;

int open_high_score(char *path)
{
	int fd;
	if (!path) {
		if (!getenv("HOME"))
			return -1;
		xasprintf(&path, "%s/.local/share/0xa0xa_score", getenv("HOME"));
	}
	if ((fd = open(path, O_RDWR | O_CREAT, 0755)) == -1) {
		free(path);
		return -1;
	}
	if (lockf(fd, F_LOCK, sizeof(high_score)) == -1) {
		close(fd);
		free(path);
		return -1;
	}
	free(path);
	return fd;
}
void close_high_score(int fd)
{
	lockf(fd, F_ULOCK, sizeof(high_score));
	close(fd);
}

void load_high_score(void)
{
	int fd;
	high_score = 0;
	if ((fd = open_high_score(NULL)) == -1) {
		return;
	}
	if (read(fd, &high_score, sizeof(high_score)) != sizeof(high_score)) {
		high_score = 0;
	}
	close_high_score(fd);
}

void store_high_score(void)
{
	int fd;
	if (score > high_score) {
		high_score = score;
		if ((fd = open_high_score(NULL)) == -1) {
			return;
		}
		write(fd, &high_score, sizeof(high_score));
		close_high_score(fd);
	}
}

#define PRINT_HELP_BOLD_DESC(bold, desc) do { \
	cu_stat_aprintw(A_BOLD, "%s", bold); \
	cu_stat_aprintw(A_NORMAL, ": %s, ", desc); \
} while (0)

void print_help(void)
{
	cu_stat_clear();
	PRINT_HELP_BOLD_DESC("Arrows/hjkl/wasd", "Move");
	PRINT_HELP_BOLD_DESC("Tab", "Select");
	PRINT_HELP_BOLD_DESC("Enter/Space", "Place");
	PRINT_HELP_BOLD_DESC("^C", "Quit");
	PRINT_HELP_BOLD_DESC("^D", "New");
	refresh();
}

struct sopt optspec[] = {
	SOPT_INIT_ARGL('c', "colors", SOPT_ARGTYPE_INT, "number", "Number of colors to use (0 for monochrome)"),
	SOPT_INITL('h', "help", "Help message"),
	SOPT_INIT_END
};

#define GAME_END(restart) do { \
	cu_stat_setw("GAME OVER (press any key) -- %d pts %s", score, (score > high_score) ? " -- NEW HIGH SCORE" : ""); \
	refresh(); \
	store_high_score(); \
	getch(); \
	sleep(1); \
	endwin(); \
	if (restart) \
		execvp(argv[0], argv); \
	exit(0); \
} while (0)


int main(int argc, char **argv)
{
	/* sopt things*/
	int opt;
	union sopt_arg soptarg;

	int i, c, add_points;

	sopt_usage_set(optspec, argv[0], "1010!-like game for the terminal");

	while ((opt = sopt_getopt_s(argc, argv, optspec, NULL, NULL, &soptarg)) != -1) {
		switch (opt) {
			case 'h':
				sopt_usage_s();
				return 0;
			case 'c':
				color_count = soptarg.i;
				break;
			default:
				sopt_usage_s();
				return 1;
		}
	}

	rnd_pcg_seed(&pcg, time(NULL) + getpid());

	setlocale(LC_ALL, "");

	cu_stat_init(CU_STAT_BOTTOM);
	initscr();
	raw();
	noecho();
	keypad(stdscr, true);
	curs_set(0);

	WINDOW *win_grid = newwin((2 * GRID_SIZE) + 2, (3 * GRID_SIZE) + 2, 1,1);

	for (i = 0; i < BANK_COUNT; ++i) {
		win_piece[i] = newwin(7, (GRID_SIZE * 2) + 2, 1 + (7 * i), 4 + (3 * GRID_SIZE));
	}


	score = 0;
	load_high_score();
	memset(base_grid, 0, sizeof(base_grid));
	fill_grid_blanks(base_grid);

	box(win_grid, 0, 0);
	refresh();

	piece_ch[PIECE_OVERLAP] = 'X';
	for(i = 1; i < PIECE_TYPE_COUNT; ++i) {
		piece_ch[i] = ' ';
	}
	/* detect monochrome if color count unspecified */
	if (color_count == -1) {
		if (!has_colors()) {
			color_count = 0;
		}
	}
	if (color_count) {
		start_color();
		if (color_count == -1) {
			color_count = COLORS;
		}
		for (i = 1; i < PIECE_TYPE_COUNT; ++i) {
			if (i <= color_count) {
				init_pair(i, COLOR_BLACK, i - 1);
			} else {
				init_pair(i, COLOR_BLACK, (i % color_count) + 1);
			}
			piece_ch[i] |= COLOR_PAIR(i);
		}
	} else if (termattrs() & A_REVERSE) {
		for (i = PIECE_BLANK + 1; i < PIECE_TYPE_COUNT; ++i) {
			piece_ch[i] |= A_REVERSE;
		}
	} else {
		for (i = PIECE_BLANK + 1; i < PIECE_TYPE_COUNT; ++i) {
			piece_ch[i] = ACS_BLOCK;
		}
	}

	for (i = 1; i < PIECE_BASE_LEN; ++i) {
		fill_grid_blanks(piece_base[i].grid);
	}
	print_help();
	refresh();
	while (1) {
		piece_bank_fill();
		piece_bank_pos = 0;
		for (i = 0; i < BANK_COUNT; ++i) {
			if (piece_bank_pos == -1) {
				for(piece_bank_pos = 0; !piece_bank_stat[piece_bank_pos]; ++piece_bank_pos);
			}
			memmove(&piece_sel, piece_bank + piece_bank_pos, sizeof(piece_sel));
			draw_piece_bank();
			draw_grid_overlay(win_grid, base_grid, piece_sel.grid);
			wrefresh(win_grid);
			while (1) {
				if (!game_valid()) {
					GAME_END(true);
				}
				c = getch();
				switch (c) {
					case KEY_LEFT:
					case 'h':
					case 'a':
						piece_move(&piece_sel, -1, 0);
						break;
					case KEY_RIGHT:
					case 'l':
					case 'd':
						piece_move(&piece_sel, 1, 0);
						break;
					case KEY_UP:
					case 'k':
					case 'w':
						piece_move(&piece_sel, 0, -1);
						break;
					case KEY_DOWN:
					case 'j':
					case 's':
						piece_move(&piece_sel, 0, 1);
						break;
					CASE_ALL_RETURN:
					case ' ':
						if ((add_points = grid_add(base_grid, piece_sel.grid))) {
							goto added;
						}else {
							cu_stat_setw("Cannot place");
						}
						break;
					case '\t':
						do {
							piece_bank_pos = (piece_bank_pos + 1) % BANK_COUNT;
						} while (!piece_bank_stat[piece_bank_pos]);
						memmove(&piece_sel, piece_bank + piece_bank_pos, sizeof(piece_sel));
						break;
					case CTRL_('c'):
						GAME_END(false);
					case CTRL_('d'):
						GAME_END(true);
					default:
						beep();
						print_help();
				}
				draw_piece_bank();
				draw_grid_overlay(win_grid, base_grid, piece_sel.grid);
				wrefresh(win_grid);
			}
added:
			score += grid_clear_lines(base_grid) + add_points;
			draw_grid(win_grid, base_grid);
			piece_bank_stat[piece_bank_pos] = false;
			piece_bank_pos = -1;
			cu_stat_setw("SCORE: %d pts\t HIGH SCORE: %d", score, high_score);
		}
	}

	getch();
	return 0;

}


