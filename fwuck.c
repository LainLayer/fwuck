#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <curl/curl.h>

#define dynamic_array_append(xs, x)                                                  \
    do {                                                                             \
        if ((xs)->count >= (xs)->capacity) {                                         \
            if ((xs)->capacity == 0) (xs)->capacity = 16;                            \
            else (xs)->capacity *= 2;                                                \
            (xs)->items = realloc((xs)->items, (xs)->capacity*sizeof(*(xs)->items)); \
            assert((xs)->items != NULL);                                             \
        }                                                                            \
                                                                                     \
        (xs)->items[(xs)->count++] = (x);                                            \
    } while (0)

#define UNUSED(variable) (void)variable
#define TODO(message) do { printf(" == TODO == " message); exit(1); } while(0)

static const char *help_text =
    "How to fwuck:\n\n"
    "  fwuck [OPTIONS] [URL]...\n\n"
    "  Action flags:\n\n"
    "    Only one of these may be passed at a time\n\n"
    "    -t,--take-apart   - Take apart a URL into its component parts\n"
    "    -p,--put-together - Assemble a URL from comonent parts\n"
    "    -r,--replace      - Replace a part of a URL\n"
    "\n"
    "  Utility Flags:\n\n"
    "    -h,--help         - Print this text\n"
    "    -n,--no-label     - Disable printing of labels like 'PORT:' when disassembling a URL\n"
    ;

void print_help() {
    puts(help_text);
}

typedef  void (*Action_Function)(void);

Action_Function action_function = NULL;
bool            print_labels    = false;

typedef struct {
    char **items;
    size_t capacity;
    size_t count;
} String_List;

String_List non_flag_arguments = {0};

typedef struct {
    char *data;
    size_t count;
} Sized_String;

typedef struct {
    char *items;
    size_t count;
    size_t capacity;
} Sized_Dynamic_String;

char *read_line_from_stdin_and_trim(void) {
    Sized_Dynamic_String buffer = {0};

    char character;
    while ((character = getchar()) != EOF && character != '\n') {
        dynamic_array_append(&buffer, character);
    }

    if(buffer.count == 0) {
        return NULL;
    }

    while(buffer.count > 0 && isspace(buffer.items[0])) {
        buffer.count -= 1;
        buffer.items += 1; // This leaks memory, I don't care.
    }

    while(buffer.count > 0 && isspace(buffer.items[buffer.count-1])) {
        buffer.count -= 1;
    }

    if(buffer.count == 0) {
        return NULL;
    }

    dynamic_array_append(&buffer, '\0');

    return buffer.items;
}

Sized_String sized_string_from_string(char *text) {
    return (Sized_String) {
        .data  = text,
        .count = strlen(text),
    };
}

bool sized_string_equals_c_string(Sized_String a, const char *b) {
    // return strcmp(a.data, b) == 0;
    size_t b_length = strlen(b);

    if(b_length != a.count) return false;

    return memcmp(b, a.data, a.count) == 0;
}

typedef enum {
    FLAG_TAKE_APART,
    FLAG_PUT_TOGETHER,
    FLAG_REPLACE,
    FLAG_HELP,
    FLAG_NO_LABEL,
} Flag;

typedef struct {
    Flag *items;
    size_t count;
    size_t capacity;
} Flag_List;

bool is_action_flag(Flag flag) {
    switch(flag) {
        case FLAG_TAKE_APART:
        case FLAG_PUT_TOGETHER:
        case FLAG_REPLACE:
            return true;
        default:
            return false;
    }
}

