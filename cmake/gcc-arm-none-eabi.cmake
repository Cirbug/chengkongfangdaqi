set(CMAKE_SYSTEM_NAME               Generic)
set(CMAKE_SYSTEM_PROCESSOR          arm)

set(CMAKE_C_COMPILER_ID GNU)
set(CMAKE_CXX_COMPILER_ID GNU)

# Locate the GNU Arm toolchain. STM32Cube for VS Code installs it below
# %LOCALAPPDATA%/stm32cube/bundles, which is normally not added to PATH.
set(STM32_TOOLCHAIN_PATH "" CACHE PATH
    "Path to the GNU Arm Embedded toolchain bin directory")

set(_ARM_GNU_HINTS)
if(STM32_TOOLCHAIN_PATH)
    list(APPEND _ARM_GNU_HINTS
        "${STM32_TOOLCHAIN_PATH}"
        "${STM32_TOOLCHAIN_PATH}/bin")
endif()

if(DEFINED ENV{STM32_TOOLCHAIN_PATH})
    list(APPEND _ARM_GNU_HINTS
        "$ENV{STM32_TOOLCHAIN_PATH}"
        "$ENV{STM32_TOOLCHAIN_PATH}/bin")
endif()

if(WIN32 AND DEFINED ENV{LOCALAPPDATA})
    file(GLOB _STM32CUBE_GNU_BINS LIST_DIRECTORIES true
        "$ENV{LOCALAPPDATA}/stm32cube/bundles/gnu-tools-for-stm32/*/bin")
    list(SORT _STM32CUBE_GNU_BINS COMPARE NATURAL ORDER DESCENDING)
    list(APPEND _ARM_GNU_HINTS ${_STM32CUBE_GNU_BINS})
endif()

find_program(ARM_NONE_EABI_GCC arm-none-eabi-gcc HINTS ${_ARM_GNU_HINTS})
find_program(ARM_NONE_EABI_GXX arm-none-eabi-g++ HINTS ${_ARM_GNU_HINTS})
find_program(ARM_NONE_EABI_OBJCOPY arm-none-eabi-objcopy HINTS ${_ARM_GNU_HINTS})
find_program(ARM_NONE_EABI_SIZE arm-none-eabi-size HINTS ${_ARM_GNU_HINTS})

if(NOT ARM_NONE_EABI_GCC OR NOT ARM_NONE_EABI_GXX)
    message(FATAL_ERROR
        "GNU Arm Embedded toolchain was not found. Install the STM32Cube "
        "toolchain or set STM32_TOOLCHAIN_PATH to its bin directory.")
endif()

set(CMAKE_C_COMPILER                "${ARM_NONE_EABI_GCC}")
set(CMAKE_ASM_COMPILER              ${CMAKE_C_COMPILER})
set(CMAKE_CXX_COMPILER              "${ARM_NONE_EABI_GXX}")
set(CMAKE_LINKER                    "${ARM_NONE_EABI_GXX}")
set(CMAKE_OBJCOPY                   "${ARM_NONE_EABI_OBJCOPY}")
set(CMAKE_SIZE                      "${ARM_NONE_EABI_SIZE}")

set(CMAKE_EXECUTABLE_SUFFIX_ASM     ".elf")
set(CMAKE_EXECUTABLE_SUFFIX_C       ".elf")
set(CMAKE_EXECUTABLE_SUFFIX_CXX     ".elf")

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# MCU specific flags
set(TARGET_FLAGS "-mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard ")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${TARGET_FLAGS}")
set(CMAKE_ASM_FLAGS "${CMAKE_C_FLAGS} -x assembler-with-cpp -MMD -MP")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -fdata-sections -ffunction-sections -fstack-usage")

# The cyclomatic-complexity parameter must be defined for the Cyclomatic complexity feature in STM32CubeIDE to work.
# However, most GCC toolchains do not support this option, which causes a compilation error; for this reason, the feature is disabled by default.
# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fcyclomatic-complexity")

set(CMAKE_C_FLAGS_DEBUG "-O0 -g3")
set(CMAKE_C_FLAGS_RELEASE "-Os -g0")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3")
set(CMAKE_CXX_FLAGS_RELEASE "-Os -g0")

set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -fno-rtti -fno-exceptions -fno-threadsafe-statics")

set(CMAKE_EXE_LINKER_FLAGS "${TARGET_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -T \"${CMAKE_SOURCE_DIR}/STM32F407xx_FLASH.ld\"")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --specs=nano.specs")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-Map=${CMAKE_PROJECT_NAME}.map -Wl,--gc-sections")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--print-memory-usage")
set(TOOLCHAIN_LINK_LIBRARIES "m")
