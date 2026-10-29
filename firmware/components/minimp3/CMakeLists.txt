idf_component_register(
    SRCS "minimp3.c"
    INCLUDE_DIRS "include"
)

# The decoder is the hot path: optimise it even in debug builds,
# and don't let third-party style warnings break the build.
target_compile_options(${COMPONENT_LIB} PRIVATE -O2 -w)
