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
