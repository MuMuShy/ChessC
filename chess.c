#include <assert.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define TIME_FMT(a) (a / 3600), ((a / 60) % 60), a % 60
#define HIS_SIZE 8192
#define TIME_LIMIT 3600 // 3600  // 時間限制，秒數。
typedef enum Chess {     // 第一項是0（ BLANK = 0 ），第二項是1（ KING = 1 ）以此類推。
    BLANK,
    KING,        // 是棋局最重要的棋，被吃就輸了。它能走直、橫或斜的，但每次只能走一格。
    QUEEN,       // 是最強的棋子，只要沒有棋子擋住，它可以直走、橫走或斜走無限步。
    BISHOP,      // 只要沒有棋子擋住，可以斜走無限步。
    KNIGHT,      // 走日
    ROOK,        // 如同象棋的「車」，只要沒棋子擋住，可直走、橫走無限步。
    PAWN_FRESH,  // 沒動過的兵，可以走一步或兩步
    PAWN,        //   動過的兵，可以走一步

} Chess;

typedef struct His {
    int player;
    int moved[8][8][2];
    int time_left[2];
    int en_passant[2];    // 可以被吃過路兵的位置
    int en_passant_flag;  // 下一回合可以吃過路兵
    Chess*** grid;
    struct His* prev;
} His;
int his_save();
int his_undo();
His* cur = NULL;
int history_id = 0;
int time_left[2];
int PLAYER_BLACK = 0, PLAYER_WHITE = 1;
int player = 1;
int moved[8][8][2] = {0};                               // 是否移動過，換位用
Chess grid[8][8][2];                                    // row, column, player (0 for black, 1 for white)
char* PLAYER_NAME[] = {"black", "white"};               // 玩家名稱
const char* CHESS_TOKEN[2] = {" kqbnrpp", " KQBNRPP"};  // 兩邊的棋子的字
int en_passant[2] = {-1, -1};                           // 可以被吃過路兵的位置
int en_passant_flag = 0;                                // 下一回合可以吃過路兵
Chess*** board_new() {
    Chess*** board = malloc(sizeof(Chess[8][8][2]));
    return board;
}
void board_del(Chess*** b) {
    free(b);
}
void history_clean() {
    while (his_undo()){}
    
}
void grid_init() {                                      // 初始化棋盤
    player = PLAYER_WHITE;
    time_left[0] = time_left[1] = TIME_LIMIT;
    history_id = 0;
    en_passant[1] = en_passant[0] = -1;
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            grid[i][j][0] = grid[i][j][1] = BLANK;
            moved[i][j][0] = moved[i][j][1] = 0;
        }
    }
    grid[0][0][0] = ROOK;
    grid[0][1][0] = KNIGHT;
    grid[0][2][0] = BISHOP;
    grid[0][3][0] = QUEEN;
    grid[0][4][0] = KING;
    grid[0][5][0] = BISHOP;
    grid[0][6][0] = KNIGHT;
    grid[0][7][0] = ROOK;
    for (int i = 0; i < 8; i++)
        grid[1][i][0] = PAWN_FRESH;

    grid[7][0][1] = ROOK;
    grid[7][1][1] = KNIGHT;
    grid[7][2][1] = BISHOP;
    grid[7][3][1] = QUEEN;
    grid[7][4][1] = KING;
    grid[7][5][1] = BISHOP;
    grid[7][6][1] = KNIGHT;
    grid[7][7][1] = ROOK;
    for (int i = 0; i < 8; i++)
        grid[6][i][1] = PAWN_FRESH;

    his_save();
}

