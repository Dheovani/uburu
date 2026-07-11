add_library(uburu_fuzzer_options INTERFACE)

target_link_libraries(uburu_fuzzer_options INTERFACE Uburu::Core Uburu::ProjectOptions)
target_link_options(uburu_fuzzer_options INTERFACE -fsanitize=fuzzer,address,undefined)

function(add_uburu_fuzzer target source)
  add_executable(${target} ${source})
  target_link_libraries(${target} PRIVATE uburu_fuzzer_options)
endfunction()

add_uburu_fuzzer(uburu_text_matcher_fuzzer text-matcher-fuzzer.cpp)
add_uburu_fuzzer(uburu_git_ignore_fuzzer git-ignore-fuzzer.cpp)
add_uburu_fuzzer(uburu_text_file_reader_fuzzer text-file-reader-fuzzer.cpp)
add_uburu_fuzzer(uburu_path_normalization_fuzzer path-normalization-fuzzer.cpp)
add_uburu_fuzzer(uburu_pdf_document_extractor_fuzzer pdf-document-extractor-fuzzer.cpp)

add_custom_target(fuzz-smoke
  COMMAND $<TARGET_FILE:uburu_text_matcher_fuzzer> -runs=256 -max_len=4096
  COMMAND $<TARGET_FILE:uburu_git_ignore_fuzzer> -runs=256 -max_len=4096
  COMMAND $<TARGET_FILE:uburu_text_file_reader_fuzzer> -runs=128 -max_len=4096
  COMMAND $<TARGET_FILE:uburu_path_normalization_fuzzer> -runs=256 -max_len=1024
  COMMAND $<TARGET_FILE:uburu_pdf_document_extractor_fuzzer> -runs=128 -max_len=65536
  DEPENDS
    uburu_text_matcher_fuzzer
    uburu_git_ignore_fuzzer
    uburu_text_file_reader_fuzzer
    uburu_path_normalization_fuzzer
    uburu_pdf_document_extractor_fuzzer
  COMMENT "Running fuzz smoke targets"
  VERBATIM
)
