# Owns the documentation build step (ADR-0002 layout). There is no docs artefact yet:
# doxygen consumes the public headers that story 1.4 provides, so a real target would
# print a green for a tool that has nothing to document. When the headers exist, this
# target gains the doxygen invocation and a source-file output check.
add_custom_target(pc_docs)
