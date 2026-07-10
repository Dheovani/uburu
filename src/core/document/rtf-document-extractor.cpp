#include "core/document/rtf-document-extractor.hpp"

#include "core/text/text-file-reader.hpp"

#include <charconv>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace uburu::document
{
  namespace
  {

    constexpr std::size_t maximumRtfGroupDepth = 256;
    constexpr std::size_t maximumRtfControlWordLength = 32;
    constexpr std::int64_t maximumRtfBinarySkipLength = 16 * 1024 * 1024;
    constexpr std::int64_t maximumRtfUnicodeFallbackCharacters = 32;
    constexpr char32_t maximumUnicodeScalar = 0x10FFFFU;
    constexpr char32_t surrogateRangeStart = 0xD800U;
    constexpr char32_t surrogateRangeEnd = 0xDFFFU;
    constexpr std::int64_t rtfSignedUnicodeModulo = 65536;
    constexpr unsigned char utf8ContinuationByteTag = 0b1000'0000U;
    constexpr unsigned char utf8TwoByteLeadTag = 0b1100'0000U;
    constexpr unsigned char utf8ThreeByteLeadTag = 0b1110'0000U;
    constexpr unsigned char utf8FourByteLeadTag = 0b1111'0000U;
    constexpr unsigned char utf8ContinuationPayloadMask = 0b0011'1111U;
    constexpr unsigned int utf8OneByteMaximum = 0x7FU;
    constexpr unsigned int utf8TwoByteMaximum = 0x7FFU;
    constexpr unsigned int utf8ThreeByteMaximum = 0xFFFFU;
    constexpr unsigned int utf8SixBitShift = 6U;
    constexpr unsigned int utf8TwelveBitShift = 12U;
    constexpr unsigned int utf8EighteenBitShift = 18U;

    struct RtfState
    {
      bool skipDestination{false};
      int unicodeFallbackCharacters{1};
    };

    struct RtfParser
    {
      RtfState state;
      std::vector<RtfState> stack;
      std::string text;
      bool pendingIgnorableDestination{false};
      std::size_t skipCharacters{0};
      bool safetyLimitExceeded{false};
      bool parserFailed{false};
    };

    [[nodiscard]]
    std::string lowerAscii(std::string value)
    {
      for (auto& character : value) {
        if (character >= 'A' && character <= 'Z')
          character = static_cast<char>(character - 'A' + 'a');
      }

      return value;
    }

    [[nodiscard]]
    bool isAsciiLetter(char character)
    {
      return (character >= 'A' && character <= 'Z') ||
             (character >= 'a' && character <= 'z');
    }

    [[nodiscard]]
    bool isAsciiDigit(char character)
    {
      return character >= '0' && character <= '9';
    }

    [[nodiscard]]
    bool isHexDigit(char character)
    {
      return isAsciiDigit(character) ||
             (character >= 'A' && character <= 'F') ||
             (character >= 'a' && character <= 'f');
    }

    [[nodiscard]]
    int hexValue(char character)
    {
      if (character >= '0' && character <= '9')
        return character - '0';

      if (character >= 'A' && character <= 'F')
        return character - 'A' + 10;

      return character - 'a' + 10;
    }

    [[nodiscard]]
    bool isValidUnicodeScalar(char32_t scalar)
    {
      return scalar <= maximumUnicodeScalar &&
             (scalar < surrogateRangeStart || scalar > surrogateRangeEnd);
    }

    void appendUtf8(char32_t scalar, std::string& output)
    {
      if (!isValidUnicodeScalar(scalar))
        return;

      if (scalar <= utf8OneByteMaximum) {
        output.push_back(static_cast<char>(scalar));

        return;
      }

      if (scalar <= utf8TwoByteMaximum) {
        output.push_back(static_cast<char>(utf8TwoByteLeadTag | (scalar >> utf8SixBitShift)));
        output.push_back(static_cast<char>(utf8ContinuationByteTag | (scalar & utf8ContinuationPayloadMask)));

        return;
      }

      if (scalar <= utf8ThreeByteMaximum) {
        output.push_back(static_cast<char>(utf8ThreeByteLeadTag | (scalar >> utf8TwelveBitShift)));
        output.push_back(
          static_cast<char>(utf8ContinuationByteTag | ((scalar >> utf8SixBitShift) & utf8ContinuationPayloadMask)));
        output.push_back(static_cast<char>(utf8ContinuationByteTag | (scalar & utf8ContinuationPayloadMask)));

        return;
      }

      output.push_back(static_cast<char>(utf8FourByteLeadTag | (scalar >> utf8EighteenBitShift)));
      output.push_back(
        static_cast<char>(utf8ContinuationByteTag | ((scalar >> utf8TwelveBitShift) & utf8ContinuationPayloadMask)));
      output.push_back(
        static_cast<char>(utf8ContinuationByteTag | ((scalar >> utf8SixBitShift) & utf8ContinuationPayloadMask)));
      output.push_back(static_cast<char>(utf8ContinuationByteTag | (scalar & utf8ContinuationPayloadMask)));
    }

    void appendLatin1AsUtf8(unsigned char character, std::string& output)
    {
      appendUtf8(character, output);
    }

    void appendWordSeparator(std::string& output)
    {
      if (output.empty() || output.back() == ' ' || output.back() == '\n' || output.back() == '\t')
        return;

      output.push_back(' ');
    }

    void appendBlockSeparator(std::string& output)
    {
      while (!output.empty() && (output.back() == ' ' || output.back() == '\t')) {
        output.pop_back();
      }

      if (output.empty() || output.back() == '\n')
        return;

      output.push_back('\n');
    }

    void trimTrailingWhitespace(std::string& output)
    {
      while (!output.empty() && (output.back() == ' ' || output.back() == '\t' || output.back() == '\n')) {
        output.pop_back();
      }
    }

    [[nodiscard]]
    bool isSkippedDestination(std::string_view word)
    {
      return word == "fonttbl"
          || word == "colortbl"
          || word == "stylesheet"
          || word == "info"
          || word == "pict"
          || word == "object"
          || word == "data"
          || word == "nonshppict"
          || word == "shp"
          || word == "header"
          || word == "footer";
    }

    [[nodiscard]]
    std::optional<std::int64_t> parseSignedInteger(std::string_view text)
    {
      if (text.empty())
        return std::nullopt;

      std::int64_t value = 0;
      const auto* begin = text.data();
      const auto* end = text.data() + text.size();
      const auto result = std::from_chars(begin, end, value);

      if (result.ec != std::errc{} || result.ptr != end)
        return std::nullopt;

      return value;
    }

    [[nodiscard]]
    char32_t unicodeScalarFromRtfParameter(std::int64_t parameter)
    {
      if (parameter < 0)
        parameter += rtfSignedUnicodeModulo;

      return static_cast<char32_t>(parameter);
    }

    void handleControlSymbol(RtfParser& parser, std::string_view line, std::size_t& offset)
    {
      if (offset >= line.size())
        return;

      const auto symbol = line[offset];
      ++offset;

      if (symbol == '*') {
        parser.pendingIgnorableDestination = true;

        return;
      }

      if (parser.state.skipDestination)
        return;

      if (symbol == '\\' || symbol == '{' || symbol == '}') {
        parser.text.push_back(symbol);

        return;
      }

      if (symbol == '~') {
        appendWordSeparator(parser.text);

        return;
      }

      if (symbol == '-') {
        return;
      }

      if (symbol == '_') {
        parser.text.push_back('-');

        return;
      }

      if (symbol == '\'') {
        if (offset + 1 >= line.size() ||
            !isHexDigit(line[offset]) ||
            !isHexDigit(line[offset + 1])) {
          parser.parserFailed = true;

          return;
        }

        const auto character = static_cast<unsigned char>((hexValue(line[offset]) << 4) | hexValue(line[offset + 1]));
        appendLatin1AsUtf8(character, parser.text);
        offset += 2;
      }
    }

    void skipFallbackCharacter(RtfParser& parser, std::string_view line, std::size_t& offset)
    {
      --parser.skipCharacters;

      if (line[offset] != '\\') {
        ++offset;

        return;
      }

      ++offset;

      if (offset >= line.size())
        return;

      if (line[offset] == '\'' && offset + 2 < line.size() && isHexDigit(line[offset + 1]) &&
          isHexDigit(line[offset + 2])) {
        offset += 3;

        return;
      }

      if (!isAsciiLetter(line[offset])) {
        ++offset;

        return;
      }

      while (offset < line.size() && isAsciiLetter(line[offset])) {
        ++offset;
      }

      if (offset < line.size() && line[offset] == '-')
        ++offset;

      while (offset < line.size() && isAsciiDigit(line[offset])) {
        ++offset;
      }

      if (offset < line.size() && line[offset] == ' ')
        ++offset;
    }

    void handleControlWord(RtfParser& parser,
                           std::string_view word,
                           std::optional<std::int64_t> parameter,
                           bool hasDelimiterSpace)
    {
      if (word.size() > maximumRtfControlWordLength) {
        parser.safetyLimitExceeded = true;

        return;
      }

      if (parser.pendingIgnorableDestination || isSkippedDestination(word)) {
        parser.state.skipDestination = true;
        parser.pendingIgnorableDestination = false;

        return;
      }

      parser.pendingIgnorableDestination = false;

      if (parser.state.skipDestination)
        return;

      if (word == "par" || word == "line") {
        appendBlockSeparator(parser.text);

        return;
      }

      if (word == "tab") {
        parser.text.push_back('\t');

        return;
      }

      if (word == "emdash") {
        appendUtf8(U'—', parser.text);

        return;
      }

      if (word == "endash") {
        appendUtf8(U'–', parser.text);

        return;
      }

      if (word == "bullet") {
        appendUtf8(U'•', parser.text);

        return;
      }

      if (word == "lquote" || word == "rquote") {
        parser.text.push_back('\'');

        return;
      }

      if (word == "ldblquote" || word == "rdblquote") {
        parser.text.push_back('"');

        return;
      }

      if (word == "uc") {
        if (parameter && *parameter >= 0 && *parameter <= maximumRtfUnicodeFallbackCharacters)
          parser.state.unicodeFallbackCharacters = static_cast<int>(*parameter);

        return;
      }

      if (word == "u") {
        if (!parameter) {
          parser.parserFailed = true;

          return;
        }

        appendUtf8(unicodeScalarFromRtfParameter(*parameter), parser.text);
        parser.skipCharacters = static_cast<std::size_t>(parser.state.unicodeFallbackCharacters);

        return;
      }

      if (word == "bin") {
        if (!parameter || *parameter < 0 || *parameter > maximumRtfBinarySkipLength) {
          parser.safetyLimitExceeded = true;

          return;
        }

        parser.skipCharacters = static_cast<std::size_t>(*parameter);

        return;
      }

      if (hasDelimiterSpace)
        return;
    }

    void parseLine(RtfParser& parser, std::string_view line)
    {
      std::size_t offset = 0;

      while (offset < line.size() && !parser.safetyLimitExceeded && !parser.parserFailed) {
        if (parser.skipCharacters > 0) {
          skipFallbackCharacter(parser, line, offset);

          continue;
        }

        const auto character = line[offset];

        if (character == '{') {
          if (parser.stack.size() >= maximumRtfGroupDepth) {
            parser.safetyLimitExceeded = true;

            return;
          }

          parser.stack.push_back(parser.state);
          ++offset;

          continue;
        }

        if (character == '}') {
          if (parser.stack.empty()) {
            parser.parserFailed = true;

            return;
          }

          parser.state = parser.stack.back();
          parser.stack.pop_back();
          parser.pendingIgnorableDestination = false;
          ++offset;

          continue;
        }

        if (character == '\\') {
          ++offset;

          if (offset >= line.size())
            return;

          if (!isAsciiLetter(line[offset])) {
            handleControlSymbol(parser, line, offset);

            continue;
          }

          const auto wordBegin = offset;

          while (offset < line.size() && isAsciiLetter(line[offset])) {
            ++offset;
          }

          const auto word = line.substr(wordBegin, offset - wordBegin);
          const auto parameterBegin = offset;

          if (offset < line.size() && line[offset] == '-')
            ++offset;

          while (offset < line.size() && isAsciiDigit(line[offset])) {
            ++offset;
          }

          std::optional<std::int64_t> parameter;

          if (offset > parameterBegin)
            parameter = parseSignedInteger(line.substr(parameterBegin, offset - parameterBegin));

          const auto hasDelimiterSpace = offset < line.size() && line[offset] == ' ';

          if (hasDelimiterSpace)
            ++offset;

          handleControlWord(parser, word, parameter, hasDelimiterSpace);

          continue;
        }

        if (!parser.state.skipDestination)
          parser.text.push_back(character);

        ++offset;
      }
    }

    [[nodiscard]]
    DocumentExtractionStatus extractionStatusFromTextStatus(text::TextReadStatus status)
    {
      switch (status) {
      case text::TextReadStatus::completed:
        return DocumentExtractionStatus::completed;
      case text::TextReadStatus::cancelled:
        return DocumentExtractionStatus::cancelled;
      case text::TextReadStatus::openFailed:
        return DocumentExtractionStatus::openFailed;
      case text::TextReadStatus::readFailed:
        return DocumentExtractionStatus::readFailed;
      case text::TextReadStatus::binarySkipped:
        return DocumentExtractionStatus::binarySkipped;
      case text::TextReadStatus::invalidEncoding:
        return DocumentExtractionStatus::invalidEncoding;
      case text::TextReadStatus::lineTooLong:
        return DocumentExtractionStatus::safetyLimitExceeded;
      }

      return DocumentExtractionStatus::parserFailed;
    }

    [[nodiscard]]
    DocumentExtractionStatus parserStatus(const RtfParser& parser)
    {
      if (parser.safetyLimitExceeded)
        return DocumentExtractionStatus::safetyLimitExceeded;

      if (parser.parserFailed || !parser.stack.empty())
        return DocumentExtractionStatus::parserFailed;

      return DocumentExtractionStatus::completed;
    }

  } // namespace

  std::string_view RtfDocumentExtractor::name() const
  {
    return "rtf";
  }

  bool RtfDocumentExtractor::supports(const std::filesystem::path& path) const
  {
    return lowerAscii(path.extension().string()) == ".rtf";
  }

  DocumentExtractionSummary RtfDocumentExtractor::extract(
    const std::filesystem::path& path,
    const DocumentExtractionOptions& options,
    const ExtractedTextSink& sink,
    std::stop_token stopToken) const
  {
    DocumentExtractionSummary extractionSummary;
    RtfParser parser;

    const auto readSummary = text::readTextFileLines(
      path,
      options.textOptions,
      [&](const text::TextLine& line) {
        if (stopToken.stop_requested())
          return false;

        parseLine(parser, line.text);

        return !parser.safetyLimitExceeded && !parser.parserFailed;
      },
      stopToken);

    extractionSummary.encoding = readSummary.encoding;
    extractionSummary.error = readSummary.error;
    extractionSummary.hadBom = readSummary.hadBom;
    extractionSummary.hadInvalidSequences = readSummary.hadInvalidSequences;
    extractionSummary.status = extractionStatusFromTextStatus(readSummary.status);

    const auto parsedStatus = parserStatus(parser);

    if (parsedStatus != DocumentExtractionStatus::completed)
      extractionSummary.status = parsedStatus;

    if (extractionSummary.status != DocumentExtractionStatus::completed)
      return extractionSummary;

    if (parser.text.empty())
      return extractionSummary;

    trimTrailingWhitespace(parser.text);

    if (parser.text.empty())
      return extractionSummary;

    if (options.maximumExtractedBytes > 0 &&
        static_cast<std::uintmax_t>(parser.text.size()) > options.maximumExtractedBytes) {
      extractionSummary.status = DocumentExtractionStatus::safetyLimitExceeded;

      return extractionSummary;
    }

    ExtractedTextSegment segment;

    segment.text = std::move(parser.text);

    if (!sink(segment)) {
      extractionSummary.status = DocumentExtractionStatus::cancelled;

      return extractionSummary;
    }

    extractionSummary.segmentsExtracted = 1;
    extractionSummary.bytesExtracted = segment.text.size();

    return extractionSummary;
  }

} // namespace uburu::document
