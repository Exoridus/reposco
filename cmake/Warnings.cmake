function(reposco_owned_target target)
  target_compile_features(${target} PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(
      ${target}
      PRIVATE /W4
              /WX
              /w44062
              /permissive-
              /sdl
              /utf-8
              /Zc:__cplusplus
              /EHsc)
    target_compile_definitions(${target} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    if(REPOSCO_SANITIZERS)
      target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
      target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    endif()
  endif()
  if(REPOSCO_USE_SCCACHE)
    find_program(REPOSCO_SCCACHE NAMES sccache REQUIRED)
    set_property(TARGET ${target} PROPERTY CXX_COMPILER_LAUNCHER "${REPOSCO_SCCACHE}")
  endif()
endfunction()
