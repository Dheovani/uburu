#pragma once

#include "fixtures/test-fixtures.hpp"
#include "helpers/temporary-paths.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>

#if defined(UBURU_HAS_LIBGIT2)
#include <git2.h>
#endif

namespace uburu::tests
{

  /**
   * Creates repository fixtures inside the working tree to keep libgit2 worktree tests stable on Windows.
   */
  class Libgit2RepositoryDirectory
  {
  public:
    explicit Libgit2RepositoryDirectory(std::string name)
      : pathValue(std::filesystem::current_path() / uniqueTemporaryPath(std::move(name)).filename())
    {
      std::error_code error;

      std::filesystem::remove_all(pathValue, error);
      std::filesystem::create_directories(pathValue);
    }

    ~Libgit2RepositoryDirectory()
    {
      std::error_code error;

      std::filesystem::remove_all(pathValue, error);
    }

    Libgit2RepositoryDirectory(const Libgit2RepositoryDirectory&) = delete;
    Libgit2RepositoryDirectory& operator=(const Libgit2RepositoryDirectory&) = delete;
    Libgit2RepositoryDirectory(Libgit2RepositoryDirectory&&) noexcept = delete;
    Libgit2RepositoryDirectory& operator=(Libgit2RepositoryDirectory&&) noexcept = delete;

    [[nodiscard]]
    const std::filesystem::path& path() const
    {
      return pathValue;
    }

  private:
    std::filesystem::path pathValue;
  };

#if defined(UBURU_HAS_LIBGIT2)

  /**
   * Fails a test with libgit2 context attached to Catch2 output.
   */
  inline void requireGitSuccess(int status, std::string_view operation, const std::filesystem::path& path)
  {
    if (status != 0) {
      const auto* lastError = git_error_last();
      INFO("operation: " << operation);
      INFO("path: " << path.string());

      if (lastError != nullptr && lastError->message != nullptr)
        INFO("libgit2: " << lastError->message);
    }

    REQUIRE(status == 0);
  }

  /**
   * Creates a commit containing all current indexable files in the repository.
   */
  inline void createCommit(git_repository* repository, std::string_view message)
  {
    git_index* index = nullptr;
    REQUIRE(git_repository_index(&index, repository) == 0);
    REQUIRE(git_index_add_all(index, nullptr, GIT_INDEX_ADD_DEFAULT, nullptr, nullptr) == 0);
    REQUIRE(git_index_write(index) == 0);

    git_oid treeOid{};
    REQUIRE(git_index_write_tree(&treeOid, index) == 0);

    git_tree* tree = nullptr;
    REQUIRE(git_tree_lookup(&tree, repository, &treeOid) == 0);

    git_signature* signature = nullptr;
    REQUIRE(git_signature_now(&signature, "Uburu Tests", "tests@uburu.local") == 0);

    git_reference* head = nullptr;
    const auto headStatus = git_repository_head(&head, repository);

    git_oid commitOid{};

    if (headStatus == GIT_ENOTFOUND || headStatus == GIT_EUNBORNBRANCH) {
      REQUIRE(git_commit_create_v(&commitOid,
                                  repository,
                                  "refs/heads/main",
                                  signature,
                                  signature,
                                  nullptr,
                                  std::string(message).c_str(),
                                  tree,
                                  0) == 0);
      REQUIRE(git_repository_set_head(repository, "refs/heads/main") == 0);
    } else {
      REQUIRE(headStatus == 0);

      git_commit* parent = nullptr;
      REQUIRE(git_commit_lookup(&parent, repository, git_reference_target(head)) == 0);
      REQUIRE(git_commit_create_v(&commitOid,
                                  repository,
                                  git_reference_name(head),
                                  signature,
                                  signature,
                                  nullptr,
                                  std::string(message).c_str(),
                                  tree,
                                  1,
                                  parent) == 0);

      git_commit_free(parent);
    }

    if (head != nullptr)
      git_reference_free(head);

    git_signature_free(signature);
    git_tree_free(tree);
    git_index_free(index);
  }

  inline void createBasicRepository(const std::filesystem::path& repositoryRoot)
  {
    git_libgit2_init();

    git_repository* repository = nullptr;
    requireGitSuccess(
      git_repository_init(&repository, repositoryRoot.string().c_str(), false), "git_repository_init", repositoryRoot);

    fixtures::writeBasicGitWorkingTreeFixture(repositoryRoot);
    createCommit(repository, "initial commit");

    git_repository_free(repository);
    git_libgit2_shutdown();
  }

  inline void commitAll(const std::filesystem::path& repositoryRoot, std::string_view message)
  {
    git_libgit2_init();

    git_repository* repository = nullptr;
    REQUIRE(git_repository_open(&repository, (repositoryRoot / ".git").string().c_str()) == 0);
    createCommit(repository, message);

    git_repository_free(repository);
    git_libgit2_shutdown();
  }

  inline void createAndCheckoutBranch(const std::filesystem::path& repositoryRoot, std::string_view branchName)
  {
    git_libgit2_init();

    git_repository* repository = nullptr;
    REQUIRE(git_repository_open(&repository, (repositoryRoot / ".git").string().c_str()) == 0);

    git_object* headCommit = nullptr;
    REQUIRE(git_revparse_single(&headCommit, repository, "HEAD^{commit}") == 0);

    git_reference* branch = nullptr;
    REQUIRE(
      git_branch_create(
        &branch, repository, std::string(branchName).c_str(), reinterpret_cast<const git_commit*>(headCommit), false) ==
      0);

    const auto branchReferenceName = std::string("refs/heads/") + std::string(branchName);
    REQUIRE(git_repository_set_head(repository, branchReferenceName.c_str()) == 0);
    REQUIRE(git_checkout_head(repository, nullptr) == 0);

    git_reference_free(branch);
    git_object_free(headCommit);
    git_repository_free(repository);
    git_libgit2_shutdown();
  }

#endif

} // namespace uburu::tests
