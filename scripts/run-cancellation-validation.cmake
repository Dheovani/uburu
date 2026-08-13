cmake_minimum_required(VERSION 3.25)

foreach(requiredVariable IN ITEMS UBURU_EXECUTABLE UBURU_DATASET_ROOT UBURU_EXPRESSION UBURU_DATASET_ID
                                  UBURU_DATASET_PROFILE UBURU_OUTPUT)
  if(NOT DEFINED ${requiredVariable} OR "${${requiredVariable}}" STREQUAL "")
    message(FATAL_ERROR "${requiredVariable} is required")
  endif()
endforeach()

if(NOT EXISTS "${UBURU_EXECUTABLE}")
  message(FATAL_ERROR "UBURU_EXECUTABLE does not exist")
endif()

if(NOT IS_DIRECTORY "${UBURU_DATASET_ROOT}")
  message(FATAL_ERROR "UBURU_DATASET_ROOT is not a directory")
endif()

if(NOT UBURU_DATASET_ID MATCHES "^[A-Za-z0-9._-]+$")
  message(FATAL_ERROR "UBURU_DATASET_ID may contain only letters, numbers, dots, underscores, and hyphens")
endif()

set(supportedProfiles
  deterministic-smoke
  code-repository
  document-corpus
  many-small-files
  few-large-files
  large-mixed-tree
)

if(NOT UBURU_DATASET_PROFILE IN_LIST supportedProfiles)
  message(FATAL_ERROR "UBURU_DATASET_PROFILE must be one of: ${supportedProfiles}")
endif()

if(NOT DEFINED UBURU_CANCELLATION_DELAY_MILLISECONDS)
  set(UBURU_CANCELLATION_DELAY_MILLISECONDS 250)
endif()

if(NOT DEFINED UBURU_MAXIMUM_COMPLETION_MILLISECONDS)
  set(UBURU_MAXIMUM_COMPLETION_MILLISECONDS 2000)
endif()

if(NOT DEFINED UBURU_MEMORY_BUDGET_MIB)
  set(UBURU_MEMORY_BUDGET_MIB 512)
endif()

if(NOT DEFINED UBURU_MAX_SIZE_MIB)
  set(UBURU_MAX_SIZE_MIB 16)
endif()

if(NOT DEFINED UBURU_THREADS)
  set(UBURU_THREADS auto)
endif()

if(NOT DEFINED UBURU_CONFIGURATION)
  set(UBURU_CONFIGURATION local)
endif()

if(NOT DEFINED UBURU_REQUIRE_CLEAN_WORKTREE)
  set(UBURU_REQUIRE_CLEAN_WORKTREE ON)
endif()

if(NOT DEFINED UBURU_REQUIRE_RELEASE_BUILD)
  set(UBURU_REQUIRE_RELEASE_BUILD ON)
endif()

if(NOT DEFINED UBURU_INCLUDE_BINARY)
  set(UBURU_INCLUDE_BINARY OFF)
endif()

if(NOT DEFINED UBURU_INCLUDE_SUBDIRECTORIES)
  set(UBURU_INCLUDE_SUBDIRECTORIES ON)
endif()

foreach(numericVariable IN ITEMS UBURU_CANCELLATION_DELAY_MILLISECONDS UBURU_MAXIMUM_COMPLETION_MILLISECONDS
                                 UBURU_MEMORY_BUDGET_MIB UBURU_MAX_SIZE_MIB)
  if(NOT ${numericVariable} MATCHES "^[0-9]+$")
    message(FATAL_ERROR "${numericVariable} must be a non-negative integer")
  endif()
endforeach()

if(UBURU_CANCELLATION_DELAY_MILLISECONDS LESS 1 OR UBURU_CANCELLATION_DELAY_MILLISECONDS GREATER 86400000)
  message(FATAL_ERROR "UBURU_CANCELLATION_DELAY_MILLISECONDS must be from 1 to 86400000")
endif()

if(UBURU_MAXIMUM_COMPLETION_MILLISECONDS LESS UBURU_CANCELLATION_DELAY_MILLISECONDS)
  message(FATAL_ERROR "UBURU_MAXIMUM_COMPLETION_MILLISECONDS must not precede the cancellation deadline")
endif()

get_filename_component(outputPath "${UBURU_OUTPUT}" ABSOLUTE)
get_filename_component(outputDirectory "${outputPath}" DIRECTORY)
file(MAKE_DIRECTORY "${outputDirectory}")

