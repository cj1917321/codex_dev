# Windows 小游戏合集

三个使用 C 和 Win32/GDI 开发的离线小游戏，包含源码、测试及可直接运行的 Windows 64 位程序。

## 贪吃蛇

运行 `贪吃蛇/贪吃蛇升级版.exe`。

- 简单、中等、困难三级速度，可按 1 / 2 / 3 切换。
- 棋盘宽、高可分别调整为 12～48 格，窗口可缩放。
- 方向键或 WASD 移动，空格或 P 暂停，R 或 Enter 重开。

## 坦克大战

运行 `坦克大战/坦克大战_连射修复版.exe`。

- 三关战役：轻型小兵、重装大兵、Boss 与护卫。
- 可破坏砖墙、钢墙掩体，Boss 低血量时进入狂暴。
- Enter 开始或进入下一关，方向键或 WASD 移动。
- 按住空格或 J 连射，P 暂停，R 从第一关重开，Esc 退出。
- 已修复逐帧清屏闪烁和持续按键状态丢失的问题。

贪吃蛇和坦克大战目录中保留了旧版可执行文件，建议运行上面注明的最新版。详细规则见各目录中的 `使用说明.txt`。

## 五子棋

运行 `五子棋/五子棋.exe`。

- 15×15 棋盘，支持人机对战与本地双人对弈，按 1 / 2 切换。
- 鼠标点击交叉点落子，黑棋先行，无禁手，连续五子及以上获胜。
- 支持悔棋（U / Ctrl+Z）、重新开始（R）、最后落子标记及胜利连线。
- 人机模式下玩家执黑，电脑执白；支持窗口缩放与鼠标落点自动适配。

## 编译

需要 MinGW-w64 GCC。在对应游戏目录中执行：

```powershell
# 贪吃蛇
gcc snake.c -o 贪吃蛇升级版.exe -std=c11 -O2 -Wall -Wextra -municode -mwindows -static-libgcc -lgdi32 -luser32

# 坦克大战
gcc tank.c -o 坦克大战_连射修复版.exe -std=c11 -O2 -Wall -Wextra -municode -mwindows -static-libgcc -lgdi32 -luser32 -lm

# 五子棋
gcc gomoku.c -o 五子棋.exe -std=c11 -O2 -Wall -Wextra -municode -mwindows -static-libgcc -lgdi32 -luser32 -lm
```

## 测试

在对应游戏目录中执行：

```powershell
# 贪吃蛇
gcc snake_test.c -o snake_test.exe -std=c11 -O2 -static-libgcc -lgdi32 -luser32
.\snake_test.exe

# 坦克大战
gcc tank_test.c -o tank_test.exe -std=c11 -O2 -static-libgcc -lgdi32 -luser32 -lm
.\tank_test.exe

# 五子棋
gcc gomoku_test.c -o gomoku_test.exe -std=c11 -O2 -static-libgcc -lgdi32 -luser32 -lm
.\gomoku_test.exe
```

坦克大战和五子棋测试会生成 `preview-*.bmp` 以检查绘制结果；测试程序和预览图已加入忽略规则。
