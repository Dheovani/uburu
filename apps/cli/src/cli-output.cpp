#include "cli-output.hpp"

#include <filesystem>
#include <ostream>
#include <sstream>
#include <string_view>

namespace uburu::cli
{
  namespace
  {

    [[nodiscard]]
    std::string jsonEscape(std::string_view value)
    {
      std::ostringstream escaped;

      for (const auto character : value) {
        switch (character) {
        case '"':
          escaped << "\\\"";
          break;
        case '\\':
          escaped << "\\\\";
          break;
        case '\b':
          escaped << "\\b";
          break;
        case '\f':
          escaped << "\\f";
          break;
        case '\n':
          escaped << "\\n";
          break;
        case '\r':
          escaped << "\\r";
          break;
        case '\t':
          escaped << "\\t";
          break;
        default:
          escaped << character;
          break;
        }
      }

      return escaped.str();
    }

    [[nodiscard]]
    std::string pathString(const std::filesystem::path& path)
    {
      return path.generic_string();
    }

    void writeHumanSearchResult(std::ostream& output, const SearchResult& result)
    {
      output << pathString(result.path) << ':' << result.line << ':' << result.column << "  " << result.lineText << '\n';
    }

    void writeJsonSearchResult(std::ostream& output, const SearchResult& result)
    {
      output << "{\"type\":\"result\"";
      output << ",\"path\":\"" << jsonEscape(pathString(result.path)) << '"';
      output << ",\"line\":" << result.line;
      output << ",\"column\":" << result.column;
      output << ",\"matchLength\":" << result.matchLength;
      output << ",\"text\":\"" << jsonEscape(result.lineText) << '"';
      output << "}\n";
    }

    [[nodiscard]]
    std::string_view searchErrorCodeName(search::SearchErrorCode code)
    {
      switch (code) {
      case search::SearchErrorCode::emptyRoot:
        return "emptyRoot";
      case search::SearchErrorCode::rootNotFound:
        return "rootNotFound";
      case search::SearchErrorCode::rootNotDirectory:
        return "rootNotDirectory";
      case search::SearchErrorCode::rootUnavailable:
        return "rootUnavailable";
      case search::SearchErrorCode::emptyExpression:
        return "emptyExpression";
      case search::SearchErrorCode::unsupportedSearchMode:
        return "unsupportedSearchMode";
      case search::SearchErrorCode::regexCompileFailed:
        return "regexCompileFailed";
      case search::SearchErrorCode::regexResourceLimitExceeded:
        return "regexResourceLimitExceeded";
      case search::SearchErrorCode::regexTimeout:
        return "regexTimeout";
      case search::SearchErrorCode::invalidRegexLimit:
        return "invalidRegexLimit";
      case search::SearchErrorCode::invalidResultLimit:
        return "invalidResultLimit";
      case search::SearchErrorCode::invalidPerFileResultLimit:
        return "invalidPerFileResultLimit";
      case search::SearchErrorCode::invalidMaximumFileSize:
        return "invalidMaximumFileSize";
      case search::SearchErrorCode::fileOpenFailed:
        return "fileOpenFailed";
      case search::SearchErrorCode::fileReadFailed:
        return "fileReadFailed";
      }

      return "unknown";
    }

    void writeHumanSearchError(std::ostream& output, const search::SearchError& error)
    {
      output << "error=" << searchErrorCodeName(error.code);

      if (!error.context.empty())
        output << " context=\"" << error.context << '"';

      if (error.offset)
        output << " offset=" << *error.offset;

      output << '\n';
    }

    void writeJsonSearchError(std::ostream& output, const search::SearchError& error)
    {
      output << "{\"type\":\"error\"";
      output << ",\"code\":\"" << searchErrorCodeName(error.code) << '"';
      output << ",\"translationKey\":\"" << jsonEscape(error.translationKey) << '"';
      output << ",\"context\":\"" << jsonEscape(error.context) << '"';

      if (error.offset)
        output << ",\"offset\":" << *error.offset;

      output << "}\n";
    }

    void writeHumanSearchSummary(std::ostream& output, const search::SearchSummary& summary)
    {
      for (const auto& error : summary.errors)
        writeHumanSearchError(output, error);

      output << "matches=" << summary.matches;
      output << " filesScanned=" << summary.filesScanned;
      output << " readErrors=" << summary.filesWithReadErrors;
      output << " cancelled=" << (summary.cancelled ? "true" : "false");
      output << '\n';
    }

    void writeJsonSearchSummary(std::ostream& output, const search::SearchSummary& summary)
    {
      for (const auto& error : summary.errors)
        writeJsonSearchError(output, error);

      output << "{\"type\":\"summary\"";
      output << ",\"matches\":" << summary.matches;
      output << ",\"filesScanned\":" << summary.filesScanned;
      output << ",\"filesWithReadErrors\":" << summary.filesWithReadErrors;
      output << ",\"cancelled\":" << (summary.cancelled ? "true" : "false");
      output << ",\"partialFailure\":" << (summary.partialFailure ? "true" : "false");
      output << "}\n";
    }

  } // namespace

  void writeSearchResult(std::ostream& output, const SearchResult& result, CliOutputFormat format)
  {
    if (format == CliOutputFormat::jsonLines) {
      writeJsonSearchResult(output, result);

      return;
    }

    writeHumanSearchResult(output, result);
  }

  void writeSearchSummary(std::ostream& output, const search::SearchSummary& summary, CliOutputFormat format)
  {
    if (format == CliOutputFormat::jsonLines) {
      writeJsonSearchSummary(output, summary);

      return;
    }

    writeHumanSearchSummary(output, summary);
  }

} // namespace uburu::cli
