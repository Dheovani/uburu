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

    constexpr std::uint64_t fnvOffsetBasis = 14695981039346656037ULL;
    constexpr std::uint64_t fnvPrime = 1099511628211ULL;
    constexpr std::size_t bytesPerKibibyte = 1024U;
    constexpr std::size_t signatureBufferKibibytes = 8U;
    constexpr std::size_t signatureBufferSize = signatureBufferKibibytes * bytesPerKibibyte;

    [[nodiscard]] std::string hexHash(std::uint64_t hash)
    {
      std::ostringstream output;
      output << std::hex << std::setw(16) << std::setfill('0') << hash;

      return output.str();
    }

    [[nodiscard]] std::string stableId(std::string_view prefix, const std::filesystem::path& path)
    {
      auto hash = fnvOffsetBasis;
      const auto key = path.lexically_normal().generic_string();

      for (const auto character : key) {
        hash ^= static_cast<unsigned char>(character);
        hash *= fnvPrime;
      }

      std::ostringstream output;
      output << prefix << '-' << hexHash(hash);

      return output.str();
    }

    [[nodiscard]] std::uint64_t updateHash(std::uint64_t hash, std::string_view bytes)
    {
      for (const auto character : bytes) {
        hash ^= static_cast<unsigned char>(character);
        hash *= fnvPrime;
      }

      return hash;
    }

    [[nodiscard]] std::string fileSignature(const std::filesystem::path& path)
    {
      if (!std::filesystem::exists(path))
        return {};

      std::ifstream file(path, std::ios::binary);

      if (!file)
        return {};

      auto hash = updateHash(fnvOffsetBasis, path.lexically_normal().generic_string());
      std::array<char, signatureBufferSize> buffer{};

      while (file) {
        file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto bytesRead = file.gcount();

        if (bytesRead > 0) {
          hash = updateHash(hash, std::string_view(buffer.data(), static_cast<std::size_t>(bytesRead)));
        }
      }

      return hexHash(hash);
    }

    [[nodiscard]] std::string combinedSignature(const std::vector<std::filesystem::path>& paths)
    {
      auto hash = fnvOffsetBasis;

      for (const auto& path : paths) {
        hash = updateHash(hash, path.lexically_normal().generic_string());
        hash = updateHash(hash, fileSignature(path));
      }

      return hexHash(hash);
    }

    [[nodiscard]] [[maybe_unused]] GitError unavailableError()
    {
      return GitError{
        .code = GitErrorCode::backendUnavailable,
        .message = "libgit2 backend is not available in this build"};
    }

