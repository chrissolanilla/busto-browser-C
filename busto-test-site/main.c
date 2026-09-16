
#include <busto/busto_script.h>
#include <stdbool.h>

static double x = 100.0;
static double y = 100.0;
static bool right_down = 0;
static bool left_down = 0;
static bool needs_render = true;

void busto_start(struct busto_api *api) {
    api->set_title("Busto App");
    api->graphics.clear();
    needs_render = true;
}

void busto_update(struct busto_api *api, double dt) {
    //dont render if no key presses ideally
    (void)dt;
    if(!needs_render) {
        return;
    }
    api->graphics.clear();
    api->graphics.set_fill(1.0, 0.2, 0.6, 1.0);
    api->graphics.fill_rect(x, y, 100, 100);
    api->request_redraw();
    needs_render = false;

    if(x >500) {
        api->set_title("You win!");
        api->set_content("<p>You win!</p>");
    }
}

void busto_on_key_press(struct busto_api *api, const char *key) {
    (void)api;

    if (!key) {
        return;
    }

    bool key_pressed = false;
    if (key[0] == 'd' && key[1] == '\0') {
        x += 20.0;
        key_pressed = true;
    }

    if (key[0] == 'a' && key[1] == '\0') {
        x -= 20.0;
        key_pressed = true;
    }

    if (key[0] == 'w' && key[1] == '\0') {
        y -= 20.0;
        key_pressed = true;
    }

    if (key[0] == 's' && key[1] == '\0') {
        y += 20.0;
        key_pressed = true;
    }

    if(key_pressed) {
        needs_render = true;
    }

}

void busto_stop(struct busto_api *api) {
    //free shit? nah they can take it
}
