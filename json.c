/*
 * json.c
 *
 *  Created on: Mar 11, 2020
 *      Author: Troi-Ryan Stoeffler
 */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "json.h"

typedef struct JsonVar {
    char name[20];
    char value[20];
} JsonVar;

static JsonVar reportedVals[MAX_VAL];
static int currIndex = 0;

// simple json parser going through each thing
void parseJSON(char *json) {
    bool quoteMode = false, colonMode = false, reportMode = false;
    static char str[20] = "", strIndex = 0;
    char i, *j;
    j = json;
    currIndex = 0;
    for (i = 0; i < 10; i++) {
        reportedVals[i] = (JsonVar) { .name = "", .value = "" };
    }
    while (*j != '\0') {
        if (quoteMode) {
            if (strIndex < 20) {
                str[strIndex++] = *j;
            }
        }
        if (*j == '\"') {
            if (!quoteMode) {
                quoteMode = true;
                strIndex = 0;
            } else {
                quoteMode = false;
                str[strIndex - 1] = '\0';
                if (strstr(str, "reported") != NULL) {
                    reportMode = true;
                } else {
                    if (reportMode) {
                        if (currIndex != MAX_VAL) {
                            if (colonMode) {
                                strcpy(reportedVals[currIndex++].value, str);
                                colonMode = false;
                            } else {
                                strcpy(reportedVals[currIndex].name, str);
                            }
                        }
                    }
                }
            }
        } else if (*j == ':') {
            colonMode = true;
        } else if (*j == '{') {
            colonMode = false;
        } else if (*j == '}') {
            if (reportMode) {
                return;
            }
        }
        j++;
    }
}

char* getValue(char *name) {
    int i;
    for (i = 0; i < 10; i++) {
        printf("%s: %s\n", reportedVals[i].name, reportedVals[i].value);
    }
    return NULL;
}
