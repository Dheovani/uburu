#include "core/search/direct-search-engine.hpp"

#include "core/text/text-matcher.hpp"

#include <fstream>
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
    if (query.expression.empty() || query.root.empty())
      return summary;

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
            const auto match = text::find_literal(line, query.expression, query.options);
            if (!match)
              continue;
            ++summary.matches;
            if (!sink(SearchResult{entry.relative_path, line_number, match->offset + 1,
                                   match->length, line}))
              return false;
            if (summary.matches >= query.options.result_limit) {
              summary.limit_reached = true;
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
