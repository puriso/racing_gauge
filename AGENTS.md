# Coding Guidelines / コーディングガイドライン

このリポジトリで作業する際は、以下のルールに従ってください。

## ブランチ運用 / Branch Workflow
- ブランチ名は必ず英語で作成してください。/ Always name branches in English.
- 作業内容に応じて、次のプレフィックスを使用してください。/ Choose the branch prefix that matches your task.

| 作業種別 / Work Type | ブランチプレフィックス / Branch Prefix |
| --- | --- |
| 不具合修正 / Bug Fix | `fix/` |
| リファクタリング / Refactor | `react/` |

## プルリクエスト / Pull Requests
- タイトルと概要は日本語と英語の二言語で記載してください。/ Provide PR titles and summaries in both Japanese and English.

## コード記述 / Code Authoring
- コード中のコメントは日本語で記述してください。/ Write code comments in Japanese.

## レビュー / Reviews
- PRレビューとレビューコメントは日本語で返してください。/ Provide PR reviews and review comments in Japanese.
- Copilotレビューコメントは日本語で記載し、以下の重要度区分を使用してください。/ Write Copilot review comments in Japanese using the following severity levels.

| ラベル / Label | 説明 / Description |
| --- | --- |
| `[MUST]` | 不具合やメンテナンスコストを考慮して必ず修正すべき内容。/ Mandatory fixes considering bugs or maintenance costs. |
| `[IMO]` + ★☆☆〜★★★ | 修正が望ましい内容を★の数で重要度表現。/ Preferred fixes with star ratings to indicate importance. |
| `[NITS]` | サジェストで即適用できる細かな指摘。/ Minor suggestions that should be provided in an easily applicable format. |

## ツールとワークフロー / Tooling & Workflow
- コミット前に `.clang-format` と `.clang-tidy` を実行してください。/ Run `.clang-format` and `.clang-tidy` before committing.
- 2回目以降の作業では、`git pull && git merge main` を実行して競合を解消してください。/ When resuming work, run `git pull && git merge main` to resolve conflicts.
- コード変更後は `act -j build` を実行してローカルでCIを確認してください。/ After modifying code, run `act -j build` to check CI locally.
- プッシュ後はGitHub Actionsの完了を待ち、エラーがあれば修正して再度プッシュしてください。/ After pushing, wait for GitHub Actions to finish and push again if any errors occur.
