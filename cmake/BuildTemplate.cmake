
message(STATUS "Project: ${CMAKE_PROJECT_NAME}")

# 获取当前版本号
function(read_changelog_first_line changelog_file_path version)
    file(STRINGS "${changelog_file_path}" file_lines)
    list(GET file_lines 0 first_line)
    set(${version} "${first_line}" PARENT_SCOPE)
endfunction(read_changelog_first_line)

set(CHANGE_LOG_FILE_NAME "${CMAKE_SOURCE_DIR}/Changelog")
read_changelog_first_line(${CHANGE_LOG_FILE_NAME} PROJECT_VERSION)
message(STATUS "Version: ${PROJECT_VERSION}")

# 获取当前的 Git commit ID
function(get_git_commit_id commit_id)
    find_package(Git QUIET)
    set(local_commit_id "unknown" PARENT_SCOPE)
    if(GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
        execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE local_commit_id
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
    endif()
    set(${commit_id} "${local_commit_id}" PARENT_SCOPE)
endfunction(get_git_commit_id)
get_git_commit_id(GIT_COMMIT)
message(STATUS "Git Commit: ${GIT_COMMIT}")

# 获取当前系统时间
execute_process(
    COMMAND date +%Y%m%d_%H%M%S
    OUTPUT_VARIABLE BUILD_TIME
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
message(STATUS "Build Time: ${BUILD_TIME}")