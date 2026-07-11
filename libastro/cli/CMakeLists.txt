# The library target is `astro`; name the executable target differently but
# ship it as the `astro` binary.
add_executable(astro_cli astro.cpp)
set_target_properties(astro_cli PROPERTIES OUTPUT_NAME astro)
target_link_libraries(astro_cli PRIVATE astro::astro astro::argparse)
