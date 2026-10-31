# Pinned C engine, without any upstream platform backend or platform main().
set(_doom_c_sources
  dummy am_map doomdef doomstat dstrings d_event d_items d_iwad d_loop d_main
  d_mode d_net f_finale f_wipe g_game hu_lib hu_stuff info i_cdmus i_endoom
  i_joystick i_scale i_sound i_system i_timer memio m_argv m_bbox m_cheat
  m_config m_controls m_fixed m_menu m_misc m_random p_ceilng p_doors p_enemy
  p_floor p_inter p_lights p_map p_maputl p_mobj p_plats p_pspr p_saveg p_setup
  p_sight p_spec p_switch p_telept p_tick p_user r_bsp r_data r_draw r_main
  r_plane r_segs r_sky r_things sha1 sounds statdump st_lib st_stuff s_sound
  tables v_video wi_stuff w_checksum w_file w_main w_wad z_zone w_file_stdc
  i_input i_video doomgeneric
)

# Exact guarded substitutions keep the fetched upstream tree pristine. These
# are host integration changes, not changes to Doom's simulation/renderer.
function(rootdoom_replace variable before after)
  string(FIND "${${variable}}" "${before}" position)
  if(position EQUAL -1)
    message(FATAL_ERROR "Pinned doomgeneric patch context changed: ${before}")
  endif()
  string(REPLACE "${before}" "${after}" result "${${variable}}")
  set(${variable} "${result}" PARENT_SCOPE)
endfunction()

set(_patched "${CMAKE_CURRENT_BINARY_DIR}/doomgeneric-patched")
file(MAKE_DIRECTORY "${_patched}")
foreach(name IN LISTS _doom_c_sources)
  set(path "${doomgeneric_SOURCE_DIR}/doomgeneric/${name}.c")
  if(name STREQUAL "i_system")
    file(READ "${path}" content)
    rootdoom_replace(content "void I_Error (char *error, ...)\n{"
      "void I_Error (char *error, ...)\n{\n    va_list rootdoom_args;\n    va_start(rootdoom_args, error);\n    RootDoom_ReportError(error, rootdoom_args);\n    va_end(rootdoom_args);\n    RootDoom_Exit(1);")
    rootdoom_replace(content "#if ORIGCODE\n    SDL_Quit();\n\n    exit(0);\n#endif\n}"
      "    RootDoom_Exit(0);\n#if ORIGCODE\n    SDL_Quit();\n\n    exit(0);\n#endif\n}")
  elseif(name STREQUAL "d_main")
    file(READ "${path}" content)
    rootdoom_replace(content "M_SetConfigDir(NULL);" "M_SetConfigDir(RootDoom_StateDirectory());")
  elseif(name STREQUAL "i_timer")
    file(READ "${path}" content)
    rootdoom_replace(content "(ticks * TICRATE) / 1000" "((uint64_t) ticks * TICRATE) / 1000")
  else()
    list(APPEND _doom_files "${path}")
    continue()
  endif()
  set(path "${_patched}/${name}.c")
  file(WRITE "${path}" "/* ROOTDoom host integration changes, 2026-10-31; see cmake/DoomEngine.cmake. */\n${content}")
  list(APPEND _doom_files "${path}")
endforeach()

add_library(doomgeneric_engine STATIC ${_doom_files})
# Doom uses generic C names such as SHA1_Update, also exported by OpenSSL in
# a ROOT host. Keep all engine implementation symbols local to our library.
set_target_properties(doomgeneric_engine PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED ON C_VISIBILITY_PRESET hidden)
target_include_directories(doomgeneric_engine PRIVATE
  "${doomgeneric_SOURCE_DIR}/doomgeneric" "${PROJECT_SOURCE_DIR}/include")
target_compile_definitions(doomgeneric_engine PRIVATE ROOTDOOM_ENGINE NORMALUNIX _DEFAULT_SOURCE)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_compile_definitions(doomgeneric_engine PRIVATE LINUX)
endif()
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  # Required to unwind C frames into the C++ worker boundary on quit/error/stop.
  target_compile_options(doomgeneric_engine PRIVATE -fexceptions
    -include "${PROJECT_SOURCE_DIR}/include/rootdoom/EngineHooks.h")
else()
  message(FATAL_ERROR "The controlled engine boundary currently requires GCC or Clang C unwind support")
endif()
if(UNIX)
  target_link_libraries(doomgeneric_engine PUBLIC m)
endif()

add_library(rootdoom_engine STATIC src/DoomPlatformAdapter.cpp)
set_target_properties(rootdoom_engine PROPERTIES
  CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN YES)
target_include_directories(rootdoom_engine PUBLIC include)
target_include_directories(rootdoom_engine PRIVATE "${doomgeneric_SOURCE_DIR}/doomgeneric")
target_link_libraries(rootdoom_engine PUBLIC rootdoom_core doomgeneric_engine)
rootdoom_project_options(rootdoom_engine)
