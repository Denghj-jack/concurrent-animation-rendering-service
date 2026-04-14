#include <assert.h>
#include <stdio.h>
#include "animate.h"

static color_t render_center(struct canvas *c) {
    color_t buf[9];   // 3x3 canvas
    animate_generate_frame(c, 0, 1, buf);
    return buf[1 + 1 * 3];
}

static void test_top_on_already_top(void) {
    struct canvas *c = animate_create_canvas(3, 3, animate_color_argb(255, 0, 0, 0));

    struct sprite *red   = animate_create_rectangle(1, 1, animate_color_argb(255, 255, 0, 0), true);
    struct sprite *green = animate_create_rectangle(1, 1, animate_color_argb(255, 0, 255, 0), true);
    struct sprite *blue  = animate_create_rectangle(1, 1, animate_color_argb(255, 0, 0, 255), true);

    struct sprite_placement *p1 = animate_place_sprite(c, red,   1, 1);
    struct sprite_placement *p2 = animate_place_sprite(c, green, 1, 1);
    struct sprite_placement *p3 = animate_place_sprite(c, blue,  1, 1);   // already top

    assert(render_center(c) == animate_color_argb(255, 0, 0, 255));

    animate_placement_top(p3);

    assert(render_center(c) == animate_color_argb(255, 0, 0, 255));

    animate_destroy_canvas(c);
    assert(animate_destroy_sprite(red) == 0);
    assert(animate_destroy_sprite(green) == 0);
    assert(animate_destroy_sprite(blue) == 0);

    (void)p1;
    (void)p2;
}


static void test_bottom_on_already_bottom(void) {
    struct canvas *c = animate_create_canvas(3, 3, animate_color_argb(255, 0, 0, 0));

    struct sprite *red   = animate_create_rectangle(1, 1, animate_color_argb(255, 255, 0, 0), true);
    struct sprite *green = animate_create_rectangle(1, 1, animate_color_argb(255, 0, 255, 0), true);
    struct sprite *blue  = animate_create_rectangle(1, 1, animate_color_argb(255, 0, 0, 255), true);

    struct sprite_placement *p1 = animate_place_sprite(c, red,   1, 1);   // already bottom
    struct sprite_placement *p2 = animate_place_sprite(c, green, 1, 1);
    struct sprite_placement *p3 = animate_place_sprite(c, blue,  1, 1);

    assert(render_center(c) == animate_color_argb(255, 0, 0, 255));

    animate_placement_bottom(p1);


    assert(render_center(c) == animate_color_argb(255, 0, 0, 255));

    animate_destroy_canvas(c);
    assert(animate_destroy_sprite(red) == 0);
    assert(animate_destroy_sprite(green) == 0);
    assert(animate_destroy_sprite(blue) == 0);

    (void)p2;
    (void)p3;
}


static void test_up_down_on_single_node(void) {
    struct canvas *c = animate_create_canvas(3, 3, animate_color_argb(255, 0, 0, 0));
    struct sprite *green = animate_create_rectangle(1, 1, animate_color_argb(255, 0, 255, 0), true);
    struct sprite_placement *p = animate_place_sprite(c, green, 1, 1);

    assert(render_center(c) == animate_color_argb(255, 0, 255, 0));

    animate_placement_up(p);
    assert(render_center(c) == animate_color_argb(255, 0, 255, 0));

    animate_placement_down(p);
    assert(render_center(c) == animate_color_argb(255, 0, 255, 0));

    animate_destroy_canvas(c);
    assert(animate_destroy_sprite(green) == 0);
}


static void test_null_operations(void) {
    animate_placement_top(NULL);
    animate_placement_bottom(NULL);
    animate_placement_up(NULL);
    animate_placement_down(NULL);
}

int main(void) {
    test_top_on_already_top();
    test_bottom_on_already_bottom();
    test_up_down_on_single_node();
    test_null_operations();

    printf("All edge case tests passed!\n");
    return 0;
}