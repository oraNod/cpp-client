if (NOT JAVA_COMPILE)
    message(FATAL_ERROR "Java javac compiler not found")
endif (NOT JAVA_COMPILE)
if (NOT JAVA_ARCHIVE)
    message(FATAL_ERROR "Java jar archiver not found")
endif (NOT JAVA_ARCHIVE)
message(STATUS "got runtime ${JAVA_RUNTIME}")

find_package(JNI)
if (NOT JNI_LIBRARIES)
    message(FATAL_ERROR "Java JNI support not found")
endif (NOT JNI_LIBRARIES)
message(STATUS "Using JNI libraries: ${JNI_LIBRARIES}" )

find_package(SWIG REQUIRED)
if (NOT SWIG_FOUND)
    message(FATAL_ERROR "SWIG not found")
endif (NOT SWIG_FOUND)
include(UseSWIG)

find_program(MVN_PROGRAM "mvn")
if (MVN_PROGRAM STREQUAL "MVN_PROGRAM-NOTFOUND")
    message(WARNING "Apache Maven (mvn) not found in path")
    message(WARNING "If you don't need the xunit test suite, this should be ok.")
endif (MVN_PROGRAM STREQUAL "MVN_PROGRAM-NOTFOUND")

if(WIN32 AND NOT CYGWIN)
  set (CLASSPATH_SEPARATOR ";")
else (WIN32 AND NOT CYGWIN)
  set (CLASSPATH_SEPARATOR ":")
endif(WIN32 AND NOT CYGWIN)

set(JNI_DIR "${CMAKE_CURRENT_BINARY_DIR}/jni")

if(EXISTS ${JNI_DIR})
    file(REMOVE_RECURSE ${JNI_DIR})
endif(EXISTS ${JNI_DIR})
  
file(COPY ${CMAKE_SOURCE_DIR}/jni DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
configure_file(jni/pom.xml ${PROJECT_BINARY_DIR}/jni/pom.xml  @ONLY IMMEDIATE)

set(CMAKE_SWIG_OUTDIR "${JNI_DIR}/src/main/java/org/infinispan/client/hotrod/jni")
set(CMAKE_SWIG_FLAGS -package "org.infinispan.client.hotrod.jni")
set_source_files_properties("jni/src/main/swig/java.i" PROPERTIES CPLUSPLUS ON)

if (${CMAKE_VERSION} VERSION_LESS "3.8.0")
    swig_add_module(hotrod-swig java "${CMAKE_CURRENT_SOURCE_DIR}/jni/src/main/swig/java.i")
else()
    swig_add_library(hotrod-swig
        LANGUAGE java
        SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/jni/src/main/swig/java.i")
endif()

include_directories(${JNI_INCLUDE_DIRS})
if(DEFINED HOTROD_PREBUILT_LIB_DIR)
    set (INCLUDE_FILES_DIR ${HOTROD_PREBUILT_LIB_DIR}/../include)
    include_directories(${HOTROD_PREBUILT_LIB_DIR}/../include)
    include_directories(${PROTOBUF_INCLUDE_DIR})
endif(DEFINED HOTROD_PREBUILT_LIB_DIR)
swig_link_libraries(hotrod-swig hotrod hotrod_protobuf ${PROTOBUF_LIBRARY})

set_target_properties(hotrod-swig
    PROPERTIES
    OUTPUT_NAME "hotrod-jni"
    PREFIX "${CMAKE_SHARED_LIBRARY_PREFIX}")
set_target_properties(hotrod-swig PROPERTIES COMPILE_DEFINITIONS "${DLLEXPORT}" )

if (CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set_source_files_properties("${JNI_DIR}/src/main/swig/javaJAVA_wrap.cxx" PROPERTIES COMPILE_FLAGS "-w -std=c++11")
endif (CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
if (MSVC)
        set_target_properties(hotrod-swig PROPERTIES COMPILE_FLAGS "/wd4275 /wd4251")
        set_source_files_properties("${JNI_DIR}/src/main/swig/javaJAVA_wrap.cxx" PROPERTIES COMPILE_FLAGS "/wd4275 /wd4251")
endif (MSVC)

file(GLOB_RECURSE JAVA_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/jni *.java)

if(DEFINED maven.version.org.infinispan)
  set(MVN_ISPN_VER_OPT "-Dversion.org.infinispan=${maven.version.org.infinispan}")
endif(DEFINED maven.version.org.infinispan)

if(DEFINED maven.settings.file)
  set(MVN_SETTINGS_FILE_OPT -s ${maven.settings.file})
else(DEFINED maven.settings.file)
  set(MVN_SETTINGS_FILE_OPT "")
endif(DEFINED maven.settings.file)

add_custom_command(OUTPUT ${JNI_DIR}/target/org/infinispan/client/jni/hotrod/JniTest.class
    COMMAND ${MVN_PROGRAM}
    ARGS "-B" ${MVN_SETTINGS_FILE_OPT} ${MVN_ISPN_VER_OPT} "package"
    DEPENDS ${JAVA_SOURCES} 
    WORKING_DIRECTORY "${JNI_DIR}" 
)

add_custom_target(JniTest ALL DEPENDS ${JNI_DIR}/target/org/infinispan/client/jni/hotrod/JniTest.class hotrod-swig)

#Target for deploying the jar to the maven repo. Usage: cmake --build . --target JniDeploy
add_custom_target(JniDeploy
  ${MVN_PROGRAM} "-B" "${MAVEN_SETTINGS_FILE_OPTS}" "${MVN_ISPN_VER_OPT}" "deploy"
  DEPENDS ${JNI_DIR}/target/org/infinispan/client/jni/hotrod/JniTest.class hotrod-swig
  WORKING_DIRECTORY "${JNI_DIR}")

#For generators with multiple configurations make sure all of the possible target locations are in the java.library.path
set(JAVA_LIBRARY_PATH ".")
foreach(loop_var ${CMAKE_CONFIGURATION_TYPES})
	set(JAVA_LIBRARY_PATH "${JAVA_LIBRARY_PATH}${CLASSPATH_SEPARATOR}${loop_var}")
endforeach(loop_var)
if(DEFINED HOTROD_PREBUILT_LIB_DIR)
	set(JAVA_LIBRARY_PATH "${JAVA_LIBRARY_PATH}${CLASSPATH_SEPARATOR}${HOTROD_PREBUILT_LIB_DIR}")
endif(DEFINED HOTROD_PREBUILT_LIB_DIR)

add_test(swig ${JAVA_RUNTIME} 
    -ea 
    "-Djava.net.preferIPv4Stack=true"
    "-Djava.library.path=${JAVA_LIBRARY_PATH}" 
    -cp "jni/target/hotrod-jni.jar${CLASSPATH_SEPARATOR}jni/target/dependency/*${CLASSPATH_SEPARATOR}jni/target/test-classes"
    #-agentlib:jdwp=transport=dt_socket,address=8787,server=y,suspend=y  # For remote debugging 
    org.infinispan.client.jni.hotrod.JniTest
)

set_property(TARGET hotrod-swig PROPERTY CXX_STANDARD 11)
set_property(TARGET hotrod-swig PROPERTY CXX_STANDARD_REQUIRED ON)
