#include "core/text/text-file-reader.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string_view>
#include <vector>

namespace uburu::text
{
  namespace
  {

    constexpr std::size_t binaryControlSampleMinimum = 32;
    constexpr std::size_t binaryControlRatioDenominator = 10;
    constexpr std::size_t binaryControlRatioNumerator = 3;
    constexpr std::size_t utf16CodeUnitSize = 2;
    constexpr std::size_t utf8BomSize = 3;
    constexpr std::size_t utf16BomSize = 2;
    constexpr std::size_t readBlockSize = 64U * 1024U;
    constexpr unsigned char utf8BomFirst = 0xEFU;
    constexpr unsigned char utf8BomSecond = 0xBBU;
    constexpr unsigned char utf8BomThird = 0xBFU;
    constexpr unsigned char utf16LeBomFirst = 0xFFU;
    constexpr unsigned char utf16LeBomSecond = 0xFEU;
    constexpr unsigned char utf16BeBomFirst = 0xFEU;
    constexpr unsigned char utf16BeBomSecond = 0xFFU;
    constexpr unsigned char asciiTab = 0x09U;
    constexpr unsigned char asciiLineFeed = 0x0AU;
    constexpr unsigned char asciiCarriageReturn = 0x0DU;
    constexpr unsigned char asciiSpace = 0x20U;
    constexpr unsigned char asciiDelete = 0x7FU;
    constexpr char32_t unicodeReplacementCharacter = 0xFFFDU;
    constexpr char32_t unicodeMaximumScalar = 0x10FFFFU;
    constexpr char32_t utf16HighSurrogateMin = 0xD800U;
    constexpr char32_t utf16HighSurrogateMax = 0xDBFFU;
    constexpr char32_t utf16LowSurrogateMin = 0xDC00U;
    constexpr char32_t utf16LowSurrogateMax = 0xDFFFU;
    constexpr char32_t utf16SurrogateBase = 0x10000U;
    constexpr std::size_t utf16HighSurrogateShift = 10;
    constexpr std::uint16_t utf16LowSurrogatePayloadMask = 0x03FFU;
    constexpr unsigned char utf8OneByteMax = 0x7FU;
    constexpr char32_t utf8TwoByteScalarMax = 0x7FFU;
    constexpr char32_t utf8ThreeByteScalarMax = 0xFFFFU;
    constexpr unsigned char utf8TwoBytePrefix = 0xC0U;
    constexpr unsigned char utf8ThreeBytePrefix = 0xE0U;
    constexpr unsigned char utf8FourBytePrefix = 0xF0U;
    constexpr unsigned char utf8ContinuationPrefix = 0x80U;
    constexpr unsigned char utf8TwoByteMin = 0xC2U;
    constexpr unsigned char utf8TwoByteMax = 0xDFU;
    constexpr unsigned char utf8ThreeByteMin = 0xE0U;
    constexpr unsigned char utf8ThreeByteMax = 0xEFU;
    constexpr unsigned char utf8ThreeByteSurrogateLead = 0xEDU;
    constexpr unsigned char utf8FourByteMin = 0xF0U;
    constexpr unsigned char utf8FourByteMax = 0xF4U;
    constexpr unsigned char utf8ContinuationMin = 0x80U;
    constexpr unsigned char utf8ContinuationMax = 0xBFU;
    constexpr unsigned char utf8SecondAfterE0Min = 0xA0U;
    constexpr unsigned char utf8SecondBeforeSurrogateMax = 0x9FU;
    constexpr unsigned char utf8SecondAfterF0Min = 0x90U;
    constexpr unsigned char utf8SecondBeforeMaxScalarMax = 0x8FU;
    constexpr unsigned char utf8TwoBytePayloadMask = 0x1FU;
    constexpr unsigned char utf8ThreeBytePayloadMask = 0x0FU;
    constexpr unsigned char utf8FourBytePayloadMask = 0x07U;
    constexpr unsigned char utf8ContinuationPayloadMask = 0x3FU;
    constexpr std::size_t utf8ContinuationPayloadBits = 6;
    constexpr std::size_t utf8ThreeByteLeadShift = 12;
    constexpr std::size_t utf8FourByteLeadShift = 18;
    constexpr std::size_t utf8FourByteSecondShift = 12;
    constexpr char32_t latin1ControlC1Min = 0x80U;
    constexpr char32_t latin1ControlC1Max = 0x9FU;

