#pragma once

#include "cbdos/language.hpp"
#include <cstdint>
#include <lvgl.h>

namespace cbdos {
namespace tablehub {
namespace lang {

enum class Str : uint16_t {
    // KDS View
    KDS_TITLE,
    KDS_STATUS_ACTIVE,
    KDS_NO_ORDERS,
    KDS_COL_PENDING,
    KDS_COL_PREPARING,
    KDS_COL_READY,

    // Tabletop View
    TABLE_TITLE,
    TABLE_HEADER,
    BTN_CALL_WAITER,
    BTN_ASK_BILL,
    ACT_CALL_WAITER,
    ACT_ASK_BILL,
    TOAST_REQ_SENT_FMT,
    MENU_CATEGORIES_TITLE,
    CAT_STARTERS,
    CAT_MAINS,
    CAT_DRINKS,
    CAT_DESSERTS,
    TOAST_CAT_OPENING,

    COUNT
};

inline const char* tr(Str id) {
    bool isEn = (cbdos::lang::getLanguage() == cbdos::lang::Lang::EN);

    if (isEn) {
        switch (id) {
            case Str::KDS_TITLE: return "Kitchen KDS";
            case Str::KDS_STATUS_ACTIVE: return LV_SYMBOL_BELL " ACTIVE KITCHEN (KDS)";
            case Str::KDS_NO_ORDERS: return "No pending orders";
            case Str::KDS_COL_PENDING: return "1. Pending";
            case Str::KDS_COL_PREPARING: return "2. In Preparation";
            case Str::KDS_COL_READY: return "3. Ready to Serve";

            case Str::TABLE_TITLE: return "TableHub Table";
            case Str::TABLE_HEADER: return LV_SYMBOL_HOME " TABLE 01 - DIGITAL MENU";
            case Str::BTN_CALL_WAITER: return LV_SYMBOL_CALL " Call Waiter";
            case Str::BTN_ASK_BILL: return LV_SYMBOL_CHARGE " Request Bill";
            case Str::ACT_CALL_WAITER: return "Call Waiter";
            case Str::ACT_ASK_BILL: return "Request Bill";
            case Str::TOAST_REQ_SENT_FMT: return "Request sent: %s";
            case Str::MENU_CATEGORIES_TITLE: return "Menu Categories";
            case Str::CAT_STARTERS: return "Starters & Tapas";
            case Str::CAT_MAINS: return "Main Courses";
            case Str::CAT_DRINKS: return "Drinks & Cocktails";
            case Str::CAT_DESSERTS: return "Desserts & Coffee";
            case Str::TOAST_CAT_OPENING: return "Opening category...";
            default: break;
        }
    }

    // Default / Spanish (sin acentos para compatibilidad de fuentes lvgl montserrat)
    switch (id) {
        case Str::KDS_TITLE: return "KDS Cocina";
        case Str::KDS_STATUS_ACTIVE: return LV_SYMBOL_BELL " COCINA ACTIVA (KDS)";
        case Str::KDS_NO_ORDERS: return "Sin comandas pendientes";
        case Str::KDS_COL_PENDING: return "1. Pendientes";
        case Str::KDS_COL_PREPARING: return "2. En Preparacion";
        case Str::KDS_COL_READY: return "3. Listos para Servir";

        case Str::TABLE_TITLE: return "TableHub Mesa";
        case Str::TABLE_HEADER: return LV_SYMBOL_HOME " MESA 01 - CARTA DIGITAL";
        case Str::BTN_CALL_WAITER: return LV_SYMBOL_CALL " Llamar Camarero";
        case Str::BTN_ASK_BILL: return LV_SYMBOL_CHARGE " Pedir Cuenta";
        case Str::ACT_CALL_WAITER: return "Llamar Camarero";
        case Str::ACT_ASK_BILL: return "Pedir Cuenta";
        case Str::TOAST_REQ_SENT_FMT: return "Solicitud enviada: %s";
        case Str::MENU_CATEGORIES_TITLE: return "Categorias de la Carta";
        case Str::CAT_STARTERS: return "Entrantes y Tapas";
        case Str::CAT_MAINS: return "Platos Principales";
        case Str::CAT_DRINKS: return "Bebidas y Cocteles";
        case Str::CAT_DESSERTS: return "Postres y Cafes";
        case Str::TOAST_CAT_OPENING: return "Abriendo categoria...";
        default: break;
    }
    return "";
}

} // namespace lang
} // namespace tablehub
} // namespace cbdos
