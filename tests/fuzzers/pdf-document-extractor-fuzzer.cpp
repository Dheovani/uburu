#include "core/document/pdf-document-extractor.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string_view>

namespace
{

  constexpr std::size_t maximumInputBytes = 65536;
  constexpr std::uintmax_t maximumExtractedBytes = 8192;
  constexpr std::size_t maximumSegments = 16;

  std::filesystem::path fuzzPdfPath()
  {
    return std::filesystem::temp_directory_path() / "uburu-pdf-document-extractor-fuzzer.pdf";
  }

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
  if (data == nullptr || size == 0)
    return 0;

  const auto contentSize = std::min(size, maximumInputBytes);
  const std::string_view content(reinterpret_cast<const char*>(data), contentSize);
  const auto path = fuzzPdfPath();

  {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(content.data(), static_cast<std::streamsize>(content.size()));
  }

  uburu::document::PdfDocumentExtractor extractor;
  uburu::document::DocumentExtractionOptions options;
  options.maximumExtractedBytes = maximumExtractedBytes;
  options.maximumSegments = maximumSegments;

  static_cast<void>(
    extractor.extract(path, options, [](const uburu::document::ExtractedTextSegment& segment) {
      static_cast<void>(segment.text.size());

      return true;
    }));

  std::filesystem::remove(path);

  return 0;
}