    struct EncodingDetection
    {
      TextEncoding encoding{TextEncoding::utf8};
      std::size_t bomSize{0};
      bool hadBom{false};
    };

    struct Utf8DecodeResult
    {
      char32_t scalar{unicodeReplacementCharacter};
      std::size_t bytesConsumed{1};
      bool valid{false};
    };

    struct LineEmitter
    {
      const SearchOptions& options;
      const TextLineSink& sink;
      TextReadSummary& summary;
      std::string line;
      std::size_t lineNumber{0};
      std::uintmax_t lineOffset{0};
      std::uintmax_t nextByteOffset{0};

      [[nodiscard]] bool append(char scalar, std::uintmax_t byteWidth)
      {
        if (line.empty())
          lineOffset = nextByteOffset;

        line.push_back(scalar);
        nextByteOffset += byteWidth;

        return line.size() <= options.maximumLineLength;
      }

      [[nodiscard]] bool appendUtf8(std::string_view utf8, std::uintmax_t byteWidth)
      {
        if (line.empty())
          lineOffset = nextByteOffset;

        line.append(utf8);
        nextByteOffset += byteWidth;

        return line.size() <= options.maximumLineLength;
      }

      [[nodiscard]] bool emit(LineEnding ending, std::uintmax_t endingByteWidth)
      {
        ++lineNumber;
        ++summary.linesRead;

        const TextLine textLine{.text = line, .lineNumber = lineNumber, .byteOffset = lineOffset, .ending = ending};
        if (!sink(textLine))
          return false;

        line.clear();
        nextByteOffset += endingByteWidth;
        lineOffset = nextByteOffset;

        return true;
      }

      [[nodiscard]] bool finish()
      {
        if (line.empty())
          return true;

        return emit(LineEnding::none, 0);
      }
    };

    bool isUtf8Continuation(unsigned char byte)
    {
      return byte >= utf8ContinuationMin && byte <= utf8ContinuationMax;
    }

    bool isSupportedFallbackEncoding(TextEncoding encoding)
    {
      return encoding == TextEncoding::utf16Le ||
             encoding == TextEncoding::utf16Be ||
             encoding == TextEncoding::latin1;
    }

    bool sampleLooksValidUtf8(std::string_view sample);

    EncodingDetection detectEncoding(std::string_view sample, TextEncoding fallbackEncoding)
    {
      if (sample.size() >= utf8BomSize && static_cast<unsigned char>(sample[0]) == utf8BomFirst &&
          static_cast<unsigned char>(sample[1]) == utf8BomSecond &&
          static_cast<unsigned char>(sample[2]) == utf8BomThird)
        return {TextEncoding::utf8, utf8BomSize, true};

      if (sample.size() >= utf16BomSize && static_cast<unsigned char>(sample[0]) == utf16LeBomFirst &&
          static_cast<unsigned char>(sample[1]) == utf16LeBomSecond)
        return {TextEncoding::utf16Le, utf16BomSize, true};

      if (sample.size() >= utf16BomSize && static_cast<unsigned char>(sample[0]) == utf16BeBomFirst &&
          static_cast<unsigned char>(sample[1]) == utf16BeBomSecond)
        return {TextEncoding::utf16Be, utf16BomSize, true};

      if (sampleLooksValidUtf8(sample))
        return {TextEncoding::utf8, 0, false};

      auto encoding = TextEncoding::utf8;
      if (isSupportedFallbackEncoding(fallbackEncoding))
        encoding = fallbackEncoding;

      return {encoding, 0, false};
    }

    bool isBinaryControlByte(unsigned char byte)
    {
      if (byte == asciiTab || byte == asciiLineFeed || byte == asciiCarriageReturn)
        return false;

      return byte < asciiSpace || byte == asciiDelete;
    }

    std::string readSample(std::ifstream& stream, std::size_t sampleSize)
    {
      std::string sample(sampleSize, '\0');
      stream.read(sample.data(), static_cast<std::streamsize>(sample.size()));
      sample.resize(static_cast<std::size_t>(stream.gcount()));
      stream.clear();
      stream.seekg(0, std::ios::beg);

      return sample;
    }

