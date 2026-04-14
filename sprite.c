#include "animate_internal.h"
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>


struct sprite* animate_create_sprite(const char* file) {
    FILE* fp = fopen(file , "rb");
    if(fp == NULL){
        return NULL;
    }
    struct bitmap_header header1;
    struct bitmapv5_header header2;
    if(fread(&header1 , sizeof(struct bitmap_header) , 1 , fp) != 1){
        fclose(fp);
        return NULL;
    }
    if(fread(&header2 , sizeof(struct bitmapv5_header) , 1 , fp) != 1){
        fclose(fp);
        return NULL;
    }
    if(header1.magic[0] != 'B' || header1.magic[1] != 'M'){
        fclose(fp);
        return NULL;
    }
    size_t width = (size_t)header2.bV5Width;
    size_t height = (size_t)header2.bV5Height;

    if(header2.bV5Size != 124){
        fclose(fp);
        return NULL;
    }

    struct sprite* sprite = (struct sprite*)malloc(sizeof(struct sprite));

    if(sprite == NULL){
        fclose(fp);
        return NULL;
    }
    sprite -> num_of_references = 0;
    sprite ->width = width;
    sprite -> height = height;
    sprite -> filled = true; 
    color_t* pixels = (color_t*)calloc(width*height , sizeof(color_t));
    

    if(pixels == NULL){
        free(sprite);
        sprite = NULL;
        fclose(fp);
        return NULL;
    }
    sprite -> pixels = pixels;

    fseek(fp , header1.pixel_offset , SEEK_SET);

    for(size_t num = 0; num < height ; num++){
        color_t* destination = pixels + (height -1 - num)*width;
        if(fread(destination , sizeof(color_t) , width , fp) != width){
            free(sprite -> pixels);
            sprite -> pixels = NULL;
            free(sprite);
            sprite = NULL;
            fclose(fp);
            return NULL;
        }
    }

    fclose(fp);
    return sprite;
}

struct sprite* animate_create_circle(size_t radius, color_t c, bool filled) {
    struct sprite* sprite =(struct sprite*)malloc(sizeof(struct sprite));
    if(sprite == NULL){
        return NULL;
    }
    size_t diameter = radius*2 + 1; //The additional 1 is for the center of the circle sprite
    sprite -> height = diameter;
    sprite -> width = diameter;
    sprite -> num_of_references = 0;
    sprite -> filled = filled;
    color_t* pixels = (color_t*)calloc(diameter*diameter , sizeof(color_t));
    if(pixels == NULL){
        free(sprite);
        sprite = NULL;
        return NULL;
    }
    sprite -> pixels = pixels;
        for(size_t i = 0 ; i < diameter ; i++){
            for(size_t j = 0 ; j < diameter ; j++){
                ssize_t y_offset = (ssize_t)i - (ssize_t)radius;
                ssize_t x_offset = (ssize_t)j - (ssize_t)radius;
                size_t distance = (size_t)(x_offset * x_offset + y_offset * y_offset);
                if(distance <= radius*radius){
                    sprite -> pixels[i*diameter + j] = c;
                }
            }
        }
    return sprite;
}

struct sprite* animate_create_rectangle(size_t width, size_t height,
                                        color_t c, bool filled){
    
    struct sprite* sprite =(struct sprite*)malloc(sizeof(struct sprite));
    if(sprite == NULL){
        return NULL;
    }
    sprite -> width = width;
    sprite ->height = height;
    sprite ->num_of_references = 0;
    sprite -> pixels = NULL;
    sprite ->filled = filled; 
    if(width == 0 || height == 0){
        return sprite;
    } //edge case
    color_t* pixels = (color_t*)calloc(width*height , sizeof(color_t));
    if(pixels == NULL){
        free(sprite);
        sprite = NULL;
        return NULL;
    }
    sprite -> pixels = pixels;
    for(size_t i = 0 ; i < height ; i++){
        for(size_t j = 0 ; j < width ; j++){
            if(i == 0 || j == 0 || i == height-1 || j == width -1){
                sprite -> pixels[i * width + j] = c;
            }
            else if(filled){
                sprite -> pixels[i * width + j] = c;
            }
        }
    }

    return sprite;


}

bool animate_destroy_sprite(struct sprite* sprite) {
    if(sprite == NULL){
        return 1;
    }
    size_t references = sprite -> num_of_references;
    if(references == 0){
        free(sprite -> pixels);
        sprite -> pixels = NULL;
        free(sprite);
        sprite = NULL;
        return 0;
    }
        return 1;
    
}