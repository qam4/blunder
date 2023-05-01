install(
    TARGETS blunder_exe
    RUNTIME COMPONENT blunder_Runtime
)

if(PROJECT_IS_TOP_LEVEL)
  include(CPack)
endif()
