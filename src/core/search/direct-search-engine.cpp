#include "core/search/direct-search-engine.hpp"

#include "core/search/search-errors.hpp"
#include "core/search/search-query-validation.hpp"
#include "core/text/regex-matcher.hpp"
#include "core/text/text-file-reader.hpp"
#include "core/text/text-matcher.hpp"

#include <deque>
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

    struct PendingResult
    {
      SearchResult result;
      std::size_t remaining_context_lines{0};
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

    void report_partial_failure(SearchSummary& summary, SearchErrorCode code, const FileEntry& entry)
    {
      ++summary.files_with_read_errors;
      summary.partial_failure = true;
      summary.errors.push_back(make_search_error(code, entry.relative_path.generic_string()));
    }

    bool report_text_read_summary(SearchSummary& summary, const FileEntry& entry, text::TextReadSummary read_summary)
    {
      if (read_summary.status == text::TextReadStatus::completed)
        return true;

      if (read_summary.status == text::TextReadStatus::binary_skipped) {
        ++summary.metrics.binary_files;
        ++summary.metrics.binary_files_skipped;

        return true;
      }

      if (read_summary.status == text::TextReadStatus::cancelled)
        return false;

      const auto code = read_summary.status == text::TextReadStatus::open_failed
        ? SearchErrorCode::file_open_failed
        : SearchErrorCode::file_read_failed;
      report_partial_failure(summary, code, entry);

      return true;
    }

    std::vector<MatchSpan> make_highlights(std::string_view line_text, const std::vector<text::MatchPosition>& matches)
    {
      std::vector<MatchSpan> highlights;
      highlights.reserve(matches.size());
      for (const auto& highlight_match : matches) {
        highlights.push_back(MatchSpan{
            .column = text::visual_column_for_byte_offset(line_text, highlight_match.offset),
            .byte_offset = highlight_match.offset,
            .byte_length = highlight_match.length});
      }

      return highlights;
    }

    std::vector<std::string> copy_context(const std::deque<std::string>& context)
    {
      return {context.begin(), context.end()};
    }

    bool publish_ready_pending(std::vector<PendingResult>& pending, const ResultSink& sink)
    {
      for (auto iterator = pending.begin(); iterator != pending.end();) {
        if (iterator->remaining_context_lines > 0) {
          ++iterator;

          continue;
        }

        if (!sink(std::move(iterator->result)))
          return false;

        iterator = pending.erase(iterator);
      }

      return true;
    }

    bool add_context_after(std::vector<PendingResult>& pending, std::string_view line_text,
                           const ResultSink& sink)
    {
      for (auto& pending_result : pending) {
        if (pending_result.remaining_context_lines == 0)
          continue;

        pending_result.result.context_after.push_back(std::string{line_text});
        --pending_result.remaining_context_lines;
      }

      return publish_ready_pending(pending, sink);
    }

    bool flush_pending(std::vector<PendingResult>& pending, const ResultSink& sink)
    {
      for (auto& pending_result : pending)
        pending_result.remaining_context_lines = 0;

      return publish_ready_pending(pending, sink);
    }

    PublishDecision publish_matches(const FileEntry& entry, SearchResultKind kind,
                                    std::size_t line_number, std::string_view line_text,
                                    const std::vector<text::MatchPosition>& matches,
                                    const SearchQuery& query, SearchSummary& summary,
                                    std::size_t& file_matches,
                                    const std::deque<std::string>& context_before,
                                    std::vector<PendingResult>& pending, const ResultSink& sink)
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

        SearchResult result{.kind = kind,
                            .path = entry.relative_path,
                            .line = line_number,
                            .column = text::visual_column_for_byte_offset(line_text, match.offset),
                            .match_length = match.length,
                            .line_text = std::string{line_text},
                            .highlights = make_highlights(line_text, matches),
                            .context_before = copy_context(context_before),
                            .context_after = {}};

        if (query.options.context_after_lines == 0) {
          if (!sink(std::move(result)))
            return PublishDecision::stop_search;

          continue;
        }

        pending.push_back(
            PendingResult{.result = std::move(result),
                          .remaining_context_lines = query.options.context_after_lines});
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

  SearchSummary DirectSearchEngine::search(const SearchQuery& query, ResultSink sink, std::stop_token stop_token) const
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
          ++summary.metrics.files_processed;
          summary.metrics.bytes_processed += entry.size;

          std::size_t file_matches = 0;
          std::deque<std::string> previous_context;
          std::vector<PendingResult> pending_results;

          if (searches_file_name(query.options.target)) {
            const auto path_text = entry.relative_path.generic_string();
            const auto path_matches = find_matches(path_text, query, regex_matcher, summary);

            if (!path_matches)
              return false;

            const auto decision = publish_matches(entry, SearchResultKind::file_name, 0, path_text,
                                                  *path_matches, query, summary, file_matches,
                                                  previous_context, pending_results, sink);

            if (!flush_pending(pending_results, sink))
              return false;

            if (decision == PublishDecision::stop_search)
              return false;

            if (decision == PublishDecision::stop_current_file)
              return true;
          }

          if (!searches_content(query.options.target))
            return true;

          bool stop_current_file = false;
          bool stop_search = false;
          const auto read_summary = text::read_text_file_lines(
              entry.absolute_path, query.options,
              [&](const text::TextLine& line) {
                if (!add_context_after(pending_results, line.text, sink)) {
                  stop_search = true;

                  return false;
                }

                const auto matches = find_matches(line.text, query, regex_matcher, summary);

                if (!matches)
                  return false;

                if (matches->empty()) {
                  previous_context.push_back(line.text);
                  while (previous_context.size() > query.options.context_before_lines)
                    previous_context.pop_front();

                  return true;
                }

                const auto decision = publish_matches(
                    entry, SearchResultKind::content, line.line_number, line.text, *matches, query,
                    summary, file_matches, previous_context, pending_results, sink);

                previous_context.push_back(line.text);
                while (previous_context.size() > query.options.context_before_lines)
                  previous_context.pop_front();

                if (decision == PublishDecision::stop_search) {
                  stop_search = true;

                  return false;
                }

                if (decision == PublishDecision::stop_current_file) {
                  stop_current_file = true;

                  return false;
                }

                return true;
              },
              stop_token);

          if (!flush_pending(pending_results, sink))
            return false;

          if (stop_search)
            return false;

          if (stop_current_file)
            return true;

          return report_text_read_summary(summary, entry, read_summary);
        },
        stop_token, &summary.metrics);

    summary.cancelled = stop_token.stop_requested();
    summary.metrics.results_emitted = summary.matches;

    return summary;
  }

} // namespace uburu::search
