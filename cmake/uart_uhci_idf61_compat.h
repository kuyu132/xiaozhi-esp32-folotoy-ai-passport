#pragma once

#include "esp_private/gdma.h"

// uart-uhci 0.3.2 uses the IDF 6.0 API, which returned two alignment values.
// IDF 6.1 returns a structure and distinguishes encrypted external memory.
static inline esp_err_t uart_uhci_get_alignment_constraints_compat(
    gdma_channel_handle_t channel, size_t* internal_alignment, size_t* external_alignment) {
    gdma_channel_alignment_info_t alignment = {};
    esp_err_t error = gdma_get_channel_alignment_constraints(channel, &alignment);
    if (error == ESP_OK) {
        *internal_alignment = alignment.int_mem_alignment;
        *external_alignment = alignment.ext_enc_mem_alignment;
    }
    return error;
}

#define gdma_get_alignment_constraints(channel, internal_alignment, external_alignment) \
    uart_uhci_get_alignment_constraints_compat(channel, internal_alignment, external_alignment)
