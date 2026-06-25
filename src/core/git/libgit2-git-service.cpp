#include "core/git/libgit2-git-service.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string_view>
#include <utility>

#if defined(UBURU_HAS_LIBGIT2)
#include <git2.h>
#endif

namespace uburu::git
{
  namespace
  {

    constexpr std::uint64_t fnv_offset_basis = 14695981039346656037ULL;
    constexpr std::uint64_t fnv_prime = 1099511628211ULL;
    constexpr std::size_t bytes_per_kibibyte = 1024U;
    constexpr std::size_t signature_buffer_kibibytes = 8U;
    constexpr std::size_t signature_buffer_size = signature_buffer_kibibytes * bytes_per_kibibyte;

    [[nodiscard]] std::string hex_hash(std::uint64_t hash)
    {
      std::ostringstream output;
      output << std::hex << std::setw(16) << std::setfill('0') << hash;

      return output.str();
    }

    [[nodiscard]] std::string stable_id(std::string_view prefix, const std::filesystem::path& path)
    {
      auto hash = fnv_offset_basis;
      const auto key = path.lexically_normal().generic_string();

      for (const auto character : key) {
        hash ^= static_cast<unsigned char>(character);
        hash *= fnv_prime;
      }

      std::ostringstream output;
      output << prefix << '-' << hex_hash(hash);

      return output.str();
    }

    [[nodiscard]] std::uint64_t update_hash(std::uint64_t hash, std::string_view bytes)
    {
      for (const auto character : bytes) {
        hash ^= static_cast<unsigned char>(character);
        hash *= fnv_prime;
      }

      return hash;
    }

    [[nodiscard]] std::string file_signature(const std::filesystem::path& path)
    {
      if (!std::filesystem::exists(path))
        return {};

      std::ifstream file(path, std::ios::binary);

      if (!file)
        return {};

      auto hash = update_hash(fnv_offset_basis, path.lexically_normal().generic_string());
      std::array<char, signature_buffer_size> buffer{};

      while (file) {
        file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto bytes_read = file.gcount();

        if (bytes_read > 0) {
          hash = update_hash(hash, std::string_view(buffer.data(), static_cast<std::size_t>(bytes_read)));
        }
      }

      return hex_hash(hash);
    }

    [[nodiscard]] std::string combined_signature(const std::vector<std::filesystem::path>& paths)
    {
      auto hash = fnv_offset_basis;

      for (const auto& path : paths) {
        hash = update_hash(hash, path.lexically_normal().generic_string());
        hash = update_hash(hash, file_signature(path));
      }

      return hex_hash(hash);
    }

    [[nodiscard]] [[maybe_unused]] GitError unavailable_error()
    {
      return GitError{
        .code = GitErrorCode::backend_unavailable,
        .message = "libgit2 backend is not available in this build"};
    }

#if defined(UBURU_HAS_LIBGIT2)

    template <typename T, void (*FreeFunction)(T*)>
    using GitPointer = std::unique_ptr<T, decltype(FreeFunction)>;

    [[nodiscard]] GitError git_error(GitErrorCode code, std::string_view fallback_message)
    {
      const auto* last_error = git_error_last();

      if (last_error != nullptr && last_error->message != nullptr) {
        return GitError{.code = code, .message = last_error->message};
      }

      return GitError{.code = code, .message = std::string(fallback_message)};
    }

    [[nodiscard]] std::string oid_to_string(const git_oid* oid)
    {
      std::array<char, GIT_OID_SHA1_HEXSIZE + 1> buffer{};
      git_oid_tostr(buffer.data(), buffer.size(), oid);

      return buffer.data();
    }

    [[nodiscard]] std::optional<std::string> head_oid(git_repository* repository)
    {
      git_oid oid{};

      if (git_reference_name_to_id(&oid, repository, "HEAD") != 0)
        return std::nullopt;

      return oid_to_string(&oid);
    }

    [[nodiscard]] std::optional<std::string> current_branch(git_repository* repository)
    {
      git_reference* raw_head = nullptr;

      if (git_repository_head(&raw_head, repository) != 0)
        return std::nullopt;

      GitPointer<git_reference, git_reference_free> head(raw_head, git_reference_free);

      if (!git_reference_is_branch(head.get()))
        return std::nullopt;

      const char* branch_name = nullptr;

      if (git_branch_name(&branch_name, head.get()) != 0 || branch_name == nullptr)
        return std::nullopt;

      return std::string(branch_name);
    }

    [[nodiscard]] RepositoryInfo repository_info(git_repository* repository)
    {
      const auto common_directory = std::filesystem::path(git_repository_commondir(repository));
      const auto* workdir = git_repository_workdir(repository);
      std::optional<std::filesystem::path> worktree_root;

      if (workdir != nullptr)
        worktree_root = std::filesystem::path(workdir);

      return RepositoryInfo{
        .id = stable_id("repo", common_directory),
        .common_git_directory = common_directory,
        .worktree_root = worktree_root,
        .current_branch = current_branch(repository),
        .head_oid = head_oid(repository).value_or(std::string{}),
        .detached_head = git_repository_head_detached(repository) == 1};
    }

