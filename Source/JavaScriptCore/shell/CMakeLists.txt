set(JSC_SOURCES
    ../jsc.cpp
)

set(JSC_LIBRARIES
    ${CMAKE_DL_LIBS}
    JavaScriptCore${DEBUG_SUFFIX}
)

if (WTF_OS_MAC_OS_X)
    list(APPEND JSC_LIBRARIES edit)
endif ()

if ("${JavaScriptCore_LIBRARY_TYPE}" MATCHES "STATIC")
    add_definitions(-DSTATICALLY_LINKED_WITH_JavaScriptCore)
endif ()

set(TESTAPI_SOURCES
    ../API/tests/CompareAndSwapTest.cpp
    ../API/tests/CustomGlobalObjectClassTest.c
    ../API/tests/ExecutionTimeLimitTest.cpp
    ../API/tests/FunctionOverridesTest.cpp
    ../API/tests/GlobalContextWithFinalizerTest.cpp
    ../API/tests/JSONParseTest.cpp
    ../API/tests/JSObjectGetProxyTargetTest.cpp
    ../API/tests/MultithreadedMultiVMExecutionTest.cpp
    ../API/tests/PingPongStackOverflowTest.cpp
    ../API/tests/TypedArrayCTest.cpp
    ../API/tests/testapi.c
)

WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()

WEBKIT_WRAP_SOURCELIST(${JSC_SOURCES})
WEBKIT_WRAP_SOURCELIST(${TESTAPI_SOURCES})
include_directories(./ ${JavaScriptCore_INCLUDE_DIRECTORIES})
include_directories(SYSTEM ${JavaScriptCore_SYSTEM_INCLUDE_DIRECTORIES})

# A bit of a hack to make sure we always have the WTF includes,
# regardless if WTF is coming to us via WTF directly or statically from
# a static WTF in a shared JavaScriptCore
get_target_property(wtfincludes WTF INTERFACE_INCLUDE_DIRECTORIES)
include_directories(${wtfincludes})
# Another bit of a hack; this is part of WTF's libs, but we might not
# be actually depending on the WTF target if WTF is static inside JSCore
if (MSVC)
  link_libraries(winmm)
endif()

add_executable(jsc ${JSC_SOURCES})
target_link_libraries(jsc ${JSC_LIBRARIES} ${WTF_LIBRARIES})

if (JSC_OUTPUT_NAME)
    set_target_properties(jsc PROPERTIES OUTPUT_NAME ${JSC_OUTPUT_NAME})
endif ()

if (JSC_LINK_FLAGS)
    ADD_TARGET_PROPERTIES(jsc LINK_FLAGS "${JSC_LINK_FLAGS}")
endif ()

if (SHOULD_INSTALL_JS_SHELL)
    install(TARGETS jsc DESTINATION "${LIBEXEC_INSTALL_DIR}")
endif ()

if (TARGET jscLib)
    add_dependencies(jsc jscLib)
endif ()

if (NOT WIN32)
    set(TESTB3_SOURCES
        ../b3/testb3.cpp
    )

    set(TESTAIR_SOURCES
        ../b3/air/testair.cpp
    )

    add_executable(testb3 ${TESTB3_SOURCES})
    target_link_libraries(testb3 ${JSC_LIBRARIES})

    add_executable(testair ${TESTAIR_SOURCES})
    target_link_libraries(testair ${JSC_LIBRARIES})

    add_executable(testapi ${TESTAPI_SOURCES})
    target_link_libraries(testapi ${JSC_LIBRARIES})
endif ()

file(COPY
    "${JAVASCRIPTCORE_DIR}/API/tests/testapi.js"
    DESTINATION
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)
file(COPY
    "${JAVASCRIPTCORE_DIR}/API/tests/testapi-function-overrides.js"
    DESTINATION
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)
