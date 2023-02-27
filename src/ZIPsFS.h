#define ANSI_RED "\x1B[41m"
#define ANSI_MAGENTA "\x1B[45m"
#define ANSI_GREEN "\x1B""[42m"
#define ANSI_BLUE "\x1B""[44m"
#define ANSI_YELLOW "\x1B""[43m"
#define ANSI_WHITE "\x1B""[47m"
#define ANSI_BLACK "\x1B""[40m"
#define ANSI_FG_GREEN "\x1B""[32m"
#define ANSI_FG_RED "\x1B""[31m"
#define ANSI_FG_MAGENTA "\x1B""[35m"
#define ANSI_FG_GRAY "\x1B""[30;1m"
#define ANSI_FG_BLUE "\x1B""[34;1m"
#define ANSI_FG_BLACK "\x1B""[100;1m"
#define ANSI_FG_YELLOW "\x1B""[33m"
#define ANSI_FG_WHITE "\x1B""[37m"
#define ANSI_INVERSE "\x1B""[7m"
#define ANSI_BOLD "\x1B""[1m"
#define ANSI_UNDERLINE "\x1B""[4m"
#define ANSI_RESET "\x1B""[0m"


#define GREEN_DONE ANSI_FG_GREEN##$ANSI_RESET
#define GREEN_SUCCESS ANSI_FG_GREEN##$ANSI_RESET
#define GREEN_ALREADY_EXISTS ANSI_FG_GREEN" Already exists "$ANSI_RESET
#define RED_FAILED ANSI_FG_RED" Failed "$ANSI_RESET
#define RED_NO_FILE ANSI_FG_RED" No file "$ANSI_RESET
#define RED_ERROR ANSI_FG_RED" Error "$ANSI_RESET
#define RED_WARNING ANSI_FG_RED" Warning "$ANSI_RESET
#define DEBUG_NOW_MAGENTA ANSI_MAGENTA""ANSI_FG_RED"DEBUG_NOW"ANSI_RESET
#define TERMINAL_CLR_LINE "\r\x1B""[K"