    [[nodiscard]] WorktreeInfo worktree_info(git_repository* repository, const RepositoryInfo& info)
    {
      const auto* workdir = git_repository_workdir(repository);
      const auto root = workdir == nullptr ? std::filesystem::path{} : std::filesystem::path(workdir);

      return WorktreeInfo{
        .id = stable_id("worktree", root),
        .repository_id = info.id,
        .root = root,
        .git_directory = std::filesystem::path(git_repository_path(repository)),
        .branch = current_branch(repository),
        .head_oid = head_oid(repository).value_or(std::string{})};
    }

    [[nodiscard]] GitFileStatus map_status(unsigned int status)
    {
      if ((status & GIT_STATUS_IGNORED) != 0U)
        return GitFileStatus::ignored;

      if ((status & GIT_STATUS_CONFLICTED) != 0U)
        return GitFileStatus::conflicted;

      if ((status & GIT_STATUS_INDEX_NEW) != 0U)
        return GitFileStatus::added;

      if ((status & GIT_STATUS_WT_NEW) != 0U)
        return GitFileStatus::untracked;

      if ((status & (GIT_STATUS_WT_DELETED | GIT_STATUS_INDEX_DELETED)) != 0U)
        return GitFileStatus::deleted;

      if ((status & (GIT_STATUS_WT_MODIFIED | GIT_STATUS_INDEX_MODIFIED | GIT_STATUS_WT_RENAMED |
                     GIT_STATUS_INDEX_RENAMED | GIT_STATUS_WT_TYPECHANGE | GIT_STATUS_INDEX_TYPECHANGE)) != 0U)
        return GitFileStatus::modified;

      return GitFileStatus::clean;
    }

#endif

  } // namespace

  Libgit2GitService::Libgit2GitService()
  {
#if defined(UBURU_HAS_LIBGIT2)
    git_libgit2_init();
#endif
  }

  Libgit2GitService::~Libgit2GitService()
  {
#if defined(UBURU_HAS_LIBGIT2)
    git_libgit2_shutdown();
#endif
  }

  GitResult<RepositoryInfo> Libgit2GitService::discover_repository(const std::filesystem::path& path) const
  {
#if defined(UBURU_HAS_LIBGIT2)
    git_repository* raw_repository = nullptr;
    const auto result = git_repository_open_ext(&raw_repository, path.string().c_str(), 0, nullptr);

    if (result == GIT_ENOTFOUND)
      return GitError{.code = GitErrorCode::not_repository, .message = "path is not inside a Git repository"};

    if (result != 0)
      return git_error(GitErrorCode::repository_open_failed, "failed to open Git repository");

    GitPointer<git_repository, git_repository_free> repository(raw_repository, git_repository_free);

    return repository_info(repository.get());
#else
    static_cast<void>(path);

    return unavailable_error();
#endif
  }

  GitResult<std::vector<WorktreeInfo>>
  Libgit2GitService::list_worktrees(const RepositoryInfo& repository) const
  {
#if defined(UBURU_HAS_LIBGIT2)
    std::vector<WorktreeInfo> worktrees;

    if (repository.worktree_root) {
      auto discovered = discover_repository(*repository.worktree_root);

      if (auto* info = std::get_if<RepositoryInfo>(&discovered)) {
        git_repository* raw_repository = nullptr;

        if (git_repository_open_ext(&raw_repository, repository.worktree_root->string().c_str(), 0, nullptr) == 0) {
          GitPointer<git_repository, git_repository_free> opened(raw_repository, git_repository_free);
          worktrees.push_back(worktree_info(opened.get(), *info));
        }
      }
    }

    git_repository* raw_repository = nullptr;

    if (git_repository_open(&raw_repository, repository.common_git_directory.string().c_str()) != 0)
      return git_error(GitErrorCode::worktree_read_failed, "failed to open repository for worktree listing");

    GitPointer<git_repository, git_repository_free> opened_repository(raw_repository, git_repository_free);
    git_strarray names{};

    if (git_worktree_list(&names, opened_repository.get()) != 0)
      return git_error(GitErrorCode::worktree_read_failed, "failed to list Git worktrees");

    for (std::size_t index = 0; index < names.count; ++index) {
      git_worktree* raw_worktree = nullptr;

      if (git_worktree_lookup(&raw_worktree, opened_repository.get(), names.strings[index]) != 0)
        continue;

      GitPointer<git_worktree, git_worktree_free> worktree(raw_worktree, git_worktree_free);
      auto discovered = discover_repository(std::filesystem::path(git_worktree_path(worktree.get())));

      if (auto* info = std::get_if<RepositoryInfo>(&discovered)) {
        if (!info->worktree_root)
          continue;

        git_repository* raw_worktree_repository = nullptr;

        const auto open_result =
          git_repository_open_ext(&raw_worktree_repository, info->worktree_root->string().c_str(), 0, nullptr);

        if (open_result == 0) {
          GitPointer<git_repository, git_repository_free> worktree_repository(raw_worktree_repository,
                                                                              git_repository_free);
          worktrees.push_back(worktree_info(worktree_repository.get(), *info));
        }
      }
    }

    git_strarray_dispose(&names);

    return worktrees;
#else
    static_cast<void>(repository);

    return unavailable_error();
#endif
  }

