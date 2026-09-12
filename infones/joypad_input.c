#include "screen.h"
#include "stdio.h"
#include "joystick.h"

int input_fd;

extern int bThread;

static unsigned char keyPad = 0;

int InitJoypadInput(void) {
  input_fd = open("/dev/stdin", 0);
  keyPad = 0;
  return 1;
}

/**
 * FC手柄 bit 键位对应关系 真实手柄中有一个定时器，处理 连A  连B
 * 0  1   2       3       4    5      6     7
 * A  B   Select  Start  Up   Down   Left  Right
 */

int GetJoypadInput(void) {
  u32 key = 0;
  u32 press = event_read_joystick(&key);
  if (press > 0) {
    /* 【诊断·可删】按键打印（只在变化时打，避免按住时每帧刷屏）：
     * press=2 为按下、1 为松开；key 为键码。用于判定"内核已收到按键（看到 keypad
     * raw=…）但应用是否真的收到事件"（xwin 模式下手柄设备的读取链路）。 */
    {
      static u32 last_key = 0xffff, last_press = 0;
      if (key != last_key || press != last_press) {
        last_key = key;
        last_press = press;
        printf("joypad press=%d key=%x\n", press, key);
      }
    }
    if (press == 2) {  // down
      switch (key) {
        case KEY_RIGHT:  // 右
          keyPad |= (1 << 7);
          break;
        case KEY_LEFT:  // 左
          keyPad |= (1 << 6);
          break;
        case KEY_DOWN:  // 下
          keyPad |= (1 << 5);
          break;
        case KEY_UP:  // 上
          keyPad |= (1 << 4);
          break;
        case KEY_BUTTON_START:  // 开始
          keyPad |= (1 << 3);
          break;
        case KEY_BUTTON_SELECT:  // 选择
          keyPad |= (1 << 2);
          break;
        case KEY_BUTTON_B:  // B
          keyPad |= (1 << 1);
          break;
        case KEY_BUTTON_A:  // A
          keyPad |= (1 << 0);
          break;
        case KEY_HOME:
          printf("quit\n");
          bThread = 0;
        default:
          break;
      }
    } else if (press == 1) {  // up
      switch (key) {
        case KEY_RIGHT:  // 右
          keyPad &= ~(1 << 7);
          break;
        case KEY_LEFT:  // 左
          keyPad &= ~(1 << 6);
          break;
        case KEY_DOWN:  // 下
          keyPad &= ~(1 << 5);
          break;
        case KEY_UP:  // 上
          keyPad &= ~(1 << 4);
          break;
        case KEY_BUTTON_START:  // 开始
          keyPad &= ~(1 << 3);
          break;
        case KEY_BUTTON_SELECT:  // 选择
          keyPad &= ~(1 << 2);
          break;
        case KEY_BUTTON_B:  // B
          keyPad &= ~(1 << 1);
          break;
        case KEY_BUTTON_A:  // A
          keyPad &= ~(1 << 0);
          break;
        default:
          break;
      }
    }
  }

  press = event_read_key(&key);
  if (press > 0) {
    if (press == 2) {  // down
      switch (key) {
        case 'd':  // 右
          keyPad |= (1 << 7);
          break;
        case 'a':  // 左
          keyPad |= (1 << 6);
          break;
        case 's':  // 下
          keyPad |= (1 << 5);
          break;
        case 'w':  // 上
          keyPad |= (1 << 4);
          break;
        case 'n':  // 开始
          keyPad |= (1 << 3);
          break;
        case 'm':  // 选择
          keyPad |= (1 << 2);
          break;
        case 'k':  // B
          keyPad |= (1 << 1);
          break;
        case 'j':  // A
          keyPad |= (1 << 0);
          break;
        case 'q':
          printf("quit\n");
          bThread = 0;
        default:
          break;
      }
    } else if (press == 1) {  // up
      switch (key) {
        case 'd':  // 右
          keyPad &= ~(1 << 7);
          break;
        case 'a':  // 左
          keyPad &= ~(1 << 6);
          break;
        case 's':  // 下
          keyPad &= ~(1 << 5);
          break;
        case 'w':  // 上
          keyPad &= ~(1 << 4);
          break;
        case 'n':  // 开始
          keyPad &= ~(1 << 3);
          break;
        case 'm':  // 选择
          keyPad &= ~(1 << 2);
          break;
        case 'k':  // B
          keyPad &= ~(1 << 1);
          break;
        case 'j':  // A
          keyPad &= ~(1 << 0);
          break;
        default:
          break;
      }
    }
  }
  /* 【修复·按键完全无效】原来这里是 `else { return -1; }`：
   * 本板子上没有 /dev/keyboard（libgui 里只有手柄设备打开成功，input_fd 一直为 -1）
   * ⇒ event_read_key() 恒返回 -1 ⇒ 每次都从这里直接 return -1 ⇒ 上面刚由实体手柄
   * 设置的 keyPad 被整个丢弃 ⇒ InfoNES_ReadJoypad() 因 ret < 0 而不更新 dwKeyPad1
   * ⇒ 游戏永远看不到任何按键（现象：日志里能看到 joypad press=2 key=6c，但按了
   * Start 游戏也不开始）。
   * 键盘没有事件 ≠ 没有输入 ⇒ 直接返回当前的 keyPad。 */
  return keyPad;
}