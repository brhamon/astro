add_executable(planets planets.cpp)
target_link_libraries(planets PRIVATE astro::astro)
