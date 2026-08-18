#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/joystick.h>
#include <cerrno>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <vector>
#include <string>

#include "led-matrix.h"
#include "graphics.h"

#include "../../../source/host.h"
#include "../../../source/hostVmShared.h"
#include "../../../source/nibblehelpers.h"
#include "../../../source/logger.h"
#include "../../../source/filehelpers.h"

using namespace rgb_matrix;

extern RGBMatrix *g_matrix;
extern FrameCanvas *g_offscreen;
extern uint8_t g_contrast_lut[256];
extern bool g_apply_lut;
extern int g_frameskip;
extern volatile bool g_interrupt_received;

static int g_joy_fd = -1;
static uint8_t g_joy_mask = 0x00;
static uint32_t g_frame_count = 0;

void Host::setPlatformParams(
    int windowWidth,
    int windowHeight,
    uint32_t sdlWindowFlags,
    uint32_t sdlRendererFlags,
    uint32_t sdlPixelFormat,
    std::string logFilePrefix,
    std::string customBiosLua,
    std::string cartDirectory)
{
  _logFilePrefix = logFilePrefix;
  _customBiosLua = customBiosLua;
  _cartDirectory = cartDirectory;

  struct stat st = {0};
  if (stat(_logFilePrefix.c_str(), &st) == -1) {
    mkdir(_logFilePrefix.c_str(), 0777);
  }
  std::string cartdatadir = _logFilePrefix + "cdata";
  if (stat(cartdatadir.c_str(), &st) == -1) {
    mkdir(cartdatadir.c_str(), 0777);
  }
  if (stat(_cartDirectory.c_str(), &st) == -1) {
    mkdir(_cartDirectory.c_str(), 0777);
  }
}

Host::Host(int windowWidth, int windowHeight) {
  setPlatformParams(
      128,
      128,
      0,
      0,
      0,
      "fake08/",
      "",
      "carts"
  );
}

void Host::oneTimeSetup(Audio* audio) {
  setUpPaletteColors();

  // USB Joystick Auto-detection
  g_joy_fd = -1;
  for (int i = 0; i < 4; ++i) {
    char dev_path[32];
    snprintf(dev_path, sizeof(dev_path), "/dev/input/js%d", i);
    g_joy_fd = open(dev_path, O_RDONLY | O_NONBLOCK);
    if (g_joy_fd != -1) {
      printf("[RGB MATRIX HOST] USB Joystick detected on %s!\n", dev_path);
      break;
    }
  }

  loadSettingsIni();
}

void Host::oneTimeCleanup() {
  saveSettingsIni();

  if (g_joy_fd != -1) {
    close(g_joy_fd);
    g_joy_fd = -1;
  }
}

void Host::setTargetFps(int targetFps) {
  // Target FPS is maintained via usleep in waitForTargetFps
}

void Host::changeStretch() {
}

void Host::forceStretch(StretchOption newStretch) {
  stretch = newStretch;
}

bool Host::shouldQuit() {
  return g_interrupt_received || quit == 1;
}

bool Host::shouldRunMainLoop() {
  return !shouldQuit();
}

InputState_t Host::scanInput() {
  if (g_joy_fd != -1) {
    struct js_event event;
    while (read(g_joy_fd, &event, sizeof(event)) > 0) {
      if (event.type & JS_EVENT_BUTTON) {
        if (event.number == 0 || event.number == 2) {
          if (event.value) g_joy_mask |= P8_KEY_O; else g_joy_mask &= ~P8_KEY_O;
        }
        if (event.number == 1 || event.number == 3) {
          if (event.value) g_joy_mask |= P8_KEY_X; else g_joy_mask &= ~P8_KEY_X;
        }
        if (event.number == 6 || event.number == 8 || event.number == 10) {
          if (event.value) g_joy_mask |= P8_KEY_PAUSE; else g_joy_mask &= ~P8_KEY_PAUSE;
        }
      } else if (event.type & JS_EVENT_AXIS) {
        if (event.number == 0 || event.number == 6) { // X Axis
          if (event.value < -16000) g_joy_mask |= P8_KEY_LEFT; else g_joy_mask &= ~P8_KEY_LEFT;
          if (event.value > 16000) g_joy_mask |= P8_KEY_RIGHT; else g_joy_mask &= ~P8_KEY_RIGHT;
        } else if (event.number == 1 || event.number == 7) { // Y Axis
          if (event.value < -16000) g_joy_mask |= P8_KEY_UP; else g_joy_mask &= ~P8_KEY_UP;
          if (event.value > 16000) g_joy_mask |= P8_KEY_DOWN; else g_joy_mask &= ~P8_KEY_DOWN;
        }
      }
    }
  }

  uint8_t newHeld = g_joy_mask;
  uint8_t kdown = (newHeld ^ currKHeld) & newHeld;
  currKHeld = newHeld;

  InputState_t inputState;
  inputState.KDown = kdown;
  inputState.KHeld = currKHeld;
  inputState.mouseX = 0;
  inputState.mouseY = 0;
  inputState.mouseBtnState = 0;
  inputState.KBdown = false;
  inputState.KBkey = "";

  return inputState;
}

