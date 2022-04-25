#pragma once


#include <list.h>


char* filepath_deescape_name(char* name);
List* filepath_split_path(char* path);
char* filepath_join_paths(List* paths);