void parse_sized_string_into_flag_list(Sized_String flag, Flag_List *flag_list) {

    #define INVALID(format, ...) do { fprintf(stderr, format, __VA_ARGS__); print_help(); exit(1); } while(0)

    if(flag.count < 2) INVALID("Invalid argument: '%s'\n", flag.data);

    if(flag.data[1] == '-') {
        if(flag.count == 2) {
            INVALID("'%s' is not a valid argument for fwuck.\n", flag.data);
        } else {
                 if(sized_string_equals_c_string(flag, "--take-apart"))   { dynamic_array_append(flag_list, FLAG_TAKE_APART);   return; }
            else if(sized_string_equals_c_string(flag, "--put-together")) { dynamic_array_append(flag_list, FLAG_PUT_TOGETHER); return; }
            else if(sized_string_equals_c_string(flag, "--replace"))      { dynamic_array_append(flag_list, FLAG_REPLACE);      return; }
            else if(sized_string_equals_c_string(flag, "--help"))         { dynamic_array_append(flag_list, FLAG_HELP);         return; }
            else if(sized_string_equals_c_string(flag, "--no-label"))     { dynamic_array_append(flag_list, FLAG_NO_LABEL);     return; }
            else { INVALID("Unrecognized flag '%s'\n", flag.data); }
        }
    }

    for(size_t i = 0; i < flag.count - 1; i++) {
        switch(flag.data[i+1]) {
            case 't': { dynamic_array_append(flag_list, FLAG_TAKE_APART);   } break;
            case 'p': { dynamic_array_append(flag_list, FLAG_PUT_TOGETHER); } break;
            case 'r': { dynamic_array_append(flag_list, FLAG_REPLACE);      } break;
            case 'h': { dynamic_array_append(flag_list, FLAG_HELP);         } break;
            case 'n': { dynamic_array_append(flag_list, FLAG_NO_LABEL);     } break;
            default:
                INVALID("Unrecognized short flag: '-%c'\n", flag.data[i+1]);
        }
    }

    #undef INVALID
}

void take_apart(void)   {

    #define PARSE_ERROR(format, ...) do { fprintf(stderr, format, __VA_ARGS__); exit(1); } while(0)

    for(size_t i = 0; i < non_flag_arguments.count; i++) {
        const char *url = non_flag_arguments.items[i];

        CURLU *h = curl_url();
        CURLUcode rc = curl_url_set(h, CURLUPART_URL, url, 0);

        if(rc != CURLUE_OK) PARSE_ERROR("error parsing URL: %s\n", curl_url_strerror(rc));

        char *fragment, *host, *password,
             *path,     *port, *query,
             *scheme,   *user, *zoneid;

        curl_url_get(h, CURLUPART_FRAGMENT, &fragment, 0);
        curl_url_get(h, CURLUPART_HOST,     &host,     0);
        curl_url_get(h, CURLUPART_PASSWORD, &password, 0);
        curl_url_get(h, CURLUPART_PATH,     &path,     0);
        curl_url_get(h, CURLUPART_PORT,     &port,     0);
        curl_url_get(h, CURLUPART_QUERY,    &query,    0);
        curl_url_get(h, CURLUPART_SCHEME,   &scheme,   0);
        curl_url_get(h, CURLUPART_USER,     &user,     0);
        curl_url_get(h, CURLUPART_ZONEID,   &zoneid,   0);

        #define X(label, string) printf("%s%s\n", (print_labels ? (label ":") : ""), (string == NULL ? "" : string));

        X("FRAGMENT", fragment);
        X("HOST",     host);
        X("PASSWORD", password);
        X("PATH",     path);
        X("PORT",     port);
        X("QUERY",    query);
        X("SCHEME",   scheme);
        X("USER",     user);
        X("ZONEID",   zoneid);

        #undef X
    }

    #undef PARSE_ERROR
}

bool string_to_curl_url_part(CURLUPart *url_part, Sized_String string) {
         if (sized_string_equals_c_string(string, "FRAGMENT")) { *url_part = CURLUPART_FRAGMENT; return true; }
    else if (sized_string_equals_c_string(string, "HOST"))     { *url_part = CURLUPART_HOST;     return true; }
    else if (sized_string_equals_c_string(string, "PASSWORD")) { *url_part = CURLUPART_PASSWORD; return true; }
    else if (sized_string_equals_c_string(string, "PATH"))     { *url_part = CURLUPART_PATH;     return true; }
    else if (sized_string_equals_c_string(string, "PORT"))     { *url_part = CURLUPART_PORT;     return true; }
    else if (sized_string_equals_c_string(string, "QUERY"))    { *url_part = CURLUPART_QUERY;    return true; }
    else if (sized_string_equals_c_string(string, "SCHEME"))   { *url_part = CURLUPART_SCHEME;   return true; }
    else if (sized_string_equals_c_string(string, "USER"))     { *url_part = CURLUPART_USER;     return true; }
    else if (sized_string_equals_c_string(string, "ZONEID"))   { *url_part = CURLUPART_ZONEID;   return true; }
    else return false;
}

