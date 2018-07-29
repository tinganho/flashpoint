
# @summary
# Print a list with each item separated with a newline
#
# @param LIST list
#
function(list_to_string RESULT DELIMITER)
    list(GET ARGV 2 TEMP)
    math(EXPR N "${ARGC}-1")
    foreach(IDX RANGE 3 ${N})
        list(GET ARGV ${IDX} STRING)
        set(TEMP "${TEMP}${DELIMITER}${STRING}")
    endforeach()
    set(${RESULT} "${TEMP}" PARENT_SCOPE)
endfunction(list_to_string)

# @summary
# Print a list with each item separated with a newline
#
# @param LIST list
#
function(print_list LIST)
    if(WIN32)
        set(NEWLINE "\r\n")
    else()
        set(NEWLINE "\n")
    endif()
    list_to_string(LIST_STRING "${NEWLINE}" ${LIST})
    message(STATUS ${LIST_STRING})
endfunction(print_list)