void grid_print() {
    printf("    A   B   C   D   E   F   G   H  \n");
    for (int i = 0; i < 8; i++) {
        printf("   --- --- --- --- --- --- --- ---\n");
        printf("%d |", i + 1);
        for (int j = 0; j < 8; j++) {
            // 至少一個是 BLANK ，就是 0 ，加起來不會有問題。
            printf(" %c |", CHESS_TOKEN[grid[i][j][1] != BLANK][grid[i][j][0] + grid[i][j][1]]);
        }
        printf("\n");
    }
    printf("   --- --- --- --- --- --- --- ---\n");
}
/*
    @return 0 不合法 1 合法 2 換位 3 吃過路兵
*/
int move_verify(int player, const int x, const int y, const int new_x, const int new_y) {
    if (new_x < 0 || new_x > 7 || new_y < 0 || new_y > 7) return 0;  // 超界了
    if (x < 0 || x > 7 || y < 0 || y > 7) return 0;  // 超界了
    const Chess chess = grid[x][y][player];
    if (chess == BLANK) return 0;
    const int dx = new_x - x;                                        // x 變化量
    const int dy = new_y - y;                                        // y 變化量
    if (dx == 0 && dy == 0) return 0;                                // 沒有動
    // 換位 特判
    if (chess == ROOK && grid[new_x][new_y][player] == KING || chess == KING && grid[new_x][new_y][player] == ROOK) {
        if (!moved[x][y][player] && !moved[x][y][player]) {
            int from = y, to = new_y;
            if (from > to) {  // make from < to.
                from ^= to;
                to ^= from;
                from ^= to;
            }
            for (int i = from + 1; i < to; i++) {
                if (grid[x][i][0] != BLANK || grid[x][i][1] != BLANK) {
                    return 0;
                }
            }
            return 2;
        }
    }
    if (grid[new_x][new_y][player] != BLANK) return 0;
    if (en_passant_flag && chess == PAWN) {
        // TODO EN_FLAG_CHK
        if (new_y == en_passant[1] && (new_x == en_passant[1] + 1 || new_x == en_passant[1] - 1)) return 3;
    }
    if (chess == PAWN_FRESH || chess == PAWN) {
        // 兵的吃跟移動比較困難，這裡獨立出來先判斷，底下再判斷其他走法合理與否。
        if (dx * dx + dy * dy == 2) {  // 斜著只能吃
            if (grid[new_x][new_y][!player] != BLANK) {
                return 1;
            }
        } else {  // 往前只能走
            if (grid[new_x][new_y][!player] != BLANK) return 0;
        }
    }
    if (chess == KING) {
        if (dx * dx + dy * dy > 2) return 0;  // 移動長度超過斜線一格
    } else if (chess == QUEEN) {
        if (dx && dy) {                              // 走斜的
            if (!(dx == -dy || dx == dy)) return 0;  // 斜著走直的跟橫的要一樣多
            int i = x, j = y;
            while (i != new_x) {  // 判斷中間有沒有棋
                if (i == x) {
                    i += (dx > 0 ? 1 : -1);
                    j += (dy > 0 ? 1 : -1);
                    continue;
                }
                printf("queen %d, %d\n", i, j);
                if (grid[i][j][0] != BLANK || grid[i][j][1] != BLANK) {
                    return 0;
                }
                i += (dx > 0 ? 1 : -1);
                j += (dy > 0 ? 1 : -1);
            }
        } else if ((dx == 0 && dy) || (dy == 0 && dx)) {  // 走直的
            if (dx) {
                for (int i = x; i != new_x; i += (dx >= 1 ? 1 : -1)) {
                    if (i == x) continue;  // 不要檢查到自己
                    if (grid[i][y][0] != BLANK || grid[i][y][1] != BLANK) return 0;
                }
            } else {
                for (int i = y; i != new_y; i += (dy >= 1 ? 1 : -1)) {
                    if (i == y) continue;  // 不要檢查到自己
                    if (grid[x][i][0] != BLANK || grid[x][i][1] != BLANK) return 0;
                }
            }
        } else {
            return 0;
        }
    } else if (chess == BISHOP) {
        if (!(dx == -dy || dx == dy)) {
            printf("%d %d\n", dx, dy);
            return 0;
        }                                        // 要正斜的
        if (!(dx == -dy || dx == dy)) return 0;  // 斜著走直的跟橫的要一樣多
        int i = x, j = y;
        while (i != new_x) {  // 判斷中間有沒有棋
            if (i == x) {
                i += (dx > 0 ? 1 : -1);
                j += (dy > 0 ? 1 : -1);
                continue;
            }
            printf("queen %d, %d\n", i, j);
            if (grid[i][j][0] != BLANK || grid[i][j][1] != BLANK) {
                return 0;
            }
            i += (dx > 0 ? 1 : -1);
            j += (dy > 0 ? 1 : -1);
        }
    } else if (chess == ROOK) {
        if (dx && dy) return 0;  // 只能走直的或是橫的
        if (dx) {
            for (int i = x; i != new_x; i += (dx >= 1 ? 1 : -1)) {
                if (i == x) continue;
                if (grid[i][y][0] != BLANK || grid[i][y][1] != BLANK) {
                    printf("hit! %d %d\n", i, y);
                    return 0;
                }
            }
        } else {
            for (int i = y; i != new_y; i += (dy >= 1 ? 1 : -1)) {
                if (i == y) continue;
                if (grid[x][i][0] != BLANK || grid[x][i][1] != BLANK) {
                    printf("hit! %d %d\n", x, i);
                    return 0;
                }
            }
        }
    } else if (chess == KNIGHT) {
        return (
            (dx == 1 && dy == 2) ||
            (dx == 2 && dy == 1) ||
            (dx == -1 && dy == 2) ||
            (dx == 1 && dy == -2) ||
            (dx == -1 && dy == -2) ||
            (dx == -2 && dy == -1) ||
            (dx == -2 && dy == 1) ||
            (dx == 2 && dy == -1));
    } else if (chess == PAWN_FRESH) {
        if (player == 0) {
            if (dx == 2) {
                if (grid[new_x - 1][y][0] != BLANK || grid[new_x - 1][y][1] != BLANK) return 0;
            }
            return (new_x - x == 1 || new_x - x == 2) && (new_y == y);
        } else {
            if (dx == -2) {
                if (grid[new_x + 1][y][0] != BLANK || grid[new_x + 1][y][1] != BLANK) return 0;
            }
            return (new_x - x == -1 || new_x - x == -2) && (new_y == y);
        }

    } else if (chess == PAWN) {
        if (player == 0) {
            return (new_x - x == 1) && (new_y == y);
        } else {
            return (new_x - x == -1) && (new_y == y);
        }
    }

    return 1;
}

