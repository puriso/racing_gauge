# SDLシミュレーターと等速での動作確認

[確認ページをブラウザーで開く](https://puriso.github.io/racing_gauge/)

本番の描画・フォント・平滑化・Rモード・LOW警告をSDLで実行し、**等速のMP4とindex.html**にまとめます。
PRのコメントから、ZIPを展開せずGitHub Pages上の確認ページを直接開けます。
すべて1.0倍速です。Rモードの3分保持も実時間どおりに保存し、確認時刻ボタンから場面へ移動できます。

動画・GIFには、320×240の製品画面の**外側**に入力と期待値を表示します。MP4を直接開いても数値を追えます。

| 外側の表示 | 意味 |
| --- | --- |
| RAW INPUT | 平滑化前の模擬入力。油温・水温・油圧・Gと方向・受光照度。電圧やADCの生カウントではありません |
| EXPECTED FILTER / TARGET | 60Hzの全入力から独立計算した平滑化値、R判定条件、想定する輝度設定 |
| EXPECT / ACTUAL | R・LOW・昼/夕方/夜・輝度0〜255・最後に取得した照度・6点中央値の期待値と実際の状態 |

期待する平滑化値は小数2桁で表示します。実画面には整数化・小数1桁化に加え、
油温0.1℃／水温0.05℃／油圧0.05bar未満の変化を描き直さない処理があるため、数字が完全一致しない場面があります。
輝度は実際の設定値を比較します。SDLの画素を暗くする加工は行いません。

## シナリオ

通常走行の油圧は、4秒周期で**3.0〜6.0barを連続往復**します。
始動直後、意図的な急落、専用の全レンジ試験を除き、5.0barで固定する場面はありません。

| シナリオ | 長さ | 確認する内容 |
| --- | --- | --- |
| `circuit` | 36秒 | 最大2.0Gの瞬間入力と油圧0.8barへの急落。短時間の非点灯、継続時のR・LOW、回復・保持・消灯 |
| `racing` | 196秒 | 50msの2.0GではRに入らず、1.0G継続で進入。G解除後も3分保持し、時限解除・再進入・詳細画面での解除を確認 |
| `ambient` | 136秒 | 100→12→2→100lux、昼→夕方→夜→昼。8秒ごとの測定・6点の上側中央値・夜間の詳細画面での最大輝度と復帰 |
| `ramp` | 200秒 | 油温-10→170→-10℃、水温-10→130→-10℃。上昇後、同じ動画内で保持から下降へ進む |
| `cooldown` | 100秒 | 高温から低温へ下降。バーの消去、赤表示の解除、低温端を確認 |
| `pressure-sweep` | 20秒 | 油圧0→10→0barの専用全レンジ試験。上昇・下降、8bar以上の赤表示を確認 |
| `warnings` | 40秒 | 通常変動中に3.1/3.0/2.9barへ250ms低下。2.9barの2秒保持でLOW点灯・保持・解除 |
| `startup` | 30秒 | 最初の1秒は70℃、その後-10℃へ戻って暖機。始動時の読み値の推移を確認 |
| `warm-start` | 30秒 | 70℃から暖機。始動後は油圧3〜6barで連続変動 |
| `menu` | 20秒 | 8〜13秒は詳細画面。油圧の変動、通常表示への復帰や残像を確認 |

`circuit`の主な時刻:

| 時刻 | 入力・確認する動作 |
| --- | --- |
| 4.00〜4.05秒 | 2.0Gと0.8barへ同時に急変。50msではR・LOWとも非点灯 |
| 8.00〜10.50秒 | 2.0Gと0.8barを継続。R進入、LOW点灯、油圧回復後の表示保持 |
| 18.00〜18.20秒 | 2.0Gと0.8barへ200msだけ低下。LOWは点灯しない |
| 22.00〜24.00秒 | 再度継続して急落。LOWの再点灯と回復を確認 |

`racing`の確認時刻は2.1秒（短いGで非進入）、4.2秒（進入）、184秒（保持中）、
184.3秒（3分後の解除）、187.3秒（再進入）、190秒（詳細画面での解除）です。
入力とゲージ表示には本番の平滑化による遅れがあります。
Rは1.0G以上が100ms継続、LOWは1.0G超かつ表示用油圧3bar以下が500ms継続すると点灯します。
`racing`は2luxの夜間入力です。R中の最大輝度と3分後の夜間輝度への復帰も確認します。

照度の判定は中央値15lux以上が昼（255）、10lux以上が夕方（200）、未満が夜（60）です。
現在の製品コードでは中央値バッファの未取得分を0として扱うため、100luxでも起動後16秒までは夜になります。
`ambient`では16秒＝昼、48秒＝夕方、88秒＝夜、96秒＝詳細画面で最大輝度、98秒＝夜へ復帰、120秒＝昼を確認できます。
自動測定はR・詳細画面中に停止し、入力照度と最後に取得した値を区別して表示します。

## ローカルで起動する

Python 3.11以降、C++17コンパイラー、SDL2とpkg-configが必要です。
macOSではXcode Command Line Toolsも必要です。

```sh
brew install sdl2 pkg-config
```

Ubuntu:

```sh
sudo apt-get update
sudo apt-get install -y build-essential libsdl2-dev pkg-config
```

リポジトリのルートで実行します。

```sh
python3 -m venv .venv
source .venv/bin/activate
python -m pip install -r simulator/requirements.txt
pio run -d simulator -e desktop
simulator/.pio/build/desktop/program --scenario circuit
```

320×240の本番描画を2倍で表示します。
Spaceで一時停止、Homeで再生を戻し、Mまたはクリックで詳細画面を切り替えます。
再生を戻しても仮想時計・ピーク値・平滑化履歴は保持します。初期状態で比較する場合は再起動してください。
`--seconds`では5〜240秒の再生時間を指定できます。Rの保持時間自体は本番設定のままです。

## 録画と確認ページを生成する

```sh
python simulator/record.py --output simulator/out/my-review
```

10シナリオを実行し、`simulator/out/my-review/index.html`を作ります。
ファイルをブラウザーで直接開けます。動画を参照するため、フォルダー全体を保持してください。
`--scenario circuit`などで個別に実行でき、ビルド済みなら`--skip-build`を付けられます。
既存の出力先は上書きしません。FFmpegはPythonパッケージに同梱されています。

- `index.html`: シナリオ一覧、確認時刻への移動、等速での連続再生、生の入力・期待値・R・LOW・照度・輝度
- `<scenario>/recording.mp4`: H.264、640×540、10fps、等速。終点の1フレーム（0.1秒）も含む。画面外に数値を合成
- `<scenario>/poster.png`: 数値付きの動画ポスター
- `<scenario>/frame-*.png`: 100msごとの全転送先フレーム。圧縮にじみのない画素確認用
- `<scenario>/contact-sheet.png`: 代表フレームの一覧
- `<scenario>/inputs.json`: 模擬入力、時刻、独立計算した期待値、実際の描画キャッシュ・調光設定、画素から確認したR・LOW
- `<scenario>/trace.csv`: 期待値計算に使用した60Hzの全入力。公開用には含めず、Actions成果物に保存
- `public/`: 公開用のindex.html・MP4・代表画像・JSON。全PNGは含めず、公開データ量を抑える

入力範囲、連続した油圧変動、温度の上昇・下降、Rの非進入・進入・保持・解除を自動検証します。
さらに実際の転送先PNGでR表示との一致、LOWの点灯・保持・消灯、油温バーの枠内描画と赤色の解除を確認します。
全8,090フレームで、独立計算した期待値と実際のR・LOW・調光状態を照合し、平滑化と描画値の差が更新幅以内か確認します。
MP4も再デコードして破損を検出します。それ以外の外観は確認ページで目視してください。

## 実機の平均FPS

**CoreS3実機の平均FPSは未計測です。** PCで得た約2,200frames/sという処理量から、
実機のFPSや改善率を求めることはできません。実機には約60Hzの上限制御があり、
SPI/DMA転送・ADC・I²C・IMUなど、PC側では再現しない処理時間があります。

実機と計測ログが用意できたら、次の条件で前後を比較します。

1. 同じCoreS3、センサー構成、ビルド設定、入力条件で測る。
2. 起動・初期化中を除外し、同じ長さの区間で、処理を完了したループ数と本体時計の経過時間を記録する。
3. FPS上限の待機・センサー取得・描画・転送を含む区間から、`平均ループFPS = 完了ループ数 × 1000 ÷ 経過時間ms`で算出する。

既存の実機FPS表示・デバッグログはループ回数の目安です。液晶転送は値が変化したときだけ行うため、
ループFPSと液晶への転送回数は別です。正確な区間平均には、実機上の経過時間も必要です。
現在は実機とログがないため、確認ページとPRコメントは「未計測」と表示します。

## 任意：PCシミュレーターの処理量を参考計測する

```sh
python simulator/benchmark.py --baseline-ref origin/main --output simulator/out/pc-benchmark.json
python simulator/record.py --benchmark-json simulator/out/pc-benchmark.json --output simulator/out/pc-review
```

PC処理量の自動計測はCIから外しています。必要な場合だけ上記コマンドを使い、
ページ内の「参考：PCシミュレーターの処理量」を開いて確認します。単位はframes/sです。
実機の平均FPS欄は引き続き「未計測」で、PC計測値では埋めません。
生の計測値・条件は`benchmark.json`から取得でき、コミット・CPU・コンパイラー・SDLも記録します。

- 同じ60Hz入力を変更前後で各5回再生し、実行順を交互にします。`--repetitions`で回数を変更できます。
- 先頭120フレームをウォームアップとして除外し、単調増加する実時計で`renderFrame`の経過時間を計測します。
- センサー値の注入・判定・描画・フレームバッファ転送を含み、入力の指紋記録・FPS上限待機・画像保存・JSON出力・動画変換を除外します。
- SDLドライバー内部の待機やOSの割り込みは処理の経過時間に含まれます。画面提示は別スレッドなので、この処理FPSはディスプレイへの表示FPSを示しません。CoreS3のSPI・DMA・ADCも対象外です。
- 平均は全計測フレーム数÷全処理時間です。シナリオ別FPSの単純平均にはしません。各実行の最小・最大値も保存します。
- 比較元の製品ソースと`renderFrame`を使い、入力・計測ドライバー・ビルド設定・ライブラリは現在版と共通化します。
- 60Hz入力の時刻・温度・油圧・G・メニュー状態・照度を指紋で照合し、不一致なら比較を中止します。
- 入力・照度注入・観測アダプターは共通化し、比較元の製品モジュールと`renderFrame`を使用します。動画・GIFの比較欄の合成時間は計測に含めません。

計測差にはPCの負荷やばらつき、シミュレーターの遷移処理が含まれます。
同じPC上での処理量を比較するための参考値です。実機性能の評価には使用しません。

## GIFが必要な場合

CIはGIFを自動生成せず、等速動画の確認ページを公開します。
個別にGIFを作る場合も、全区間を10fps・1.0倍速で保存します。倍速化や途中区間の省略はしません。
動画と同じ比較欄を付けます。GIFを作る前に、新しい録画処理で`inputs.json`と原画像を生成してください。

```sh
python simulator/make_demo.py --recordings simulator/out/my-review --scenario circuit --output simulator/out/circuit.gif
```

## GitHub PagesとCI

[SDL Visual Recording](../.github/workflows/visual-simulator.yml)は、描画・設定・シミュレーターに関係するPRとmainへのpushで実行します。
手動実行ではシナリオを選べます。

同じリポジトリ内のPRでは、録画・検証が成功するとGitHub Pagesへ公開し、既存のPRコメントを更新します。
確認URLは`previews/pr-<番号>/<コミットSHA>/`です。コミットごとの録画を保持し、PRごとの入口からは最新の成功した録画を開けます。
公開用ファイルは`visual-previews`ブランチの`docs/`へ保存し、GitHub Pages用の成果物として明示的にデプロイします。
main・PRのコードは公開処理で変更しません。

PagesはGitHub Actions方式で設定し、`github-pages`環境から信頼する同一リポジトリ内PRの公開を許可します。
fork・Dependabotでは公開とコメントを省略します。録画ジョブは読み取り専用です。
公開ジョブは検証済みの成果物だけを使い、許可したパス・通常ファイル・サイズ上限を確認します。
公開ブランチのコードや成果物内のスクリプトは実行しません。公開を直列化し、別PRの録画も保持します。

全PNGを含む`visual-simulator`成果物はActionsに14日間保存します。
公開用の動画とHTMLはGit履歴として残ります。mainへのpushと手動実行では成果物の保存までを行います。

## 実装範囲

[M5GFX SDL](https://github.com/m5stack/M5GFX/blob/master/examples/PlatformIO_SDL/README.md)の本番レンダラーを使用します。
`rendering.cpp`が`src/modules/`を直接リンクし、センサー配列と時刻を仮想化します。
毎秒60回の本番処理を実行し、6回に1回、転送先フレームバッファを保存します。
仮想実行自体は実時間より速く完了しますが、生成した動画の再生速度は等速です。
録画の10fpsは実機FPSの測定値ではありません。ADC・I²C・IMU・電源・タッチの実機ドライバーは検証しません。
ファームウェア側のビルド・書き込み設定は変更しません。

## English

The simulator runs production rendering, smoothing, LOW warnings and racing-mode logic through M5GFX SDL.
Ten real-time scenarios cover continuous 3–6bar pressure, sudden 2.0G/pressure drops, R activation/three-minute hold/expiry,
heating followed by cooling, pressure range sweeps, startup, menu transitions and ambient-light changes.
Both videos and optional GIFs carry raw physical inputs, independently calculated filtered expectations,
G, ambient light and expected/actual brightness outside the unmodified 320×240 production screen.
The HTML review page offers normal-speed video, timestamp links and input/state readouts.
Trusted same-repository PRs publish a commit-specific GitHub Pages URL; full PNG artifacts remain in Actions for 14 days.
Optional GIF generation is also real-time. Hardware accuracy, vehicle behavior and hardware FPS are outside this simulator's scope.
Hardware average FPS remains unmeasured. Host throughput is optional and is never converted into hardware FPS.
