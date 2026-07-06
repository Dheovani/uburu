#include "core/text/text-file-reader.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

namespace
{

  constexpr std::size_t maximumInputBytes = 4096;
  constexpr std::size_t maximumAcceptedLines = 32;
  constexpr std::size_t smallBinarySampleSize = 64;
  constexpr std::size_t smallMaximumLineLength = 512;
  constexpr unsigned char latin1FallbackFlag = 0b0000'0001U;
  constexpr unsigned char utf16LittleEndianFallbackFlag = 0b0000'0010U;
  constexpr unsigned char includeBinaryFlag = 0b0000'0100U;

  std::filesystem::path fuzzInputPath()
  {
    return std::filesystem::temp_directory_path() / "uburu-text-file-reader-fuzzer-input.bin";
  }

  uburu::TextEncoding fallbackEncoding(unsigned char flags)
  {
    if ((flags & latin1FallbackFlag) != 0)
      return uburu::TextEncoding::latin1;

    if ((flags & utf16LittleEndianFallbackFlag) != 0)
      return uburu::TextEncoding::utf16Le;

    return uburu::TextEncoding::utf8;
  }

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
  if (size == 0)
    return 0;

  const auto flags = data[0];
  const auto payloadSize = std::min(maximumInputBytes, size);
  const auto path = fuzzInputPath();

  {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(payloadSize));
  }

  uburu::SearchOptions options;
  options.binarySampleSize = smallBinarySampleSize;
  options.maximumLineLength = smallMaximumLineLength;
  options.fallbackEncoding = fallbackEncoding(flags);
  options.includeBinary = (flags & includeBinaryFlag) != 0;

  std::size_t lines = 0;
  const auto summary = uburu::text::readTextFileLines(path, options, [&](const uburu::text::TextLine& line) {
    static_cast<void>(uburu::text::visualColumnForByteOffset(line.text, line.text.size()));
    ++lines;

    return lines < maximumAcceptedLines;
  });

  static_cast<void>(summary.linesRead);
  std::filesystem::remove(path);

  return 0;
}
