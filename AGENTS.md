# Coding Guidelines

- ブランチ名は英語のみを使用してください。/ Use English only for branch names.
  - 不具合修正: `fix/` プレフィックスを使用 / For bug fixes, use the `fix/` prefix.
  - リファクタ: `react/` プレフィックスを使用 / For refactoring, use the `react/` prefix.
- プルリクエストのタイトルと概要は、日本語と英語の二言語で記述してください。/ Provide PR titles and descriptions in both Japanese and English.
- プルリクエストを作成する際はレビュワーに `copilot` を追加してください。/ When creating a PR, add `copilot` as a reviewer.
- コード中のコメントは日本語で記述してください。/ Write code comments in Japanese.
- コミットする前に `.clang-format` と `.clang-tidy` を実行してください。/ Run `.clang-format` and `.clang-tidy` before committing.
- 二回目以降のコード作成を行う場合、`git pull && git merge main` を実行して競合を解決してください。/ When working on code again, run `git pull && git merge main` to resolve conflicts.
