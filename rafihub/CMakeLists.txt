# RafiHub – game-launcher van RafiOS (C++ en Qt 6)
cmake_minimum_required(VERSION 3.16)
project(rafihub VERSION 0.4 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Test)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# De kern: alles behalve de interface. Wordt ook door de tests gebruikt.
add_library(rafihubkern STATIC
    src/vdf.cpp
    src/steambibliotheek.cpp
    src/opslag.cpp
    src/startopdracht.cpp
    src/gamemodel.cpp
    src/hub.cpp
)
target_link_libraries(rafihubkern PUBLIC Qt6::Core Qt6::Gui)

add_executable(rafihub src/main.cpp src/gamepad.cpp qml/qml.qrc)
target_link_libraries(rafihub PRIVATE rafihubkern Qt6::Qml Qt6::Quick Qt6::QuickControls2)
target_compile_definitions(rafihub PRIVATE RAFIHUB_VERSIE="${PROJECT_VERSION}")
# Controller: SDL2 wordt pas bij het starten geladen (zie src/gamepad.cpp),
# dus er is geen SDL2-ontwikkelpakket nodig om te bouwen.

# Tests: 'make test' (of ctest)
enable_testing()
add_executable(test_kern tests/test_kern.cpp)
target_link_libraries(test_kern PRIVATE rafihubkern Qt6::Test)
add_test(NAME kern COMMAND test_kern)
add_test(NAME interface COMMAND rafihub --zelftest)
set_tests_properties(kern interface PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

install(TARGETS rafihub RUNTIME DESTINATION bin)
install(FILES data/rafihub.desktop DESTINATION share/applications)
install(FILES data/rafihub.svg DESTINATION share/icons/hicolor/scalable/apps)
