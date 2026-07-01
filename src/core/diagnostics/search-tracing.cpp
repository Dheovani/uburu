#include "core/diagnostics/search-tracing.hpp"

#include <algorithm>
#include <utility>

namespace uburu::diagnostics
{
  namespace
  {

    constexpr std::string_view redactedTraceFieldValue = "<redacted>";

    void sanitizeTraceFields(std::vector<LogField>& fields, const SearchTracingOptions& options)
    {
      if (options.includeSensitiveFields)
        return;

      for (auto& field : fields) {
        if (field.sensitive)
          field.value = redactedTraceFieldValue;
      }
    }

  } // namespace

  SearchTraceRecorder::SearchTraceRecorder(SearchTracingOptions options) : options(std::move(options))
  {
    recordedEvents.reserve(std::min(this->options.maximumEvents, std::size_t{64}));
  }

  bool SearchTraceRecorder::enabled() const noexcept
  {
    return options.enabled;
  }

  void SearchTraceRecorder::record(std::string name,
                                   LogCategory category,
                                   std::chrono::nanoseconds elapsed,
                                   std::vector<LogField> fields)
  {
    if (!options.enabled)
      return;

    if (recordedEvents.size() >= options.maximumEvents)
      return;

    sanitizeTraceFields(fields, options);

    SearchTraceEvent event;

    event.name = std::move(name);
    event.category = category;
    event.startedAt = std::chrono::steady_clock::now() - elapsed;
    event.elapsed = elapsed;
    event.fields = std::move(fields);

    recordedEvents.push_back(std::move(event));
  }

  const std::vector<SearchTraceEvent>& SearchTraceRecorder::events() const noexcept
  {
    return recordedEvents;
  }

  void SearchTraceRecorder::clear()
  {
    recordedEvents.clear();
  }

  SearchTraceScope::SearchTraceScope(SearchTraceRecorder& recorder,
                                     std::string name,
                                     LogCategory category,
                                     std::vector<LogField> fields)
    : recorder(recorder.enabled() ? &recorder : nullptr),
      name(std::move(name)),
      category(category),
      fields(std::move(fields)),
      startedAt(std::chrono::steady_clock::now())
  {}

  SearchTraceScope::SearchTraceScope(SearchTraceScope&& other) noexcept
    : recorder(std::exchange(other.recorder, nullptr)),
      name(std::move(other.name)),
      category(other.category),
      fields(std::move(other.fields)),
      startedAt(other.startedAt)
  {}

  SearchTraceScope& SearchTraceScope::operator=(SearchTraceScope&& other) noexcept
  {
    if (this == &other)
      return *this;

    recorder = std::exchange(other.recorder, nullptr);
    name = std::move(other.name);
    category = other.category;
    fields = std::move(other.fields);
    startedAt = other.startedAt;

    return *this;
  }

  SearchTraceScope::~SearchTraceScope()
  {
    if (recorder == nullptr)
      return;

    recorder->record(std::move(name), category, std::chrono::steady_clock::now() - startedAt, std::move(fields));
  }

  SearchTraceScope
  traceSearchScope(SearchTraceRecorder& recorder, std::string name, LogCategory category, std::vector<LogField> fields)
  {
    return SearchTraceScope{recorder, std::move(name), category, std::move(fields)};
  }

} // namespace uburu::diagnostics
