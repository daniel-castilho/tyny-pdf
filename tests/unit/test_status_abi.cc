#include "pdfcore/status.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    FILE *f = fopen("tests/golden/status_enum.txt", "r");
    if (!f) {
        fprintf(stderr, "Cannot open golden file\n");
        return 1;
    }

    char line[256];
    int expected = 0;
    int errors = 0;

    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        int val = atoi(eq + 1);
        if (val != expected) {
            fprintf(stderr, "Mismatch: %s expected %d got %d\n", line, expected, val);
            errors++;
        }
        expected++;
    }
    fclose(f);

    if (errors) {
        fprintf(stderr, "FAIL: %d mismatches\n", errors);
        return 1;
    }

    pc_status s = PC_STATUS_INIT;
    if (s.size != sizeof(pc_status)) {
        fprintf(stderr, "size mismatch\n");
        return 1;
    }
    if (!pc_status_is_ok(&s)) {
        fprintf(stderr, "PC_STATUS_INIT not ok\n");
        return 1;
    }

    pc_status_set(&s, PC_ERR_ARGUMENT, 42, "test detail");
    if (s.code != PC_ERR_ARGUMENT || s.detail_id != 42 || strcmp(s.detail, "test detail") != 0) {
        fprintf(stderr, "pc_status_set failed\n");
        return 1;
    }

    printf("status_abi: PASS\n");
    return 0;
}