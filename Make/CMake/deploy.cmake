if(CMAKE_HOST_WIN32)
  add_custom_target(deploy "${BHUMAN_PREFIX}/Util/CallTool/bin/CallTool.exe" CONFIG=$<CONFIG> /c "deploy %%CONFIG%% 192.168.5.xxx -nc" /t Deploy "deploy %%CONFIG%% ip [-b] (restart) [-d] (delete logs) [-l location] [-p player] [-s scenario] [-t team] [-v volume] [-w wifi]" WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../Windows")
  set_property(TARGET deploy PROPERTY FOLDER Utils)
  add_dependencies(deploy Nao)
endif()
