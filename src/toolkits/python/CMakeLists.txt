# http://stackoverflow.com/questions/13298504/using-cmake-with-setup-py
# http://bloerg.net/2012/11/10/cmake-and-distutils.html

find_program(PYTHON "python")

if (PYTHON)
  set (SETUP_PY_IN "${CMAKE_CURRENT_SOURCE_DIR}/setup.py.in")
  set (SETUP_PY "${CMAKE_CURRENT_BINARY_DIR}/setup.py")
  set (DEPS "${CMAKE_CURRENT_SOURCE_DIR}/mechanic/__init__.py")
  set (OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/build/timestamp")

  configure_file(${SETUP_PY_IN} ${SETUP_PY})
  add_custom_command(OUTPUT ${OUTPUT}
    COMMAND ${PYTHON} setup.py build
    COMMAND ${CMAKE_COMMAND} -E touch ${OUTPUT}
    DEPENDS ${DEPS})

  add_custom_target(python_toolkit ALL DEPENDS ${OUTPUT})
  install(CODE "execute_process(COMMAND ${PYTHON} ${SETUP_PY} install)")
endif (PYTHON)
