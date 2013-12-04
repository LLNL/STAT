%module tomod

%{
#include "to.h"
%}

typedef struct {
    char *basepath;
    char *filename;
    int line;
} to_tuple;

char *get_lex_string(to_tuple input);
int run_to();
int add_program_point(to_tuple input);
int clear_program_points();
char *get_temporal_string(to_tuple input);
int add_include_path(char *path);