/*
    @return 0 移動失敗, 1 移動成功, 2 0 贏, 3 1 贏
*/
int move(int player, int x, int y, int new_x, int new_y) {
    int verify_result = 0;
    if (!(verify_result = move_verify(player, x, y, new_x, new_y))) return 0;
    Chess chess = grid[x][y][player];
        if (verify_result == 3) {
        // 吃過路兵
        grid[en_passant[0]][en_passant[1]][!player] = BLANK;
        grid[new_x][new_y][player] = PAWN;
        grid[x][y][player] = BLANK;
        en_passant_flag = 0;
        return 1;
    }
    if (verify_result == 2) {  // swap rook & king
        grid[x][y][player] ^= grid[new_x][new_y][player];
        grid[new_x][new_y][player] ^= grid[x][y][player];
        grid[x][y][player] ^= grid[new_x][new_y][player];
        return 1;
    }
    if (grid[new_x][new_y][!player] != BLANK) {  // !player 就是對面的，0變成1，1變成0
        if (grid[new_x][new_y][!player] == KING) return player + 2;
        grid[new_x][new_y][!player] = BLANK;
    }
    grid[new_x][new_y][player] = grid[x][y][player];
    grid[x][y][player] = BLANK;
    moved[x][y][player] = 1;
    moved[new_x][new_y][player] = 1;
    if (grid[new_x][new_y][player] == PAWN_FRESH) {
        if (new_x == 3 || new_x == 4) {
            en_passant_flag = 1;  // TODO EN_PASSENT_CHK
            en_passant[0] = new_x;
            en_passant[1] = new_y;
        }
        grid[new_x][new_y][player] = PAWN;
    }
    return 1;
}
int his_save() {
    // if (history_id > HIS_SIZE - 2) {
    //     return 0;
    // }
    // His* his = &histories[history_id++];
    His* his = malloc(sizeof(His));
    if (his == NULL) return 0;
    his->grid = board_new();
    if (his->grid == NULL) return 0;
    // int history
    // int moved[8][8][2];
    // int time_left[2];
    // int en_passant[2];    // 可以被吃過路兵的位置
    // int en_passant_flag;  // 下一回合可以吃過路兵
    // Chess grid[8][8][2];
    memcpy(his->moved, moved, sizeof(moved));
    memcpy(his->time_left, time_left, sizeof(time_left));
    memcpy(his->en_passant, en_passant, sizeof(en_passant));
    memcpy(his->grid, grid, sizeof(grid));
    his->en_passant_flag = en_passant_flag;
    his->player = player;
    his->prev = cur;
    cur = his;
    return 1;
}

