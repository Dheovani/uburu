#include "core/search/direct-search-engine.hpp"

#include "core/search/search-errors.hpp"
#include "core/search/search-query-validation.hpp"
#include "core/text/regex-matcher.hpp"
#include "core/text/text-matcher.hpp"

#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace uburu::search
{
  namespace
  {

    enum class PublishDecision
    {
      continue_search,
      stop_current_file,
      stop_search
    };

    SearchErrorCode error_code_from_regex_status(text::RegexMatchStatus status)
    {
      if (status == text::RegexMatchStatus::timed_out)
        return SearchErrorCode::regex_timeout;

      return SearchErrorCode::regex_resource_limit_exceeded;
    }

    bool searches_content(SearchTarget target)
    {
      return target == SearchTarget::content || target == SearchTarget::content_and_file_name;
    }

    bool searches_file_name(SearchTarget target)
    {
      return target == SearchTarget::file_name || target == SearchTarget::content_and_file_name;
    }

    std::vector<text::MatchPosition> find_literal_matches(std::string_view text,
                                                          const SearchQuery& query)
    {
      return text::find_all_literals(text, query.expression, query.options);
    }

    std::optional<std::vector<text::MatchPosition>>
    find_regex_matches(std::string_view text, const text::RegexMatcher& regex_matcher,
                       SearchSummary& summary)
    {
      const auto regex_result = regex_matcher.find_all(text);

      if (regex_result.status != text::RegexMatchStatus::completed) {
        summary.errors.push_back(
            make_search_error(error_code_from_regex_status(regex_result.status),
                              std::to_string(regex_result.backend_error_code)));

        return std::nullopt;
      }

      return std::move(regex_result.matches);
    }

    std::optional<std::vector<text::MatchPosition>>
    find_matches(std::string_view text, const SearchQuery& query,
                 const std::optional<text::RegexMatcher>& regex_matcher, SearchSummary& summary)
    {
      if (regex_matcher)
        return find_regex_matches(text, *regex_matcher, summary);

      return find_literal_matches(text, query);
    }

    void report_partial_failure(SearchSummary& summary, SearchErrorCode code,
                                const FileEntry& entry)
    {
      ++summary.files_with_read_errors;
      summary.partial_failure = true;
      summary.errors.push_back(make_search_error(code, entry.relative_path.generic_string()));
    }

    PublishDecision publish_matches(const FileEntry& entry, SearchResultKind kind,
                                    std::size_t line_number, std::string_view line_text,
                                    const std::vector<text::MatchPosition>& matches,
                                    const SearchQuery& query, SearchSummary& summary,
                                    std::size_t& file_matches, const ResultSink& sink)
    {
      for (const auto& match : matches) {
        if (summary.matches >= query.options.result_limit) {
          summary.limit_reached = true;

          return PublishDecision::stop_search;
        }

        if (file_matches >= query.options.per_file_result_limit) {
          ++summary.files_with_match_limit_reached;

          return PublishDecision::stop_current_file;
        }

        ++summary.matches;
        ++file_matches;

        if (!sink(SearchResult{kind, entry.relative_path, line_number, match.offset + 1,
                               match.length, std::string{line_text}}))
          return PublishDecision::stop_search;
      }

      return PublishDecision::continue_search;
    }

  } // namespace

  DirectSearchEngine::DirectSearchEngine(std::shared_ptr<const filesystem::FileScanner> scanner)
      : scanner_(std::move(scanner))
  {
    if (!scanner_)
      throw std::invalid_argument("DirectSearchEngine requires a file scanner");
  }

  SearchSummary DirectSearchEngine::search(const SearchQuery& query, ResultSink sink,
                                           std::stop_token stop_token) const
  {
    SearchSummary summary;
    summary.errors = validate_search_query(query);
    if (!summary.errors.empty())
      return summary;

    std::optional<text::RegexMatcher> regex_matcher;
    if (query.options.mode == SearchMode::regex) {
      auto compiled = text::compile_regex(query.expression, query.options);
      if (!compiled.matcher) {
        const auto& error = compiled.error;
        summary.errors.push_back(make_search_error(
            SearchErrorCode::regex_compile_failed, error ? error->message : std::string{},
            error ? error->offset : std::optional<std::size_t>{}));

        return summary;
      }
      summary.regex_execution_mode = compiled.matcher->jit_enabled()
                                         ? RegexExecutionMode::jit
                                         : RegexExecutionMode::interpreted_fallback;
      regex_matcher = std::move(compiled.matcher);
    }

    scanner_->scan(
        query.root, query.options,
        [&](FileEntry entry) {
          if (stop_token.stop_requested())
            return false;

          ++summary.files_scanned;
          std::size_t file_matches = 0;

          if (searches_file_name(query.options.target)) {
            const auto path_text = entry.relative_path.generic_string();
            const auto path_matches = find_matches(path_text, query, regex_matcher, summary);

            if (!path_matches)
              return false;

            const auto decision =
                publish_matches(entry, SearchResultKind::file_name, 0, path_text, *path_matches,
                                query, summary, file_matches, sink);

            if (decision == PublishDecision::stop_search)
              return false;

            if (decision == PublishDecision::stop_current_file)
              return true;
          }

          if (!searches_content(query.options.target))
            return true;

          std::ifstream stream(entry.absolute_path, std::ios::binary);
          if (!stream) {
            report_partial_failure(summary, SearchErrorCode::file_open_failed, entry);

            return true;
          }

          std::string line;
          std::size_t line_number = 0;
          while (std::getline(stream, line)) {
            ++line_number;
            if (!query.options.include_binary && text::looks_binary(line))
              return true;

            const auto matches = find_matches(line, query, regex_matcher, summary);

            if (!matches)
              return false;

            if (matches->empty())
              continue;

            const auto decision =
                publish_matches(entry, SearchResultKind::content, line_number, line, *matches,
                                query, summary, file_matches, sink);

            if (decision == PublishDecision::stop_search)
              return false;

            if (decision == PublishDecision::stop_current_file)
              return true;
          }

          if (stream.bad())
            report_partial_failure(summary, SearchErrorCode::file_read_failed, entry);

          return true;
        },
        stop_token);

    summary.cancelled = stop_token.stop_requested();
    return summary;
  }

} // namespace uburu::search
