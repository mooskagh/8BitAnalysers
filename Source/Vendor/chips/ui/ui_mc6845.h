#pragma once
/*#
    # ui_mc6845.h

    Debug visualization for mc6845.h

    Do this:
    ~~~C
    #define CHIPS_IMPL
    ~~~
    before you include this file in *one* C++ file to create the 
    implementation.

    Optionally provide the following macros with your own implementation
    
    ~~~C
    CHIPS_ASSERT(c)
    ~~~
        your own assert macro (default: assert(c))

    Include the following headers before the including the *declaration*:
        - mc6845.h
        - ui_chip.h

    Include the following headers before including the *implementation*:
        - imgui.h
        - mc6845.h
        - ui_chip.h
        - ui_util.h

    All string data provided to ui_mc6845_init() must remain alive until
    until ui_mc6845_discard() is called!

    ## zlib/libpng license

    Copyright (c) 2018 Andre Weissflog
    This software is provided 'as-is', without any express or implied warranty.
    In no event will the authors be held liable for any damages arising from the
    use of this software.
    Permission is granted to anyone to use this software for any purpose,
    including commercial applications, and to alter it and redistribute it
    freely, subject to the following restrictions:
        1. The origin of this software must not be misrepresented; you must not
        claim that you wrote the original software. If you use this software in a
        product, an acknowledgment in the product documentation would be
        appreciated but is not required.
        2. Altered source versions must be plainly marked as such, and must not
        be misrepresented as being the original software.
        3. This notice may not be removed or altered from any source
        distribution. 
#*/
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* setup parameters for ui_mc6845_init()
    NOTE: all string data must remain alive until ui_mc6845_discard()!
*/
typedef struct {
    const char* title;          /* window title */
    mc6845_t* mc6845;           /* pointer to mc6845_t instance to track */
    int x, y;                   /* initial window pos */
    int w, h;                   /* initial window size, or 0 for default size */
    bool open;                  /* initial open state */
    ui_chip_desc_t chip_desc;   /* chip visualization desc */
} ui_mc6845_desc_t;

typedef struct {
    const char* title;
    mc6845_t* mc6845;
    float init_x, init_y;
    float init_w, init_h;
    bool open;
    bool valid;
    ui_chip_t chip;
} ui_mc6845_t;

void ui_mc6845_init(ui_mc6845_t* win, const ui_mc6845_desc_t* desc);
void ui_mc6845_discard(ui_mc6845_t* win);
void ui_mc6845_draw(ui_mc6845_t* win);

#ifdef __cplusplus
} /* extern "C" */
#endif

/*-- IMPLEMENTATION (include in C++ source) ----------------------------------*/
#ifdef CHIPS_IMPL
#ifndef __cplusplus
#error "implementation must be compiled as C++"
#endif
#include <string.h> /* memset */
#ifndef CHIPS_ASSERT
    #include <assert.h>
    #define CHIPS_ASSERT(c) assert(c)
#endif

void ui_mc6845_init(ui_mc6845_t* win, const ui_mc6845_desc_t* desc) {
    CHIPS_ASSERT(win && desc);
    CHIPS_ASSERT(desc->title);
    CHIPS_ASSERT(desc->mc6845);
    memset(win, 0, sizeof(ui_mc6845_t));
    win->title = desc->title;
    win->mc6845 = desc->mc6845;
    win->init_x = (float) desc->x;
    win->init_y = (float) desc->y;
    win->init_w = (float) ((desc->w == 0) ? 460 : desc->w);
    win->init_h = (float) ((desc->h == 0) ? 370 : desc->h);
    win->open = desc->open;
    win->valid = true;
    ui_chip_init(&win->chip, &desc->chip_desc);
}

void ui_mc6845_discard(ui_mc6845_t* win) {
    CHIPS_ASSERT(win && win->valid);
    win->valid = false;
}

