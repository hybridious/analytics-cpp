#
# Copyright 2017 Segment Inc. <friends@segment.com>
#
# This software is supplied under the terms of the MIT License, a
# copy of which should be located in the distribution where this
# file was obtained (LICENSE.txt).  A copy of the license may also be
# found online at https://opensource.org/licenses/MIT.
#

# We use a macro to define additional tests
find_program(VALGRIND valgrind)

macro(add_a_test NAME TIMEOUT)
    # We could link statically, but this is easier.
    add_executable(${NAME} ${NAME}.cpp catch.hpp)
    target_link_libraries(${NAME} ${PROJECT_NAME}_static ${CURL_LIBRARIES})
    add_test(NAME ${NAME} COMMAND ${NAME})
    set_tests_properties(${NAME} PROPERTIES TIMEOUT ${TIMEOUT})
    list(APPEND all_tests ${NAME})

    # add a valgrind check if one exists
    # DISABLED, because libcurl using openssl on Linux (the default) breaks
    # valgrind horribly (Ubuntu Trusty).  Not that this does wonders to
    # improve our confidence in openssl... (NB: gnutls and nss variants
    # seem to be OK, but suffer from their own internal memory leaks --
    # which seem to be primarily global scope, not continuing.)
    # if(VALGRIND)
    #    # Note that we cannot use --leak-check=full because libcurl leaks.
    #    add_test(NAME valgrind-${NAME}
    #        COMMAND ${VALGRIND} ${NAME})
    #    list(APPEND all_tests valgrind-${NAME})
    #endif()
endmacro(add_a_test)

# 60 seconds because gcov tests can take a while
add_a_test(test-submit 60)