get_filename_component(projectRoot "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(privateDirectory "${projectRoot}/build/product-validation-private/${UBURU_DATASET_ID}-cancellation")

if(IS_DIRECTORY "${outputPath}")
  message(FATAL_ERROR "UBURU_OUTPUT must name a Markdown file, not an existing directory")
endif()

cmake_path(IS_PREFIX privateDirectory "${outputPath}" NORMALIZE outputInsidePrivateDirectory)

if(outputInsidePrivateDirectory)
  message(FATAL_ERROR "UBURU_OUTPUT must remain outside the disposable private validation directory")
endif()

file(REMOVE_RECURSE "${privateDirectory}")
file(MAKE_DIRECTORY "${privateDirectory}")

function(validationFailure failureMessage)
  file(REMOVE_RECURSE "${privateDirectory}")
  message(FATAL_ERROR "${failureMessage}")
endfunction()

function(readJsonField json field outputVariable)
  string(JSON fieldValue ERROR_VARIABLE jsonError GET "${json}" "${field}")

  if(jsonError)
    validationFailure("Uburu emitted an invalid cancellation summary")
  endif()

  set(${outputVariable} "${fieldValue}" PARENT_SCOPE)
endfunction()

set(command
  "${UBURU_EXECUTABLE}"
  search
  "${UBURU_DATASET_ROOT}"
  "${UBURU_EXPRESSION}"
  --strategy
  direct
  --format
  jsonl
  --summary-only
  --cancel-after-ms
  "${UBURU_CANCELLATION_DELAY_MILLISECONDS}"
  --memory-budget-mib
  "${UBURU_MEMORY_BUDGET_MIB}"
  --max-size-mib
  "${UBURU_MAX_SIZE_MIB}"
  --threads
  "${UBURU_THREADS}"
)

if(DEFINED UBURU_TYPES AND NOT UBURU_TYPES STREQUAL "")
  list(APPEND command --types "${UBURU_TYPES}")
endif()

if(UBURU_INCLUDE_BINARY)
  list(APPEND command --binary)
endif()

if(NOT UBURU_INCLUDE_SUBDIRECTORIES)
  list(APPEND command --no-subdirectories)
endif()

message(STATUS "Measuring cooperative cancellation for dataset ${UBURU_DATASET_ID}")
execute_process(
  COMMAND ${command}
  WORKING_DIRECTORY "${privateDirectory}"
  RESULT_VARIABLE exitCode
  OUTPUT_VARIABLE commandOutput
  ERROR_VARIABLE commandError
)

string(REGEX MATCH "\\{\"type\":\"summary\"[^\r\n]*\\}" summaryJson "${commandOutput}")

if(summaryJson STREQUAL "")
  validationFailure("Uburu did not emit a cancellation summary; private output was not retained")
endif()

foreach(field IN ITEMS cancelled matches filesScanned filesWithReadErrors totalTimeNanoseconds filesProcessed
                       bytesProcessed workerQueuePeakItems fileResultQueuePeakItems extractionCompleted
                       extractionUnsupported extractionSafetyLimited extractionProtected extractionParserFailures)
  readJsonField("${summaryJson}" "${field}" ${field})
endforeach()

math(EXPR totalMilliseconds "${totalTimeNanoseconds} / 1000000")
math(EXPR cancellationOvershootMilliseconds
  "${totalMilliseconds} - ${UBURU_CANCELLATION_DELAY_MILLISECONDS}"
)

if(cancellationOvershootMilliseconds LESS 0)
  set(cancellationOvershootMilliseconds 0)
endif()

set(validationStatus Passed)
set(validationNotes "The scheduled deadline reached the cooperative cancellation path within the configured bound.")

if(NOT exitCode EQUAL 4)
  set(validationStatus Failed)
  set(validationNotes "The cancelled search did not return the stable cancellation exit code 4.")
elseif(NOT cancelled)
  set(validationStatus Failed)
  set(validationNotes "The search summary did not report cancellation.")
elseif(totalMilliseconds GREATER UBURU_MAXIMUM_COMPLETION_MILLISECONDS)
  set(validationStatus Failed)
  set(validationNotes "The cancelled search exceeded the configured completion bound.")
endif()

execute_process(
  COMMAND git rev-parse HEAD
  WORKING_DIRECTORY "${projectRoot}"
  RESULT_VARIABLE gitResult
  OUTPUT_VARIABLE gitCommit
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET
)

if(NOT gitResult EQUAL 0)
  set(gitCommit unknown)
endif()

execute_process(
  COMMAND git status --porcelain
  WORKING_DIRECTORY "${projectRoot}"
  OUTPUT_VARIABLE gitStatusOutput
  ERROR_QUIET
)

string(TOLOWER "${UBURU_CONFIGURATION}" normalizedConfiguration)

if(normalizedConfiguration MATCHES "release")
  set(releaseBuild ON)
else()
  set(releaseBuild OFF)
endif()

if(gitStatusOutput STREQUAL "")
  set(gitWorktreeState clean)
else()
  set(gitWorktreeState modified)
endif()

if(gitWorktreeState STREQUAL clean AND releaseBuild)
  set(evidenceClassification formal)
else()
  set(evidenceClassification preliminary)
endif()

if(UBURU_REQUIRE_CLEAN_WORKTREE AND NOT gitWorktreeState STREQUAL clean)
  set(validationStatus Blocked)
  set(validationNotes "The cancellation check passed, but formal evidence requires a clean Git worktree.")
endif()

if(UBURU_REQUIRE_RELEASE_BUILD AND NOT releaseBuild)
  set(validationStatus Blocked)
  set(validationNotes "The cancellation check passed, but formal evidence requires an optimized release build.")
endif()

file(SHA256 "${UBURU_EXECUTABLE}" executableSha256)
cmake_host_system_information(RESULT logicalCores QUERY NUMBER_OF_LOGICAL_CORES)
cmake_host_system_information(RESULT processorName QUERY PROCESSOR_NAME)
cmake_host_system_information(RESULT osName QUERY OS_NAME)
cmake_host_system_information(RESULT osRelease QUERY OS_RELEASE)
string(TIMESTAMP validationTimestamp "%Y-%m-%dT%H:%M:%SZ" UTC)

set(report "# Uburu cancellation-validation evidence\n\n")
string(APPEND report
  "> This record contains aggregate evidence only. Dataset paths, expressions, matching paths, and content "
  "were intentionally omitted.\n\n"
)
string(APPEND report "## Run metadata\n\n")
string(APPEND report "- Timestamp (UTC): `${validationTimestamp}`\n")
string(APPEND report "- Git commit: `${gitCommit}`\n")
string(APPEND report "- Git worktree: `${gitWorktreeState}`\n")
string(APPEND report "- Evidence classification: `${evidenceClassification}`\n")
string(APPEND report "- Build/artifact: `${UBURU_CONFIGURATION}`\n")
string(APPEND report "- Optimized release build: `${releaseBuild}`\n")
string(APPEND report "- Executable SHA-256: `${executableSha256}`\n")
string(APPEND report "- Platform: `${osName} ${osRelease}`\n")
string(APPEND report "- Processor: `${processorName}`\n")
string(APPEND report "- Logical cores: `${logicalCores}`\n")
string(APPEND report "- Dataset identifier: `${UBURU_DATASET_ID}`\n")
string(APPEND report "- Dataset profile: `${UBURU_DATASET_PROFILE}`\n")
string(APPEND report "- Extension filter: `${UBURU_TYPES}`\n")
string(APPEND report "- Include binary files: `${UBURU_INCLUDE_BINARY}`\n")
string(APPEND report "- Include subdirectories: `${UBURU_INCLUDE_SUBDIRECTORIES}`\n")
string(APPEND report "- Maximum file size: `${UBURU_MAX_SIZE_MIB} MiB`\n")
string(APPEND report "- Configured memory budget: `${UBURU_MEMORY_BUDGET_MIB} MiB`\n")
string(APPEND report "- Configured direct-search workers: `${UBURU_THREADS}`\n\n")
string(APPEND report "## Cancellation evidence\n\n")
string(APPEND report "- Requested cancellation deadline: `${UBURU_CANCELLATION_DELAY_MILLISECONDS} ms`\n")
string(APPEND report "- Maximum accepted completion time: `${UBURU_MAXIMUM_COMPLETION_MILLISECONDS} ms`\n")
string(APPEND report "- Observed search completion time: `${totalMilliseconds} ms`\n")
string(APPEND report "- Observed deadline overshoot: `${cancellationOvershootMilliseconds} ms`\n")
string(APPEND report "- Stable process exit code: `${exitCode}`\n")
string(APPEND report "- Summary reported cancellation: `${cancelled}`\n")
string(APPEND report "- Matches published before cancellation: `${matches}`\n")
string(APPEND report "- Files scanned before cancellation: `${filesScanned}`\n")
string(APPEND report "- Files processed before cancellation: `${filesProcessed}`\n")
string(APPEND report "- Bytes processed before cancellation: `${bytesProcessed}`\n")
string(APPEND report "- Read errors before cancellation: `${filesWithReadErrors}`\n")
string(APPEND report "- Worker/file queue peaks: `${workerQueuePeakItems}/${fileResultQueuePeakItems}`\n")
string(APPEND report
  "- Extraction completed/unsupported/safety-limited/protected/parser-failed: "
  "`${extractionCompleted}/${extractionUnsupported}/${extractionSafetyLimited}/${extractionProtected}/"
  "${extractionParserFailures}`\n\n"
)
string(APPEND report "## Automated decision\n\n")
string(APPEND report "- Status: **${validationStatus}**\n")
string(APPEND report "- Observation: ${validationNotes}\n")
string(APPEND report
  "- This gate measures deterministic end-to-end cooperative cancellation. Physical `Ctrl+C` handling and desktop "
  "cancellation responsiveness remain manual release checks.\n"
)

file(WRITE "${outputPath}" "${report}")
file(REMOVE_RECURSE "${privateDirectory}")

message(STATUS "Sanitized cancellation evidence written to ${outputPath}")

if(NOT validationStatus STREQUAL Passed)
  message(FATAL_ERROR "Cancellation validation failed; inspect the sanitized evidence record")
endif()
