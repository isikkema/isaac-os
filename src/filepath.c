#include <filepath.h>
#include <stdbool.h>
#include <string.h>
#include <kmalloc.h>
#include <rs_int.h>
#include <printf.h>


char* filepath_deescape_name(char* name) {
    char* deescaped_name;
    char* tmp;
    char c;
    bool escape;
    u32 idx;
    u32 new_idx;

    tmp = kzalloc(strlen(name) + 1);
    new_idx = 0;
    escape = false;
    for (idx = 0; idx < (u32) strlen(name); idx++) {
        c = name[idx];

        if (c == '\\' && !escape) {
            escape = true;
            continue;
        } else {
            escape = false;
        }

        tmp[new_idx] = c;

        new_idx++;
    }

    deescaped_name = kzalloc(strlen(tmp) + 1);
    memcpy(deescaped_name, tmp, strlen(tmp));

    kfree(tmp);
    return deescaped_name;
}

List* filepath_split_path(char* path) {
    List* list;
    ListNode* it;
    char* chunk;
    u32 start_idx;
    u32 end_idx;
    bool escape;
    char pc;
    char c;
    u32 i;

    list = list_new();

    start_idx = 0;
    end_idx = strlen(path);
    escape = false;
    pc = '\0';
    for (i = 0; i < (u32) strlen(path); i++) {
        c = path[i];

        if (c == '/') {
            if (escape) {
                // treat as regular character
                escape = false;
            } else {
                if (pc == '/') {
                    // Many '/'s in a row. Ignore all but the first.
                    start_idx++;
                } else if (pc == '\0') {
                    // First character is '/'. This is an absolute path. Add a "/" as the first name.
                    chunk = kzalloc(2);
                    memcpy(chunk, "/", 1);

                    list_insert_after(list, list->last, chunk);

                    start_idx = i + 1;
                    end_idx = i + 1;
                } else {
                    // Copy from beginning or last '/' through here into list.
                    end_idx = i;

                    chunk = kzalloc(end_idx - start_idx + 1);
                    memcpy(chunk, path + start_idx, end_idx - start_idx);

                    list_insert_after(list, list->last, chunk);

                    start_idx = i + 1;
                    end_idx = i + 1;
                }
            }
        } else if (c == '\\') {
            if (escape) {
                // treat as regular character
                escape = false;
            } else {
                escape = true;
            }
        } else {
            if (escape) {
                printf("filepath_path_split: warning: cannot escape character (%c)\n", c);
                escape = false;
            } else {
                // Regular character. Do nothing
            }
        }

        pc = c;
    }

    end_idx = i;
    if (start_idx != end_idx) {
        chunk = kzalloc(end_idx - start_idx + 1);
        memcpy(chunk, path + start_idx, end_idx - start_idx);

        list_insert_after(list, list->last, chunk);
    }

    for (it = list->head; it != NULL; it = it->next) {
        chunk = it->data;
        it->data = filepath_deescape_name(chunk);
        kfree(chunk);
    }

    return list;
}

char* filepath_join_paths(List* paths) {
    List* paths_list;
    List* path_names;
    ListNode* it;
    ListNode* it2;
    size_t total_size;
    size_t name_size;
    char* path;

    paths_list = list_new();
    for (it = paths->head; it != NULL; it = it->next) {
        path_names = filepath_split_path(it->data);
        
        // If this is not the first path in the list, remove the beginning "/" if it exists
        if (it != paths->head && strcmp(path_names->head->data, "/") == 0) {
            kfree(path_names->head->data);
            list_remove(path_names, path_names->head->data);
        }

        list_insert_after(paths_list, paths_list->last, path_names);
    }

    total_size = 0;
    for (it = paths_list->head; it != NULL; it = it->next) {
        path_names = it->data;

        for (it2 = path_names->head; it2 != NULL; it2 = it2->next) {
            if (it != paths_list->head || it2 != path_names->head || strcmp(it2->data, "/") != 0) {
                total_size += strlen(it2->data) + 1;
            }
        }
    }

    // printf("filepath_join_paths: counted %d\n", total_size);

    path = kzalloc(total_size + 1);

    total_size = 0;
    for (it = paths_list->head; it != NULL; it = it->next) {
        path_names = it->data;

        for (it2 = path_names->head; it2 != NULL; it2 = it2->next) {
            if (it != paths_list->head || it2 != path_names->head /* || is_absolute || strcmp(it2->data, "/") != 0 */) {
                path[total_size] = '/';
                total_size++;
            }

            name_size = strlen(it2->data);
            memcpy(path + total_size, it2->data, name_size);

            total_size += name_size;
        }
    }

    // printf("filepath_join_paths: copied %d\n", total_size);

    return path;
}
