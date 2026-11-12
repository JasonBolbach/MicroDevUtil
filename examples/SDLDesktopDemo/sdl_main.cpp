#include <lgfx/v1/platforms/sdl/Panel_sdl.hpp>
#if defined ( SDL_h_ )

// Points at LovyanGFX's run flag; false once the window is closed (see DesktopInput::Quitting)
bool* g_running = nullptr;

void setup(void);
void loop(void);

__attribute__((weak))
int user_func(bool* running)
{
  g_running = running;
  setup();
  do
  {
    loop();
  } while (*running);
  return 0;
}

int main(int, char**)
{
  return lgfx::Panel_sdl::main(user_func);
}

#endif
