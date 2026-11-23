#pragma once

#include <stdint.h>
#include <stddef.h>

/* Entrada generica de inicializacion DCS para paneles MIPI-DSI en CBDos. */
typedef struct {
    int            cmd;
    const void    *data;
    size_t         data_bytes;
    unsigned int   delay_ms;
} cbdos_panel_init_cmd_t;

static const uint8_t jc4880_cmd_ff_13[]    = {0x77, 0x01, 0x00, 0x00, 0x13};
static const uint8_t jc4880_cmd_ef_08[]    = {0x08};
static const uint8_t jc4880_cmd_ff_10[]    = {0x77, 0x01, 0x00, 0x00, 0x10};
static const uint8_t jc4880_cmd_c0[]       = {0x63, 0x00};
static const uint8_t jc4880_cmd_c1[]       = {0x0D, 0x02};
static const uint8_t jc4880_cmd_c2[]       = {0x10, 0x08};
static const uint8_t jc4880_cmd_cc[]       = {0x10};
static const uint8_t jc4880_cmd_b0_gamma[] = {0x80, 0x09, 0x53, 0x0C, 0xD0, 0x07, 0x0C, 0x09, 0x09, 0x28, 0x06, 0xD4, 0x13, 0x69, 0x2B, 0x71};
static const uint8_t jc4880_cmd_b1_gamma[] = {0x80, 0x94, 0x5A, 0x10, 0xD3, 0x06, 0x0A, 0x08, 0x08, 0x25, 0x03, 0xD3, 0x12, 0x66, 0x6A, 0x0D};
static const uint8_t jc4880_cmd_ff_11[]    = {0x77, 0x01, 0x00, 0x00, 0x11};
static const uint8_t jc4880_cmd_b0[]       = {0x5D};
static const uint8_t jc4880_cmd_b1[]       = {0x58};
static const uint8_t jc4880_cmd_b2[]       = {0x87};
static const uint8_t jc4880_cmd_b3[]       = {0x80};
static const uint8_t jc4880_cmd_b5[]       = {0x4E};
static const uint8_t jc4880_cmd_b7[]       = {0x85};
static const uint8_t jc4880_cmd_b8[]       = {0x21};
static const uint8_t jc4880_cmd_b9[]       = {0x10, 0x1F};
static const uint8_t jc4880_cmd_bb[]       = {0x03};
static const uint8_t jc4880_cmd_bc[]       = {0x00};
static const uint8_t jc4880_cmd_c1_78[]    = {0x78};
static const uint8_t jc4880_cmd_c2_78[]    = {0x78};
static const uint8_t jc4880_cmd_d0[]       = {0x88};
static const uint8_t jc4880_cmd_e0[]       = {0x00, 0x3A, 0x02};
static const uint8_t jc4880_cmd_e1[]       = {0x04, 0xA0, 0x00, 0xA0, 0x05, 0xA0, 0x00, 0xA0, 0x00, 0x40, 0x40};
static const uint8_t jc4880_cmd_e2[]       = {0x30, 0x00, 0x40, 0x40, 0x32, 0xA0, 0x00, 0xA0, 0x00, 0xA0, 0x00, 0xA0, 0x00};
static const uint8_t jc4880_cmd_e3[]       = {0x00, 0x00, 0x33, 0x33};
static const uint8_t jc4880_cmd_e4[]       = {0x44, 0x44};
static const uint8_t jc4880_cmd_e5[]       = {0x09, 0x2E, 0xA0, 0xA0, 0x0B, 0x30, 0xA0, 0xA0, 0x05, 0x2A, 0xA0, 0xA0, 0x07, 0x2C, 0xA0, 0xA0};
static const uint8_t jc4880_cmd_e6[]       = {0x00, 0x00, 0x33, 0x33};
static const uint8_t jc4880_cmd_e7[]       = {0x44, 0x44};
static const uint8_t jc4880_cmd_e8[]       = {0x08, 0x2D, 0xA0, 0xA0, 0x0A, 0x2F, 0xA0, 0xA0, 0x04, 0x29, 0xA0, 0xA0, 0x06, 0x2B, 0xA0, 0xA0};
static const uint8_t jc4880_cmd_eb[]       = {0x00, 0x00, 0x4E, 0x4E, 0x00, 0x00, 0x00};
static const uint8_t jc4880_cmd_ec[]       = {0x08, 0x01};
static const uint8_t jc4880_cmd_ed[]       = {0xB0, 0x2B, 0x98, 0xA4, 0x56, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xF7, 0x65, 0x4A, 0x89, 0xB2, 0x0B};
static const uint8_t jc4880_cmd_ef[]       = {0x08, 0x08, 0x08, 0x45, 0x3F, 0x54};
static const uint8_t jc4880_cmd_ff_00[]    = {0x77, 0x01, 0x00, 0x00, 0x00};
static const uint8_t jc4880_cmd_nop[]      = {0x00};

