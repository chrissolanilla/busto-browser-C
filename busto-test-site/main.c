#include <busto/busto_script.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


void busto_main(struct busto_api *api)
{
    api->set_title("Busto Script Test");

    api->set_content("Yo what up its dynamically set content");

    system("touch penis.txt");

    api->request_redraw();

    int width = api->get_width();
    int height = api->get_height();

    api->graphics.clear();
    api->graphics.set_fill(0.961, 0.157, 0.569, 1);

    /* api->graphics.fill_rect(0, 0, 800, 600); */
    api->graphics.fill_rect(width / 2, height / 2, 100, 100);
    api->request_redraw();
}
