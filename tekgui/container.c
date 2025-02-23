#include "container.h"

#include <stdlib.h>
#include <math.h>

typedef struct TekGuiContainer {
    TekGuiSize padding;
    TekGuiSize margin;
    flag alignment;
    flag arrangement;
    flag fill;
    TekGuiObject** children;
    uint num_children;
} TekGuiContainer;

float tekGuiContainerGetWidth(const TekGuiObject* gui_object) {
    // get a pointer to the container's data
    const TekGuiContainer* container_ptr = (TekGuiContainer*)gui_object->ptr;
    const flag fill = container_ptr->fill;

    // find some constants
    const float double_margin = 2.f * tekGuiSizeToPixels(container_ptr->margin);
    const float usable_width = gui_object->parent->interface->tekGuiGetUsableWidth(gui_object->parent) - double_margin;
    const float min_width = fminf(double_margin, usable_width);

    // find the largest width possible if we want to expand
    if (hasFlag(fill, FILL_X_EXPAND)) return fmaxf(usable_width, min_width);

    // find the smallest width possible if we want to shrink
    const flag arrangement = container_ptr->arrangement;
    const float padding = tekGuiSizeToPixels(container_ptr->padding);
    if (hasFlag(fill, FILL_X_SHRINK)) {
        float width = padding;
        if (hasFlag(arrangement, ARRANGE_HORIZONTAL)) {
            // going horizontally, we need to sum the widths of children
            for (uint i = 0; i < container_ptr->num_children; i++) {
                TekGuiObject* child = container_ptr->children[i];
                width += child->interface->tekGuiGetWidth(child) + padding;
            }
        } else if (hasFlag(arrangement, ARRANGE_VERTICAL)) {
            // going vertically, we only really care about the width of the widest child
            const float double_padding = 2.f * padding;
            for (uint i = 0; i < container_ptr->num_children; i++) {
                TekGuiObject* child = container_ptr->children[i];
                const float child_width = child->interface->tekGuiGetWidth(child) + double_padding;
                if (child_width > width) width = child_width;
            }
        }
        // return width, making sure it is less/= than maximum, and greater/= than minimum
        return fminf(usable_width, fmaxf(width, min_width));
    }

    return min_width;
}

float tekGuiContainerGetHeight(const TekGuiObject* gui_object) {
    // get a pointer to the container's data
    const TekGuiContainer* container_ptr = (TekGuiContainer*)gui_object->ptr;
    const flag fill = container_ptr->fill;

    // find some constants
    const float double_margin = 2.f * tekGuiSizeToPixels(container_ptr->margin);
    const float usable_height = gui_object->parent->interface->tekGuiGetUsableHeight(gui_object->parent) - double_margin;
    const float min_height = fminf(double_margin, usable_height);

    // find the largest width possible if we want to expand
    if (hasFlag(fill, FILL_Y_EXPAND)) return fmaxf(usable_height, min_height);

    // find the smallest width possible if we want to shrink
    const flag arrangement = container_ptr->arrangement;
    const float padding = tekGuiSizeToPixels(container_ptr->padding);
    if (hasFlag(fill, FILL_Y_SHRINK)) {
        float height = padding;
        if (hasFlag(arrangement, ARRANGE_VERTICAL)) {
            // going vertically, we need to sum the heights of children
            for (uint i = 0; i < container_ptr->num_children; i++) {
                TekGuiObject* child = container_ptr->children[i];
                height += child->interface->tekGuiGetHeight(child) + padding;
            }
        } else if (hasFlag(arrangement, ARRANGE_HORIZONTAL)) {
            // going horizontally, we only really care about the height of the tallest child
            const float double_padding = 2.f * padding;
            for (uint i = 0; i < container_ptr->num_children; i++) {
                TekGuiObject* child = container_ptr->children[i];
                const float child_height = child->interface->tekGuiGetHeight(child) + double_padding;
                if (child_height > height) height = child_height;
            }
        }
        // return height, making sure it is less/= than maximum, and greater/= than minimum
        return fminf(usable_height, fmaxf(height, min_height));
    }

    return min_height;
}

float tekGuiContainerGetUsableWidth(const TekGuiObject* gui_object) {
    const float margin
    return gui_object->parent->interface->tekGuiGetUsableWidth(gui_object->parent);
}

void tekGuiContainerGetBbox(TekGuiObject* gui_object, TekGuiBbox* bbox) {

}

const TekGuiInterface container_interface = {
    CONTAINER_PTR,
    tekGuiContainerGetWidth,
    tekGuiContainerGetHeight,
    tekGuiContainerGetBbox
};

exception tekGuiCreateContainer(const TekGuiSize padding, const TekGuiSize margin, const flag alignment, const flag arrangement, const flag fill, TekGuiObject* container) {
    TekGuiContainer* container_ptr = (TekGuiContainer*)malloc(sizeof(TekGuiContainer));
    if (!container_ptr) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for container pointer.")

    container_ptr->padding = padding;
    container_ptr->margin = margin;
    container_ptr->alignment = alignment;
    container_ptr->arrangement = arrangement;
    container_ptr->fill = fill;

    container->interface = &container_interface;
    container->ptr = container_ptr;

    return SUCCESS;
}