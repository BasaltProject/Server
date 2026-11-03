file(MAKE_DIRECTORY "${OUT}")
execute_process(
    COMMAND "${JAVAC}" --release 25 -proc:none -d "${OUT}"
        "${SOURCE_DIR}/java/org/basaltmc/bukkit/fs/PathTranslator.java"
        "${SOURCE_DIR}/test/java/PathTranslatorTest.java"
    RESULT_VARIABLE compiled)
if(NOT compiled EQUAL 0)
    message(FATAL_ERROR "PathTranslatorTest did not compile")
endif()
execute_process(COMMAND "${JAVA}" -cp "${OUT}" PathTranslatorTest RESULT_VARIABLE ran)
if(NOT ran EQUAL 0)
    message(FATAL_ERROR "PathTranslatorTest failed")
endif()
