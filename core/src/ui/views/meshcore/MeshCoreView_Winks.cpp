#include "../MeshCoreView.hpp"
#include "../UIManager.hpp"
#include "../themes/DefaultTheme.h"
#include "cbdos/display.hpp"
#include "cbdos/serial.hpp"
#include "cbdos/storage.hpp"
#include "cbdos/persistence.hpp"
#include "cbdos/meshcore/meshcore_store.hpp"
#include "cbdos/meshcore/mesh_emoji.hpp"
#include "../../assets/lottie_sample.h"
#include "../../assets/wink_star_assets.h"
#include "MeshCoreCommon.hpp"
#include <algorithm>
#include <map>
#include <vector>

namespace cbdos {
namespace ui {
using namespace meshcore_ui;

void MeshCoreView::sendWink(int winkId) {
    const char* token = meshcore::emoji::winkTokenForId(winkId);
    auto& client = meshcore::MeshCoreClient::getInstance();
    bool ok = false;
    if (m_pickerTarget == 1 && !m_activePrefix.empty()) {
        ok = client.sendDMByPrefix(m_activePrefix, token);
        if (ok) {
            m_lastRenderedConvMsgs = (size_t)-1;
            m_lastRenderedConvPending = (size_t)-1;
            refreshConversation();
        }
    } else {
        ok = client.sendChannelMessage(m_channelIdx, token);
        if (ok) refreshChatLog();
    }
    if (!ok) UIManager::showToast("Dongle desconectado. Revisa Radio.");
}

void MeshCoreView::showEmojiPicker(int target) {
    if (!m_overlay || !m_overlayCard) return;
    m_pickerTarget = target;
    hideOverlay();
    lv_obj_remove_flag(m_overlay, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* title = lv_label_create(m_overlayCard);
    lv_label_set_text(title, target == 1 ? "Emoji / Wink -> DM" : "Emoji / Wink -> Canal");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x00E5FF), 0);