    void appendUtf8Scalar(char32_t scalar, std::string& output)
    {
      if (scalar <= utf8OneByteMax) {
        output.push_back(static_cast<char>(scalar));

        return;
      }

      if (scalar <= utf8TwoByteScalarMax) {
        output.push_back(static_cast<char>(utf8TwoBytePrefix | (scalar >> utf8ContinuationPayloadBits)));
        output.push_back(static_cast<char>(utf8ContinuationPrefix | (scalar & utf8ContinuationPayloadMask)));

        return;
      }

      if (scalar <= utf8ThreeByteScalarMax) {
        output.push_back(static_cast<char>(utf8ThreeBytePrefix | (scalar >> utf8ThreeByteLeadShift)));
        output.push_back(static_cast<char>(utf8ContinuationPrefix |
                                           ((scalar >> utf8ContinuationPayloadBits) & utf8ContinuationPayloadMask)));
        output.push_back(static_cast<char>(utf8ContinuationPrefix | (scalar & utf8ContinuationPayloadMask)));

        return;
      }

      output.push_back(static_cast<char>(utf8FourBytePrefix | (scalar >> utf8FourByteLeadShift)));
      output.push_back(static_cast<char>(utf8ContinuationPrefix |
                                         ((scalar >> utf8FourByteSecondShift) & utf8ContinuationPayloadMask)));
      output.push_back(static_cast<char>(utf8ContinuationPrefix |
                                         ((scalar >> utf8ContinuationPayloadBits) & utf8ContinuationPayloadMask)));
      output.push_back(static_cast<char>(utf8ContinuationPrefix | (scalar & utf8ContinuationPayloadMask)));
    }

    std::string utf8FromScalar(char32_t scalar)
    {
      std::string output;
      appendUtf8Scalar(scalar, output);

      return output;
    }

    Utf8DecodeResult decodeUtf8At(std::string_view text, std::size_t offset)
    {
      const auto first = static_cast<unsigned char>(text[offset]);
      if (first <= utf8OneByteMax)
        return {.scalar = first, .bytesConsumed = 1, .valid = true};

      const auto has = [&](std::size_t index) { return offset + index < text.size(); };

      const auto byte = [&](std::size_t index) { return static_cast<unsigned char>(text[offset + index]); };

      if (first >= utf8TwoByteMin && first <= utf8TwoByteMax && has(1) && isUtf8Continuation(byte(1))) {
        const char32_t scalar =
          ((first & utf8TwoBytePayloadMask) << utf8ContinuationPayloadBits) | (byte(1) & utf8ContinuationPayloadMask);

        return {.scalar = scalar, .bytesConsumed = 2, .valid = true};
      }

      if (first >= utf8ThreeByteMin && first <= utf8ThreeByteMax && has(2) && isUtf8Continuation(byte(1)) &&
          isUtf8Continuation(byte(2))) {
        if ((first == utf8ThreeByteMin && byte(1) < utf8SecondAfterE0Min) ||
            (first == utf8ThreeByteSurrogateLead && byte(1) > utf8SecondBeforeSurrogateMax))
          return {};

        const char32_t scalar = ((first & utf8ThreeBytePayloadMask) << utf8ThreeByteLeadShift) |
                                ((byte(1) & utf8ContinuationPayloadMask) << utf8ContinuationPayloadBits) |
                                (byte(2) & utf8ContinuationPayloadMask);

        return {.scalar = scalar, .bytesConsumed = 3, .valid = true};
      }

      if (first >= utf8FourByteMin && first <= utf8FourByteMax && has(3) && isUtf8Continuation(byte(1)) &&
          isUtf8Continuation(byte(2)) && isUtf8Continuation(byte(3))) {
        if ((first == utf8FourByteMin && byte(1) < utf8SecondAfterF0Min) ||
            (first == utf8FourByteMax && byte(1) > utf8SecondBeforeMaxScalarMax))
          return {};

        const char32_t scalar = ((first & utf8FourBytePayloadMask) << utf8FourByteLeadShift) |
                                ((byte(1) & utf8ContinuationPayloadMask) << utf8FourByteSecondShift) |
                                ((byte(2) & utf8ContinuationPayloadMask) << utf8ContinuationPayloadBits) |
                                (byte(3) & utf8ContinuationPayloadMask);
        if (scalar > unicodeMaximumScalar)
          return {};

        return {.scalar = scalar, .bytesConsumed = 4, .valid = true};
      }

      return {};
    }