void Host::waitForTargetFps() {
  if (g_frameskip == 0) {
    usleep(16666);
  } else {
    usleep(16666 / (g_frameskip + 1));
  }
}

void Host::drawFrame(uint8_t* picoFb, uint8_t* screenPaletteMap, uint8_t drawMode) {
  g_frame_count++;
  bool skip_this_frame = (g_frameskip > 0) && ((g_frame_count % (g_frameskip + 1)) != 0);
  if (skip_this_frame) return;

  if (!g_matrix || !g_offscreen) return;

  ::Color* master_pal = GetPaletteColors();

  for (int y = 0; y < 128; ++y) {
    for (int x = 0; x < 128; ++x) {
      uint8_t c = getPixelNibble(x, y, picoFb);
      uint8_t pal_idx = screenPaletteMap[c] & 0x8F;
      ::Color col = master_pal[pal_idx];

      uint8_t r = col.Red;
      uint8_t g = col.Green;
      uint8_t b = col.Blue;

      if (g_apply_lut) {
        r = g_contrast_lut[r];
        g = g_contrast_lut[g];
        b = g_contrast_lut[b];
      }

      g_offscreen->SetPixel(x, y, r, g, b);
    }
  }

  g_offscreen = g_matrix->SwapOnVSync(g_offscreen);
}

bool Host::shouldFillAudioBuff() {
  return false;
}

void* Host::getAudioBufferPointer() {
  return nullptr;
}

size_t Host::getAudioBufferSize() {
  return 0;
}

void Host::playFilledAudioBuffer() {
}

const char* Host::logFilePrefix() {
  return _logFilePrefix.c_str();
}

std::string Host::customBiosLua() {
  return _customBiosLua;
}

std::string Host::getCartDirectory() {
  return _cartDirectory;
}

std::vector<std::string> Host::listcarts() {
  std::vector<std::string> carts;
  DIR *dir;
  struct dirent *ent;
  if ((dir = opendir(_cartDirectory.c_str())) != NULL) {
    while ((ent = readdir(dir)) != NULL) {
      if (isCartFile(ent->d_name)) {
        carts.push_back(_cartDirectory + "/" + ent->d_name);
      }
    }
    closedir(dir);
  }
  return carts;
}

std::vector<std::string> Host::listdirs() {
  std::vector<std::string> dirs;
  DIR *dir;
  struct dirent *ent;
  if ((dir = opendir(_cartDirectory.c_str())) != NULL) {
    while ((ent = readdir(dir)) != NULL) {
      if (ent->d_name[0] == '.') {
        continue;
      }
      std::string fullPath = _cartDirectory + "/" + ent->d_name;
      DIR* testDir = opendir(fullPath.c_str());
      if (testDir != NULL) {
        closedir(testDir);
        dirs.push_back(ent->d_name);
      }
    }
    closedir(dir);
  }
  return dirs;
}

void Host::overrideLogFilePrefix(const char* newPrefix) {
  _logFilePrefix = newPrefix;
  struct stat st = {0};
  std::string cartdatadir = _logFilePrefix + "cdata";
  if (stat(cartdatadir.c_str(), &st) == -1) {
    mkdir(cartdatadir.c_str(), 0777);
  }
}

double Host::deltaTMs() {
  return 0.0;
}

