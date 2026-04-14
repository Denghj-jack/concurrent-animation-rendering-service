#include "animate_internal.h"
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>

#define UNUSED(x) (void)(x)

struct canvas* animate_create_canvas(size_t height, size_t width,
                                     color_t background_color){
    struct canvas* canvas = (struct canvas*)malloc(sizeof(struct canvas));
    if (canvas != NULL){
        canvas -> width = width;
        canvas -> height = height;
        canvas -> background_color = background_color;
        canvas -> bottom = NULL;
        canvas -> top = NULL;
    }
    else{
        return NULL;
    }
    return canvas;
}

void animate_destroy_canvas(struct canvas* canvas){
    if(canvas == NULL){
        return;
    }
    struct sprite_placement* current = canvas -> bottom;
    while(current != NULL){
        struct sprite_placement* temp = current -> next;
        current -> sprite ->num_of_references --;
        free(current);
        current = temp;
    }
    free(canvas);
    canvas = NULL;
    return;

}

size_t animate_frame_size_bytes(struct canvas* canvas){
    if(canvas == NULL){
        return 0;
    }
    size_t total_bytes = canvas ->width * canvas-> height * sizeof(color_t);
    return total_bytes;
    
}

unsigned animate_alpha(color_t color){ 
    return (unsigned)((color >> 24) & 0XFFu);
}

color_t forced_alpha(color_t color){
    return (color | 0XFF000000u);
}

void animate_generate_frame(const struct canvas* canvas, size_t frame,
                            size_t frame_rate, void* buf) {
    if(canvas == NULL || frame_rate == 0){
        return;
    }
    if(buf == NULL){
        return;
    }
    color_t* final_buf = (color_t*)buf;
    color_t applied_alpha_color = forced_alpha(canvas ->background_color);
    for(size_t i = 0 ; i < canvas -> width * canvas ->height ; i++){
        final_buf[i] = applied_alpha_color; //Set the background 
    }

    double time = (double)frame / (double)frame_rate;


    struct sprite_placement* current =  canvas -> bottom;
    while(current != NULL){//drawing from the bottom placement
        if(current -> sprite == NULL || current -> sprite -> pixels == NULL){
            current = current -> next;
            continue;
        } 
        struct sprite_placement* temp = current -> next;

        double x_movement = current ->x + current ->vx * time + current ->ax * (time*time)/2.0;
        double y_movement = current ->y + current ->vy * time + current ->ay * (time*time)/2.0;
 
        for(size_t i = 0 ; i < current -> sprite -> height ; i++){
            for(size_t j = 0 ; j < current -> sprite -> width ; j++){
                if(animate_alpha(current ->sprite -> pixels[current -> sprite -> width * i + j]) == 0){
                    //The current pixel should be drawn/added to the frame
                    continue;
                }
                else{
                    ssize_t final_x_coordinate = (ssize_t)j + x_movement;
                    ssize_t final_y_coordinate = (ssize_t)i + y_movement;
                    if(final_x_coordinate < 0 || final_y_coordinate < 0){
                        continue;
                    }
                    size_t fx = (size_t) final_x_coordinate;
                    size_t fy = (size_t) final_y_coordinate;
                    if(fx >= current ->canvas ->width || fy >= current ->canvas ->height){
                        continue;
                    }
                    final_buf[final_y_coordinate * current -> canvas -> width + final_x_coordinate] = forced_alpha(current -> sprite ->pixels[i*current -> sprite -> width + j]);
                }
            }
        }
        current = temp;
    }
    return;
}

// Optional extension
void animate_set_animation_function(struct sprite_placement* sprite_placement,
                                    animate_fn n, void* priv) {
                                        UNUSED(sprite_placement);
                                        UNUSED(priv);
                                        UNUSED(n);
                                        return;
                    
}