    bool sampleLooksValidUtf8(std::string_view sample)
    {
      for (std::size_t offset = 0; offset < sample.size();) {
        const auto decoded = decodeUtf8At(sample, offset);

        if (!decoded.valid) {
          const auto first = static_cast<unsigned char>(sample[offset]);
          const auto remaining = sample.size() - offset;

          if (first >= utf8TwoByteMin && first <= utf8TwoByteMax)
            return remaining < 2;

          if (first >= utf8ThreeByteMin && first <= utf8ThreeByteMax)
            return remaining < 3;

          if (first >= utf8FourByteMin && first <= utf8FourByteMax)
            return remaining < 4;

          return false;
        }

        offset += decoded.bytesConsumed;
      }

      return true;
    }

    std::uint16_t readUtf16CodeUnit(std::string_view bytes, std::size_t offset, TextEncoding encoding)
    {
      const auto first = static_cast<unsigned char>(bytes[offset]);
      const auto second = static_cast<unsigned char>(bytes[offset + 1]);

      if (encoding == TextEncoding::utf16Le)
        return static_cast<std::uint16_t>(first | (second << 8U));

      return static_cast<std::uint16_t>((first << 8U) | second);
    }

    bool processDecodedScalar(LineEmitter& emitter, char32_t scalar, std::uintmax_t originalByteWidth)
    {
      if (scalar == U'\n')
        return emitter.emit(LineEnding::lf, originalByteWidth);

      if (scalar == U'\r')
        return emitter.emit(LineEnding::cr, originalByteWidth);

      return emitter.appendUtf8(utf8FromScalar(scalar), originalByteWidth);
    }

    TextReadStatus decodeUtf8Lines(std::ifstream& stream,
                                   const SearchOptions& options,
                                   TextReadSummary& summary,
                                   const TextLineSink& sink,
                                   std::stop_token stop_token,
                                   std::size_t bomSize)
    {
      LineEmitter emitter{.options = options,
                          .sink = sink,
                          .summary = summary,
                          .line = {},
                          .lineNumber = 0,
                          .lineOffset = 0,
                          .nextByteOffset = 0};
      emitter.nextByteOffset = bomSize;
      stream.seekg(static_cast<std::streamoff>(bomSize), std::ios::beg);

      std::string pending;
      std::vector<char> block(readBlockSize);
      while (!stop_token.stop_requested()) {
        stream.read(block.data(), static_cast<std::streamsize>(block.size()));
        const auto bytesRead = static_cast<std::size_t>(stream.gcount());
        if (bytesRead == 0)
          break;

        pending.append(block.data(), bytesRead);
        std::size_t offset = 0;
        while (offset < pending.size()) {
          const auto decoded = decodeUtf8At(pending, offset);
          if (!decoded.valid) {
            if (pending.size() - offset < 4 && !stream.eof())
              break;

            summary.hadInvalidSequences = true;
            if (options.invalidUtf8Policy == InvalidUtf8Policy::fail)
              return TextReadStatus::invalidEncoding;

            if (options.invalidUtf8Policy == InvalidUtf8Policy::replace &&
                !processDecodedScalar(emitter, unicodeReplacementCharacter, 1))
              return TextReadStatus::cancelled;

            ++offset;
            continue;
          }

          if (decoded.scalar == U'\r' && offset + decoded.bytesConsumed < pending.size() &&
              pending[offset + decoded.bytesConsumed] == '\n') {
            if (!emitter.emit(LineEnding::crlf, 2))
              return TextReadStatus::cancelled;

            offset += 2;
            continue;
          }

          if (decoded.scalar == U'\r' && offset + decoded.bytesConsumed >= pending.size() && !stream.eof())
            break;

          if (!processDecodedScalar(emitter, decoded.scalar, decoded.bytesConsumed))
            return TextReadStatus::lineTooLong;

          offset += decoded.bytesConsumed;
        }

        pending.erase(0, offset);
      }

      if (stop_token.stop_requested())
        return TextReadStatus::cancelled;

      if (stream.bad())
        return TextReadStatus::readFailed;

      return emitter.finish() ? TextReadStatus::completed : TextReadStatus::cancelled;
    }

