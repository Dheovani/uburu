#include "core/search/direct-search-engine.hpp"

#include "core/search/search-query-validation.hpp"
#include "core/text/regex-matcher.hpp"
#include "core/text/text-matcher.hpp"

#include <fstream>
#include <optional>
#include <stdexcept>

namespace uburu::search
{

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
        summary.errors.push_back(
          SearchError{SearchErrorCode::regex_compile_failed,
                      error ? error->message : std::string{},
                      error ? error->offset : 0}
        );
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
          std::ifstream stream(entry.absolute_path, std::ios::binary);
          if (!stream)
            return true;

          std::string line;
          std::size_t line_number = 0;
          while (std::getline(stream, line)) {
            ++line_number;
            if (!query.options.include_binary && text::looks_binary(line))
              return true;
            const auto matches =
                regex_matcher ? regex_matcher->find_all(line)
                              : text::find_all_literals(line, query.expression, query.options);
            if (matches.empty())
              continue;
            for (const auto& match : matches) {
              if (summary.matches >= query.options.result_limit) {
                summary.limit_reached = true;
                return false;
              }
              ++summary.matches;
              if (!sink(SearchResult{entry.relative_path, line_number, match.offset + 1,
                                     match.length, line}))
                return false;
            }
          }
          return true;
        },
        stop_token);

    summary.cancelled = stop_token.stop_requested();
    return summary;
  }

} // namespace uburu::search
