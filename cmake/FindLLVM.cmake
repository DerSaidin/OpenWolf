# Detect LLVM and set various variable to link against the different component of LLVM
#
# LLVM_BIN_DIR : directory with LLVM binaries
# LLVM_LIB_DIR : directory with LLVM library
# LLVM_INCLUDE_DIR : directory with LLVM include
#
# LLVM_COMPILE_FLAGS : compile flags needed to build a program using LLVM headers
# LLVM_LDFLAGS : ldflags needed to link
# LLVM_LIBS : ldflags needed for our LLVM stuff

SET(LLVM_FOUND FALSE)

if (LLVM_INCLUDE_DIR)
  set(LLVM_FOUND TRUE)
else (LLVM_INCLUDE_DIR)

  find_program(LLVM_CONFIG_EXECUTABLE
      NAMES llvm-config
      PATHS
      /opt/local/bin
  )

  if(LLVM_CONFIG_EXECUTABLE)
      
      message(STATUS "Found llvm-config: ${LLVM_CONFIG_EXECUTABLE}")
      set(LLVM_CONFIG_SHELL perl)
      
      MACRO(FIND_LLVM_LIBS LLVM_CONFIG_EXECUTABLE _libname_ LIB_VAR OBJECT_VAR)
        exec_program(${LLVM_CONFIG_SHELL} ARGS ${LLVM_CONFIG_EXECUTABLE} --libs ${_libname_}  OUTPUT_VARIABLE ${LIB_VAR} )
        STRING(REGEX MATCHALL "[^ ]*[.]o[ $]"  ${OBJECT_VAR} ${${LIB_VAR}})
        SEPARATE_ARGUMENTS(${OBJECT_VAR})
        STRING(REGEX REPLACE "[^ ]*[.]o[ $]" ""  ${LIB_VAR} ${${LIB_VAR}})
      ENDMACRO(FIND_LLVM_LIBS)
      
      
      exec_program(${LLVM_CONFIG_SHELL} ARGS ${LLVM_CONFIG_EXECUTABLE} --bindir OUTPUT_VARIABLE LLVM_BIN_DIR )
      exec_program(${LLVM_CONFIG_SHELL} ARGS ${LLVM_CONFIG_EXECUTABLE} --libdir OUTPUT_VARIABLE LLVM_LIB_DIR )
      #MESSAGE(STATUS "LLVM lib dir: " ${LLVM_LIB_DIR})
      exec_program(${LLVM_CONFIG_SHELL} ARGS ${LLVM_CONFIG_EXECUTABLE} --includedir OUTPUT_VARIABLE LLVM_INCLUDE_DIR )
      
      
      exec_program(${LLVM_CONFIG_SHELL} ARGS ${LLVM_CONFIG_EXECUTABLE} --cppflags  OUTPUT_VARIABLE TEMP )
      STRING(REGEX REPLACE "[\n\r]" "" LLVM_COMPILE_FLAGS "${TEMP}")
      exec_program(${LLVM_CONFIG_SHELL} ARGS ${LLVM_CONFIG_EXECUTABLE} --ldflags   OUTPUT_VARIABLE TEMP )
      STRING(REGEX REPLACE "[\n\r]" " " TEMP "${TEMP}")
      STRING(REGEX MATCHALL "-l[^ ]+" LLVM_LIBS1 "${TEMP}")
      STRING(REGEX REPLACE "-l[^ ]+" "" LLVM_LDFLAGS "${TEMP}")
      exec_program(${LLVM_CONFIG_SHELL} ARGS ${LLVM_CONFIG_EXECUTABLE} --libs ${LLVM_COMPONENTS} OUTPUT_VARIABLE TEMP )
      STRING(REGEX REPLACE "[\n\r]" "" LLVM_LIBS2 "${TEMP}")
      
      set(LLVM_LIBS "${LLVM_LIBS2} ${LLVM_LIBS1}")

      MESSAGE(STATUS "LLVM CXX flags: " ${LLVM_COMPILE_FLAGS})
      MESSAGE(STATUS "LLVM LD flags: " ${LLVM_LDFLAGS})
      MESSAGE(STATUS "LLVM libs: " ${LLVM_LIBS})

  endif(LLVM_CONFIG_EXECUTABLE)
  
  if(LLVM_INCLUDE_DIR)
    set(LLVM_FOUND TRUE)
    
    set(LLVM_BIN_DIR ${LLVM_BIN_DIR} CACHE PATH "LLVM bin directory")
    set(LLVM_LIB_DIR ${LLVM_LIB_DIR} CACHE PATH "LLVM lib directory")
    set(LLVM_INCLUDE_DIR ${LLVM_INCLUDE_DIR} CACHE PATH "LLVM include directory")
    set(LLVM_COMPILE_FLAGS ${LLVM_COMPILE_FLAGS} CACHE STRING "LLVM compile flags")
    set(LLVM_LDFLAGS ${LLVM_LDFLAGS} CACHE STRING "LLVM linker flags")
    set(LLVM_LIBS ${LLVM_LIBS} CACHE STRING "LLVM libraries")
    
  endif(LLVM_INCLUDE_DIR)
  
  MARK_AS_ADVANCED(
    LLVM_BIN_DIR
    LLVM_LIB_DIR
    LLVM_INCLUDE_DIR
    LLVM_COMPILE_FLAGS
    LLVM_LDFLAGS
    LLVM_LIBS
  )

  if(LLVM_FOUND)
    message(STATUS "Found LLVM: ${LLVM_INCLUDE_DIR}")
  else(LLVM_FOUND)
    if(LLVM_FIND_REQUIRED)
      message(FATAL_ERROR "LLVM was not found.")
    endif(LLVM_FIND_REQUIRED)
  endif(LLVM_FOUND)
  
endif (LLVM_INCLUDE_DIR)
