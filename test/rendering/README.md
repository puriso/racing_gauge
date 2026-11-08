# 描画転送の回帰テスト / Display transfer regression tests

GCC/G++ と Python 3 がある環境で、リポジトリのルートから実行します。

```sh
python test/rendering/run_tests.py
python test/rendering/run_tests.py --baseline-ref <comparison-ref>
```

`CXX` 環境変数でコンパイラのパスを指定できます。テスト用バイナリは一時ディレクトリで作成・削除します。

実際の `display.cpp`、低油圧警告、R表示、FPS表示を描画スタブに接続し、次をFPS表示の無効・有効それぞれで検証します。

- 油圧・水温・油温・最高油温だけを更新したときの転送範囲
- 複数箇所の更新を1回にまとめ、未変更時は転送しないこと
- 警告の開始・継続・解除、R表示、毎秒のFPS表示
- メニューと復帰時の全画面転送、呼び出し前のクリップ範囲の復元
- 転送先の画素バッファが、全画面転送した場合と一致すること

600フレームの比較では60FPSで10秒間を模擬し、油圧を毎フレーム、水温・油温を30フレームごとに変更します。転送画素数とRGB565のデータ量を出力します。

スタブはフォントや弧の見た目、SPI/DMAの所要時間を再現しません。転送量の削減率を実機FPSの向上率として扱わないでください。

Run the commands above with Python 3 and GCC/G++ available (or set `CXX`). The tests exercise the production rendering and overlay logic with an instrumented graphics stub, with the FPS overlay both disabled and enabled. They check transfer bounds, framebuffer coverage, overlay transitions, menu restoration, clipping, and a 600-frame transfer-volume comparison. They do not measure hardware FPS, SPI/DMA timing, or actual font/arc rendering.
