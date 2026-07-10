add_library(queue queue.c)
add_library(http http.c)
target_include_directories(
  http
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
target_link_libraries(http PRIVATE queue)
