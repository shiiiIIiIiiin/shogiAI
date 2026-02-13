# shogiAI

cshogi を使って「探索強化＋学習」の将棋AIを作る最小構成。

## 目的

- Python で探索エンジンを実装
- cshogi で局面表現と指し手生成を扱う
- 教師あり学習で「指し手確率」を学習し、探索の手順最適化に使う

## ディレクトリ

- src/
  - engine.py: 探索エンジン
  - usi.py: USIプロトコル対応エンジン
  - play.py: 簡易対局CLI
  - train_policy.py: 指し手分類の学習
  - dataset.py: 棋譜データの読み込み
- data/
  - ここに棋譜データを置く
- run_usi.bat: 将棋所用起動スクリプト

## セットアップ

### 1. 依存パッケージのインストール
```bash
python -m venv .venv
.venv\Scripts\activate  # Windows
pip install cshogi torch
```

### 2. 将棋所で対局（推奨）
1. 将棋所をダウンロード（http://shogidokoro.starfree.jp/）
2. 将棋所を起動し、メニューから「対局」→「エンジン管理」
3. 「追加」ボタンをクリック
4. `run_usi.bat` のフルパスを指定（例: `C:\Users\...\shogiAI\run_usi.bat`）
5. エンジン名に「shogiAI」と入力して登録
6. 対局画面で「対局」→「対局開始」から shogiAI を選択

**時間設定**: 初心者は秒読み60〜120秒がおすすめ

### 3. コマンドラインで対局
```bash
.venv\Scripts\python.exe src\play.py
```
USI形式（例: 7g7f）で指し手を入力

## 現在の実装状況

- ✅ 基本的な探索エンジン（Negamax + αβ枝刈り、深さ3）
- ✅ 駒割り評価関数
- ✅ USIプロトコル対応（将棋所で対局可能）
- ✅ CLI対局インターフェース
- ⬜ 反復深化・置換表・静止探索
- ⬜ 評価関数の強化（位置価値、王の安全度）
- ⬜ 棋譜からの学習
