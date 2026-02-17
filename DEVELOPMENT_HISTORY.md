# 開発履歴メモ（ShogiAlgo）

## 目的

- WebとUSI両対応の将棋AIを作る
- 友人配布用の .exe も作る
- 反則手を出さないことを最優先で検証する

---

## 主な変更の流れ

### 1. 最小構成のエンジン

- `Negamax + Alpha-Beta` を導入
- 評価は駒割りのみ（高速化優先）
- ここで反則手が出ることがあった

### 2. 静止探索の導入

- `depth=0` の評価を capture-only で伸ばす
- 戦術は良くなるが、探索量が一気に増えやすい

### 3. Web UI / USI対応

- Web: [web/app.py](web/app.py) + [web/templates/index.html](web/templates/index.html)
- USI: [src/usi.py](src/usi.py)
- UIでの動作確認を優先

### 4. 反則手対応

- `board.is_legal()` で最終手チェック
- ダメなら最初の合法手にフォールバック
- ログ出力を強化（[shogialgo_debug.log](shogialgo_debug.log)）

### 5. 詰み探索（先読み）

- `ShogiEngine.mate_search_depth` を追加
- 先読み詰みが見つかれば即返す方式

---

## 失敗・注意点（再発防止）

### 1. `is_checkmate()` が存在しない問題

- cshogi環境によっては `board.is_checkmate()` が無い
- 代替は「王手中かつ合法手ゼロ」の判定

### 2. 二重フォルダ問題（SHOGIAI / shogiAI）

- 将棋所の実行パスが古いフォルダを指していた
- 変更が反映されず、古いバグが再発

### 3. 詰み探索が重い局面

- 終盤の分岐増加で時間が急増
- 先読み深さは小さく保つ（例: 1〜2）

---

## これからの方針

- 反則手ゼロを最優先
- 詰み探索は軽量な範囲でのみ実施
- Web/USIの動作パスを常に同じに保つ
