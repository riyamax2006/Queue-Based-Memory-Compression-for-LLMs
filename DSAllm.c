#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define STQ_SIZE 5
#define LTQ_SIZE 10
#define MSG_LEN 256

//------------------------------------------------------------
// DICTIONARY-BASED SYMBOL COMPRESSION
//------------------------------------------------------------
typedef struct {
    const char* word;
    const char* symbol;
} Dict;

/* dictionary */
Dict dict[] = {
    {"information", "@"},
    {"message", "#"},
    {"compression", "$"},
    {"system", "?"},
    {"temperature", "&"},
    {"sensor", "*"},
    {"battery", "+"},
    {"current", "="},
    {"vibration", "~"},
    {"monitor", "!"}
};

#define DICT_SIZE 10

/* stopwords */
const char* stopwords[] = {
    "the","is","am","are","to","a","an","in",
    "on","of","and","for","with","this","that", NULL
};

//------------------------------------------------------------
// UTILITY FUNCTIONS
//------------------------------------------------------------
int is_stopword(const char* word) {
    for (int i = 0; stopwords[i]; i++) {
        if (strcmp(word, stopwords[i]) == 0)
            return 1;
    }
    return 0;
}

const char* dict_replace(const char* word) {
    for (int i = 0; i < DICT_SIZE; i++) {
        if (strcmp(word, dict[i].word) == 0)
            return dict[i].symbol;
    }
    return word;
}

/* simple stemming */
void stem(char* word) {
    int len = strlen(word);

    if (len > 4 && strcmp(word + len - 3, "ing") == 0)
        word[len - 3] = '\0';
    else if (len > 3 && strcmp(word + len - 2, "ed") == 0)
        word[len - 2] = '\0';
    else if (len > 3 && strcmp(word + len - 2, "es") == 0)
        word[len - 2] = '\0';
    else if (len > 2 && word[len - 1] == 's')
        word[len - 1] = '\0';
}

//------------------------------------------------------------
// COMPRESSION FUNCTION
//------------------------------------------------------------
char* compress_message(const char* msg) {
    static char output[MSG_LEN];
    char clean[MSG_LEN];
    char word[64];

    int i = 0, j = 0;

    /* normalize: lowercase + remove punctuation */
    for (; msg[i] && j < MSG_LEN - 1; i++) {
        if (isalnum((unsigned char)msg[i]) || isspace((unsigned char)msg[i])) {
            clean[j++] = tolower((unsigned char)msg[i]);
        }
    }
    clean[j] = '\0';

    output[0] = '\0';
    i = 0;

    while (clean[i]) {

        while (isspace(clean[i])) i++;
        if (!clean[i]) break;

        j = 0;
        while (clean[i] && !isspace(clean[i]) && j < 63) {
            word[j++] = clean[i++];
        }
        word[j] = '\0';

        if (is_stopword(word))
            continue;

        stem(word);

        const char* out = dict_replace(word);

        if (strlen(output) + strlen(out) + 2 < MSG_LEN) {
            strcat(output, out);
            strcat(output, " ");
        }
    }

    int len = strlen(output);
    if (len > 0 && output[len - 1] == ' ')
        output[len - 1] = '\0';

    return output;
}

//------------------------------------------------------------
// SHORT-TERM QUEUE (RAW)
//------------------------------------------------------------
char STQ[STQ_SIZE][MSG_LEN];
int stq_front = 0;
int stq_count = 0;

void stq_push(const char* msg) {
    strcpy(STQ[(stq_front + stq_count) % STQ_SIZE], msg);
    stq_count++;
}

char* stq_pop_oldest() {
    char* oldest = STQ[stq_front];
    stq_front = (stq_front + 1) % STQ_SIZE;
    stq_count--;
    return oldest;
}

//------------------------------------------------------------
// LONG-TERM QUEUE (COMPRESSED)
//------------------------------------------------------------
char LTQ[LTQ_SIZE][MSG_LEN];
int ltq_count = 0;

void ltq_push(const char* msg) {
    if (ltq_count < LTQ_SIZE) {
        strcpy(LTQ[ltq_count++], msg);
    }
}

//------------------------------------------------------------
// EXTERNAL INTERFACE (CALLED FROM PYTHON / LLM)
//------------------------------------------------------------
void process_message(const char* msg) {
    if (stq_count < STQ_SIZE) {
        stq_push(msg);
    } else {
        char* oldest = stq_pop_oldest();
        char* compressed = compress_message(oldest);
        ltq_push(compressed);
        stq_push(msg);
    }
}

void export_context(const char* filename) {
    FILE* f = fopen(filename, "w");
    if (!f) return;

    for (int i = 0; i < ltq_count; i++) {
        fprintf(f, "%s\n", LTQ[i]);
    }
    fclose(f);
}
void export_full_state(const char* filename) {
    FILE* f = fopen(filename, "w");
    if (!f) return;

    fprintf(f, "=== SHORT TERM QUEUE (STQ) ===\n");
    for (int i = 0; i < stq_count; i++) {
        int idx = (stq_front + i) % STQ_SIZE;
        fprintf(f, "%d: %s\n", i + 1, STQ[idx]);
    }

    if (stq_count == 0)
        fprintf(f, "[EMPTY]\n");

    fprintf(f, "\n=== LONG TERM QUEUE (LTQ / COMPRESSED) ===\n");
    for (int i = 0; i < ltq_count; i++) {
        fprintf(f, "%d: %s\n", i + 1, LTQ[i]);
    }

    if (ltq_count == 0)
        fprintf(f, "[EMPTY]\n");

    fclose(f);
}
