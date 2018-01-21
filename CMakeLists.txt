cmake_minimum_required(VERSION 3.9)
project(osproject)
add_executable(os /home/bdrogers/usloss/src/main.c)
install(TARGETS os DESTINATION bin)