static const cbdos_panel_init_cmd_t cbdos_panel_init_cmds[] = {
    {0xFF, jc4880_cmd_ff_13,    sizeof(jc4880_cmd_ff_13),    0},
    {0xEF, jc4880_cmd_ef_08,    sizeof(jc4880_cmd_ef_08),    0},
    {0xFF, jc4880_cmd_ff_10,    sizeof(jc4880_cmd_ff_10),    0},
    {0xC0, jc4880_cmd_c0,       sizeof(jc4880_cmd_c0),       0},
    {0xC1, jc4880_cmd_c1,       sizeof(jc4880_cmd_c1),       0},
    {0xC2, jc4880_cmd_c2,       sizeof(jc4880_cmd_c2),       0},
    {0xCC, jc4880_cmd_cc,       sizeof(jc4880_cmd_cc),       0},
    {0xB0, jc4880_cmd_b0_gamma, sizeof(jc4880_cmd_b0_gamma), 0},
    {0xB1, jc4880_cmd_b1_gamma, sizeof(jc4880_cmd_b1_gamma), 0},
    {0xFF, jc4880_cmd_ff_11,    sizeof(jc4880_cmd_ff_11),    0},
    {0xB0, jc4880_cmd_b0,       sizeof(jc4880_cmd_b0),       0},
    {0xB1, jc4880_cmd_b1,       sizeof(jc4880_cmd_b1),       0},
    {0xB2, jc4880_cmd_b2,       sizeof(jc4880_cmd_b2),       0},
    {0xB3, jc4880_cmd_b3,       sizeof(jc4880_cmd_b3),       0},
    {0xB5, jc4880_cmd_b5,       sizeof(jc4880_cmd_b5),       0},
    {0xB7, jc4880_cmd_b7,       sizeof(jc4880_cmd_b7),       0},
    {0xB8, jc4880_cmd_b8,       sizeof(jc4880_cmd_b8),       0},
    {0xB9, jc4880_cmd_b9,       sizeof(jc4880_cmd_b9),       0},
    {0xBB, jc4880_cmd_bb,       sizeof(jc4880_cmd_bb),       0},
    {0xBC, jc4880_cmd_bc,       sizeof(jc4880_cmd_bc),       0},
    {0xC1, jc4880_cmd_c1_78,    sizeof(jc4880_cmd_c1_78),    0},
    {0xC2, jc4880_cmd_c2_78,    sizeof(jc4880_cmd_c2_78),    0},
    {0xD0, jc4880_cmd_d0,       sizeof(jc4880_cmd_d0),       0},
    {0xE0, jc4880_cmd_e0,       sizeof(jc4880_cmd_e0),       0},
    {0xE1, jc4880_cmd_e1,       sizeof(jc4880_cmd_e1),       0},
    {0xE2, jc4880_cmd_e2,       sizeof(jc4880_cmd_e2),       0},
    {0xE3, jc4880_cmd_e3,       sizeof(jc4880_cmd_e3),       0},
    {0xE4, jc4880_cmd_e4,       sizeof(jc4880_cmd_e4),       0},
    {0xE5, jc4880_cmd_e5,       sizeof(jc4880_cmd_e5),       0},
    {0xE6, jc4880_cmd_e6,       sizeof(jc4880_cmd_e6),       0},
    {0xE7, jc4880_cmd_e7,       sizeof(jc4880_cmd_e7),       0},
    {0xE8, jc4880_cmd_e8,       sizeof(jc4880_cmd_e8),       0},
    {0xEB, jc4880_cmd_eb,       sizeof(jc4880_cmd_eb),       0},
    {0xEC, jc4880_cmd_ec,       sizeof(jc4880_cmd_ec),       0},
    {0xED, jc4880_cmd_ed,       sizeof(jc4880_cmd_ed),       0},
    {0xEF, jc4880_cmd_ef,       sizeof(jc4880_cmd_ef),       0},
    {0xFF, jc4880_cmd_ff_00,    sizeof(jc4880_cmd_ff_00),    0},
    {0x11, jc4880_cmd_nop,      sizeof(jc4880_cmd_nop),      120},
    {0x20, jc4880_cmd_nop,      sizeof(jc4880_cmd_nop),      10},
    {0x29, jc4880_cmd_nop,      sizeof(jc4880_cmd_nop),      20},
};

#define CBDOS_PANEL_INIT_CMDS_SIZE \
    (sizeof(cbdos_panel_init_cmds) / sizeof(cbdos_panel_init_cmds[0]))