    TextReadStatus decodeLatin1Lines(std::ifstream& stream,
                                     const SearchOptions& options,
                                     TextReadSummary& summary,
                                     const TextLineSink& sink,
                                     std::stop_token stop_token)
    {
      LineEmitter emitter{.options = options,
                          .sink = sink,
                          .summary = summary,
                          .line = {},
                          .lineNumber = 0,
                          .lineOffset = 0,
                          .nextByteOffset = 0};
      bool pendingCr = false;
      std::vector<char> block(readBlockSize);
      while (!stop_token.stop_requested()) {
        stream.read(block.data(), static_cast<std::streamsize>(block.size()));
        const auto bytesRead = static_cast<std::size_t>(stream.gcount());
        if (bytesRead == 0)
          break;

        for (std::size_t offset = 0; offset < bytesRead; ++offset) {
          const auto byte = static_cast<unsigned char>(block[offset]);

          if (pendingCr) {
            pendingCr = false;
            if (byte == asciiLineFeed) {
              if (!emitter.emit(LineEnding::crlf, 2))
                return TextReadStatus::cancelled;

              continue;
            }

            if (!emitter.emit(LineEnding::cr, 1))
              return TextReadStatus::cancelled;
          }

          if (byte == asciiCarriageReturn) {
            if (offset + 1 < bytesRead && block[offset + 1] == '\n') {
              if (!emitter.emit(LineEnding::crlf, 2))
                return TextReadStatus::cancelled;

              ++offset;
              continue;
            }

            if (offset + 1 == bytesRead) {
              pendingCr = true;

              continue;
            }

            if (!emitter.emit(LineEnding::cr, 1))
              return TextReadStatus::cancelled;

            continue;
          }

          if (byte == asciiLineFeed) {
            if (!emitter.emit(LineEnding::lf, 1))
              return TextReadStatus::cancelled;

            continue;
          }

          const auto scalar = byte >= latin1ControlC1Min && byte <= latin1ControlC1Max ? unicodeReplacementCharacter
                                                                                       : static_cast<char32_t>(byte);
          if (!emitter.appendUtf8(utf8FromScalar(scalar), 1))
            return TextReadStatus::lineTooLong;
        }
      }

      if (stop_token.stop_requested())
        return TextReadStatus::cancelled;

      if (stream.bad())
        return TextReadStatus::readFailed;

      if (pendingCr && !emitter.emit(LineEnding::cr, 1))
        return TextReadStatus::cancelled;

      return emitter.finish() ? TextReadStatus::completed : TextReadStatus::cancelled;
    }

