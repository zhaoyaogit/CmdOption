include_directories ("${PROJECT_SOURCE_DIR}/src")
aux_source_directory(. SRC_LIST)

add_library(cmdoption ${SRC_LIST})

# ��װĿ��
install(
    DIRECTORY "${PROJECT_SOURCE_DIR}/src"
	DESTINATION include/cmdoption
    FILES_MATCHING PATTERN "*.h"
)
install(
    TARGETS cmdoption
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib${ARCH_SUFFIX}
    ARCHIVE DESTINATION lib${ARCH_SUFFIX}
)
