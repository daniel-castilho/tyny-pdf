# Font fallback policy (R20.x) - lives in src/core, never touches a font engine (R-M8).
target_sources(pdfcore PRIVATE
    fallback.cc
)