    TextReadStatus decodeUtf16Lines(std::ifstream& stream,
                                    TextEncoding encoding,
                                    const SearchOptions& options,
                                    TextReadSummary& summary,
                                    const TextLineSink& sink,
                                    std::stop_token stop_token,
                                    std::size_t bomSize)
    {
      LineEmitter emitter{.options = options,
                          .sink = sink,
                          .summary = summary,
                          .line = {},
                          .lineNumber = 0,
                          .lineOffset = 0,
                          .nextByteOffset = 0};
      emitter.nextByteOffset = bomSize;
      stream.seekg(static_cast<std::streamoff>(bomSize), std::ios::beg);

      std::string pending;
      std::vector<char> block(readBlockSize);
      while (!stop_token.stop_requested()) {
        stream.read(block.data(), static_cast<std::streamsize>(block.size()));
        const auto bytesRead = static_cast<std::size_t>(stream.gcount());
        pending.append(block.data(), bytesRead);

        std::size_t offset = 0;
        while (offset + 1 < pending.size()) {
          if (stop_token.stop_requested())
            return TextReadStatus::cancelled;

          const auto unit = readUtf16CodeUnit(pending, offset, encoding);
          char32_t scalar = unit;
          std::uintmax_t byteWidth = utf16CodeUnitSize;

          if (unit >= utf16HighSurrogateMin && unit <= utf16HighSurrogateMax) {
            if (offset + 3 >= pending.size()) {
              if (!stream.eof())
                break;

              summary.hadInvalidSequences = true;
              scalar = unicodeReplacementCharacter;
            } else {
              const auto low = readUtf16CodeUnit(pending, offset + utf16CodeUnitSize, encoding);
              if (low < utf16LowSurrogateMin || low > utf16LowSurrogateMax) {
                summary.hadInvalidSequences = true;
                scalar = unicodeReplacementCharacter;
              } else {
                const char32_t highPayload = unit - utf16HighSurrogateMin;
                const char32_t lowPayload = low & utf16LowSurrogatePayloadMask;
                scalar = utf16SurrogateBase + (highPayload << utf16HighSurrogateShift) + lowPayload;
                byteWidth = utf16CodeUnitSize * 2;
              }
            }
          } else if (unit >= utf16LowSurrogateMin && unit <= utf16LowSurrogateMax) {
            summary.hadInvalidSequences = true;
            scalar = unicodeReplacementCharacter;
          }

          if (scalar == U'\r' && offset + byteWidth + 1 >= pending.size() && !stream.eof())
            break;

          if (scalar == U'\r' && offset + byteWidth + 1 < pending.size() &&
              readUtf16CodeUnit(pending, offset + static_cast<std::size_t>(byteWidth), encoding) == U'\n') {
            if (!emitter.emit(LineEnding::crlf, byteWidth + utf16CodeUnitSize))
              return TextReadStatus::cancelled;

            offset += static_cast<std::size_t>(byteWidth) + utf16CodeUnitSize;
            continue;
          }

          if (!processDecodedScalar(emitter, scalar, byteWidth))
            return TextReadStatus::lineTooLong;

          offset += static_cast<std::size_t>(byteWidth);
        }

        pending.erase(0, offset);

        if (bytesRead == 0)
          break;
      }

      if (stop_token.stop_requested())
        return TextReadStatus::cancelled;

      if (stream.bad())
        return TextReadStatus::readFailed;

      if (!pending.empty()) {
        if (stop_token.stop_requested())
          return TextReadStatus::cancelled;

        summary.hadInvalidSequences = true;
        if (options.invalidUtf8Policy == InvalidUtf8Policy::fail)
          return TextReadStatus::invalidEncoding;
      }

      return emitter.finish() ? TextReadStatus::completed : TextReadStatus::cancelled;
    }

  } // namespace

  bool sampleLooksBinary(std::string_view sample, TextEncoding encoding)
  {
    if (encoding == TextEncoding::utf16Le || encoding == TextEncoding::utf16Be)
      return false;

    if (sample.empty())
      return false;

    const auto hasNul = std::ranges::find(sample, '\0') != sample.end();
    if (hasNul)
      return true;

    if (sample.size() < binaryControlSampleMinimum)
      return false;

    const auto controlCount = static_cast<std::size_t>(
      std::ranges::count_if(sample, [](char value) { return isBinaryControlByte(static_cast<unsigned char>(value)); }));

    return controlCount * binaryControlRatioDenominator > sample.size() * binaryControlRatioNumerator;
  }

  std::size_t visualColumnForByteOffset(std::string_view utf8Text, std::size_t byteOffset)
  {
    std::size_t column = 1;
    for (std::size_t offset = 0; offset < byteOffset && offset < utf8Text.size();) {
      const auto decoded = decodeUtf8At(utf8Text, offset);
      offset += decoded.valid ? decoded.bytesConsumed : 1;
      ++column;
    }

    return column;
  }

  TextReadSummary readTextFileLines(const std::filesystem::path& path,
                                    const SearchOptions& options,
                                    const TextLineSink& sink,
                                    std::stop_token stop_token)
  {
    TextReadSummary summary;
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
      summary.status = TextReadStatus::openFailed;

      return summary;
    }

    const auto sample = readSample(stream, options.binarySampleSize);
    const auto detected = detectEncoding(sample, options.fallbackEncoding);
    summary.encoding = detected.encoding;
    summary.hadBom = detected.hadBom;

    if (!options.includeBinary && sampleLooksBinary(sample, detected.encoding)) {
      summary.status = TextReadStatus::binarySkipped;

      return summary;
    }

    switch (detected.encoding) {
    case TextEncoding::utf8:
      summary.status = decodeUtf8Lines(stream, options, summary, sink, stop_token, detected.bomSize);
      break;
    case TextEncoding::utf16Le:
    case TextEncoding::utf16Be:
      summary.status =
        decodeUtf16Lines(stream, detected.encoding, options, summary, sink, stop_token, detected.bomSize);
      break;
    case TextEncoding::latin1:
      summary.status = decodeLatin1Lines(stream, options, summary, sink, stop_token);
      break;
    }

    return summary;
  }

} // namespace uburu::text
