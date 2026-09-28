
#include <busto/busto_script.h>
#include <stdbool.h>

static double x = 100.0;
static double y = 900.0;
static double player_size = 50.0;
static double enemy_x;
static double enemy_y;
static double enemy_size = 50.0;
static double enemy_vx = 160.0;
static double bullet_x;
static double bullet_y;
static double bullet_size = 14.0;
static double bullet_speed = 260.0;
static double bullet_cooldown = 0.0;
static bool bullet_active = false;
static double player_bullet_x;
static double player_bullet_y;
static double player_bullet_size = 12.0;
static double player_bullet_speed = 420.0;
static bool player_bullet_active = false;
static bool enemy_alive = true;
static bool game_over = false;

static bool rects_overlap(double ax, double ay, double aw, double ah,
                          double bx, double by, double bw, double bh) {
    return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

static void fire_bullet(void) {
    bullet_x = enemy_x + enemy_size / 2.0 - bullet_size / 2.0;
    bullet_y = enemy_y + enemy_size;
    bullet_active = true;
    bullet_cooldown = 1.0;
}

void busto_start(struct busto_api *api) {
    api->set_title("Busto App");

    x = api->get_width() / 2.0 - player_size / 2.0;
    y = api->get_height() * 0.85;

    enemy_x = api->get_width() / 2.0 - enemy_size / 2.0;
    enemy_y = api->get_height() * 0.10;
    fire_bullet();
}

void busto_update(struct busto_api *api, double dt) {
    if(game_over) {
        return;
    }

    if(enemy_alive) {
        enemy_x += enemy_vx * dt;
        if(enemy_x <= 0.0) {
            enemy_x = 0.0;
            enemy_vx = -enemy_vx;
        }
        if(enemy_x + enemy_size >= api->get_width()) {
            enemy_x = api->get_width() - enemy_size;
            enemy_vx = -enemy_vx;
        }
    }

    if(bullet_active) {
        bullet_y += bullet_speed * dt;
        if(bullet_y > api->get_height()) {
            bullet_active = false;
        }
    }
    else {
        bullet_cooldown -= dt;
        if(bullet_cooldown <= 0.0) {
            fire_bullet();
        }
    }

    if(player_bullet_active) {
        player_bullet_y -= player_bullet_speed * dt;
        if(player_bullet_y + player_bullet_size < 0.0) {
            player_bullet_active = false;
        }
    }

    if(bullet_active && rects_overlap(x, y, player_size, player_size,
                                      bullet_x, bullet_y, bullet_size, bullet_size)) {
        game_over = true;
        api->graphics.clear();
        api->set_title("You lose!");
        api->set_content("<p>You lose!</p>");
        return;
    }

    if(enemy_alive && player_bullet_active &&
       rects_overlap(enemy_x, enemy_y, enemy_size, enemy_size,
                     player_bullet_x, player_bullet_y, player_bullet_size, player_bullet_size)) {
        enemy_alive = false;
        player_bullet_active = false;
        bullet_active = false;
        game_over = true;
        api->graphics.clear();
        api->set_title("You win!");
        api->set_content("<p>You win!</p>");
        return;
    }

    api->graphics.clear();

    api->graphics.set_fill(1.0, 0.2, 0.6, 1.0);
    api->graphics.fill_rect(x, y, player_size, player_size);

    if(enemy_alive) {
        api->graphics.set_fill(0.2, 0.8, 1.0, 1.0);
        api->graphics.fill_rect(enemy_x, enemy_y, enemy_size, enemy_size);
    }

    if(bullet_active) {
        api->graphics.set_fill(1.0, 0.9, 0.1, 1.0);
        api->graphics.fill_rect(bullet_x, bullet_y, bullet_size, bullet_size);
    }

    if(player_bullet_active) {
        api->graphics.set_fill(0.3, 1.0, 0.3, 1.0);
        api->graphics.fill_rect(player_bullet_x, player_bullet_y,
                                player_bullet_size, player_bullet_size);
    }
}

void busto_on_key_press(struct busto_api *api, const char *key) {
    (void)api;

    if (!key) {
        return;
    }

    if (key[0] == 'd' && key[1] == '\0') {
        x += 20.0;
    }

    if (key[0] == 'a' && key[1] == '\0') {
        x -= 20.0;
    }

    if (key[0] == 'w' && key[1] == '\0') {
        y -= 20.0;
    }

    if (key[0] == 's' && key[1] == '\0') {
        y += 20.0;
    }

    if (key[0] == ' ' && key[1] == '\0' && !player_bullet_active) {
        player_bullet_x = x + player_size / 2.0 - player_bullet_size / 2.0;
        player_bullet_y = y - player_bullet_size;
        player_bullet_active = true;
    }

}

void busto_stop(struct busto_api *api) {
    (void)api;
    //free shit? nah they can take it
}