  GitResult<GitFileStatus> Libgit2GitService::file_status(const WorktreeInfo& worktree,
                                                          const std::filesystem::path& relative_path) const
  {
#if defined(UBURU_HAS_LIBGIT2)
    git_repository* raw_repository = nullptr;

    if (git_repository_open_ext(&raw_repository, worktree.root.string().c_str(), 0, nullptr) != 0)
      return git_error(GitErrorCode::repository_open_failed, "failed to open Git repository");

    GitPointer<git_repository, git_repository_free> repository(raw_repository, git_repository_free);
    unsigned int status = GIT_STATUS_CURRENT;
    const auto path = relative_path.generic_string();

    if (git_status_file(&status, repository.get(), path.c_str()) != 0)
      return git_error(GitErrorCode::status_read_failed, "failed to read file Git status");

    return map_status(status);
#else
    static_cast<void>(worktree);
    static_cast<void>(relative_path);

    return unavailable_error();
#endif
  }

  GitResult<std::optional<std::string>>
  Libgit2GitService::blob_hash(const WorktreeInfo& worktree, const std::filesystem::path& relative_path) const
  {
#if defined(UBURU_HAS_LIBGIT2)
    git_repository* raw_repository = nullptr;

    if (git_repository_open_ext(&raw_repository, worktree.root.string().c_str(), 0, nullptr) != 0)
      return git_error(GitErrorCode::repository_open_failed, "failed to open Git repository");

    GitPointer<git_repository, git_repository_free> repository(raw_repository, git_repository_free);
    git_object* raw_tree = nullptr;

    if (git_revparse_single(&raw_tree, repository.get(), "HEAD^{tree}") != 0)
      return std::optional<std::string>{};

    GitPointer<git_object, git_object_free> tree_object(raw_tree, git_object_free);
    const auto* tree = reinterpret_cast<const git_tree*>(tree_object.get());
    git_tree_entry* raw_entry = nullptr;
    const auto path = relative_path.generic_string();

    if (git_tree_entry_bypath(&raw_entry, tree, path.c_str()) != 0)
      return std::optional<std::string>{};

    GitPointer<git_tree_entry, git_tree_entry_free> entry(raw_entry, git_tree_entry_free);

    if (git_tree_entry_type(entry.get()) != GIT_OBJECT_BLOB)
      return std::optional<std::string>{};

    return std::optional<std::string>{oid_to_string(git_tree_entry_id(entry.get()))};
#else
    static_cast<void>(worktree);
    static_cast<void>(relative_path);

    return unavailable_error();
#endif
  }

  GitResult<GitChangeState> Libgit2GitService::change_state(const WorktreeInfo& worktree) const
  {
#if defined(UBURU_HAS_LIBGIT2)
    git_repository* raw_repository = nullptr;

    if (git_repository_open_ext(&raw_repository, worktree.root.string().c_str(), 0, nullptr) != 0)
      return git_error(GitErrorCode::repository_open_failed, "failed to open Git repository");

    GitPointer<git_repository, git_repository_free> repository(raw_repository, git_repository_free);
    const auto git_directory = std::filesystem::path(git_repository_path(repository.get()));
    const auto common_directory = std::filesystem::path(git_repository_commondir(repository.get()));
    std::vector<std::filesystem::path> relevant_ref_paths;

    if (auto branch = current_branch(repository.get())) {
      relevant_ref_paths.push_back(common_directory / "refs" / "heads" / *branch);
    }

    relevant_ref_paths.push_back(common_directory / "packed-refs");

    return GitChangeState{
      .branch = current_branch(repository.get()),
      .head_oid = head_oid(repository.get()).value_or(std::string{}),
      .detached_head = git_repository_head_detached(repository.get()) == 1,
      .head_signature = file_signature(git_directory / "HEAD"),
      .index_signature = file_signature(git_directory / "index"),
      .relevant_refs_signature = combined_signature(relevant_ref_paths)};
#else
    static_cast<void>(worktree);

    return unavailable_error();
#endif
  }

} // namespace uburu::git
