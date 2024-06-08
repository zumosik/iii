cmake_minimum_required(VERSION 3.10)

project(iii)

add_executable(iii src/main.c src/memory.c src/chunk.c src/value.c src/debug.c src/vm.c src/compiler.c src/scanner.c src/object.c src/table.c src/locals.c)