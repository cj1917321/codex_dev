# Windows 小游戏合集

两个使用 C 和 Win32/GDI 开发的离线小游戏，包含源码、测试及可直接运行的 Windows 64 位程序。

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

两个目录中保留了旧版可执行文件，建议运行上面注明的最新版。详细规则见各目录中的 `使用说明.txt`。

## 编译

需要 MinGW-w64 GCC。在对应游戏目录中执行：

```powershell
# 贪吃蛇
gcc snake.c -o 贪吃蛇升级版.exe -std=c11 -O2 -Wall -Wextra -municode -mwindows -static-libgcc -lgdi32 -luser32

# 坦克大战
gcc tank.c -o 坦克大战_连射修复版.exe -std=c11 -O2 -Wall -Wextra -municode -mwindows -static-libgcc -lgdi32 -luser32 -lm
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
```

坦克大战测试会生成 `preview-*.bmp` 以检查绘制结果；测试程序和预览图已加入忽略规则。
