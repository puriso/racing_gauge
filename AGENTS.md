# Coding Guidelines

- ブランチ名は英語のみを使用してください。/ Use English only for branch names.
  - 不具合修正: `fix/` プレフィックスを使用 / For bug fixes, use the `fix/` prefix.
  - リファクタ: `react/` プレフィックスを使用 / For refactoring, use the `react/` prefix.
- プルリクエストのタイトルと概要は、日本語と英語の二言語で記述してください。/ Provide PR titles and descriptions in both Japanese and English.
- コード中のコメントは日本語で記述してください。/ Write code comments in Japanese.
- コミットする前に `.clang-format` と `.clang-tidy` を実行してください。/ Run `.clang-format` and `.clang-tidy` before committing.
- 二回目以降のコード作成を行う場合、`git pull && git merge main` を実行して競合を解決してください。/ When working on code again, run `git pull && git merge main` to resolve conflicts.
- コードを変更したら `act -j build` でCIを実行し、失敗した場合は修正して再度実行してください。プッシュした後は GitHub Actions が完了するまで待ち、失敗したら修正して再度プッシュしてください。/ After modifying code, run `act -j build` to execute CI locally. If it fails, fix the issues and rerun. After pushing, wait for the GitHub Actions run to finish. If it fails, fix the issues and push again.
