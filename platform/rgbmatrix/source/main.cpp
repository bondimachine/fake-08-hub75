#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <algorithm>
#include <string>
#include <vector>

#include "led-matrix.h"
#include "graphics.h"

#include "../../../source/vm.h"
#include "../../../source/logger.h"
#include "../../../source/host.h"
#include "../../../source/hostVmShared.h"
#include "../../../source/PicoRam.h"
#include "../../../source/Audio.h"

using namespace rgb_matrix;

RGBMatrix *g_matrix = nullptr;
FrameCanvas *g_offscreen = nullptr;
uint8_t g_contrast_lut[256];
bool g_apply_lut = false;
int g_frameskip = 0;
int g_contrast = 0;
int g_minb = 0;
int g_maxb = 255;
volatile bool g_interrupt_received = false;

static void InterruptHandler(int signo) {
  g_interrupt_received = true;
}

int main(int argc, char* argv[]) {
  RGBMatrix::Options matrix_options;
  rgb_matrix::RuntimeOptions runtime_opt;

  // Exact default parameters matching pico8.cc, nes.cc, gb.cc, etc.
  matrix_options.panel_type = "fm6373";
  matrix_options.spwm_row_address_type = 1;
  matrix_options.spwm_scan_rows = 64;
  matrix_options.rows = 128;
  matrix_options.cols = 128;
  matrix_options.spwm_register_config = 2;
  matrix_options.show_refresh_rate = true;
  runtime_opt.gpio_slowdown = 2;
  matrix_options.chain_length = 1;
  matrix_options.parallel = 1;

  for (int i = 1; i < argc; ++i) {
    if (argv[i] != NULL) {
      if (strncmp(argv[i], "--frameskip=", 12) == 0) {
        g_frameskip = atoi(argv[i] + 12);
      } else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
        g_frameskip = atoi(argv[++i]);
      } else if (strncmp(argv[i], "--contrast=", 11) == 0) {
        g_contrast = atoi(argv[i] + 11);
      } else if (strcmp(argv[i], "--contrast") == 0 && i + 1 < argc) {
        g_contrast = atoi(argv[i + 1]);
      } else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
        g_contrast = atoi(argv[i + 1]);
      } else if (strncmp(argv[i], "-minb=", 6) == 0) {
        g_minb = atoi(argv[i] + 6);
      } else if (strncmp(argv[i], "--minb=", 7) == 0) {
        g_minb = atoi(argv[i] + 7);
      } else if (strcmp(argv[i], "-minb") == 0 && i + 1 < argc) {
        g_minb = atoi(argv[i + 1]);
      } else if (strcmp(argv[i], "--minb") == 0 && i + 1 < argc) {
        g_minb = atoi(argv[i + 1]);
      } else if (strncmp(argv[i], "-maxb=", 6) == 0) {
        g_maxb = atoi(argv[i] + 6);
      } else if (strncmp(argv[i], "--maxb=", 7) == 0) {
        g_maxb = atoi(argv[i] + 7);
      } else if (strcmp(argv[i], "-maxb") == 0 && i + 1 < argc) {
        g_maxb = atoi(argv[i + 1]);
      } else if (strcmp(argv[i], "--maxb") == 0 && i + 1 < argc) {
        g_maxb = atoi(argv[i + 1]);
      }
    }
  }

  if (g_contrast < 0) g_contrast = 0;
  if (g_contrast > 10) g_contrast = 10;
  if (g_minb < 0) g_minb = 0;
  if (g_minb > 255) g_minb = 255;
  if (g_maxb < 0) g_maxb = 0;
  if (g_maxb > 255) g_maxb = 255;
  if (g_minb > g_maxb) std::swap(g_minb, g_maxb);

  float contrast_factor = 1.0f + (g_contrast * 0.15f);
  g_apply_lut = (g_contrast > 0 || g_minb > 0 || g_maxb < 255);

  for (int i = 0; i < 256; ++i) {
    float val;
    if (g_contrast > 0) {
      val = (i - 128.0f) * contrast_factor + 128.0f;
    } else {
      val = g_minb + (i / 255.0f) * (g_maxb - g_minb);
    }
    if (val < (float)g_minb) val = (float)g_minb;
    if (val > (float)g_maxb) val = (float)g_maxb;
    g_contrast_lut[i] = (uint8_t)roundf(val);
  }

  if (!rgb_matrix::ParseOptionsFromFlags(&argc, &argv, &matrix_options, &runtime_opt)) {
    return 1;
  }

  runtime_opt.drop_privileges = 0;

  g_matrix = RGBMatrix::CreateFromOptions(matrix_options, runtime_opt);
  if (g_matrix == NULL) {
    fprintf(stderr, "Failed to initialize RGB Matrix.\n");
    return 1;
  }

  signal(SIGTERM, InterruptHandler);
  signal(SIGINT, InterruptHandler);

  g_offscreen = g_matrix->CreateFrameCanvas();

  printf("====================================================\n");
  printf("   FAKE-08 PICO-8 Emulator for RGB Matrix          \n");
  printf("====================================================\n");
  printf("Display Mode: 128x128 PICO-8 Viewport mapped 1:1 onto LED Matrix\n");
  printf("Parameters: Frameskip=%d | Contrast=%d | MinB=%d | MaxB=%d\n\n",
         g_frameskip, g_contrast, g_minb, g_maxb);

  Host *host = new Host(128, 128);
  PicoRam *memory = new PicoRam();
  memory->Reset();
  Audio *audio = new Audio(memory);

  Logger_Initialize(host->logFilePrefix());
  Logger_Write("Initializing Vm\n");
  Vm *vm = new Vm(host, memory, nullptr, nullptr, audio);

  host->setUpPaletteColors();
  host->oneTimeSetup(audio);
  host->setTargetFps(60);

#if LOAD_PACK_INS
  host->unpackCarts();
#endif

  vm->SetCartList(host->listcarts());

  bool loadCart = false;
  char* cart = nullptr;

  for (int i = 1; i < argc; ++i) {
    if (argv[i] != NULL && argv[i][0] != '\0' && argv[i][0] != '-') {
      cart = argv[i];
      loadCart = true;
      break;
    }
  }

  if (loadCart) {
    printf("[FAKE-08] Loading cart: %s\n", cart);
    vm->LoadCart(cart, true);
  } else {
    printf("[FAKE-08] No external cart specified. Loading default BIOS cart.\n");
    vm->LoadBiosCart();
  }

  vm->vm_run();
  vm->GameLoop();

  vm->CloseCart();
  host->oneTimeCleanup();

  delete vm;
  delete host;
  delete memory;
  delete audio;

  Logger_Exit();

  if (g_matrix) {
    g_matrix->Clear();
    delete g_matrix;
    g_matrix = nullptr;
  }

  printf("\nFAKE-08 PICO-8 Emulator shutdown cleanly.\n");
  return 0;
}
