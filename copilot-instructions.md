# Copilot Review Guidelines / Copilotレビューガイドライン

Copilotでレビューを行う際は、次の指針を必ず確認してください。

## ブランチとPRの基本 / Branch & PR Basics
- ブランチ名は英語のみを使用し、作業内容に応じて指定のプレフィックスを選んでください。/ Use only English branch names and apply the appropriate prefix.

| 作業種別 / Work Type | ブランチプレフィックス / Branch Prefix |
| --- | --- |
| 不具合修正 / Bug Fix | `fix/` |
| リファクタリング / Refactor | `react/` |

- プルリクエストのタイトルと概要は、日本語と英語の二言語で記載されているか確認してください。/ Ensure PR titles and summaries are provided in both Japanese and English.

## コメントスタイル / Comment Style
- レビューコメントはすべて日本語で記載してください。/ Write every review comment in Japanese.
- コードコメントも日本語で統一されているかを確認し、必要に応じて指摘してください。/ Check that code comments remain in Japanese and point out deviations.

## 重要度ラベル / Importance Labels
- 指摘には以下のラベルを使用し、必要に応じてスター数で重要度を示してください。/ Apply the following labels to feedback, using star ratings when appropriate.

| ラベル / Label | 説明 / Description |
| --- | --- |
| `[MUST]` | 不具合やメンテコストを意識すると修正が必須な項目。/ Mandatory fixes due to bugs or maintenance costs. |
| `[IMO]` + ★☆☆〜★★★ | 修正が望ましい内容を星の数で重要度表現。/ Preferred fixes, with stars indicating priority. |
| `[NITS]` | サジェストを承認すればすぐ修正できる細かな指摘。/ Minor suggestions that should be provided in an easily applicable format. |

## ツールとワークフローの確認 / Tooling & Workflow Checks
- コミット前に `.clang-format` と `.clang-tidy` が実行されているか確認してください。/ Confirm `.clang-format` and `.clang-tidy` have been run before committing.
- 追加作業時は `git pull && git merge main` による最新化が行われているか注目してください。/ Ensure contributors refresh their branches with `git pull && git merge main` when resuming work.
- コード変更後に `act -j build` を実行し、CIの結果に問題がないか確認するよう促してください。/ Encourage running `act -j build` after modifications to verify CI results.
- プッシュ後はGitHub Actionsの完了を待ち、エラー対応が行われているか確認してください。/ Verify that contributors wait for GitHub Actions to finish and address any errors after pushing.