int his_undo() {
    // printf("[debug] history_id: %d\n", history_id);
    // if (history_id <= 1) {  // 沒得退了
    if (cur == NULL || cur->prev == NULL) {  // 沒得退了
        return 0;
    }
    His* his = cur->prev;
    memcpy(moved, his->moved, sizeof(moved));
    memcpy(time_left, his->time_left, sizeof(time_left));
    memcpy(en_passant, his->en_passant, sizeof(en_passant));
    memcpy(grid, his->grid, sizeof(grid));
    en_passant_flag = his->en_passant_flag;
    player = his->player;
    board_del(cur->grid);
    free(cur);
    cur = his;
    return 1;
}

int main() {
    grid_init();
    grid_print();
    // char buf[8192];

    int x, y, new_x, new_y;
    int inp_x, inp_new_x;
    char inp_y, inp_new_y;
    char line[8192];
    while (1) {
        time_t inp_start;
        time(&inp_start);
        printf("%s's turn! %02d:%02d:%02d\n", PLAYER_NAME[player], TIME_FMT(time_left[player]));
        fflush(stdout);
        
        if (!fgets(line, 8192, stdin)) {
            fputs("\n", stdout);
            exit(0);
        }
	    if(!strncmp(line, "restart", 7)) {
            history_clean();
            grid_print();
            continue;
        }	
        if (!strncmp(line, "undo", 4)) {
            if (his_undo()) {
                fprintf(stdout, "undo!");
                grid_print();
            } else {
                fputs("undo fail! Has it started already?\n", stdout);
            }
            continue;
        }
        sscanf(line, "%c%d %c%d", &inp_y, &inp_x, &inp_new_y, &inp_new_x);
        // char buf[100];
        printf("en_passant_flag: %d\n", en_passant_flag);
        // 每輪之前印出現在換誰，然後檢查輸入
        x = inp_x - 1;
        y = inp_y - 'A';
        new_x = inp_new_x - 1;
        new_y = inp_new_y - 'A';
        int result = move(player, x, y, new_x, new_y);
        time_t inp_end;
        time(&inp_end);
        time_left[player] -= (int)difftime(inp_end, inp_start);
        grid_print();
        
        int time_out_player = -1;
        if (time_left[player] < 0) time_out_player = player;
        else if (time_left[!player] < 0) time_out_player = !player;
        if (time_out_player != -1) {
            printf("Timeout! %s wins!\n", PLAYER_NAME[!time_out_player]);
            goto RESTART_GAME_ASK;
        }

        if (result == 0) {
			printf(line);
            printf("illegal move!\n\n");
            continue;
        } else if (result == 1 || result == -1) {
            printf("%s %02d:%02d:%02d\n", PLAYER_NAME[player], TIME_FMT(time_left[player]));
            printf("Player %s moves %c%d to %c%d!\n", PLAYER_NAME[player], inp_y, inp_x, inp_new_y, inp_new_x);
            printf("(%d, %d) -> (%d, %d)\n", x, y, new_x, new_y);
            player = !player;
            assert(his_save() && "history buffer is fulled.");
        } else if (result > 1) {
            printf("%s wins!\n", PLAYER_NAME[result - 2]);
        RESTART_GAME_ASK:;
            printf("restart game? (y/n) ");
            fflush(stdout);
            char ans;
            while ((ans = getchar()) != 'y' && ans != 'n') {
                if (isspace(ans)) continue;
                printf("What do you mean? input 'y' or 'n'\n");
            }

            printf("'%c'\n", ans);
            // getchar();
            if (ans == 'y') {
                history_clean();
                while (getchar() != '\n');
                grid_print();
                continue;
            }
            break;
        }
    }
    his_clean();

    return 0;
}
