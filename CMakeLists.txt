# система сборки для сишки

# минимальная версия CMake
cmake_minimum_required(VERSION 3.16)

# название + язык (C)
project(vtsh C)

# стандарт языка C11 
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# флажки для компилятора:
# -Wall -Wextra -Wpedantic - включить все предупреждения
# -g - добавить отладочную информацию (имена переменных и т д)
add_compile_options(-Wall -Wextra -Wpedantic -g)

# список файлов для компиляции
add_executable(vtsh
    src/main.c
    src/repl.c
)

# заголовочные файлы в include
target_include_directories(vtsh PRIVATE include)

# билдим в build/bin/
set_target_properties(vtsh PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)