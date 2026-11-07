# SDL シミュレーターと CI 録画

実機なしで、racing_gauge の表示を動かして目視確認するための環境です。
[M5GFX の SDL シミュレーター](https://github.com/m5stack/M5GFX/blob/master/examples/PlatformIO_SDL/README.md)を使い、
**本番の描画・フォント・平滑化・ピーク記録・警告処理**をデスクトップ向けにビルドします。
別の PlatformIO プロジェクトなので、実機のビルド・書き込み設定には影響しません。

## ローカルで表示する

Python 3.11 以降と C++17 コンパイラーが必要です。macOS と Ubuntu の録画を検証対象とします。
Windows への SDL 導入は上記の公式手順を参照してください。このプロジェクトでの Windows 動作は未検証です。

macOS（Xcode Command Line Tools を導入済みの場合）:

```sh
brew install sdl2 pkg-config
```

Ubuntu:

```sh
sudo apt-get update
sudo apt-get install -y build-essential libsdl2-dev pkg-config
```

以下はリポジトリのルートで実行します。

```sh
python3 -m venv .venv
source .venv/bin/activate
python -m pip install -r simulator/requirements.txt
pio run -d simulator -e desktop
simulator/.pio/build/desktop/program --scenario ramp
```

320×240 の画面を 2 倍で表示します。`ramp` は100秒で全レンジを確認し、終点で止まります。
油温・水温は90秒かけて上昇し、最初の2秒と最後の8秒は端の値を保持します。
数値は本番と同じ平滑化を通るため、入力値に対して表示に遅れがあります。

| 操作 | 動作 |
| --- | --- |
| Space | 一時停止・再開 |
| Home | シナリオを最初から再生（ピーク値・平滑化履歴は保持） |
| M または画面クリック | 詳細画面を表示・解除 |
| ウィンドウを閉じる | 終了 |

`--scenario warnings`・`startup`・`warm-start`・`menu` でも起動できます。
Space で一時停止すると、入力・仮想時刻・平滑化・描画をまとめて停止します（M/クリックの反映は再開後）。
まっさらな状態で比較する場合は、プロセスを再起動してください。

## 録画する

```sh
python simulator/record.py
```

ウィンドウを開かずに 5 シナリオを実行し、`simulator/out/<日時>/index.html` にまとめます。
ブラウザーで HTML を開くか、各フォルダーの **recording.mp4** を再生してください。
HTML は動画を参照するため、出力フォルダー全体を保持してください。

```sh
python simulator/record.py --scenario ramp --output simulator/out/my-review
```

既にビルド済みなら `--skip-build` を付けられます。指定した出力先が存在する場合は停止し、上書きしません。
FFmpeg は Python パッケージに同梱されたバイナリーを使用するため、追加インストールは不要です。

| シナリオ | 長さ | 入力・確認箇所 |
| --- | --- | --- |
| `ramp` | 100秒 | 油温 **-10→170℃**・水温 **-10→130℃**を90秒で上昇。油圧 **0→10bar**。低温・高温の両端を保持 |
| `warnings` | 40秒 | 油温70→90℃・水温70→85℃をゆっくり上昇。油圧5barからLOW境界の直上・一致・直下へ短時間低下して回復 |
| `startup` | 30秒 | 油温・水温が始動直後の1秒だけ70℃。-10℃に戻り、油温50℃・水温40℃までゆっくり上昇。ピーク値の残り方も確認 |
| `warm-start` | 30秒 | 油温・水温が70℃から始まり、そのまま油温95℃・水温90℃まで暖機。始動1秒で油圧0→5bar |
| `menu` | 20秒 | 油温70→90℃・水温70→85℃、油圧5bar。8〜13秒は詳細画面。ピーク値と通常表示への復帰 |

`warnings` の油圧入力（各低下の後は5barへ回復）:

| 時刻 | 入力 | 確認する動作 |
| --- | --- | --- |
| 8.00〜8.25秒 | 3.1bar | LOW境界の直上へ250msだけ低下 |
| 12.00〜12.25秒 | 3.0bar | LOW境界に250msだけ一致 |
| 16.00〜16.25秒 | 2.9bar | 境界の直下へ250msだけ低下。LOWが点灯しないこと |
| 22.00〜24.00秒 | 2.9bar | 2秒保持してLOW点灯。その後5barへ回復し、表示保持を経て解除 |

現行のLOW条件は **1G超かつ平滑化後の油圧3bar以下が0.5秒継続**です。
この録画では6〜30秒に1.2Gを与えます。短時間の3.0bar入力は平滑化により表示側が3bar以下に達しない場合があり、
その挙動も本番コードのまま確認します。温度の上昇速度は最大2℃/秒です（始動時70℃から低温へ戻る1回の変化を除く）。
`startup` は70℃表示の原因や異常判定を仮定せず、読み値の推移だけを再現します。
CLIの `--seconds` ではシナリオ全体を伸縮できます。CI録画は上表の時間を固定して使用します。

出力内容:

- `recording.mp4`: 320×240、H.264、10fps。シナリオの長さ＋終点1フレーム（0.1秒）の動画
- `frame-*.png`: 100msごとの全フレーム。5シナリオ合計2,205枚。圧縮によるにじみのない静止画確認用
- `contact-sheet.png`: シナリオ別の代表フレーム。油圧の境界値・警告・回復や、始動時70℃の前後も抜粋
- `inputs.json`: 各フレームの模擬入力・時刻・センサーの有効設定。表示値ではなくセンサー境界へ渡す前の物理量

録画処理は、フレーム数・時刻・画面サイズ・空白・描画の変化・`ramp` 入力の単調増加を検査し、
温度の上昇速度・始動時70℃・油圧の境界値と回復も検査します。
既定の有効センサー構成では、LOW領域のPNGから短い低下での非点灯、継続低下での点灯・保持・回復後の消灯も検証します。
全レンジ録画では、130℃を超えた油温バーが数値領域へはみ出さないことも検証します。
MP4 が正常にデコードできることも確認します。それ以外の見た目の判定は人が行います。

## CI で確認する

[SDL Visual Recording](../.github/workflows/visual-simulator.yml) が、描画・設定・シミュレーターを変更した PR と
main への push で実行されます。手動実行ではシナリオを選べます（ワークフローが main に入った後）。

同じリポジトリ内の PR では、録画成功後に **録画レポートをダウンロード** というリンクを自動コメントします。
リンク先の ZIP を展開して `index.html` を開いてください。再実行時はコメントを増やさず、同じコメントを更新します。
HTML を公開ホスティングする機能はありません。

fork と Dependabot の PR はコメントを省略し、Checks から成果物を開く方法を使用します。
録画ジョブは読み取り権限で実行し、PR コメントの権限はコードを実行しない専用ジョブに限定しています。

1. PR の Checks、または GitHub の Actions から **SDL Visual Recording** の実行を開きます。
2. **Artifacts → visual-simulator** をダウンロード・展開します。
3. `index.html` または各 `recording.mp4` を開きます。成果物は 14 日間保存されます。

実行結果ごとの録画を比較すれば、バーの欠け、文字の重なり、警告の出入り、詳細画面から戻った際の残像を確認できます。
CI の実行画面から SDL を遠隔操作する機能はありません。操作したい場合はローカルのウィンドウを使用します。

## 実装範囲

`rendering.cpp` から `src/modules/` の本番コードを直接使用します。
センサーの取得境界だけを置き換え、サンプル配列へ物理量を渡します。
照度センサーは固定値です。ADC の電圧換算、I²C、IMU、電源回路、タッチの実機ドライバーは実行しません。

録画では仮想時刻で毎秒 60 回の描画処理を行い、6 回に 1 回、**転送先のフレームバッファ**を取得します。
実時間の経過を待たずに実行でき、PC の速さによって警告の保持時間や入力が変わりません。
出力動画の 10fps は録画の設定値であり、実機 FPS の測定結果ではありません。

M5GFX は `0.3.0` に固定しています。ファームウェア側の解決バージョンを更新した際は、
`simulator/platformio.ini` も確認してください。画面の回転は SDL の CoreS3 パネルに合わせて `0` にしています。
実機の初期化処理全体をエミュレートするものではありません。

## English

This separate PlatformIO native project runs the production gauge renderer on M5GFX SDL.
Use `pio run -d simulator -e desktop`, then launch `simulator/.pio/build/desktop/program`.
`python simulator/record.py` captures the full-range ramp, pressure-boundary dips, transient/warm startup and menu scenarios into MP4, PNG and an offline HTML report.
GitHub Actions publishes these files as the `visual-simulator` artifact.
Sensor inputs and application time are simulated; hardware FPS, ADC, I²C and power behavior are outside its scope.
