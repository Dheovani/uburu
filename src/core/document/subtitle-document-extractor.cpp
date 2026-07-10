#include "core/document/subtitle-document-extractor.hpp"

#include "core/text/text-file-reader.hpp"

#include <cctype>
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

    constexpr std::uintmax_t millisecondsPerSecond = 1000;
    constexpr std::uintmax_t secondsPerMinute = 60;
    constexpr std::uintmax_t minutesPerHour = 60;
    constexpr std::size_t minimumTimestampDigits = 2;

    struct CueState
    {
      DocumentLocation location;
      std::vector<std::string> lines;
      bool active{false};
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
    std::string_view trimAsciiWhitespace(std::string_view text)
    {
      while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) {
        text.remove_prefix(1);
      }

      while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) {
        text.remove_suffix(1);
      }

      return text;
    }

    [[nodiscard]]
    bool startsWithAsciiCaseInsensitive(std::string_view text, std::string_view prefix)
    {
      if (text.size() < prefix.size())
        return false;

      for (std::size_t index = 0; index < prefix.size(); ++index) {
        auto character = text[index];
        auto expected = prefix[index];

        if (character >= 'A' && character <= 'Z')
          character = static_cast<char>(character - 'A' + 'a');

        if (expected >= 'A' && expected <= 'Z')
          expected = static_cast<char>(expected - 'A' + 'a');

        if (character != expected)
          return false;
      }

      return true;
    }

    [[nodiscard]]
    std::optional<std::uintmax_t> parseUnsignedInteger(std::string_view text)
    {
      if (text.empty())
        return std::nullopt;

      std::uintmax_t value = 0;
      const auto* begin = text.data();
      const auto* end = text.data() + text.size();
      const auto result = std::from_chars(begin, end, value);

      if (result.ec != std::errc{} || result.ptr != end)
        return std::nullopt;

      return value;
    }

    [[nodiscard]]
    std::optional<std::uintmax_t> parseTimestampMilliseconds(std::string_view text)
    {
      text = trimAsciiWhitespace(text);

      const auto firstColon = text.find(':');
      const auto decimalSeparator = text.find_first_of(".,");

      if (decimalSeparator == std::string_view::npos)
        return std::nullopt;

      std::uintmax_t hours = 0;
      std::uintmax_t minutes = 0;
      std::uintmax_t seconds = 0;

      if (firstColon != std::string_view::npos && firstColon < decimalSeparator) {
        const auto secondColon = text.find(':', firstColon + 1);

        if (secondColon != std::string_view::npos && secondColon < decimalSeparator) {
          const auto parsedHours = parseUnsignedInteger(text.substr(0, firstColon));
          const auto parsedMinutes = parseUnsignedInteger(text.substr(firstColon + 1, secondColon - firstColon - 1));
          const auto parsedSeconds =
            parseUnsignedInteger(text.substr(secondColon + 1, decimalSeparator - secondColon - 1));

          if (!parsedHours || !parsedMinutes || !parsedSeconds)
            return std::nullopt;

          hours = *parsedHours;
          minutes = *parsedMinutes;
          seconds = *parsedSeconds;
        } else {
          const auto parsedMinutes = parseUnsignedInteger(text.substr(0, firstColon));
          const auto parsedSeconds =
            parseUnsignedInteger(text.substr(firstColon + 1, decimalSeparator - firstColon - 1));

          if (!parsedMinutes || !parsedSeconds)
            return std::nullopt;

          minutes = *parsedMinutes;
          seconds = *parsedSeconds;
        }
      } else {
        const auto parsedSeconds = parseUnsignedInteger(text.substr(0, decimalSeparator));

        if (!parsedSeconds)
          return std::nullopt;

        seconds = *parsedSeconds;
      }

      auto millisecondsText = std::string{trimAsciiWhitespace(text.substr(decimalSeparator + 1))};

      if (millisecondsText.size() < minimumTimestampDigits)
        return std::nullopt;

      if (millisecondsText.size() > 3)
        millisecondsText.resize(3);

      while (millisecondsText.size() < 3) {
        millisecondsText.push_back('0');
      }

      const auto milliseconds = parseUnsignedInteger(millisecondsText);

      if (!milliseconds)
        return std::nullopt;

      const auto totalSeconds = ((hours * minutesPerHour) + minutes) * secondsPerMinute + seconds;

      return totalSeconds * millisecondsPerSecond + *milliseconds;
    }

    [[nodiscard]]
    std::optional<DocumentLocation> parseTimingLine(std::string_view line)
    {
      const auto arrow = line.find("-->");

      if (arrow == std::string_view::npos)
        return std::nullopt;

      const auto startTime = parseTimestampMilliseconds(line.substr(0, arrow));

      if (!startTime)
        return std::nullopt;

      DocumentLocation location;

      location.kind = DocumentLocationKind::timestamp;
      location.primary = static_cast<std::size_t>(*startTime);
      location.label = std::string{trimAsciiWhitespace(line.substr(0, arrow))};

      return location;
    }

    [[nodiscard]]
    std::string decodedCueText(std::string_view text)
    {
      std::string decoded;
      decoded.reserve(text.size());

      for (std::size_t offset = 0; offset < text.size(); ++offset) {
        if (text[offset] == '<') {
          const auto tagEnd = text.find('>', offset + 1);

          if (tagEnd == std::string_view::npos)
            continue;

          offset = tagEnd;

          continue;
        }

        decoded.push_back(text[offset]);
      }

      return std::string{trimAsciiWhitespace(decoded)};
    }

    [[nodiscard]]
    std::string cueText(const CueState& cue)
    {
      std::string text;

      for (const auto& line : cue.lines) {
        const auto decodedLine = decodedCueText(line);

        if (decodedLine.empty())
          continue;

        if (!text.empty())
          text.push_back(' ');

        text += decodedLine;
      }

      return text;
    }

    [[nodiscard]]
    bool wouldExceedByteLimit(const DocumentExtractionOptions& options,
                              std::uintmax_t bytesExtracted,
                              std::size_t nextSegmentBytes)
    {
      return options.maximumExtractedBytes > 0 &&
             bytesExtracted + static_cast<std::uintmax_t>(nextSegmentBytes) > options.maximumExtractedBytes;
    }

    [[nodiscard]]
    bool reachedSegmentLimit(const DocumentExtractionOptions& options, std::size_t segmentsExtracted)
    {
      return options.maximumSegments > 0 && segmentsExtracted >= options.maximumSegments;
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

    bool publishCue(
      CueState& cue,
      DocumentExtractionSummary& summary,
      const DocumentExtractionOptions& options,
      const ExtractedTextSink& sink)
    {
      if (!cue.active) {
        cue.lines.clear();

        return true;
      }

      auto text = cueText(cue);

      cue.active = false;
      cue.lines.clear();

      if (text.empty())
        return true;

      if (wouldExceedByteLimit(options, summary.bytesExtracted, text.size()) ||
          reachedSegmentLimit(options, summary.segmentsExtracted)) {
        summary.status = DocumentExtractionStatus::safetyLimitExceeded;

        return false;
      }

      ExtractedTextSegment segment;

      segment.text = std::move(text);
      segment.location = cue.location;

      if (!sink(segment)) {
        summary.status = DocumentExtractionStatus::cancelled;

        return false;
      }

      ++summary.segmentsExtracted;
      summary.bytesExtracted += segment.text.size();

      return true;
    }

  } // namespace

  std::string_view SubtitleDocumentExtractor::name() const
  {
    return "subtitle";
  }

  bool SubtitleDocumentExtractor::supports(const std::filesystem::path& path) const
  {
    const auto extension = lowerAscii(path.extension().string());

    return extension == ".srt" || extension == ".vtt";
  }

  DocumentExtractionSummary SubtitleDocumentExtractor::extract(
    const std::filesystem::path& path,
    const DocumentExtractionOptions& options,
    const ExtractedTextSink& sink,
    std::stop_token stopToken) const
  {
    DocumentExtractionSummary extractionSummary;
    CueState cue;
    bool skippingNote = false;

    const auto readSummary = text::readTextFileLines(
      path,
      options.textOptions,
      [&](const text::TextLine& line) {
        if (stopToken.stop_requested())
          return false;

        auto lineText = trimAsciiWhitespace(line.text);

        if (lineText.empty()) {
          skippingNote = false;

          return publishCue(cue, extractionSummary, options, sink);
        }

        if (skippingNote)
          return true;

        if (!cue.active && startsWithAsciiCaseInsensitive(lineText, "WEBVTT"))
          return true;

        if (!cue.active && startsWithAsciiCaseInsensitive(lineText, "NOTE")) {
          skippingNote = true;

          return true;
        }

        if (const auto location = parseTimingLine(lineText)) {
          if (!publishCue(cue, extractionSummary, options, sink))
            return false;

          cue.location = *location;
          cue.active = true;

          return true;
        }

        if (cue.active)
          cue.lines.push_back(std::string{lineText});

        return true;
      },
      stopToken);

    extractionSummary.encoding = readSummary.encoding;
    extractionSummary.error = readSummary.error;
    extractionSummary.hadBom = readSummary.hadBom;
    extractionSummary.hadInvalidSequences = readSummary.hadInvalidSequences;

    if (extractionSummary.status == DocumentExtractionStatus::completed)
      extractionSummary.status = extractionStatusFromTextStatus(readSummary.status);

    if (extractionSummary.status == DocumentExtractionStatus::completed &&
        !publishCue(cue, extractionSummary, options, sink))
      return extractionSummary;

    return extractionSummary;
  }

} // namespace uburu::document
