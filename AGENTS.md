#Coding Guidelines

- ブランチ名は英語のみを使用してください。/ Use English only for branch names.
  - 不具合修正: `fix/` プレフィックスを使用 / For bug fixes, use the `fix/` prefix.
  - リファクタ: `react/` プレフィックスを使用 / For refactoring, use the `react/` prefix.
- プルリクエストのタイトルと概要は、日本語と英語の二言語で記述してください。/ Provide PR titles and descriptions in both Japanese and English.
- コード中のコメントは日本語で記述してください。/ Write code comments in Japanese.
- PRレビューやレビューコメントは日本語で返してください。/ Provide PR reviews and review comments in Japanese.
- Copilot のレビューコメントは日本語で行い、以下の重要度区分を使用してください。/ Write Copilot review comments in Japanese and use the following importance levels.
  - [MUST]: 不具合やメンテコストを意識すると修正が必須 / Fix is mandatory when considering bugs or maintenance costs.
  - [IMO★3]: できれば修正したい内容 / Prefer to fix; mark with ★3.
  - [NITS]: 細かい指摘で、サジェストを承認すればすぐ修正できる形式にしてください。/ Minor suggestions that should be provided in an easily applicable suggestion format.
- コミットする前に `.clang-format` と `.clang-tidy` を実行してください。/ Run `.clang-format` and `.clang-tidy` before committing.
- 二回目以降のコード作成を行う場合、`git pull && git merge main` を実行して競合を解決してください。/ When working on code again, run `git pull && git merge main` to resolve conflicts.
- コードを変更したら `act -j build` を実行してCIを確認し、プッシュ後は GitHub Actions の終了を待ち、エラーがあれば修正して再度プッシュしてください。/ After modifying code, run `act -j build` to check CI locally. After pushing, wait for GitHub Actions to finish and push again if errors occur.
