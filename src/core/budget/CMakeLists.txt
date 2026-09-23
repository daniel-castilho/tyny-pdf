# Budget subsystem — memory ceilings for render tiles and transactions.
# Engine-free, builds on all platforms.

add_library(tynypdf-budget OBJECT
    budget.cc
)

target_include_directories(tynypdf-budget PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)



pc_apply_core_flags(tynypdf-budget)
pc_apply_warnings(tynypdf-budget)

# Alias for backward compatibility
add_library(tynypdf-budget-static STATIC $<TARGET_OBJECTS:tynypdf-budget>)
