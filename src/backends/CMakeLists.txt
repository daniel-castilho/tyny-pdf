# Backend adapters: the only tree allowed to know an engine (ADR-0001 rule 4,
# ADR-0002 rule 4, ADR-0011 R-M5). The mupdf backend is added with story 1.3/1.4 once the
# engine is vendored; every backend has a null counterpart, and the contract suite runs
# against all of them.
add_subdirectory(null)
