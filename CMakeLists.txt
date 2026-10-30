cmake_minimum_required(VERSION 3.10)

if(DEFINED ENV{VITASDK} AND NOT DESKTOP)
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  set(VITA ON)
endif()

project(ft2vita CXX C)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

file(GLOB SRC src/*.cpp)

if(VITA)
  include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)
  set(VITA_APP_NAME "Fish Tycoon 2")
  set(VITA_TITLEID  "FTYC00002")
  set(VITA_VERSION  "01.00")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -fno-strict-aliasing -Wno-psabi")
  add_executable(${PROJECT_NAME} ${SRC})
  set(PKG_CONFIG_EXECUTABLE "$ENV{VITASDK}/bin/arm-vita-eabi-pkg-config")
  find_package(PkgConfig)
  if(PKG_CONFIG_FOUND)
    pkg_check_modules(VSDL REQUIRED SDL2_mixer SDL2_ttf SDL2_image sdl2)
    target_include_directories(${PROJECT_NAME} PRIVATE ${VSDL_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME} ${VSDL_LDFLAGS} SceAppUtil_stub SceAppMgr_stub ScePower_stub stdc++ m)
  else()
    target_link_libraries(${PROJECT_NAME}
      SDL2_mixer SDL2_ttf SDL2_image SDL2
      freetype harfbuzz freetype png jpeg webp webpdemux vorbisfile vorbis ogg FLAC mikmod mpg123 modplug opusfile opus xmp z bz2 m stdc++
      SceDisplay_stub SceCtrl_stub SceAudio_stub SceAudioIn_stub SceSysmodule_stub SceGxm_stub SceCommonDialog_stub
      SceTouch_stub SceHid_stub SceMotion_stub ScePower_stub SceAppMgr_stub SceAppUtil_stub SceIme_stub SceKernelDmacMgr_stub pthread)
  endif()
  vita_create_self(${PROJECT_NAME}.self ${PROJECT_NAME} UNSAFE)
  vita_create_vpk(${PROJECT_NAME}.vpk ${VITA_TITLEID} ${PROJECT_NAME}.self
    VERSION ${VITA_VERSION}
    NAME ${VITA_APP_NAME}
    FILE sce_sys/icon0.png sce_sys/icon0.png
    FILE sce_sys/livearea/contents/bg.png sce_sys/livearea/contents/bg.png
    FILE sce_sys/livearea/contents/startup.png sce_sys/livearea/contents/startup.png
    FILE sce_sys/livearea/contents/template.xml sce_sys/livearea/contents/template.xml
  )
else()
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(SDL2 REQUIRED sdl2 SDL2_image SDL2_ttf SDL2_mixer)
  add_executable(${PROJECT_NAME} ${SRC})
  target_include_directories(${PROJECT_NAME} PRIVATE ${SDL2_INCLUDE_DIRS})
  target_link_libraries(${PROJECT_NAME} ${SDL2_LIBRARIES})
  target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wno-unused-variable -Wno-unused-function)
endif()
