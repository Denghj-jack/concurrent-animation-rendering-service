#include "animate_internal.h"
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>

struct sprite_placement* animate_place_sprite(struct canvas* canvas,
                                              struct sprite* sprite,
                                              ssize_t x, ssize_t y) {
    if(canvas == NULL || sprite == NULL){
        return NULL;
    }
    struct sprite_placement* placement = (struct sprite_placement*)calloc(1 , sizeof(struct sprite_placement));
    if(placement == NULL){
        return NULL;
    }
    placement -> x = x;
    placement -> y = y;
    placement -> canvas = canvas;
    placement -> sprite = sprite;
    placement -> sprite -> num_of_references ++;
    placement -> vx = 0;
    placement -> vy = 0;
    placement -> ax = 0;
    placement -> ay = 0;
    placement -> previous = NULL;
    placement -> next = NULL;

    if(canvas -> bottom == NULL && canvas -> top == NULL){
        canvas ->top = placement;
        canvas ->bottom = placement;
    }
    else{
        placement -> previous = canvas -> top;
        placement -> next = NULL;
        canvas -> top -> next = placement;
        canvas -> top = placement;
        
        
    }
    return placement;


}

void animate_placement_up(struct sprite_placement* sprite_placement){
    if(sprite_placement == NULL || sprite_placement ->canvas == NULL){
        return;
    }
    if(sprite_placement -> next == NULL){ //at the top(including only one placement in the list)
        return;
    } 
    else if(sprite_placement -> previous == NULL && sprite_placement -> next != NULL){ //at the bottom
        struct sprite_placement* next_placement = sprite_placement -> next;
        if(next_placement ->next == NULL){//only 2 placements in the canvas
            next_placement -> next = sprite_placement;
            next_placement -> previous = NULL;
            sprite_placement -> previous = next_placement;
            sprite_placement -> next = NULL;
            sprite_placement ->canvas ->top = sprite_placement;
            sprite_placement -> canvas -> bottom = next_placement;
            return;
        }
        else{//more than 2 placements in the canvas
            struct sprite_placement* next_next_placement = next_placement -> next;
            next_next_placement -> previous = sprite_placement;
            next_placement -> previous = NULL;
            next_placement -> next = sprite_placement;
            sprite_placement -> next = next_next_placement;
            sprite_placement -> previous = next_placement;
            sprite_placement -> canvas -> bottom = next_placement;
            return;
        }
    }
    else if(sprite_placement -> previous != NULL && sprite_placement -> next != NULL){//middle
        struct sprite_placement* next_placement = sprite_placement -> next;
        if(next_placement ->next == NULL){
            sprite_placement -> previous -> next = next_placement;
            next_placement -> previous = sprite_placement -> previous;
            next_placement -> next = sprite_placement;
            sprite_placement -> previous = next_placement;
            sprite_placement -> next = NULL;
            sprite_placement-> canvas -> top = sprite_placement;
            return;
        }
        else{
            struct sprite_placement* next_next_placement = next_placement -> next;
            next_next_placement -> previous = sprite_placement;
            sprite_placement -> previous -> next = next_placement;
            next_placement -> previous = sprite_placement -> previous;
            next_placement -> next = sprite_placement;
            sprite_placement -> next = next_next_placement;
            sprite_placement -> previous = next_placement;
            return;
        }
    } 
    return;

}

void animate_placement_down(struct sprite_placement* sprite_placement){
    if(sprite_placement == NULL || sprite_placement ->canvas == NULL){
        return;
    }
    if(sprite_placement -> previous == NULL){ //at the bottom(including only one placement in the list)
        return;
    } 
    else if(sprite_placement -> previous != NULL && sprite_placement -> next == NULL){ //at the top
        struct sprite_placement* previous_placement = sprite_placement-> previous;
        if(previous_placement -> previous == NULL){//only two placements
            previous_placement -> next = NULL;
            previous_placement -> previous = sprite_placement;
            sprite_placement -> previous = NULL;
            sprite_placement -> next = previous_placement;
            sprite_placement -> canvas -> bottom = sprite_placement;
            sprite_placement -> canvas -> top = previous_placement;
            return;
        }
        else{//more than 2 placements in the canvas
            struct sprite_placement* previous_previous_placement = previous_placement-> previous;
            previous_previous_placement -> next =sprite_placement;
            previous_placement -> next = NULL;
            previous_placement -> previous = sprite_placement;
            sprite_placement -> previous = previous_previous_placement;
            sprite_placement -> next = previous_placement;
            sprite_placement -> canvas -> top = previous_placement;
            return;
        }
    }

    else if(sprite_placement -> previous != NULL && sprite_placement -> next != NULL){//middle
        struct sprite_placement* previous_placement = sprite_placement-> previous;
        if(previous_placement -> previous == NULL){
            previous_placement -> previous = sprite_placement;
            previous_placement -> next = sprite_placement -> next;
            sprite_placement -> next -> previous =previous_placement;
            sprite_placement -> previous = NULL;
            sprite_placement -> next = previous_placement;
            sprite_placement -> canvas -> bottom = sprite_placement;
            return;
        }
        else{
            struct sprite_placement* previous_previous_placement = previous_placement-> previous;
            previous_previous_placement -> next =sprite_placement;
            previous_placement -> next = sprite_placement -> next;
            previous_placement -> previous = sprite_placement;
            sprite_placement -> next -> previous = previous_placement;
            sprite_placement -> previous = previous_previous_placement;
            sprite_placement -> next = previous_placement;
            return;
        }
    }
    return;
}

