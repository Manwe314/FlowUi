foreach(runtime_library IN LISTS RUNTIME_LIBRARIES)
    file(COPY "${runtime_library}" DESTINATION "${DESTINATION_DIRECTORY}")
endforeach()