// Draw register value.
// Change text colour if register's value has been modified from the default value.
void DrawRegValue(uint8_t& reg, const char* text, uint8_t default_val)
{
    bool bModified = false;
    bModified = reg != default_val;
    if (bModified) ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255,255,0,255));
    reg = ui_util_input_u8(text, reg); ImGui::NextColumn();
    if (bModified) ImGui::PopStyleColor();
}

static void _ui_mc6845_draw_state(ui_mc6845_t* win) {
    CHIPS_ASSERT(win && win->mc6845);
    mc6845_t* mc = win->mc6845;

    switch (mc->type) {
        case MC6845_TYPE_UM6845:    ImGui::Text("Type: UM6845"); break;
        case MC6845_TYPE_UM6845R:   ImGui::Text("Type: UM6845R"); break;
        case MC6845_TYPE_MC6845:    ImGui::Text("Type: MC6845"); break;
        default:                    ImGui::Text("Type: ???"); break;
    }
    ImGui::Separator();

    ImGui::Columns(2, "##regs", false);
    ImGui::SetColumnWidth(0, 124);
    ImGui::SetColumnWidth(1, 124);
    
    DrawRegValue(mc->h_total, "R0 HTotal", 63);
    DrawRegValue(mc->h_displayed, "R1 HDisp", 40);
    DrawRegValue(mc->h_sync_pos, "R2 HSyncPos", 46);
    DrawRegValue(mc->sync_widths, "R3 SyncWidth", 142);
    DrawRegValue(mc->v_total, "R4 VTotal", 38);
    DrawRegValue(mc->v_total_adjust, "R5 VTotalAdj", 0);
    DrawRegValue(mc->v_displayed, "R6 VDisp", 25);
    DrawRegValue(mc->v_sync_pos, "R7 VSyncPos", 30);
    DrawRegValue(mc->interlace_mode, "R8 Interl", 0);
    DrawRegValue(mc->max_scanline_addr, "R9 MaxScanl", 7);
    DrawRegValue(mc->cursor_start, "R10 CursStart", 0);
    DrawRegValue(mc->cursor_end, "R11 CursEnd", 0);
    DrawRegValue(mc->start_addr_hi, "R12 AddrHi", 30);
    DrawRegValue(mc->start_addr_lo, "R13 AddrLo", 0);
    DrawRegValue(mc->cursor_hi, "R14 CursHi", 0);
    DrawRegValue(mc->cursor_lo, "R15 CursLo", 0);
    DrawRegValue(mc->lightpen_hi, "R16 LPenHi", 0);
    DrawRegValue(mc->lightpen_lo, "R17 LPenLo", 0);
    ImGui::Columns();
    ImGui::Separator();
    ImGui::Text("Memory Addr: %04X  Row Start: %04X", mc->ma, mc->ma_row_start);
    ImGui::Text("Row Ctr:     %02X    Scanline Ctr: %02X", mc->v_ctr, mc->r_ctr);
    ImGui::Text("Hori Ctr:    %02X", mc->h_ctr);
    ImGui::Text("HSync Ctr:   %02X    VSync Ctr: %02X", mc->hsync_ctr, mc->vsync_ctr);
    ImGui::Text("Reg Sel:     R%d", mc->sel);
}

void ui_mc6845_draw(ui_mc6845_t* win) {
    CHIPS_ASSERT(win && win->valid && win->title && win->mc6845);
    if (!win->open) {
        return;
    }
    ImGui::SetNextWindowPos(ImVec2(win->init_x, win->init_y), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(win->init_w, win->init_h), ImGuiCond_Once);
    if (ImGui::Begin(win->title, &win->open)) {
        ImGui::BeginChild("##chip", ImVec2(176, 0), true);
        ui_chip_draw(&win->chip, win->mc6845->pins);
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("##state", ImVec2(0, 0), true);
        _ui_mc6845_draw_state(win);
        ImGui::EndChild();
    }
    ImGui::End();
}

#endif /* CHIPS_IMPL */
