#ifndef PDFCORE_H
#define PDFCORE_H

// Aggregate public surface of the core (ADR-0002). A file that wants the whole seam includes
// this one header; the individual headers stay includable on their own.

#include "pdfcore/backend.h"
#include "pdfcore/doc.h"
#include "pdfcore/geom.h"
#include "pdfcore/page.h"
#include "pdfcore/status.h"

#endif