Sized_String get_label_from_string(char *input) {
    Sized_String result = {0};

    if(strlen(input) == 0) {
        fprintf(stderr, "Encountered empty input line. Did you make a mistake in your input?\n");
        exit(1);
    }

    result.data  = input;
    result.count = 0;

    while(true) {
        if(result.data[result.count] == '\0') {
            fprintf(stderr, "Invalid input line: '%s'\n", input);
            exit(1);
        } else if(result.data[result.count] == ':') {
            return result;
        }

        result.count += 1;
    }
}

void put_together(void) {
    CURLU *h = curl_url();

    for(size_t i = 0; i < non_flag_arguments.count; i++) {
        char *line = non_flag_arguments.items[i];
        Sized_String label = get_label_from_string(line);

        size_t line_length = strlen(line);
        if(line_length == label.count+1) continue; // empty field

        const char *value = label.data + label.count + 1;

        CURLUPart url_part;
        bool success = string_to_curl_url_part(&url_part, label);

        if(!success) {
            fprintf(stderr, "Unknown label '%.*s'\n", (int)label.count, label.data);
            exit(1);
        }

        CURLUcode rc = curl_url_set(h, url_part, value, 0);

        if(rc != 0) {
            fprintf(stderr, "Failed to set URL part at label '%.*s' with value '%s': %s\n", (int)label.count, label.data, value, curl_url_strerror(rc));
            exit(1);
        }
    }

    char *final_url;
    CURLUcode rc = curl_url_get(h, CURLUPART_URL, &final_url, 0);

    if(rc != 0) {
        fprintf(stderr, "Failed to assemble URL: %s\n", curl_url_strerror(rc));
        exit(1);
    }

    puts(final_url);
}

void replace(void)      { TODO("replace!\n");      }

void process_flag_list(Flag_List flag_list) {

    for(size_t i = 0; i < flag_list.count; i++) {
        if(flag_list.items[i] == FLAG_HELP) {
            print_help();
            exit(0);
        }
    }

    print_labels = true;

    bool found_action_flag = false;

    #define FLAG_ERROR(message) do { fprintf(stderr, message); print_help(); exit(1); } while(0)

    for(size_t i = 0; i < flag_list.count; i++) {
        if(is_action_flag(flag_list.items[i])) {
            if(!found_action_flag) {
                found_action_flag = true;

                switch(flag_list.items[i]) {
                    case FLAG_TAKE_APART:   { action_function = take_apart;   } break;
                    case FLAG_PUT_TOGETHER: { action_function = put_together; } break;
                    case FLAG_REPLACE:      { action_function = replace;      } break;
                    default:
                        assert(false && "Only action flags should be able to reach this switch.");
                }

            } else {
                FLAG_ERROR("Passing more than one action flag is not allowed.\n");
            }
        } else {
            switch(flag_list.items[i]) {
                case FLAG_NO_LABEL: { print_labels = false; } break;
                default:
                    assert(false && "Unimplemented flag.");
            }
        }
    }

    if(!found_action_flag) FLAG_ERROR("At least one action flag is required.\n");

    if(non_flag_arguments.count == 0) {
        char *line;
        while((line = read_line_from_stdin_and_trim()) != NULL) {
            dynamic_array_append(&non_flag_arguments, line);
        }
    }

    if(non_flag_arguments.count == 0) {
        FLAG_ERROR("Missing input. Cloud not find input in arguments or STDIN.\n");
    }

    assert(action_function != NULL && "Action function is NULL!");

    #undef FLAG_ERROR
}

int main(int argc, char **argv) {

    Flag_List flag_list = {0};

    for(int i = 1; i < argc; i++) {

        char *current_argument = argv[i];

        switch(current_argument[0]) {
            case '-': {
                parse_sized_string_into_flag_list(sized_string_from_string(current_argument), &flag_list);
            } break;
            default:
                dynamic_array_append(&non_flag_arguments, current_argument);
        }
    }

    process_flag_list(flag_list);

    action_function();

    return 0;
}


