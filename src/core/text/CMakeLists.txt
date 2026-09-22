# Font fallback policy (R20.x) and grapheme break/caret arithmetic (R21.x, R23.x) - live in
# src/core, never touch a font engine (R-M8).
target_sources(pdfcore PRIVATE
    fallback.cc
    break.cc
    caret.cc
)