void animate_placement_top(struct sprite_placement* sprite_placement){
    if(sprite_placement == NULL){
        return;
    }
    struct canvas* corresponding_canvas = sprite_placement -> canvas;
    if(corresponding_canvas == NULL){
        return;
    }
    if(corresponding_canvas -> bottom == NULL && corresponding_canvas -> top == NULL){
        corresponding_canvas ->top = sprite_placement;
        corresponding_canvas ->bottom = sprite_placement;
        sprite_placement -> previous = NULL;
        sprite_placement -> next = NULL;
        return;
    }
    if(sprite_placement -> previous == NULL && sprite_placement -> next == NULL){
        return;
    }
    if(sprite_placement -> next == NULL){
        return;
    }
    if(sprite_placement -> previous == NULL){
        corresponding_canvas -> bottom = sprite_placement -> next;
        sprite_placement -> next -> previous = NULL;
        sprite_placement -> previous = corresponding_canvas -> top;
        sprite_placement -> next = NULL;
        corresponding_canvas -> top -> next = sprite_placement;
        corresponding_canvas -> top = sprite_placement;
        return;
    }
    else if(sprite_placement -> previous != NULL){
        sprite_placement -> previous -> next = sprite_placement -> next;
        sprite_placement -> next -> previous = sprite_placement -> previous;
        sprite_placement -> next = NULL;
        sprite_placement -> previous = corresponding_canvas -> top;
        corresponding_canvas -> top -> next = sprite_placement;
        corresponding_canvas -> top = sprite_placement;
        return;
    }
    return;
}

void animate_placement_bottom(struct sprite_placement* sprite_placement){
    if(sprite_placement == NULL){
        return;
    }
    struct canvas* corresponding_canvas = sprite_placement -> canvas;
    if(corresponding_canvas == NULL){
        return;
    }
    if(corresponding_canvas -> bottom == NULL && corresponding_canvas -> top == NULL){
        corresponding_canvas ->top = sprite_placement;
        corresponding_canvas ->bottom = sprite_placement;
        return;
    }
    if(sprite_placement -> previous == NULL){
        return;
    }
    if(sprite_placement -> next == NULL){
        corresponding_canvas -> top  = sprite_placement -> previous;
        sprite_placement -> previous -> next = NULL;
        sprite_placement -> previous = NULL;
        sprite_placement -> next = corresponding_canvas -> bottom;
        corresponding_canvas -> bottom -> previous = sprite_placement;
        corresponding_canvas -> bottom = sprite_placement;
        return;
    }
    else if(sprite_placement -> next != NULL){
        sprite_placement -> previous -> next = sprite_placement -> next;
        sprite_placement -> next -> previous = sprite_placement -> previous;
        sprite_placement -> previous = NULL;
        sprite_placement -> next = corresponding_canvas -> bottom;
        corresponding_canvas -> bottom -> previous = sprite_placement;
        corresponding_canvas -> bottom = sprite_placement;
        return;
        
    }
    return;
}

void animate_destroy_placement(struct sprite_placement* sprite_placement){
    if(sprite_placement == NULL){
        return;
    }
    struct canvas* corrsponding_canvas = sprite_placement -> canvas;
    if(corrsponding_canvas == NULL){
        sprite_placement -> sprite -> num_of_references --;
        free(sprite_placement);
        sprite_placement = NULL;
        return;
    }

    if(sprite_placement -> next == NULL && sprite_placement -> previous == NULL){//only one placement in the canvas
        corrsponding_canvas -> bottom = NULL;
        corrsponding_canvas -> top = NULL;
    }
    else if(sprite_placement -> next == NULL && sprite_placement -> previous != NULL){ //top
        sprite_placement -> canvas ->top = sprite_placement -> previous;
        sprite_placement -> previous -> next = NULL;

    }
    else if(sprite_placement -> next != NULL &&sprite_placement -> previous == NULL){ //bottom
        sprite_placement -> canvas ->bottom = sprite_placement -> next;
        sprite_placement -> next -> previous = NULL;

    }
    else if(sprite_placement -> next != NULL && sprite_placement -> previous != NULL){ //middle
        sprite_placement ->next ->previous = sprite_placement -> previous;
        sprite_placement ->previous ->next = sprite_placement -> next;
    }

    if(sprite_placement ->sprite != NULL){
        sprite_placement -> sprite ->num_of_references --;
    }
    free(sprite_placement);
    sprite_placement = NULL;
    return;
}

void animate_set_animation_params(struct sprite_placement* sprite_placement,
                                  ssize_t vx, ssize_t vy,
                                  ssize_t ax, ssize_t ay){
    if(sprite_placement == NULL){
        return;
    }
    sprite_placement -> vx = vx;
    sprite_placement -> vy = vy;
    sprite_placement -> ax = ax;
    sprite_placement -> ay = ay;

    return;
}