    lv_obj_t* hint = lv_label_create(m_overlayCard);
    lv_label_set_text(hint, "Winks: solo 5-6 B en el aire. El resto ve :star: / :cat:.");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, DefaultTheme::getMutedTextColor(), 0);
    lv_obj_set_width(hint, LV_PCT(100));
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);

    // Fila de winks: tap = enviar directo estilo Telegram.
    lv_obj_t* winkRow = lv_obj_create(m_overlayCard);
    lv_obj_set_size(winkRow, LV_PCT(100), 44);
    lv_obj_set_style_bg_opa(winkRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(winkRow, 0, 0);
    lv_obj_set_flex_flow(winkRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(winkRow, 8, 0);
    DefaultTheme::disableScroll(winkRow);

    lv_obj_t* bStar = makeButton(winkRow, "Estrella " LV_SYMBOL_PLAY, 130, 36, 0x1B5E20);
    lv_obj_set_user_data(bStar, (void*)(uintptr_t)0);
    lv_obj_add_event_cb(bStar, winkSendCb, LV_EVENT_CLICKED, this);

    lv_obj_t* bCat = makeButton(winkRow, "Gato " LV_SYMBOL_PLAY, 110, 36, 0x1B5E20);
    lv_obj_set_user_data(bCat, (void*)(uintptr_t)1);
    lv_obj_add_event_cb(bCat, winkSendCb, LV_EVENT_CLICKED, this);

    // Caritas amarillas: tap = insertar en el input (no se envia solo).
    size_t nFaces = 0;
    const auto* faces = meshcore::emoji::emojiTable(nFaces);
    for (size_t i = 0; i < nFaces; ++i) {
        lv_obj_t* b = makeButton(m_overlayCard, faces[i].name, 220, 32, 0);
        lv_obj_set_user_data(b, (void*)(uintptr_t)i);
        lv_obj_add_event_cb(b, emojiPickCb, LV_EVENT_CLICKED, this);
    }

    lv_obj_t* bClose = makeButton(m_overlayCard, "Cerrar", 220, 36, 0);
    lv_obj_add_event_cb(bClose, overlayCloseCb, LV_EVENT_CLICKED, this);
}

void MeshCoreView::freeChatWinkBufs() {
#if LV_USE_LOTTIE
    for (auto* b : m_chatWinkBufs) {
        if (b) lv_draw_buf_destroy(b);
    }
#endif
    m_chatWinkBufs.clear();
}

void MeshCoreView::freeConvWinkBufs() {
#if LV_USE_LOTTIE
    for (auto* b : m_convWinkBufs) {
        if (b) lv_draw_buf_destroy(b);
    }
#endif
    m_convWinkBufs.clear();
}

lv_obj_t* MeshCoreView::createInlineWink(lv_obj_t* bubble, int winkId, bool isDM) {
    // Cuadro de animacion dentro del chat: 96px en loop continuo mientras
    // la vista esta visible; tap = pausa/retomar. Al salir de la vista los
    // objetos mueren con sus animaciones (cero CPU fuera de la vista).
    lv_obj_t* card = lv_obj_create(bubble);
    lv_obj_set_size(card, 124, 142);
    lv_obj_set_style_radius(card, 10, 0);
    lv_obj_set_style_pad_all(card, 6, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(card, 2, 0);
    DefaultTheme::disableScroll(card);

    char cap[48];
#if LV_USE_LOTTIE
    const char* src = nullptr;
    size_t srcLen = 0;
    bool missing = false;
    if (winkId == 1) {
        // Gato: se lee una vez de la SD y se cachea (ThorVG copia el JSON
        // al cargar, asi que el std::string puede vivir en el miembro).
        if (m_catJson.empty()) {
            m_catJson = cbdos::storage::readFile("/sdcard/lottie/catmov.json");
            if (m_catJson.empty()) m_catJson = cbdos::storage::readFile("/sdcard/catmov.json");
        }
        if (!m_catJson.empty()) {
            src = m_catJson.c_str();
            srcLen = m_catJson.size();
        } else {
            src = cbdos::assets::WINK_STAR_JSON;
            srcLen = cbdos::assets::WINK_STAR_JSON_SIZE;
            missing = true;
        }
    } else {
        // Estrella: override desde SD si existe, si no la embebida segura
        // (solo circulos; se evita "sr"/polystar que este ThorVG no traga).
        if (m_starSd.empty()) {
            m_starSd = cbdos::storage::readFile("/sdcard/lottie/star.json");
        }
        if (!m_starSd.empty()) {
            src = m_starSd.c_str();
            srcLen = m_starSd.size();
        } else {
            src = cbdos::assets::WINK_STAR_JSON;
            srcLen = cbdos::assets::WINK_STAR_JSON_SIZE;
        }
    }

    lv_obj_t* anim = lv_lottie_create(card);
    lv_obj_set_size(anim, 96, 96);
    lv_draw_buf_t* db =
        lv_draw_buf_create(96, 96, LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED, LV_STRIDE_AUTO);
    if (db && src) {
        if (isDM) m_convWinkBufs.push_back(db);
        else m_chatWinkBufs.push_back(db);
        lv_lottie_set_draw_buf(anim, db);
        lv_lottie_set_src_data(anim, src, srcLen);
        // Loop continuo mientras la vista esta visible; el tap pausa/retoma.
        lv_anim_set_repeat_count(lv_lottie_get_anim(anim), LV_ANIM_REPEAT_INFINITE);
        lv_obj_set_user_data(anim, (void*)(uintptr_t)0);  // 0 = en loop
        lv_obj_add_flag(anim, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(anim, winkReplayCb, LV_EVENT_CLICKED, this);
    } else {
        if (db) lv_draw_buf_destroy(db);
        lv_obj_t* err = lv_label_create(card);
        lv_label_set_text(err, "Sin memoria anim.");
        lv_obj_set_style_text_font(err, &lv_font_montserrat_12, 0);
    }
    if (missing) {
        snprintf(cap, sizeof(cap), "falta catmov.json: estrella");
    } else {
        snprintf(cap, sizeof(cap), "%s (%s) - toca = pausa",
                 meshcore::emoji::winkNameForId(winkId),
                 meshcore::emoji::winkTokenForId(winkId));
    }
#else
    (void)winkId;
    snprintf(cap, sizeof(cap), "Lottie OFF en build");
#endif

    lv_obj_t* lblCap = lv_label_create(card);
    lv_label_set_text(lblCap, cap);
    lv_obj_set_style_text_font(lblCap, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblCap, DefaultTheme::getMutedTextColor(), 0);
    lv_obj_set_width(lblCap, LV_PCT(100));
    lv_label_set_long_mode(lblCap, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(lblCap, LV_TEXT_ALIGN_CENTER, 0);
    return card;
}

void MeshCoreView::emojiBtnCb(lv_event_t* e) {
    auto* self = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    lv_obj_t* target = (lv_obj_t*)lv_event_get_target(e);
    if (!self || !target) return;
    int tgt = (int)(uintptr_t)lv_obj_get_user_data(target);
    self->showEmojiPicker(tgt);
}

void MeshCoreView::emojiPickCb(lv_event_t* e) {
    auto* self = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    lv_obj_t* target = (lv_obj_t*)lv_event_get_target(e);
    if (!self || !target) return;
    size_t idx = (size_t)(uintptr_t)lv_obj_get_user_data(target);
    std::string txt = meshcore::emoji::sendTextForEmoji(idx);
    lv_obj_t* ta = (self->m_pickerTarget == 1) ? self->m_taDmInput : self->m_taInput;
    if (ta && lv_obj_is_valid(ta)) {
        lv_textarea_add_text(ta, txt.c_str());
    }
    self->hideOverlay();
}

void MeshCoreView::winkSendCb(lv_event_t* e) {
    auto* self = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    lv_obj_t* target = (lv_obj_t*)lv_event_get_target(e);
    if (!self || !target) return;
    int id = (int)(uintptr_t)lv_obj_get_user_data(target);
    self->hideOverlay();
    self->sendWink(id);
}

void MeshCoreView::winkReplayCb(lv_event_t* e) {
    lv_obj_t* anim = (lv_obj_t*)lv_event_get_target(e);
    if (!anim || !lv_obj_is_valid(anim)) return;
#if LV_USE_LOTTIE
    // Tap = pausa / retomar. Pausar deja terminar la pasada actual y la
    // animacion sale del scheduler (no gasta CPU hasta retomar).
    bool paused = (bool)(uintptr_t)lv_obj_get_user_data(anim);
    lv_anim_t* a = lv_lottie_get_anim(anim);
    if (!a) return;
    if (paused) {
        lv_anim_set_repeat_count(a, LV_ANIM_REPEAT_INFINITE);
        lv_anim_start(a);  // seguro aunque siga corriendo (deduplica)
        lv_obj_set_user_data(anim, (void*)(uintptr_t)0);
    } else {
        lv_anim_set_repeat_count(a, 1);
        lv_obj_set_user_data(anim, (void*)(uintptr_t)1);
    }
#endif
}


} // namespace ui
} // namespace cbdos
