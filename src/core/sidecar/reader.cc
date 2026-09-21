#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pdfcore/sidecar.h"
#include "pdfcore/status.h"

pc_status pc_sidecar_read(const char* doc_path, pc_sidecar** out_sidecar);

void pc_sidecar_free(pc_sidecar* sidecar);