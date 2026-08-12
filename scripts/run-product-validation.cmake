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

if(NOT DEFINED UBURU_MEMORY_BUDGET_MIB)
  set(UBURU_MEMORY_BUDGET_MIB 512)
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

if(NOT DEFINED UBURU_ALLOW_PARTIAL_FAILURE)
  set(UBURU_ALLOW_PARTIAL_FAILURE OFF)
endif()

if(NOT DEFINED UBURU_MINIMUM_MATCHES)
  set(UBURU_MINIMUM_MATCHES 1)
endif()

if(NOT DEFINED UBURU_MAX_SIZE_MIB)
  set(UBURU_MAX_SIZE_MIB 16)
endif()

get_filename_component(outputPath "${UBURU_OUTPUT}" ABSOLUTE)
get_filename_component(outputDirectory "${outputPath}" DIRECTORY)
file(MAKE_DIRECTORY "${outputDirectory}")

get_filename_component(projectRoot "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(privateDirectory "${projectRoot}/build/product-validation-private/${UBURU_DATASET_ID}")
set(databasePath "${privateDirectory}/uburu-validation.db")
file(REMOVE_RECURSE "${privateDirectory}")
file(MAKE_DIRECTORY "${privateDirectory}")

string(REPLACE ";" "\\;" executableArgument "${UBURU_EXECUTABLE}")
string(REPLACE ";" "\\;" datasetRootArgument "${UBURU_DATASET_ROOT}")
string(REPLACE ";" "\\;" expressionArgument "${UBURU_EXPRESSION}")
string(REPLACE ";" "\\;" databaseArgument "${databasePath}")

function(validationFailure failureMessage)
  file(REMOVE_RECURSE "${privateDirectory}")
  message(FATAL_ERROR "${failureMessage}")
endfunction()

function(readJsonField json field outputVariable)
  string(JSON fieldValue ERROR_VARIABLE jsonError GET "${json}" "${field}")

  if(jsonError)
    validationFailure("Uburu emitted an invalid validation summary")
  endif()

  set(${outputVariable} "${fieldValue}" PARENT_SCOPE)
endfunction()

function(extractJsonRecord output recordType outputVariable)
  string(REGEX MATCH "\\{\"type\":\"${recordType}\"[^\r\n]*\\}" record "${output}")

  if(record STREQUAL "")
    validationFailure("Uburu did not emit the expected ${recordType} record")
  endif()

  set(${outputVariable} "${record}" PARENT_SCOPE)
endfunction()

function(runSearch strategy prefix)
  set(command
    "${executableArgument}"
    search
    "${datasetRootArgument}"
    "${expressionArgument}"
    --strategy
    "${strategy}"
    --format
    jsonl
    --summary-only
    --database
    "${databaseArgument}"
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

  execute_process(
    COMMAND ${command}
    WORKING_DIRECTORY "${privateDirectory}"
    RESULT_VARIABLE exitCode
    OUTPUT_VARIABLE commandOutput
    ERROR_VARIABLE commandError
  )

  extractJsonRecord("${commandOutput}" summary summaryJson)

  foreach(field IN ITEMS matches filesScanned filesWithReadErrors cancelled partialFailure resultLimitReached
                         memoryLimitReached resultMemoryBytes timeToFirstResultNanoseconds totalTimeNanoseconds
                         filesProcessed bytesProcessed filesPerSecond bytesPerSecond workerQueuePeakItems
                         fileResultQueuePeakItems)
    readJsonField("${summaryJson}" "${field}" fieldValue)
    set(${prefix}_${field} "${fieldValue}")
    set(${prefix}_${field} "${fieldValue}" PARENT_SCOPE)
  endforeach()

  if(NOT exitCode EQUAL 0)
    if(UBURU_ALLOW_PARTIAL_FAILURE AND ${prefix}_partialFailure AND NOT ${prefix}_cancelled AND exitCode EQUAL 3)
      return()
    endif()

    validationFailure(
      "${strategy} validation search failed with exit code ${exitCode}; private output was not retained"
    )
  endif()
endfunction()

function(runIndexCommand commandName recordType prefix)
  set(command
    "${executableArgument}"
    "${commandName}"
    "${datasetRootArgument}"
    --format
    jsonl
    --database
    "${databaseArgument}"
    --memory-budget-mib
    "${UBURU_MEMORY_BUDGET_MIB}"
    --max-size-mib
    "${UBURU_MAX_SIZE_MIB}"
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

  execute_process(
    COMMAND ${command}
    WORKING_DIRECTORY "${privateDirectory}"
    RESULT_VARIABLE exitCode
    OUTPUT_VARIABLE commandOutput
    ERROR_VARIABLE commandError
  )

  extractJsonRecord("${commandOutput}" "${recordType}" recordJson)
  set(${prefix}_json "${recordJson}" PARENT_SCOPE)

  if(NOT exitCode EQUAL 0)
    readJsonField("${recordJson}" failed failed)
    readJsonField("${recordJson}" cancelled cancelled)
    readJsonField("${recordJson}" memoryLimitReached memoryLimitReached)

    if(UBURU_ALLOW_PARTIAL_FAILURE AND failed GREATER 0 AND NOT cancelled AND NOT memoryLimitReached AND
       exitCode EQUAL 3)
      return()
    endif()

    validationFailure("${commandName} failed with exit code ${exitCode}; private output was not retained")
  endif()
endfunction()

function(indexField prefix field outputVariable)
  readJsonField("${${prefix}_json}" "${field}" fieldValue)
  set(${outputVariable} "${fieldValue}" PARENT_SCOPE)
endfunction()

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
  RESULT_VARIABLE gitStatusResult
  OUTPUT_VARIABLE gitStatusOutput
  ERROR_QUIET
)

if(NOT gitStatusResult EQUAL 0)
  set(gitWorktreeState unknown)
elseif(gitStatusOutput STREQUAL "")
  set(gitWorktreeState clean)
else()
  set(gitWorktreeState modified)
endif()

string(TOLOWER "${UBURU_CONFIGURATION}" normalizedConfiguration)
if(normalizedConfiguration MATCHES "release")
  set(releaseBuild ON)
else()
  set(releaseBuild OFF)
endif()

if(gitWorktreeState STREQUAL clean AND releaseBuild)
  set(evidenceClassification formal)
else()
  set(evidenceClassification preliminary)
endif()

file(SHA256 "${UBURU_EXECUTABLE}" executableSha256)
cmake_host_system_information(RESULT logicalCores QUERY NUMBER_OF_LOGICAL_CORES)
cmake_host_system_information(RESULT physicalMemoryMiB QUERY TOTAL_PHYSICAL_MEMORY)
cmake_host_system_information(RESULT processorName QUERY PROCESSOR_NAME)
cmake_host_system_information(RESULT osName QUERY OS_NAME)
cmake_host_system_information(RESULT osRelease QUERY OS_RELEASE)
string(TIMESTAMP validationTimestamp "%Y-%m-%dT%H:%M:%SZ" UTC)

message(STATUS "Running privacy-safe direct validation for dataset ${UBURU_DATASET_ID}")
runSearch(direct direct)

message(STATUS "Building initial validation index")
runIndexCommand(index-rebuild indexSummary initialIndex)

message(STATUS "Repeating index update to measure reuse")
runIndexCommand(index-rebuild indexSummary incrementalIndex)

message(STATUS "Checking index status")
runIndexCommand(index-status indexStatus indexStatus)

message(STATUS "Running privacy-safe indexed and hybrid validation")
runSearch(indexed indexed)
runSearch(hybrid hybrid)

foreach(field IN ITEMS
        indexed
        reusedByCatalog
        reusedByBlob
        reusedByHash
        removed
        failed
        skippedUnsupportedFormat
        skippedBinary
        workingMemoryPeakBytes
        memoryLimitReached
        cancelled)
  indexField(initialIndex "${field}" initialIndex_${field})
  indexField(incrementalIndex "${field}" incrementalIndex_${field})
endforeach()

foreach(field IN ITEMS state headChanged branchChanged)
  indexField(indexStatus "${field}" indexStatus_${field})
endforeach()

set(validationStatus Passed)
set(validationNotes "Strategies converged and the rebuilt index is fresh.")

if(UBURU_ALLOW_PARTIAL_FAILURE AND (direct_partialFailure OR indexed_partialFailure OR hybrid_partialFailure))
  set(validationNotes "Strategies converged, the rebuilt index is fresh, and declared partial failures were preserved.")
endif()

if(UBURU_REQUIRE_CLEAN_WORKTREE AND NOT gitWorktreeState STREQUAL clean)
  set(validationStatus Blocked)
  set(validationNotes "The automated checks passed, but formal evidence requires a clean Git worktree.")
endif()

if(UBURU_REQUIRE_RELEASE_BUILD AND NOT releaseBuild)
  set(validationStatus Blocked)
  set(validationNotes "The automated checks passed, but product evidence requires an optimized release build.")
endif()

if(NOT direct_matches EQUAL indexed_matches OR NOT direct_matches EQUAL hybrid_matches)
  set(validationStatus Failed)
  set(validationNotes "Direct, indexed, and hybrid match counts did not converge.")
endif()

if(direct_matches LESS UBURU_MINIMUM_MATCHES)
  set(validationStatus Failed)
  set(validationNotes "The known expression did not produce the configured minimum match count.")
endif()

if(NOT indexStatus_state STREQUAL fresh)
  set(validationStatus Failed)
  set(validationNotes "The rebuilt index was not reported as fresh.")
endif()

foreach(prefix IN ITEMS direct indexed hybrid)
  if(${prefix}_cancelled OR ${prefix}_resultLimitReached OR ${prefix}_memoryLimitReached)
    set(validationStatus Failed)
    set(validationNotes "At least one strategy reported cancellation or budget exhaustion.")
  endif()

  if(${prefix}_partialFailure AND NOT UBURU_ALLOW_PARTIAL_FAILURE)
    set(validationStatus Failed)
    set(validationNotes "At least one strategy reported an undeclared partial failure.")
  endif()
endforeach()

if(initialIndex_cancelled OR initialIndex_memoryLimitReached OR incrementalIndex_cancelled OR
   incrementalIndex_memoryLimitReached)
  set(validationStatus Failed)
  set(validationNotes "Indexing reported cancellation or memory exhaustion.")
endif()

if((initialIndex_failed GREATER 0 OR incrementalIndex_failed GREATER 0) AND NOT UBURU_ALLOW_PARTIAL_FAILURE)
  set(validationStatus Failed)
  set(validationNotes "Indexing reported undeclared partial failures.")
endif()

math(EXPR directFirstMilliseconds "${direct_timeToFirstResultNanoseconds} / 1000000")
math(EXPR directTotalMilliseconds "${direct_totalTimeNanoseconds} / 1000000")
math(EXPR indexedFirstMilliseconds "${indexed_timeToFirstResultNanoseconds} / 1000000")
math(EXPR indexedTotalMilliseconds "${indexed_totalTimeNanoseconds} / 1000000")
math(EXPR hybridFirstMilliseconds "${hybrid_timeToFirstResultNanoseconds} / 1000000")
math(EXPR hybridTotalMilliseconds "${hybrid_totalTimeNanoseconds} / 1000000")

set(report "# Uburu product-validation evidence\n\n")
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
string(APPEND report "- Physical memory: `${physicalMemoryMiB} MiB`\n")
string(APPEND report "- Dataset identifier: `${UBURU_DATASET_ID}`\n")
string(APPEND report "- Dataset profile: `${UBURU_DATASET_PROFILE}`\n")
string(APPEND report "- Extension filter: `${UBURU_TYPES}`\n")
string(APPEND report "- Include binary files: `${UBURU_INCLUDE_BINARY}`\n")
string(APPEND report "- Include subdirectories: `${UBURU_INCLUDE_SUBDIRECTORIES}`\n")
string(APPEND report "- Allow declared partial failures: `${UBURU_ALLOW_PARTIAL_FAILURE}`\n")
string(APPEND report "- Minimum expected matches: `${UBURU_MINIMUM_MATCHES}`\n")
string(APPEND report "- Maximum file size: `${UBURU_MAX_SIZE_MIB} MiB`\n")
string(APPEND report "- Configured memory budget: `${UBURU_MEMORY_BUDGET_MIB} MiB`\n")
string(APPEND report "- Configured direct-search workers: `${UBURU_THREADS}`\n\n")
string(APPEND report "## Search evidence\n\n")
string(APPEND report
  "| Strategy | Matches | Files scanned | Read errors | First result | Total | Files/s | Bytes/s | Result memory | "
  "Worker/file queue peaks |\n"
)
string(APPEND report "| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |\n")
string(APPEND report
  "| Direct | ${direct_matches} | ${direct_filesScanned} | ${direct_filesWithReadErrors} | "
  "${directFirstMilliseconds} ms | ${directTotalMilliseconds} ms | ${direct_filesPerSecond} | "
  "${direct_bytesPerSecond} | ${direct_resultMemoryBytes} B | "
  "${direct_workerQueuePeakItems}/${direct_fileResultQueuePeakItems} |\n"
)
string(APPEND report
  "| Indexed | ${indexed_matches} | ${indexed_filesScanned} | ${indexed_filesWithReadErrors} | "
  "${indexedFirstMilliseconds} ms | ${indexedTotalMilliseconds} ms | ${indexed_filesPerSecond} | "
  "${indexed_bytesPerSecond} | ${indexed_resultMemoryBytes} B | "
  "${indexed_workerQueuePeakItems}/${indexed_fileResultQueuePeakItems} |\n"
)
string(APPEND report
  "| Hybrid | ${hybrid_matches} | ${hybrid_filesScanned} | ${hybrid_filesWithReadErrors} | "
  "${hybridFirstMilliseconds} ms | ${hybridTotalMilliseconds} ms | ${hybrid_filesPerSecond} | "
  "${hybrid_bytesPerSecond} | ${hybrid_resultMemoryBytes} B | "
  "${hybrid_workerQueuePeakItems}/${hybrid_fileResultQueuePeakItems} |\n\n"
)
string(APPEND report "## Index evidence\n\n")
string(APPEND report
  "| Run | Indexed | Catalog reuse | Blob reuse | Hash reuse | Removed | Failed | Unsupported | Binary | "
  "Peak memory |\n"
)
string(APPEND report "| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |\n")
string(APPEND report
  "| Initial | ${initialIndex_indexed} | ${initialIndex_reusedByCatalog} | ${initialIndex_reusedByBlob} | "
  "${initialIndex_reusedByHash} | ${initialIndex_removed} | ${initialIndex_failed} | "
  "${initialIndex_skippedUnsupportedFormat} | ${initialIndex_skippedBinary} | "
  "${initialIndex_workingMemoryPeakBytes} B |\n"
)
string(APPEND report
  "| Incremental | ${incrementalIndex_indexed} | ${incrementalIndex_reusedByCatalog} | "
  "${incrementalIndex_reusedByBlob} | ${incrementalIndex_reusedByHash} | ${incrementalIndex_removed} | "
  "${incrementalIndex_failed} | ${incrementalIndex_skippedUnsupportedFormat} | "
  "${incrementalIndex_skippedBinary} | ${incrementalIndex_workingMemoryPeakBytes} B |\n\n"
)
string(APPEND report "- Index status after rebuild: `${indexStatus_state}`\n")
string(APPEND report "- HEAD changed: `${indexStatus_headChanged}`\n")
string(APPEND report "- Branch changed: `${indexStatus_branchChanged}`\n\n")
string(APPEND report "## Automated decision\n\n")
string(APPEND report "- Status: **${validationStatus}**\n")
string(APPEND report "- Observation: ${validationNotes}\n")
string(APPEND report
  "- Manual correctness, cancellation, UI responsiveness, preview, and clean-machine checks remain required by "
  "`docs/validation.md`.\n"
)

file(WRITE "${outputPath}" "${report}")
file(REMOVE_RECURSE "${privateDirectory}")

message(STATUS "Sanitized validation evidence written to ${outputPath}")

if(NOT validationStatus STREQUAL Passed)
  message(FATAL_ERROR "Automated product validation did not qualify as passed; inspect the sanitized evidence record")
endif()