#if defined(UBURU_HAS_LIBGIT2)

    template <typename T, void (*FreeFunction)(T*)>
    using GitPointer = std::unique_ptr<T, decltype(FreeFunction)>;

    [[nodiscard]] GitError git_error(GitErrorCode code, std::string_view fallbackMessage)
    {
      const auto* lastError = git_error_last();

      if (lastError != nullptr && lastError->message != nullptr) {
        return GitError{.code = code, .message = lastError->message};
      }

      return GitError{.code = code, .message = std::string(fallbackMessage)};
    }

    [[nodiscard]] std::string oidToString(const git_oid* oid)
    {
      std::array<char, GIT_OID_SHA1_HEXSIZE + 1> buffer{};
      git_oid_tostr(buffer.data(), buffer.size(), oid);

      return buffer.data();
    }

    [[nodiscard]] std::optional<std::string> headOid(git_repository* repository)
    {
      git_oid oid{};

      if (git_reference_name_to_id(&oid, repository, "HEAD") != 0)
        return std::nullopt;

      return oidToString(&oid);
    }

    [[nodiscard]] std::optional<std::string> currentBranch(git_repository* repository)
    {
      git_reference* rawHead = nullptr;

      if (git_repository_head(&rawHead, repository) != 0)
        return std::nullopt;

      GitPointer<git_reference, git_reference_free> head(rawHead, git_reference_free);

      if (!git_reference_is_branch(head.get()))
        return std::nullopt;

      const char* branchName = nullptr;

      if (git_branch_name(&branchName, head.get()) != 0 || branchName == nullptr)
        return std::nullopt;

      return std::string(branchName);
    }

    [[nodiscard]] RepositoryInfo repositoryInfo(git_repository* repository)
    {
      const auto commonDirectory = std::filesystem::path(git_repository_commondir(repository));
      const auto* workdir = git_repository_workdir(repository);
      std::optional<std::filesystem::path> worktreeRoot;

      if (workdir != nullptr)
        worktreeRoot = std::filesystem::path(workdir);

      return RepositoryInfo{
        .id = stableId("repo", commonDirectory),
        .commonGitDirectory = commonDirectory,
        .worktreeRoot = worktreeRoot,
        .currentBranch = currentBranch(repository),
        .headOid = headOid(repository).value_or(std::string{}),
        .detachedHead = git_repository_head_detached(repository) == 1};
    }

    [[nodiscard]] WorktreeInfo worktreeInfo(git_repository* repository, const RepositoryInfo& info)
    {
      const auto* workdir = git_repository_workdir(repository);
      const auto root = workdir == nullptr ? std::filesystem::path{} : std::filesystem::path(workdir);

      return WorktreeInfo{
        .id = stableId("worktree", root),
        .repositoryId = info.id,
        .root = root,
        .gitDirectory = std::filesystem::path(git_repository_path(repository)),
        .branch = currentBranch(repository),
        .headOid = headOid(repository).value_or(std::string{})};
    }

    [[nodiscard]] GitFileStatus mapStatus(unsigned int status)
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

  GitResult<RepositoryInfo> Libgit2GitService::discoverRepository(const std::filesystem::path& path) const
  {
#if defined(UBURU_HAS_LIBGIT2)
    git_repository* rawRepository = nullptr;
    const auto result = git_repository_open_ext(&rawRepository, path.string().c_str(), 0, nullptr);

    if (result == GIT_ENOTFOUND)
      return GitError{.code = GitErrorCode::notRepository, .message = "path is not inside a Git repository"};

    if (result != 0)
      return git_error(GitErrorCode::repositoryOpenFailed, "failed to open Git repository");

    GitPointer<git_repository, git_repository_free> repository(rawRepository, git_repository_free);

    return repositoryInfo(repository.get());
#else
    static_cast<void>(path);

    return unavailableError();
#endif
  }

  GitResult<std::vector<WorktreeInfo>>
  Libgit2GitService::listWorktrees(const RepositoryInfo& repository) const
  {
#if defined(UBURU_HAS_LIBGIT2)
    std::vector<WorktreeInfo> worktrees;

    if (repository.worktreeRoot) {
      auto discovered = discoverRepository(*repository.worktreeRoot);

      if (auto* info = std::get_if<RepositoryInfo>(&discovered)) {
        git_repository* rawRepository = nullptr;

        if (git_repository_open_ext(&rawRepository, repository.worktreeRoot->string().c_str(), 0, nullptr) == 0) {
          GitPointer<git_repository, git_repository_free> opened(rawRepository, git_repository_free);
          worktrees.push_back(worktreeInfo(opened.get(), *info));
        }
      }
    }

    git_repository* rawRepository = nullptr;

    if (git_repository_open(&rawRepository, repository.commonGitDirectory.string().c_str()) != 0)
      return git_error(GitErrorCode::worktreeReadFailed, "failed to open repository for worktree listing");

    GitPointer<git_repository, git_repository_free> openedRepository(rawRepository, git_repository_free);
    git_strarray names{};

    if (git_worktree_list(&names, openedRepository.get()) != 0)
      return git_error(GitErrorCode::worktreeReadFailed, "failed to list Git worktrees");

    for (std::size_t index = 0; index < names.count; ++index) {
      git_worktree* rawWorktree = nullptr;

      if (git_worktree_lookup(&rawWorktree, openedRepository.get(), names.strings[index]) != 0)
        continue;

      GitPointer<git_worktree, git_worktree_free> worktree(rawWorktree, git_worktree_free);
      auto discovered = discoverRepository(std::filesystem::path(git_worktree_path(worktree.get())));

      if (auto* info = std::get_if<RepositoryInfo>(&discovered)) {
        if (!info->worktreeRoot)
          continue;

        git_repository* rawWorktreeRepository = nullptr;

        const auto openResult =
          git_repository_open_ext(&rawWorktreeRepository, info->worktreeRoot->string().c_str(), 0, nullptr);

        if (openResult == 0) {
          GitPointer<git_repository, git_repository_free> worktreeRepository(rawWorktreeRepository,
                                                                              git_repository_free);
          worktrees.push_back(worktreeInfo(worktreeRepository.get(), *info));
        }
      }
    }

    git_strarray_dispose(&names);

    return worktrees;
#else
    static_cast<void>(repository);

    return unavailableError();
#endif
  }

  GitResult<GitFileStatus> Libgit2GitService::fileStatus(const WorktreeInfo& worktree,
                                                          const std::filesystem::path& relativePath) const
  {
#if defined(UBURU_HAS_LIBGIT2)
    git_repository* rawRepository = nullptr;

    if (git_repository_open_ext(&rawRepository, worktree.root.string().c_str(), 0, nullptr) != 0)
      return git_error(GitErrorCode::repositoryOpenFailed, "failed to open Git repository");

    GitPointer<git_repository, git_repository_free> repository(rawRepository, git_repository_free);
    unsigned int status = GIT_STATUS_CURRENT;
    const auto path = relativePath.generic_string();

    if (git_status_file(&status, repository.get(), path.c_str()) != 0)
      return git_error(GitErrorCode::statusReadFailed, "failed to read file Git status");

    return mapStatus(status);
#else
    static_cast<void>(worktree);
    static_cast<void>(relativePath);

    return unavailableError();
#endif
  }

  GitResult<std::optional<std::string>>
  Libgit2GitService::blobHash(const WorktreeInfo& worktree, const std::filesystem::path& relativePath) const
  {
#if defined(UBURU_HAS_LIBGIT2)
    git_repository* rawRepository = nullptr;

    if (git_repository_open_ext(&rawRepository, worktree.root.string().c_str(), 0, nullptr) != 0)
      return git_error(GitErrorCode::repositoryOpenFailed, "failed to open Git repository");

    GitPointer<git_repository, git_repository_free> repository(rawRepository, git_repository_free);
    git_object* rawTree = nullptr;

    if (git_revparse_single(&rawTree, repository.get(), "HEAD^{tree}") != 0)
      return std::optional<std::string>{};

    GitPointer<git_object, git_object_free> treeObject(rawTree, git_object_free);
    const auto* tree = reinterpret_cast<const git_tree*>(treeObject.get());
    git_tree_entry* rawEntry = nullptr;
    const auto path = relativePath.generic_string();

    if (git_tree_entry_bypath(&rawEntry, tree, path.c_str()) != 0)
      return std::optional<std::string>{};

    GitPointer<git_tree_entry, git_tree_entry_free> entry(rawEntry, git_tree_entry_free);

    if (git_tree_entry_type(entry.get()) != GIT_OBJECT_BLOB)
      return std::optional<std::string>{};

    return std::optional<std::string>{oidToString(git_tree_entry_id(entry.get()))};
#else
    static_cast<void>(worktree);
    static_cast<void>(relativePath);

    return unavailableError();
#endif
  }

  GitResult<GitChangeState> Libgit2GitService::changeState(const WorktreeInfo& worktree) const
  {
#if defined(UBURU_HAS_LIBGIT2)
    git_repository* rawRepository = nullptr;

    if (git_repository_open_ext(&rawRepository, worktree.root.string().c_str(), 0, nullptr) != 0)
      return git_error(GitErrorCode::repositoryOpenFailed, "failed to open Git repository");

    GitPointer<git_repository, git_repository_free> repository(rawRepository, git_repository_free);
    const auto gitDirectory = std::filesystem::path(git_repository_path(repository.get()));
    const auto commonDirectory = std::filesystem::path(git_repository_commondir(repository.get()));
    std::vector<std::filesystem::path> relevantRefPaths;

    if (auto branch = currentBranch(repository.get())) {
      relevantRefPaths.push_back(commonDirectory / "refs" / "heads" / *branch);
    }

    relevantRefPaths.push_back(commonDirectory / "packed-refs");

    return GitChangeState{
      .branch = currentBranch(repository.get()),
      .headOid = headOid(repository.get()).value_or(std::string{}),
      .detachedHead = git_repository_head_detached(repository.get()) == 1,
      .headSignature = fileSignature(gitDirectory / "HEAD"),
      .indexSignature = fileSignature(gitDirectory / "index"),
      .relevantRefsSignature = combinedSignature(relevantRefPaths)};
#else
    static_cast<void>(worktree);

    return unavailableError();
#endif
  }

} // namespace uburu::git
