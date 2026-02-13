from __future__ import annotations

from pathlib import Path
from typing import List

import torch
import torch.nn as nn
import torch.optim as optim

from dataset import board_to_input, read_kif_folder
import cshogi


class PolicyNet(nn.Module):
    def __init__(self, input_size: int, output_size: int) -> None:
        super().__init__()
        self.net = nn.Sequential(
            nn.Linear(input_size, 128),
            nn.ReLU(),
            nn.Linear(128, output_size),
        )

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        return self.net(x)


def main() -> None:
    data_dir = Path("../data")
    _ = list(read_kif_folder(data_dir))

    # まずはダミーで形だけ作る
    input_size = 1
    output_size = 1  # TODO: USI指し手のクラス数に置き換える

    model = PolicyNet(input_size, output_size)
    optimizer = optim.Adam(model.parameters(), lr=1e-3)
    criterion = nn.CrossEntropyLoss()

    # TODO: データローダを作成して学習
    dummy_x = torch.zeros((1, input_size))
    dummy_y = torch.zeros((1,), dtype=torch.long)

    logits = model(dummy_x)
    loss = criterion(logits, dummy_y)
    optimizer.zero_grad()
    loss.backward()
    optimizer.step()

    torch.save(model.state_dict(), "policy.pt")


if __name__ == "__main__":
    main()
