# ShogiAlgo - 将棋AI

Negamax アルゴリズムと静止探索を備えた将棋AI エンジン。Web UI と USI インターフェース対応。

Python版とC++版の2つのエンジンがあります。

## 特徴

- ✅ **Negamax + Alpha-Beta 枝狩り** による効率的な探索
- ✅ **静止探索（Quiescence Search）** でキャプチャーシーケンスを自動評価
- ✅ **反復深化 + 時間制御** で対局サーバーの持ち時間に対応
- ✅ **C++版 (ShogiAlgo-cpp)** で高速探索（Python版の数十倍）
- ✅ **Web UI** でブラウザから対戦可能（ドラッグ&ドロップ対応）
- ✅ **USI プロトコル対応** で将棋所・Floodgateと接続可能
- ✅ **配布アプリ化** - ShogiAlgo.exe で友達に配布できます

## 必要なもの

### Python版
- Python 3.8+
- cshogi
- Flask
- numpy

### C++版
- CMake 3.15+
- C++17対応コンパイラ（Visual Studio 2022 等）

## セットアップ

```bash
git clone <repo-url>
cd shogiAI

# 仮想環境作成
python -m venv .venv
source .venv/Scripts/activate  # Windows: .venv\Scripts\activate

# 依存パッケージインストール
pip install -r requirements.txt
```

## 使い方

### 1. Web UI で対戦（ローカル）

```bash
python web/app.py
```

ブラウザで `http://localhost:5000` を開いて対戦開始。

**操作方法：**

- 盤面をクリック/ドラッグで駒を動かす
- 右側の「プレイヤー（先手）」セクションでドラッグ&ドロップで持ち駒を打つ

### 2. USI エンジンとして使用（将棋所など）

**Python版:**
```bash
python src/usi.py
```

**C++版（高速・推奨）:**
```bash
cd cpp
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```
生成された `cpp/build/Release/ShogiAlgo.exe` を将棋所にエンジン登録して対戦できます。

### 3. Floodgate に参加

1. 将棋所に C++版エンジン (`ShogiAlgo.exe`) を登録
2. 「対局」→「サーバー通信対局(floodgate)」を選択
3. ログイン名とパスワードを設定して対局開始

### 4. 配布用 .exe 作成

```bash
pyinstaller --onefile --console --name "ShogiAlgo" \
  --hidden-import=numpy \
  --hidden-import=cshogi \
  --hidden-import=cshogi._cshogi \
  --add-data "web/templates:web/templates" \
  --add-data "src:src" \
  web/app.py
```

生成された `dist/ShogiAlgo.exe` を友達に配布。ダブルクリックで自動起動します。

## ファイル構成

```
shogiAI/
├── src/
│   ├── engine.py       # Python版 Negamax エンジン
│   ├── usi.py          # Python版 USI プロトコルハンドラ
│   └── play.py         # CLI版（テスト用）
├── cpp/
│   ├── main.cpp        # C++版 エンジン + USI（反復深化・時間制御付き）
│   ├── CMakeLists.txt  # ビルド設定
│   └── cshogi/         # cshogi C++ライブラリ（盤面管理・合法手生成）
├── web/
│   ├── app.py          # Flask サーバー
│   └── templates/
│       └── index.html  # Web UI
├── requirements.txt
└── README.md
```

## 技術仕様

### 探索アルゴリズム

- **Negamax**: ゼロサム評価の効率的なミニマックス
- **Alpha-Beta 枝狩り**: 探索ノード削減
- **静止探索（Quiescence Search）**: depth=0時にキャプチャーのみ深く探索
- **手順最適化**: キャプチャー → 王手 → その他の順で生成

### 評価関数

駒割り評価のみ（シンプル実装）:

- 歩: 100点
- 香/桂: 300点
- 銀: 400点
- 角: 700点
- 飛: 800点
- 金: 500点
- 王: 10000点

### 探索深さ

- **Python版**: 固定 depth = 5
- **C++版**: 反復深化（時間制限内で可能な限り深く探索）
  - Floodgate（10分+10秒フィッシャー）では depth 7〜9 程度

## トラブルシューティング

**Q: ShogiAlgo.exe を実行してもブラウザが開かない**

- Windows ファイアウォールの許可を確認
- `http://localhost:5000` をブラウザに直接入力

**Q: 反則負けになる**

- ターミナルの DEBUG ログを確認
- 非合法な手が返されている場合、自動的にフォールバック手を使用

**Q: 思考時間が速すぎる/遅すぎる**

- `web/app.py` の `depth` 値を調整（デフォルト: 5）
- depth を上げると強くなりますが遅くなります

## ライセンス

MIT License

## 参考資料

- cshogi: https://github.com/TadaoYamaoka/cshogi
- Negamax: https://en.wikipedia.org/wiki/Negamax
- Quiescence Search: https://en.wikipedia.org/wiki/Quiescence_search
