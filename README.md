# ShogiAlgo - 将棋AI

Negamax アルゴリズムと静止探索を備えた将棋AI エンジン。Web UI と USI インターフェース対応。

## 特徴

- ✅ **Negamax + Alpha-Beta 枝狩り** による効率的な探索
- ✅ **静止探索（Quiescence Search）** でキャプチャーシーケンスを自動評価
- ✅ **Web UI** でブラウザから対戦可能（ドラッグ&ドロップ対応）
- ✅ **USI プロトコル対応** で将棋所などのGUIと接続可能
- ✅ **配布アプリ化** - ShogiAlgo.exe で友達に配布できます

## 必要なもの

- Python 3.8+
- cshogi
- Flask
- numpy

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

```bash
python src/usi.py
```

将棋所などのGUIで、このエンジンをエンジン登録して対戦できます。

### 3. 配布用 .exe 作成

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
│   ├── engine.py       # Negamax エンジン実装
│   ├── usi.py          # USI プロトコルハンドラ
│   └── play.py         # CLI版（テスト用）
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

- Web UI: depth = 5
- USI: depth = 5
- 調整可能：`web/app.py` または `src/usi.py` の `depth` 変数を変更

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
